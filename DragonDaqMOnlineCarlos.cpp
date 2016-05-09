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




struct EVT{
  int Delay; //muse
  int Time;
  int Event;
  int Trigger;
  float Adc;
  int Counter;

  int Id;
  int Channel;
  int LowGain;
  int CellId;
  int Roi;

  int AdcCorr;

  int Status;
}Ev;


const int _ncells=4096;
const int _nmodules=19;
const int _ngains=2;
const int _nchannels=7;
const int _npointers=4;

int eventsMap[_nmodules][_nchannels][_ncells][_ngains][_npointers]={-1};  // This is an event map of the ADC counts. We keep always two of them
int eventsMapUpt[_nmodules][_nchannels][_ncells][_ngains]={0};  // This is an event map of the ADC counts


#include "TFile.h"
#include "TTree.h"
#include "TRandom.h"
#include "TSystem.h"

bool ShouldStore=false;
TFile *ftree=0;
TTree *otree=0;
bool Analysis(unsigned short *buffer,int start,int end,int store,int threshold=10);
double probFunc(double diff){
  double d1=diff; d1*=d1/3.36391e+00/3.36391e+00;
  double d2=diff; d2*=d2/3.45210e+01/3.45210e+01;

  return log(1+5.19912e+04/1.12390e+07*exp(-0.5*d2+0.5*d1))+log(1.12390e+07)+0.5*d1;
}

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
    {"wait" ,required_argument   ,NULL ,'w'},
    {"time" ,required_argument   ,NULL ,'t'},
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
  int Waiting = 100;
  unsigned int Time = 0;  // Number of channels beyond threshold to be considered as bad
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
      printf("-w|--wait                            : Numbers of events to start dalying default is 100 .\n");
      printf("-t|--time                            : Minimum time between events in us is 0 .\n");
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
      fileNameHeader=optarg;
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
    case 'w' :
      Waiting = atoi(optarg);
      break;
    case 't' :
      Time = atoi(optarg);
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
  unsigned char __real_buffer[evsize+16];
  unsigned char *__g_buff=__real_buffer+4; // Trick to change the endianess in a single pass

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
    }
  else{
    printf("The file %s already exists. Data will be added to it.\n",daqmesfile.str().c_str());
  }

   /******************************************/
  // Preparation of Data File
  /******************************************/
  //Initialization of Data File
  char datafile[48][128];
  FILE *fp_d[48];
  for(int i =0;i<nServ;i++)
    {
      int DragonId = atoi(IPAddr[i].substr(10).c_str());
      sprintf(datafile[i],"%s_FEB%d_IP%d.dat",fileName.str().c_str(),i, DragonId);
      cout<<"File "<<i+1<<" "<<datafile[i]<<endl;
      fp_d[i] = fopen(datafile[i],"wb");
    }

  /******************************/
  // Initialization fo roofile 
  /******************************/
  ftree=new TFile(Form("%s.root",fileNameHeader.c_str()),"RECREATE");
  otree=new TTree("Data","Data");
  ftree->cd();
  otree->Branch("Data",
		&Ev.Delay,
		"Delay/I"
		":Time/I"
		":Event/I"
		":Trigger/I"
		":Adc/F"
		":Counter/I"
		":Id/I"
		":Channel/I"
		":LowGain/I"
		":CellId/I"
		":Roi/I"
		":AdcSub/I"

		//		":Status/I"
	);
  gRandom->SetSeed(time(0));

  Ev.Time=(int)time(NULL);
  Ev.Counter=0;
  Ev.Delay=Time;

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

      memset(__real_buffer,0,sizeof(__real_buffer));		
      fd_set fds, readfds;

      FD_ZERO(&readfds);                                       // Clear the set of file descriptors
      for(int i=0;i<nServ;i++)FD_SET(sock[i], &readfds);       // Add our guys to the set of file descriptos
      for(int i=1;i<nServ;i++) if(sock[i]>maxfd)maxfd=sock[i]; // Update the max file descriptor

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

      struct timespec prev_time;
      clock_gettime(CLOCK_REALTIME,&prev_time);

      
      while(!RunEnd)
	{
	  memcpy(&fds,&readfds,sizeof(fd_set));   // Copy the file descriptos set
      	  select(maxfd+1, &fds, NULL, NULL,&tv);  // Look for those ready to be read


	  if(Ev.Counter<=Waiting) // Ensure we clear the Dragon memory
	    usleep(Time);
	  //	  else
	  //	    clock_gettime(CLOCK_REALTIME,&prev_time);

	  for(int i=0;i<nServ;i++){
	    // Bring __g_buff to its real value
	    __g_buff=__real_buffer+4;
	    
	    if( FD_ISSET(sock[i], &fds))
	      {
		int n=0;
		while(n<evsize)
		  {
		    int ret = read( sock[i],__g_buff+n,evsize-n);  // Read it
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
		
		if(datacreate==1)                 // Analyze
		  {
		    // Correct endiness, including the flags and capacitor id
		    for(int b=HeaderSize-8*3*2; b<HeaderSize+16*rddepth*2;b+=2) *(__g_buff+b-1)=*(__g_buff+b+1);
		    __g_buff--;

		    Ev.Id=i;
		    Ev.Status=0;

		    bool corrupted=false;
		    Ev.Counter++;	  
		    struct timespec curr_time;
		    clock_gettime(CLOCK_REALTIME,&curr_time);
		    unsigned long long delta=GetRealTimeInterval(&prev_time,&curr_time);
		    Ev.Time=int(delta);
		    //		    prev_time=curr_time;
		    corrupted=Analysis((unsigned short*)__g_buff,    // Where
				       (HeaderSize-8*3*2)/2,
				       (HeaderSize+16*rddepth*2)/2,
				       0,
				       0);

		    
		    
		    if(corrupted)
		      {
			ShouldStore=true;
			//			fwrite(__g_buff,n,1,fp_d[i]);
			WrittenNumberOfEvents[i]++;
			cout<<"CORRUPTED ? ? ? ? "<<i<<endl;
		      }
		  }
		llRead[i] += (unsigned long long)n;
		if( llRead[i] >= (unsigned long long)lReadBytes ) 
		  {
		    clock_gettime(CLOCK_REALTIME,&tsEnd);
		    printf("finished %d \n",i);
		    RunEnd=true;
		    break;
		  }

	      }/**if(FD_ISSET(sock[i],&fds))**/
	  }/**for(i<nServ)**/
	}/**for(;;)**/
      printf("***** Data Acquisition End *****\n");
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
	}
      //output to measurement file
      fprintf(fp_ms,"%6d     ",infreq);
      for(int i=0;i<nServ;i++)
	fprintf(fp_ms,"%10.3f   %10.3f   ",readfreq[i],readrate[i]);
      for(int i=0;i<nServ;i++)
	{

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
	    }
	  else
	    {
	      if(mkdir("DragonDaqMes",
		       S_IRUSR|S_IWUSR|S_IXUSR|
		       S_IRGRP|S_IWGRP|S_IXGRP|
		       S_IROTH|S_IWOTH|S_IXOTH)==0)	
		{
		  chdir("DragonDaqMes");
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

  if(ftree){
    ftree->cd();
    //    ShouldStore=ShouldStore || gRandom->Uniform()<0.1;
    if(ShouldStore) otree->Write();
    //    otree->Write();
    ftree->Close();
    if(!ShouldStore) gSystem->Exec(Form("rm %s",ftree->GetName()));
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




///////////////////////////////////////////////////////
// ANALYSIS
///////////////////////////////////////////////////////





bool analyze(unsigned short *buffer,
	     int start,
	     int end,
	     int store_prob,  //ignored
	     int Time,
	     int dump=false){  // ignored
  bool corrupted=false;

  using namespace std;

  const int last_row=(end-start)/8;
  int roi_size=(last_row-3.0)/2.0;
  //cout<<"ROI SIZE "<<roi_size<<endl;
  int roi_border=start+3+roi_size;
  

  int &Event=Ev.Event;
  int &Trigger=Ev.Trigger;
  unsigned short *stopCellId=0;

  double adc_sum=0;
  int adc_counter=0;


  int odd=0; // Set to 1 for the odd channels

  int counter=0;
  int other_counter=0;
  
  int guys=0;
  double mean=0;

  for(int row=0;row<last_row;row++){
    
    if(row==0){
      // Evt Number and so on
      unsigned short *prow=&buffer[start+row*8];
      Event=prow[1]+0xffff*prow[0];
      Trigger=prow[3]+0xffff*prow[2];

      if(dump && 0) cout<<"###>>"<<endl<<"EVENT "<<Event<<" TRIGGER "<<Trigger<<" Corr Read Evt:"<<Ev.Counter<<endl;
      continue;
    }
    
    if(row==1) continue; // FLAGS
    
    if(row==2){
      stopCellId=&buffer[start+row*8];
      if(dump && 0){
	cout<<"###>>"<<"STOP CELLS ";
	for(int ii=0;ii<8;ii++) cout<<"###>>"<<buffer[ii]<<" ";
      }
      continue;
    }
    
    if(row>=roi_border) odd=1;

    for(int record=0;record<8;record++){
      if(odd && record>5) break; // DO NOT READ THE _TAG, whatever it is
      
      int channel=(record&0xfffe)+odd;
      int low_gain=record%2;
      
      int cellId=row-(odd?roi_border:3)        // Slices-offset
	+stopCellId[channel];                  // Stop cell id
      

      int adc=buffer[start+row*8+record];
      int roi=row-(odd?roi_border:3);

      if(dump && 0){
	if(record==0) cout<<"###>>"<<endl<<"ROI "<<roi<<" ";
	fprintf(stderr,"(c%i,g%i) %i ",channel,low_gain,adc);
      }

      Ev.Channel=channel;
      Ev.LowGain=low_gain;
      Ev.CellId=cellId%4096;
      Ev.Roi=roi;
      Ev.Adc=adc;

      // Update the map
      int *value=eventsMap[Ev.Id][Ev.Channel][Ev.CellId][Ev.LowGain];
      int &upts=eventsMapUpt[Ev.Id][Ev.Channel][Ev.CellId][Ev.LowGain];


      if(roi>1 && roi<roi_size-1){ 

	  // Search for the closer one
	  int closer=_npointers;
	  int vals[_npointers+1];
	  vals[closer]=9999999;
	  for(int i=0;i<_npointers;i++) vals[i]=abs(Ev.Adc-value[i]);  // Try to use vectorization
	  for(int i=0;i<_npointers;i++) closer=vals[i]<vals[closer]?i:closer;
	  
	  if(closer<_npointers && value[closer]>0){
	    
	    Ev.AdcCorr=value[closer];
	    if(Ev.Trigger>100){
	      guys++;
	      mean+=(Ev.Adc-Ev.AdcCorr);
	    }
	    //	    if(Ev.Adc-Ev.AdcCorr<-150) corrupted=true;
	    Ev.Status=0;


	    /*
	    if(otree){
	      bool store_it=false;
	      if(gRandom->Uniform()<0.01) {Ev.Status+=1;store_it=true;}
	      if(Ev.Trigger!=Ev.Event){Ev.Status+=10; store_it=true;}
	      if(Ev.Adc-Ev.AdcCorr<-150) {Ev.Status+=100; store_it=true;}
	      if(Ev.Adc<50) {Ev.Status+=1000; store_it=true;}
	      if(store_it) otree->Fill();
	    }
	    */
	    if(dump) otree->Fill();

	  }
	  
	  adc_sum+=Ev.Adc;
	  adc_counter++;

	  if(Ev.Adc>0){
	    value[upts%_npointers]=Ev.Adc;
	    upts++;
	  }
      }
      
    }
  }
  

  Ev.Adc=adc_sum/adc_counter;


  if(otree && 0) otree->Fill();


  double sigmas=fabs(mean/guys)*sqrt(guys)/7.993;
  if(guys>1 && sigmas>4) corrupted=1;
  
  return corrupted;
}


bool Analysis(unsigned short *buffer,int start,int end,int store,int threshold){
  bool corrupted=analyze(buffer,start,end,store,threshold);
  if(Ev.Adc<200){corrupted=1;analyze(buffer,start,end,store,threshold,1);} ///// TEST TO DUMP CORRUPTED
  return corrupted;
}

///////////////////////////////////////////////////////////////////////////////////////////
// ALL END
///////////////////////////////////////////////////////////////////////////////////////////


