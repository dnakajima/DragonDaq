///////////////////////////////////////////////////////////////////////////////////////////
// DragonDaqM.cpp
// by Kazuma Ishio Univ. of Tokyo
// Last update on 2015/04/27
//
// bugs below were fixed
//   2015/04/27
//   - ndaq was modified from int to unsigned int
//   2015/04/07
//   - Problem in calculation Throughput measurement in large data acquisition 
//   - Improper start point of time measurement 
//           (500usec sleep before start reading socket
//            not to include time to wait connection establishment)
//   - Improper treatments for various read depth
//
// ****Function****
//  This program
//    (1)collects datas from multiple Dragon FEBs by TCP/IP connection.
//    (2)can save datas from them.
//    (3)measures throughput of taking data from FEBs.
//    (4)can also measure each read() function for FEBs (close inspection mode).
//
// ****Usage****
// 0.Deploy DragonDaqM.cpp, DragonDaqM.hh, and Connection.conf 
// 1.If no executable file, Compile these with: 
//         ****************************************************
//         *       g++ -o DragonDaqM DragonDaqM.cpp -lrt        *
//         ****************************************************
// 2.Edit connection configuration in Connection.conf
//    which is a table of IP address and port number of the Dragon FEBs.
//   Even if you rewrite ip address, you don't have to re-compile.
//
// 3.Submit this with: 
//         **********************************************************
//         *    ./DragonDaqM -<optarg> <argValues>
//         **********************************************************
//     note1:If you want to know how to specify options more, just type ./DragonDaqM -h
//     note2:The option -i (input frequency) have nothing to do with DAQ, it is just for measurement.
//     note3:Unless you specify -s, data will not be created. 
//            Instead, just blank file will be created.
///////////////////////////////////////////////////////////////////////////////////////////

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <iostream>
//#include "termcolor.h"
#include <stdlib.h>

#include <time.h>     //for measuring time
#include <sys/time.h> //for making filename

#include <errno.h>
#include <ctime>
#include <unistd.h>   //for inspecting directory
#include <sys/stat.h> //for making directory

#include <fstream>
#include <sstream>
#include <string>

#include "DragonDaqM.hh"


///////////////////////////////////////////////////////////////////////////////////////////
// main program
///////////////////////////////////////////////////////////////////////////////////////////

//eth_dragon sock[48];
 
#include <getopt.h>
#include <stdlib.h>

struct option options[] =
  {
    {"help"     ,no_argument       ,NULL ,'h'},
    {"infreq"   ,required_argument ,NULL ,'i'},
    {"ndata"    ,required_argument ,NULL ,'n'},
    {"output"   ,required_argument ,NULL ,'o'},
    {"readdepth",required_argument ,NULL ,'r'},
    {"save"     ,no_argument       ,NULL ,'s'},
    {"version"  ,required_argument ,NULL ,'v'},
    {"closeinspect" ,no_argument   ,NULL ,'c'},
    {"configfile" ,required_argument   ,NULL ,'f'},
    {"prescale" ,required_argument   ,NULL ,'p'},
    {"threshold" ,required_argument   ,NULL ,'t'},
    {0,0,0,0}
  };

int main(int argc, char *argv[])
{
  using namespace std;
  
  int rddepth=30;
  int dragonVer=5;
  int infreq=0;
  bool datacreate=false;
  unsigned int ndaq=1000;
  string fileNameHeader;
  stringstream fileName;
  bool closeinspect=false;
  string configfile = "Connection.conf";
  int PreScaleFactor = 1;
  unsigned int ADCthreshold = 0;
  /******************************************/
  //  Handling input arguments
  /******************************************/
  int opt;
  int index;
  while((opt=getopt_long(argc,argv,"hi:n:o:r:sv:cf:p:t:",options,&index)) !=-1){
    switch(opt){
    case 'h':
 TERM_COLOR_RED;
      printf("Usage:\n");
      printf("you run this program with some options like\n");
      printf("%s -o MyFileNameHeader -s -r 50 -n 10000\n",argv[0]);
      printf("***** LIST OF OPTIONS *****\n");
      printf("-i|--infreq <Input Frequency[Hz]>    : Trigger frequency. \n");
      printf("-o|--output <FileNameHeader>         : Header of data file .\n");
      printf("-s|--save                            : Datasave. Default is false.\n");
      printf("-r|--readdepth <ReadDepth>           : Default is 30.\n");
      printf("-n|--ndaq <#of data to acquire>      : Default is 1000.\n");
      printf("-v|--version   <Dragon Version>      : Default is 5.\n");
      printf("-c|--closeinspect                    : Default is false.\n");
      printf("-f|--configfile                      : .\n");
      printf("-p|--prescale                        : Default is 1 (no pre-scaling) .\n");
      printf("-t|--threshold                       : Default is 0 .\n");
      printf("********* CAUTION ********\n");
      printf("Make sure to specify readdepth to Dragon through rpcp command.\n");
      printf("If RD=1024,limit is 3kHz at 1Gbps. so 10000events will take 10s. \n");
      printf("If RD=30,limit is 120kHz at 1Gbps. so 1000000events will take 10s. \n");
      printf("Close inspection mode will store\n");
      printf("  time differences between events.\n");
      printf("  as a file named RDXXinfreqXX_MMDD_HHMMSS.dat\n");
      printf("  in DragonDaqMes directory which will be made automatically.\n");
      printf("\n");
  TERM_COLOR_RESET;
      exit(0);
    case 'i':
      infreq=atoi(optarg);
      break;
    case 'o':
      //sprintf(fileNameHeader,"%s",optarg);
      //if(sizeof(optarg)==0);
      //      printf("%d\n",sizeof(optarg));
      fileNameHeader=optarg;
      //outputfile=optarg;
      break;
    case 's':
      datacreate=true;
      break;
    case 'n':
      ndaq=(unsigned int)atoi(optarg);
      printf("ndaq %d\n",ndaq);
      break;
    case 'r':
      rddepth=atoi(optarg);
      break;
    case 'v':
      dragonVer=atoi(optarg);
     break;
    case 'c':
      closeinspect=true;
      break;
    case 'f' :
      configfile=optarg;
      break;
    case 'p' :
      PreScaleFactor = atoi(optarg);
      break;
    case 't' :
      ADCthreshold = atoi(optarg);
      break;
    default:
      printf("%s -h for usage\n",argv[0]);
    }
  }
  printf("");
  fileName<<fileNameHeader<<"RD"<<rddepth;

  //Definition of Event Size
  int evsize;
  int HeaderSize=0;
  if(dragonVer>4)
    {
      //evsize=16*(rddepth*2+2);
      evsize=2+2+4+4+4+8+8+2*8+2*8+2*8*2*rddepth; //bytes
      HeaderSize=
	2+ //0xAAAA
	2+ //PPS counter (2byte)
	4+ // 10MHzCounter (4byte)
	4+ // EventCounter (4byte)
	4+ // TriggerCounter (4byte)
	8+ // Local 133 MHz clock (8byte)
	8+ // 0xDDDD_DDDD_DDDD_DDDD
	2*8+ // flag
	2*8; // first capacitor  bytes 
    }
  else
    {
      evsize=16*(rddepth*2+3);
      HeaderSize =
	16+ //EventCount+TriggerCount+ClockCount;
	2*8+ // flag
	2*8; // first capacitor id
    }
  unsigned char __g_buff[evsize];//receive buffer
  //Definition of Data Size
  unsigned long lReadBytes = (unsigned long)evsize*(unsigned long)ndaq; //data size to read.
  TERM_COLOR_BLUE;
  printf("*********************************************\n");
  printf("*********************************************\n");
  printf("**                Dragon DAQ               **\n");
  printf("**          For multiple clusters          **\n");
  printf("**            K.Ishio 2015 April           **\n");
  printf("**                                         **\n");
  printf("**  Aquired Data:                          **\n");
  printf("**    %s_FEBNN.dat from connection #NN    \n",fileName.str().c_str());
  printf("**  Measurement Data:                      **\n");
  printf("**    Summary is  DragonDaq.dat            **\n");
  printf("**    Close inspection is in DragonDaqMes  **\n");
  printf("*********************************************\n");
  printf("*********************************************\n");
  TERM_COLOR_RESET;

 /******************************************/
  //  Difinitions for Close Inspection
  /******************************************/
  unsigned long long llstartdiffusec;
  unsigned long long lltime_diff[1000]={0};
  
  /******************************************/
  //  Reading Connection Configuration
  /******************************************/
  char szAddr[48][16];
  std::string IPAddr[48];
  unsigned short shPort[48]={0};
  unsigned long lConnected[48] ={0};
  //  const char *ConfFile = "Connection.conf";
  const char *ConfFile = configfile.c_str();
  cout<<"Config file = "<<configfile<<endl;
  std::ifstream ifs(ConfFile);
  std::string str;
  int nServ=0;
  while (std::getline(ifs,str)){
    if(str[0]== '#' || str.length()==0)continue;
    std::istringstream iss(str);
    iss >> szAddr[nServ] >> shPort[nServ];
    nServ++;
  }
  for(int nserver=0;nserver<nServ;nserver++)
    {
      IPAddr[nserver] = szAddr[nserver];
    }

  if(nServ>48){
    printf("The number of connections excessed limit.");
    exit(1);
  }
  cout<<"Num Server = "<<nServ<<endl;

  /******************************************/
  //  preparation of measurement summary file
  /******************************************/
  stringstream daqmesfile;
  daqmesfile<<"DragonDaqM_RD"<<rddepth<<".dat";
  FILE *fp_ms;
  bool isnewfile;
  if ((fp_ms = fopen(daqmesfile.str().c_str(),"r"))== NULL)
    {
      isnewfile=true;
    }
  else
    {
      std::string tempstr;
      int frddepth;
      fscanf(fp_ms, "The result of DragonDaqM RD%d\n",&frddepth);
      if(frddepth!=rddepth)
	{
	  printf("Confirm readdepth you specify and that in %s\n",daqmesfile.str().c_str());
	  //	  exit(EXIT_FAILURE);
	}
    }

  if ((fp_ms = fopen(daqmesfile.str().c_str(),"a"))== NULL)
    {
      printf("output file open error! exit");
      exit(EXIT_FAILURE);
    }
  else if(isnewfile)
    {
      fprintf(fp_ms,"The result of DragonDaqM RD%d\n",rddepth);
      fprintf(fp_ms,"InFreq[Hz] ");
      for(int i=0;i<nServ;i++)fprintf(fp_ms,"RdFreq%d[Hz] RdRate%d[Mbps] ",i,i);
      fprintf(fp_ms,"\n");
      
      //printf("result is NULL ,fp_ms = %d\n",fp_ms);
      //      std::cout<<"fp_ms="<<fp_ms<<std::endl;
    }
  else{
    printf("The file %s already exists. Data will be added to it.\n",daqmesfile.str().c_str());
    //printf("result is not NULL ,fp_ms = %d\n" , fp_ms);
  }

   /******************************************/
  // Preparation of Data File
  /******************************************/
  //Initialization of Data File
  char datafile[48][128];
  FILE *fp_d[48];
  // if(nServ==1)
  //   {
  //     sprintf(datafile[0],"%s.dat",outputfile,i);      
  //     fp_d[0] = fopen(datafile[0],"wb");
  //   }
  // else
    // {
      for(int i =0;i<nServ;i++)
	{
	  //	  sprintf(datafile[i],"%s_FEB%d.dat",fileName.str().c_str(),i);
	  int DragonId = atoi(IPAddr[i].substr(10).c_str());
	  sprintf(datafile[i],"%s_FEB%d_IP%d.dat",fileName.str().c_str(),i, DragonId);
	  cout<<"File "<<i+1<<" "<<datafile[i]<<endl;
	  fp_d[i] = fopen(datafile[i],"wb");
	}
    // }

  /******************************************/
  //  Connection Initialization
  /******************************************/
  int sock[48];
  int isconnect=0;
  for(int i =0;i<nServ;i++)
    {
      printf("read from server(%s:%u) %lu bytes\n",szAddr[i],shPort[i],lReadBytes);
      sock[i] = ConnectTcp(szAddr[i],shPort[i],lConnected[i]);
      printf("connection established\n");
      printf("lConnected[%d]=%lu\n",i,lConnected[i]);
      if(sock[i]<0)isconnect=1;
      //      usleep(10000);
    }


  /******************************************/
  //  Data Extraction
  /******************************************/
  //Maximum value of file discriptor
  int maxfd=sock[0];

  int NumberOfEvents[48]={0};
  int WrittenNumberOfEvents[48]={0};
  unsigned short tempADCcount=0;
  int DataCorruption[48]={0};
  int PrevDataCorruption[48]={0};
  if(isconnect==0)
    {
      memset(__g_buff,0,sizeof(__g_buff));		
      fd_set fds, readfds;
      FD_ZERO(&readfds);
      for(int i=0;i<nServ;i++)FD_SET(sock[i], &readfds);
      for(int i=1;i<nServ;i++)
	{
	  if(sock[i]>maxfd)maxfd=sock[i];
	}

      struct timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 10000;
      int n= 0;
      unsigned long long llRead[48] = {0};
      struct timespec tsStart,tsEnd,tsRStart;
      struct timespec tsctime1,tsctime2;
      int readcount =-1;
      clock_gettime(CLOCK_REALTIME,&tsStart);
      int j=0;

      clock_gettime(CLOCK_REALTIME,&tsctime1);
      tsRStart=tsctime1;
      llstartdiffusec = GetRealTimeInterval(&tsStart,&tsctime1);
      bool RunEnd=false;
      while(!RunEnd)
	{
	  memcpy(&fds,&readfds,sizeof(fd_set));
      	  select(maxfd+1, &fds, NULL, NULL,&tv);
	  if(readcount==0)
	    {
	      //usleep(500000);
	      // clock_gettime(CLOCK_REALTIME,&tsctime1);
	      // tsRStart=tsctime1;
	      // llstartdiffusec = GetRealTimeInterval(&tsStart,&tsctime1);
	      // readcount++;
	    }

	  // {
	  //   if( FD_ISSET(sock[0], &fds) )
	  //     {
	  // 	//printf("come here %d\n",__LINE__);
	  // 	int n=0;
	  // 	while(n<evsize)
	  // 	  {
	  // 	    int ret = read( sock[0],__g_buff+n,evsize-n);
	  // 	    if(ret<0)
	  // 	      {
	  // 		fprintf(fp_ms,"read() from sock[%d] failed\n",0);
	  // 		exit(1);
	  // 	      }
	  // 	    n+=ret;
	  // 	  }
	  // 	//printf("FEB1 read %d \n ",n);
	  // 	if(datacreate)
	  // 	  {
	  // 	    fwrite(__g_buff,n,1,fp_d[0]);
	  // 	  }
	  // 	if(j<1000)
	  // 	  {
	  // 	    clock_gettime(CLOCK_REALTIME,&tsctime2);					     
	  // 	    lltime_diff[j] = GetRealTimeInterval(&tsctime1,&tsctime2);
	  // 	    tsctime1=tsctime2;
	  // 	    //cout<<j<<endl;
	  // 	  }
	  // 	readcount++;
	  // 	//if(readcount%100==0)printf("n=%d\n",n);
	  // 	llRead[0] += (unsigned long long)n;
	  // 	if( llRead[0] >= (unsigned long long)lReadBytes ) 
	  // 	  {
	  // 	    clock_gettime(CLOCK_REALTIME,&tsEnd);
	  // 	    printf("finished here \n");
	  // 	    break;
	  // 	  }
	  // 	j++;
	  //     }/**if(FD_ISSET(sock1,&fds))**/
	  // }

	  // printf("come here %d\n",__LINE__);
	  for(int i=0;i<nServ;i++){
	    if( FD_ISSET(sock[i], &fds) )
	      {
		int n=0;
		while(n<evsize)
		  {
		    int ret = read( sock[i],__g_buff+n,evsize-n);
		    if(ret<0)
		      {
			fprintf(fp_ms,"read() from sock[%d] failed\n",i);
			exit(1);
		      }
		    n+=ret;
		  }

		NumberOfEvents[i]++;
		PrevDataCorruption[i]=0;
		DataCorruption[i]=0;

		int first_record=999999;
		int last_record=-1;
		int latest_value=0;
		  
		if(datacreate==1)
		  {
	
		    //		    for(int b=HeaderSize; b<evsize;b++)
		    for(int b=HeaderSize; b<HeaderSize+16*rddepth*2;b+=2)
		      {
			unsigned char tempbuf[32];
			tempbuf[0] = __g_buff[b+1];
			tempbuf[1] = __g_buff[b];
			memcpy((char *)&tempADCcount,tempbuf,sizeof(unsigned short));
			//			cout<<"board["<<i<<"] "<<b<<" "<<tempADCcount<<endl;

			int slice=((b-HeaderSize)/16)%40;

			if(tempADCcount<ADCthreshold && slice!=0)
			  //			if(tempADCcount<ADCthreshold)
			  {
			    if(b>last_record) last_record=b;
			    if(b<first_record) first_record=b;
			    latest_value=tempADCcount;
			    DataCorruption[i]++;
			    //			    cout<<"Data corrupted !!!!!"<<DataCorruption[i]<<" "<< b<<" "<<tempADCcount<<endl;
			  }
		      }

		    if(DataCorruption[i]){
		      //		    if(DataCorruption[i]>4){
		      cout<<"DATA CORRUPTED FOR EVENT "<<NumberOfEvents[i]<<" "<<szAddr[i]<<" From "<<first_record<<" TO "<<last_record<<" latest "<<latest_value<<" RECORDS "<<DataCorruption[i]<<endl;
		      PrevDataCorruption[i]=DataCorruption[i];

		      //DUMP
		      int counter=0;
		      for(int b=HeaderSize; b<HeaderSize+16*rddepth*2;b+=2)
			{
			  unsigned char tempbuf[32];
			  tempbuf[0] = __g_buff[b+1];
			  tempbuf[1] = __g_buff[b];
			  memcpy((char *)&tempADCcount,tempbuf,sizeof(unsigned short));
		
			  if(counter==0)
			    {
			    cout<<" ## "<<b<<" ## ";
			    int slice=((b-HeaderSize)/16)%40;
			    cout<<slice<<"s ## ";
			  }
			  cout<<tempADCcount<<" ";
			  counter++;
			  if(counter==8){cout<<endl;counter=0;}

			}
		      
		    }

		    if(NumberOfEvents[i]%PreScaleFactor==0 || DataCorruption[i]>0)
		      {
			fwrite(__g_buff,n,1,fp_d[i]);
			WrittenNumberOfEvents[i]++;
		      }
		  }
		//printf("FEB[%d] read %d Bytes\n ",i,n);
		//		readcount++;
		//if(readcount%100==0)printf("n=%d\n",n);
		    llRead[i] += (unsigned long long)n;
		if( llRead[i] >= (unsigned long long)lReadBytes ) 
		  {
		    clock_gettime(CLOCK_REALTIME,&tsEnd);
		    printf("finished %d \n",i);
		    RunEnd=true;
		    break;
		  }
		    // if( llRead[0] >= lReadBytes ) 
		    // 	{
		    // 	  clock_gettime(CLOCK_REALTIME,&tsEnd);
		    // 	  break;
		    // 	}
	      }/**if(FD_ISSET(sock[i],&fds))**/
	  }/**for(i<nServ)**/
	}/**for(;;)**/
      printf("***** Data Acquisition End *****\n");
      //printf("readcount :%d\n",readcount);
      for(int i=0;i<nServ;i++)
	{
	  close(sock[i]);
	  fclose(fp_d[i]);
	}
      /******************************************/
      //  Measurement summary 
      /******************************************/
      unsigned long long llusec = GetRealTimeInterval(&tsRStart,&tsEnd);
      double * readfreq = new double[nServ];
      double * readrate = new double[nServ];
      for(int i=0;i<nServ;i++)
	{
	  readfreq[i] = (double)llRead[i]/(double)(evsize)/llusec*1000000.0;
	  readrate[i] = (double)(llRead[i]*8.0)/llusec*1000000.0/1000./1000.;
	  /*In throughput definition,unit factor is 1000 instead of 1024.*/
	  /*actually,duration for DAQ started after acquiring the first event */
	}
      //output to measurement file
      fprintf(fp_ms,"%6d     ",infreq);
      for(int i=0;i<nServ;i++)
	  fprintf(fp_ms,"%10.3f   %10.3f   ",readfreq[i],readrate[i]);
      for(int i=0;i<nServ;i++)
	{
	  // fprintf(fp_ms,"%s ",szAddr[i]);

	  /*modified(1)*/
	  // char a[2];
	  // char b[16]="";
	  // strncpy(b,szAddr[i],16);
	  // strncpy(a,b+11,sizeof(b)-11);
	  // fprintf(fp_ms,"%s ",a);
	  
	  /*modified(2)*/
	  char a[2];
	  strncpy(a,szAddr[i]+11,sizeof(szAddr[i]-11));
	  fprintf(fp_ms,"%s ",a);

	}
      fprintf(fp_ms,"\n");
      //output to terminal
      printf("***** Throughput *****\n");
      for(int i=0;i<nServ;i++)
	printf("From %s: %llu bytes/%llu usec = %gMbps\n",
	       szAddr[i]  ,llRead[i],llusec,readrate[i]);
      printf("***** # of events *****\n");
      for(int i=0;i<nServ;i++)
	printf("From %s: %6.0f events were read with residual of %d bytes\n",
	       szAddr[i]  ,(double)llRead[i]/(double)evsize,llRead[i]%evsize);
      printf("InFreq[Hz]  ReadFreq[Hz] DataSize[Bytes] ReadTime[us] ReadRate[Mbps] IPaddress  NumberOfEvents(Written) \n");
      for(int i=0;i<nServ;i++){
      printf("%d      %g        %llu       %llu     %g    %s     %d(%d) \n" ,
	     infreq, 
	     readfreq[i],
	     llRead[i],
	     llusec,
	     readrate[i],
	     szAddr[i],
	     NumberOfEvents[i],
	     WrittenNumberOfEvents[i]
	     );
      }	      
      delete[] readfreq;
      delete[] readrate;
      /****************************************************/
      /***** Detailed measurement report output START *****/
      /****************************************************/
      if(closeinspect)
	{
	  if(chdir("DragonDaqMes")==0)
	    {
	      //printf("MeasurementData will be created in DragonDaqMes.\n");
	    }
	  else
	    {
	      if(mkdir("DragonDaqMes",
		       S_IRUSR|S_IWUSR|S_IXUSR|
		       S_IRGRP|S_IWGRP|S_IXGRP|
		       S_IROTH|S_IWOTH|S_IXOTH)==0)	
		{
		  chdir("DragonDaqMes");
		  //printf("Directory DragonDaqMes is created.\n");
		  //printf("MeasurementData will be created in DragonDaqMes.\n");
		}
	      else
		{
		  printf("Directory creation error on creating Measurement file");
		  return -1;
		}
	    }
	  time_t tnow;
	  struct tm *sttnow;
	  time(&tnow);
	  sttnow = localtime(&tnow);
	  char buf[126];
	  sprintf(buf,"RD%dinfreq%d_%02d%02d_%02d%02d%02d.dat"
		  ,rddepth
		  ,infreq
		  ,sttnow->tm_mon
		  ,sttnow->tm_mday
		  ,sttnow->tm_hour
		  ,sttnow->tm_min
		  ,sttnow->tm_sec);
	  FILE *fp_md;
	  fp_md = fopen(buf,"w");
	  //  fprintf(fp_md,"InFreq[Hz]  RdFreq[Hz] WrFreq[Hz] DataSize[Bytes] RdTime[us] WrTime[us] RdRate[Mbps] wst-rst[usec] wen-ren[usec]\n");
	  //  fprintf(fp_md,"%d      %g    %g       %d       %llu       %llu     %g       %llu       %llu\n",
	  fprintf(fp_md,"InFreq[Hz]  RdFreq[Hz] DataSize[Bytes] RdTime[us]  RdRate[Mbps] ctime1-Start[usec]\n");
	  fprintf(fp_md,"%d      %g       %llu       %llu     %g       %llu\n",
		  infreq, 
		  (double)llRead[0]/976.0/llusec*1000000.0,
		  llRead,
		  llusec,
		  (double)llRead[0]*8.0/llusec*1000000.0/1024.0/1024.0,
		  llstartdiffusec
		  );
	  for(int i=0;i<1000;i++)
	    {
	      fprintf(fp_md,"%llu\n",lltime_diff[i]);
	    }
	  fclose(fp_md);
	}
      /****************************************************/
      /***** Detailed measurement report output END   *****/
      /****************************************************/
    }else{
    printf("can't connect to servert\n");
  }
  fclose(fp_ms);
}
///////////////////////////////////////////////////////////////////////////////////////////
// main program END
///////////////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////////////
// connect to a server(SiTCP)
///////////////////////////////////////////////////////////////////////////////////////////
int ConnectTcp(const char *pszHost, unsigned short shPort, unsigned long &lConnectedIP )
{
  int sockTcp = -1;
  struct sockaddr_in addrTcp;
  sockTcp = socket(AF_INET, SOCK_STREAM, 0);
  if( sockTcp < 0 ){
    perror("socket");
    return -1;
  }
  addrTcp.sin_family = AF_INET;
  addrTcp.sin_port = htons(shPort);
  addrTcp.sin_addr.s_addr = inet_addr(pszHost);
  if( addrTcp.sin_addr.s_addr == 0xffffffff ){
    struct hostent *host = gethostbyname(pszHost);
    if( host == NULL ){
      if( h_errno == HOST_NOT_FOUND ){
	printf("ConnectTcp() host not found : %s\n",pszHost);
      }else{
	printf("ConnectTcp() %s : %s\n",hstrerror(h_errno),pszHost);
	fprintf(stderr,"%s : %s\n",hstrerror(h_errno),pszHost);
      }
      return -2;
    }
    unsigned int **addrptr = (unsigned int **)host->h_addr_list;
    while( *addrptr != NULL){
      addrTcp.sin_addr.s_addr = *(*addrptr);
      if(connect(sockTcp,(struct sockaddr *)&addrTcp,sizeof(addrTcp)) ==0 )
	{			
	  lConnectedIP = (unsigned long )addrTcp.sin_addr.s_addr;
	  break;
	}
      addrptr++;
    }
    if( addrptr == NULL) {
      perror("ConnectTCP()::connect(1)");
      printf("ERROR:ConnectTCP:: host not found (%d) to %s:%u\n",sockTcp,pszHost,shPort);
      return -3;
    }else{
      printf("ConnectTCP::Connected0(%d) to %s:%u\n",sockTcp,pszHost,shPort);
    }
  }else{
    if(connect(sockTcp,(struct sockaddr *)&addrTcp,sizeof(addrTcp)) !=0 )
      {
	perror("ConnectTCP()::connect(2)");
	printf("ERROR:ConnectTCP:: can't connect not found (%d) to %08X %s:%u\n",addrTcp.sin_addr.s_addr,sockTcp,pszHost,shPort);
	sockTcp = -1;
	close(sockTcp);
	return -4;
      }else{
      lConnectedIP = (unsigned long )addrTcp.sin_addr.s_addr;
      printf("ConnectTCP::Connected1(%d) %s:%d\n",sockTcp,pszHost,shPort);
    }
  }
  return( sockTcp );
}


///////////////////////////////////////////////////////////////////////////////////////////
// time calc
///////////////////////////////////////////////////////////////////////////////////////////
#define TIME_SEC2NSEC	1000000000 
unsigned long long GetRealTimeInterval(const  struct timespec *pFrom, const struct timespec *pTo)
{
  unsigned long long  llStart = (unsigned long long )((unsigned long long )pFrom->tv_sec*TIME_SEC2NSEC + (unsigned long long )pFrom->tv_nsec);
  unsigned long long  llEnd = (unsigned long long )((unsigned long long )pTo->tv_sec*TIME_SEC2NSEC + (unsigned long long )pTo->tv_nsec);
  return( (llEnd - llStart)/1000 );
}




///////////////////////////////////////////////////////////////////////////////////////////
// ALL END
///////////////////////////////////////////////////////////////////////////////////////////


