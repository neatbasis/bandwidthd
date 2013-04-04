 %{
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "bandwidthd.h"

extern unsigned int SubnetCount;
extern unsigned int NotSubnetCount;
extern struct SubnetData SubnetTable[];
extern struct SubnetData NotSubnetTable[];
extern struct config config;

int bdconfig_lex(void);
int LineNo = 1;

void bdconfig_error(const char *str)
    {
    fprintf(stderr, "Syntax Error \"%s\" on line %d in bandwidthd.conf\n", str, LineNo);
  syslog(LOG_ERR, "Syntax Error \"%s\" on line %d in bandwidthd.conf", str, LineNo);
  exit(1);
    }

int bdconfig_wrap()
  {
  return(1);
  }
%}

%token TOKJUNK TOKSUBNET TOKNOTSUBNET TOKDEV TOKSLASH TOKSKIPINTERVALS TOKGRAPHCUTOFF TOKDESCRIPTION
%token TOKPROMISC TOKOUTPUTCDF TOKRECOVERCDF TOKGRAPH TOKNEWLINE TOKFILTER TOKMANAGEMENTURL
%token TOKMETAREFRESH TOKPGSQLCONNECTSTRING TOKSENSORID TOKHTDOCSDIR TOKLOGDIR TOKEXTENSIONS
%token TOKSQLITEFILENAME
%union
{
    int number;
    char *string;
}

%token <string> IPADDR
%token <number> NUMBER
%token <string> STRING
%token <number> STATE
%type <string> string
%%

commands: /* EMPTY */
    | commands command
    ;

command:
  subnet
  |
  notsubnet
  |
  device
  |
  skip_intervals
  |
  graph_cutoff
  |
  promisc
  |
  extensions
  |
  output_cdf
  |
  recover_cdf
  |
  graph
  |
  newline
  |
  filter
  |
  meta_refresh
  |
  pgsql_connect_string
  |
  sensor_name
  |
  htdocs_dir
  |
  log_dir
  |
  description
  |
  management_url
  |
  sqlite_filename
  ;

subnet:
  subneta
  |
  subnetb
  ;

newline:
  TOKNEWLINE
  {
  LineNo++;
  }
  ;

notsubnet:
  notsubneta
  |
  notsubnetb
  ;

notsubneta:
  TOKNOTSUBNET IPADDR IPADDR
  {
  //struct in_addr addr, addr2;

  IgnoreMonitorSubnet(inet_network($2), inet_network($3));

  /*
  NotSubnetTable[NotSubnetCount].ip = inet_network($2) & inet_network($3);
      NotSubnetTable[NotSubnetCount].mask = inet_network($3);

  addr.s_addr = ntohl(NotSubnetTable[NotSubnetCount].ip);
  addr2.s_addr = ntohl(NotSubnetTable[NotSubnetCount++].mask);
  syslog(LOG_INFO, "Ignoring subnet %s with netmask %s", inet_ntoa(addr), inet_ntoa(addr2));
  */
  }
  ;

notsubnetb:
  TOKNOTSUBNET IPADDR TOKSLASH NUMBER
  {
  unsigned int Subnet, Counter, Mask;
  //struct in_addr addr, addr2;

  Mask = 1; Mask <<= 31;
  for (Counter = 0, Subnet = 0; Counter < $4; Counter++)
    {
    Subnet >>= 1;
    Subnet |= Mask;
    }

  IgnoreMonitorSubnet(inet_network($2), Subnet);

  /*
  NotSubnetTable[NotSubnetCount].mask = Subnet;
  NotSubnetTable[NotSubnetCount].ip = inet_network($2) & Subnet;
  addr.s_addr = ntohl(NotSubnetTable[NotSubnetCount].ip);
  addr2.s_addr = ntohl(NotSubnetTable[NotSubnetCount++].mask);
  syslog(LOG_INFO, "Ignoring subnet %s with netmask %s", inet_ntoa(addr), inet_ntoa(addr2));
  */
  }
  ;


subneta:
  TOKSUBNET IPADDR IPADDR
  {
  //struct in_addr addr, addr2;
  
  MonitorSubnet(inet_network($2), inet_network($3));

  /*
  SubnetTable[SubnetCount].ip = inet_network($2) & inet_network($3);
      SubnetTable[SubnetCount].mask = inet_network($3);  

  addr.s_addr = ntohl(SubnetTable[SubnetCount].ip);
  addr2.s_addr = ntohl(SubnetTable[SubnetCount++].mask);
  syslog(LOG_INFO, "Monitoring subnet %s with netmask %s", inet_ntoa(addr), inet_ntoa(addr2));
  */
  }
  ;

subnetb:
  TOKSUBNET IPADDR TOKSLASH NUMBER
  {
  unsigned int Subnet, Counter, Mask;
  //struct in_addr addr, addr2;

  Mask = 1; Mask <<= 31;
  for (Counter = 0, Subnet = 0; Counter < $4; Counter++)
    {
    Subnet >>= 1;
    Subnet |= Mask;
    }

  MonitorSubnet(inet_network($2), Subnet);

  /*   SubnetTable[SubnetCount].mask = Subnet; 
  SubnetTable[SubnetCount].ip = inet_network($2) & Subnet;
  addr.s_addr = ntohl(SubnetTable[SubnetCount].ip);
  addr2.s_addr = ntohl(SubnetTable[SubnetCount++].mask);
  syslog(LOG_INFO, "Monitoring subnet %s with netmask %s", inet_ntoa(addr), inet_ntoa(addr2));
  */
  }
  ;

string:
    STRING
    {
    $1[strlen($1)-1] = '\0';
    $$ = $1+1;
    }
    ;

device:
  TOKDEV string
  {
  config.dev = $2;
  }
  ;

management_url:
  TOKMANAGEMENTURL string
  {
  config.management_url = $2;
  }
  ;

description:
  TOKDESCRIPTION string
  {
  config.description = $2;
  }
  ;

htdocs_dir:
  TOKHTDOCSDIR string
  {
  config.htdocs_dir = $2;
  }
  ;

log_dir:
  TOKLOGDIR string
  {
  config.log_dir = $2;
  }
  ;

filter:
  TOKFILTER string
  {
  config.filter = $2;
  }
  ;

meta_refresh:
  TOKMETAREFRESH NUMBER
  {
  config.meta_refresh = $2;
  }
  ;

skip_intervals:
  TOKSKIPINTERVALS NUMBER
  {
  config.skip_intervals = $2+1;
  }
  ;

graph_cutoff:
  TOKGRAPHCUTOFF NUMBER
  {
  config.graph_cutoff = $2*1024;
  }
  ;

promisc:
  TOKPROMISC STATE
  {
  config.promisc = $2;
  }
  ;

extensions:
  TOKEXTENSIONS STATE
  {
  config.extensions = $2;
  }
  ;

output_cdf:
  TOKOUTPUTCDF STATE
  {
  config.output_cdf = $2;
  }
  ;

recover_cdf:
  TOKRECOVERCDF STATE
  {
  config.recover_cdf = $2;
  }
  ;

graph:
  TOKGRAPH STATE
  {
  config.graph = $2;
  }
  ;

pgsql_connect_string:
    TOKPGSQLCONNECTSTRING string
    {
    config.db_connect_string = $2;
  config.output_database = DB_PGSQL;
    }
    ;

sqlite_filename:
  TOKSQLITEFILENAME string
  {
  config.db_connect_string = $2;
  config.output_database = DB_SQLITE;
  }
  ;

sensor_name:
    TOKSENSORID string
    {
    config.sensor_name = $2;
    }
    ;
