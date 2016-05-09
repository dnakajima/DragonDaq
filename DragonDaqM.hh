#ifndef DRAGON_H
#define DRAGON_H




int ConnectTcp(const char *pszHost, unsigned short shPort, unsigned long &lConnectedIP );
unsigned long long GetRealTimeInterval(const  struct timespec *pFrom, const struct timespec *pTo);
#endif
#define TERM_COLOR_BLUE printf("\033[34m")
#define TERM_COLOR_RED  printf("\033[31m")
#define TERM_COLOR_RESET printf("\033[0m")
