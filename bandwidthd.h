#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_PYTHON
#include <Python.h>
#endif

#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include <net/if.h>

#include <netinet/in.h>

#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#include <stdio.h>
#include <pcap.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/time.h>

#include <sys/wait.h>
#include <dirent.h>

#if HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifdef TYPEDEF_UINT32
typedef u_int32_t uint32_t;
#endif

#ifdef TYPEDEF_UINT16
typedef u_int16_t uint16_t;
#endif

#ifdef HAVE_GD_H
#include <gd.h>
#else 
#ifdef HAVE_GD_GD_H
#include <gd/gd.h>
#endif
#endif

#ifdef HAVE_GDFONTS_H
#include <gdfonts.h>
#else 
#ifdef HAVE_GD_GDFONTS_H
#include <gd/gdfonts.h>
#endif
#endif

#include <netdb.h>

#include <time.h>

#include <syslog.h>

#define BUILD 17

#define IP_NUM 256*256			// TODO: Do this dynamicly to save ram and/or scale bigger
#define SUBNET_NUM 100

#define XWIDTH 900
#define YHEIGHT 256L
#define XOFFSET 90L
#define YOFFSET 45L

#define NR_WORKER_CHILDS 3

#define RANGE1 172800.0	// 2 days
#define RANGE2 604800.0	// 7 days
#define RANGE3 3024000.0	// 35 days
#define RANGE4 35640000.0	// 412.5ish days 

#define INTERVAL1 200L		// 150 -60 (213 is the perfect interval?)
#define INTERVAL2 600L		// 10 minutes
#define INTERVAL3 3600L		// 1 hour
#define INTERVAL4 43200L	// 12 hours 
#define CONFIG_GRAPHINTERVALS 1    // 2 -5 Number of Intervals to wait before redrawing the graphs

#define CONFIG_GRAPHCUTOFF 1024*1024	// If total data transfered doesn't reach at least this number we don't graph the ip

#define LEAD .05	// % Of time to lead the graph

#define TRUE 1
#define FALSE 0

#define CONFIG_METAREFRESH 150

#define PERMIS 1

#define MAX_FILENAME 1024

#define DB_PGSQL 1
#define DB_MYSQL 2 // No mysql support yet
#define DB_SQLITE 3

struct config
	{
	char *dev;
	char *filter;
	unsigned int skip_intervals;
	unsigned long long graph_cutoff;
	int promisc;
	int extensions;
	int output_cdf;
	int recover_cdf;
	int graph;
	double range;
	unsigned long long interval;
	char tag;
	unsigned int meta_refresh;
	int output_database;
	char *db_connect_string;
	char *sensor_name;
	char *log_dir;
	char *htdocs_dir;
	char *description;
	char *management_url;
	};

struct SubnetData
	{
	uint32_t ip;
	uint32_t mask;
	} SubnetTable[SUBNET_NUM];

struct Statistics
	{
	unsigned long long packet_count;
	unsigned long long total;

	unsigned long long icmp;
	unsigned long long udp;
	unsigned long long tcp;

	unsigned long long ftp;
	unsigned long long http;
	unsigned long long mail;
	unsigned long long p2p;
	};

struct IPData
	{
	time_t timestamp;
	char *mac[1][20];
	uint32_t ip;	// Host byte order
	struct Statistics Send;
	struct Statistics Receive;
	} IpTable[IP_NUM];

struct SummaryData
	{
	uint32_t IP;
	int Graph;	// TRUE or FALSE, Did we write out a graph for this ip
	unsigned long long Total;
	unsigned long long TotalSent;
	unsigned long long TotalReceived;
	unsigned long long ICMP;
	unsigned long long UDP;
	unsigned long long TCP;
	unsigned long long FTP;
	unsigned long long HTTP;
	unsigned long long MAIL;
	unsigned long long P2P;
	};

struct IPDataStore
	{
	uint32_t ip;
	struct DataStoreBlock *FirstBlock;	// This is structure is allocated at the same time, so it always exists.
		
	struct IPDataStore *Next;
	};

#define IPDATAALLOCCHUNKS 100
struct DataStoreBlock
	{
	time_t LatestTimestamp;
	int NumEntries;  // Is the index of the first unused entry in IPData
	struct IPData *Data;  // These are allocated at creation, and thus always exist

	struct DataStoreBlock *Next;
	};

struct Broadcast
  {
  char *sensor_name;
  char *interface;
  time_t received;

  struct Broadcast *next;
  };

// Vlan header

struct VlanHeader
{
  u_int8_t  ether_dhost[6];  /* destination eth addr */
  u_int8_t  ether_shost[6];  /* source ether addr  */
  u_int8_t  ether_type[2];   /* packet type ID field */
  u_int8_t  vlan_tag[2];     /* vlan tag information */
} __attribute__ ((__packed__));

struct extensions {
	char *name;
	char *value;
	struct extensions *next;
};

// ****************************************************************************************
// ** Function Prototypes
// ****************************************************************************************

// ************ A fork that orphans the child
int				fork2();

// ************ The function that gets called with each packet
void			PacketCallback(u_char *user, const struct pcap_pkthdr *h, const u_char *p);

// ************ Reads a CDF file from a previous run
void RecoverDataFromCDF(void);

// ************ Adds subnets to the list of subnets that are monitored
void MonitorSubnet(unsigned int ip, unsigned int mask);


// ************ This function converts and IP to a char string
char inline 	*HostIp2CharIp(unsigned long ipaddr, char *buffer);

// ************ This function converts the numbers for each quad into an IP
inline uint32_t IpAddr(unsigned char q1, unsigned char q2, unsigned char q3, unsigned char q4);

// ************ This function adds the packet's size to the proper entries in the data structure
inline void		Credit(struct Statistics *Stats, const struct ip *ip);

// ************ Finds an IP in our IPTable
inline struct IPData *FindIp(uint32_t ipaddr);

// ************ Writes our IPTable to Disk or to the Ram cache
void			CommitData(time_t timestamp);

// ************ Creates our Graphs
void			GraphIp(struct IPDataStore *DataStore, struct SummaryData *SummaryData, time_t timestamp);
void			PrepareXAxis(gdImagePtr im, time_t timestamp);
void			PrepareYAxis(gdImagePtr im, unsigned long long int YMax);
unsigned long long GraphData(gdImagePtr im, gdImagePtr im2, struct IPDataStore *DataStore, time_t timestamp,  struct SummaryData *SummaryData);


// ************ Misc
inline void		DstCredit(uint32_t ipaddr, unsigned int psize);
void			MakeIndexPages(int NumGraphs, struct SummaryData *SummaryData[]);

// ************ Pgsql
void pgsqlStoreIPData(struct IPData IncData[], struct extensions *);

// ************ SQLite
void sqliteStoreIPData(struct IPData IncData[], struct extensions *extension_data);

// ************ Extensions
struct extensions *execute_extensions(void);
void destroy_extension_data(struct extensions *ext);

// ************ Python
#ifdef HAVE_PYTHON
void iptable_Transform(uint32_t Counter);
void initbandwidthd(void);
#endif


// ** Global Variables

#define SNAPLEN 100

/*
static pcap_t *pd;
unsigned int GraphIntervalCount = 0;
unsigned int IpCount = 0;
unsigned int SubnetCount = 0;
unsigned int NotSubnetCount = 0;
int IntervalFinished = FALSE;
time_t IntervalStart;
time_t ProgramStart;
int RotateLogs = FALSE;
int PacketCallbackLock = 0;
pid_t pidGraphingChild = 0;
struct SubnetData SubnetTable[SUBNET_NUM];
struct SubnetData NotSubnetTable[SUBNET_NUM];
struct IPData IpTable[IP_NUM];
size_t ICGrandTotalDataPoints = 0;

#ifdef HAVE_PYTHON
PyObject *IpTableDict;
#endif

int DataLink;
int IP_Offset;
struct IPDataStore *IPDataStore = NULL;
extern int bdconfig_parse(void);
void BroadcastState(int fd);
extern FILE *bdconfig_in;
struct config config;
struct Broadcast *Broadcasts = NULL;
pid_t workerchildpids[NR_WORKER_CHILDS];
void ResetTrafficCounters(void);
void CloseInterval(void);
*/
