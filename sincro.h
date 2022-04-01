#include <stdio.h>
#include <malloc.h>
#include <windows.h>
#include <fcntl.h>

#define TRUE 1
#define FALSE 0

#define DBHOST "estech.homeip.net"
// #define DBHOST "192.168.1.244"
#define DBBASE "Cfpark"

#define SELECT "select * from %s"
#define DELETE_REG "delete from %s where %s"

char *logfile = "sincro.log";
FILE *logfilefd;

char *cfgfile = "sincro.cfg";
FILE *cfgfd;
