#include "bandwidthd.h"
#include  <netinet/if_ether.h>

// We must call regular exit to write out profile data, but child forks are supposed to usually
// call _exit?
#ifdef PROFILE
#define _exit(x) exit(x)
#endif

/*
#ifdef DEBUG
#define fork() (0)
#endif
*/

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


void signal_handler(int sig)
	{
	switch (sig) 
		{
		case SIGHUP:
			signal(SIGHUP, signal_handler);
			RotateLogs++;
			if (config.tag == '1') 
				{
				int i;
				/* signal children */
				for (i=0; i < NR_WORKER_CHILDS; i++) 
					if (workerchildpids[i]) // this is a child
						kill(workerchildpids[i], SIGHUP);
				}
			break;
		case SIGTERM:
			if (config.tag == '1') 
				{
				int i;
				/* send term signal to children */
				for (i=0; i < NR_WORKER_CHILDS; i++) 
					if (workerchildpids[i])
						kill(workerchildpids[i], SIGTERM);
				}
			// TODO: Might want to make sure we're not in the middle of wrighting out a log file
			exit(0);
			break;
		}
	}

void bd_CollectingData()
	{
	char FileName[4][MAX_FILENAME];
	FILE *index;
	int Counter;
	snprintf(FileName[0], MAX_FILENAME, "%s/index.html", config.htdocs_dir);
	snprintf(FileName[1], MAX_FILENAME, "%s/index2.html", config.htdocs_dir);
	snprintf(FileName[2], MAX_FILENAME, "%s/index3.html", config.htdocs_dir);	
	snprintf(FileName[3], MAX_FILENAME, "%s/index4.html", config.htdocs_dir);
	for(Counter = 0; Counter < 4; Counter++)
		{
		index = fopen(FileName[Counter], "wt");	
		if (index)
			{
			fprintf(index, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n");
			fprintf(index, "<HTML><HEAD><TITLE>Bandwidthd</TITLE>\n");
			if (config.meta_refresh)
				fprintf(index, "<META HTTP-EQUIV=\"REFRESH\" content=\"%u\">\n",
						config.meta_refresh);
			fprintf(index, "<META HTTP-EQUIV=\"EXPIRES\" content=\"-1\">\n");
			fprintf(index, "<META HTTP-EQUIV=\"PRAGMA\" content=\"no-cache\">\n");
			fprintf(index, "</HEAD>\n<BODY><center><img src=\"logo.gif\" ALT=\"Logo\"><BR>\n");
			fprintf(index, "<BR>\n - <a href=\"index.html\">Daily</a> -- <a href=\"index2.html\">Weekly</a> -- ");
			fprintf(index, "<a href=\"index3.html\">Monthly</a> -- <a href=\"index4.html\">Yearly</a><BR>\n");
			fprintf(index, "</CENTER><BR>bandwidthd has nothing to graph.  This message should be replaced by graphs in a few minutes.	If it's not, please see the section titled \"Known Bugs and Troubleshooting\" in the README");		
			fprintf(index, "</BODY></HTML>\n");
			fclose(index);
			}
		else
			{
			syslog(LOG_ERR, "Cannot open %s for writing", FileName[Counter]);
			exit(1);
			}
		}
	}

pid_t WriteOutWebpages(long int timestamp)
{
	struct IPDataStore *DataStore = IPDataStore;
	struct SummaryData **SummaryData;
	int NumGraphs = 0;
	pid_t graphpid;
	int Counter;
	/* Did we catch any packets since last time? */
	if (!DataStore) 
		return -2;
	// break off from the main line so we don't miss any packets while we graph
	graphpid = fork();
	switch (graphpid) {
		case 0: /* we're the child, graph. */
			{
#ifdef PROFILE
			// Got this incantation from a message board.  Don't forget to set
			// GMON_OUT_PREFIX in the shell
			extern void _start(void), etext(void);
			syslog(LOG_INFO, "Calling profiler startup...");
			monstartup((u_long) &_start, (u_long) &etext);
#endif
			signal(SIGHUP, SIG_IGN);
			nice(4); // reduce priority so I don't choke out other tasks
			// Count Number of IP's in datastore
			for (DataStore = IPDataStore, Counter = 0; DataStore; Counter++, DataStore = DataStore->Next);
			// +1 because we don't want to accidently allocate 0
			SummaryData = malloc(sizeof(struct SummaryData *)*Counter+1);
			DataStore = IPDataStore;
			while (DataStore) // Is not null
				{
				if (DataStore->FirstBlock->NumEntries > 0)
					{
					SummaryData[NumGraphs] = (struct SummaryData *) malloc(sizeof(struct SummaryData));
					GraphIp(DataStore, SummaryData[NumGraphs++], timestamp+LEAD*config.range);
					}
				DataStore = DataStore->Next;
				}
			MakeIndexPages(NumGraphs, SummaryData);
			_exit(0);
			}
		break;
		case -1:
			syslog(LOG_ERR, "Forking grapher child failed!");
			return -1;
		break;
		default: /* parent + successful fork, assume graph success */
			return(graphpid);
		break;
	}
}

void setchildconfig (int level) {
	static unsigned long long graph_cutoff;
	switch (level) 
	{
		case 0:
			config.range = RANGE1;
			config.interval = INTERVAL1;
			config.tag = '1';
			graph_cutoff = config.graph_cutoff;
			break;
		case 1:
			// Overide skip_intervals for children
			config.skip_intervals = CONFIG_GRAPHINTERVALS;
			config.range = RANGE2;
			config.interval = INTERVAL2;
			config.tag = '2';
			config.graph_cutoff = graph_cutoff*(RANGE2/RANGE1);	
			break;
		case 2:
			// Overide skip_intervals for children
			config.skip_intervals = CONFIG_GRAPHINTERVALS;
			config.range = RANGE3;
			config.interval = INTERVAL3;
			config.tag = '3';
			config.graph_cutoff = graph_cutoff*(RANGE3/RANGE1);
			break;
		case 3:
			// Overide skip_intervals for children
			config.skip_intervals = CONFIG_GRAPHINTERVALS;
			config.range = RANGE4;
			config.interval = INTERVAL4;
			config.tag = '4';
			config.graph_cutoff = graph_cutoff*(RANGE4/RANGE1);
			break;
		default:
			syslog(LOG_ERR, "setchildconfig got an invalid level argument: %d", level);
			_exit(1);
	}
}

void makepidfile(pid_t pid) 
	{
	FILE *pidfile;
	pidfile = fopen("/var/run/bandwidthd.pid", "wt");
	if (pidfile) 
		{
		if (fprintf(pidfile, "%d\n", pid) == 0) 
			{
			syslog(LOG_ERR, "Bandwidthd: failed to write '%d' to /var/run/bandwidthd.pid", pid);
			fclose(pidfile);
			unlink("/var/run/bandwidthd.pid");
			}
		else
			fclose(pidfile);
		}
	else
		syslog(LOG_ERR, "Could not open /var/run/bandwidthd.pid for write");
	}

void PrintHelp(void)
	{
	printf("\nUsage: bandwidthd [OPTION]\n\nOptions:\n");
	printf("\t-D\t\tDo not fork to background\n");
	printf("\t-l\t\tList detected devices\n");
	printf("\t-c filename\tAlternate configuration file\n");
	printf("\t--help\t\tShow this help\n");
	printf("\n");
	exit(0);
	}

static void handle_interval(int signal)
	{
	if (!PacketCallbackLock)
		CloseInterval();
	else // Set flag to have main loop close interval
		IntervalFinished = TRUE;
	}

int main(int argc, char **argv)
	{
	struct bpf_program fcode;
	u_char *pcap_userdata = 0;
#ifdef HAVE_PCAP_FINDALLDEVS
	pcap_if_t *Devices;
#endif
	char Error[PCAP_ERRBUF_SIZE];
	struct stat StatBuf;
	int i;
	int ForkBackground = TRUE;
	int ListDevices = FALSE;
	int Counter;
	char *bd_conf = NULL;
	struct in_addr addr, addr2;
	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	ProgramStart = time(NULL);
	// Init worker child pids to zero so we don't kill random things when we shut down
	for (Counter = 0; Counter < NR_WORKER_CHILDS; Counter++)
		workerchildpids[Counter] = 0;
	for(Counter = 1; Counter < argc; Counter++)
		{
		if (argv[Counter][0] == '-')
			{
			switch(argv[Counter][1])
				{
				case 'D':
					ForkBackground = FALSE;
					break;
				case 'l':
					ListDevices = TRUE; 
					break;
				case 'c':
					if (argv[Counter+1])
						{
						bd_conf = argv[Counter+1];
						Counter++;
						}
					else
						PrintHelp();
					break;
				default:
					printf("Improper argument: %s\n", argv[Counter]);
				case '-':
					PrintHelp();
				}
			}
		}
	config.dev = NULL;
	config.description = "No Description";
	config.management_url = "https://127.0.0.1";
	config.filter = "ip or ether proto 1537";
	//config.filter = "ip";
	config.skip_intervals = CONFIG_GRAPHINTERVALS;
	config.graph_cutoff = CONFIG_GRAPHCUTOFF;
	config.promisc = TRUE;
	config.extensions = FALSE;
	config.graph = TRUE;
	config.output_cdf = FALSE;
	config.recover_cdf = FALSE;
	config.meta_refresh = CONFIG_METAREFRESH;
	config.output_database = FALSE;
	config.db_connect_string = NULL;
	config.sensor_name = "unset";  
	config.log_dir = LOG_DIR;
	config.htdocs_dir = HTDOCS_DIR;
	openlog("bandwidthd", LOG_CONS, LOG_DAEMON);
	// Locate configuration file
	if (!(bd_conf && !stat(bd_conf, &StatBuf)))
		{
		if (bd_conf)
			{
			printf("Could not find %s\n", bd_conf);
			exit(1);
			}
		else
			if (!stat("bandwidthd.conf", &StatBuf))
				bd_conf = "bandwidthd.conf";
			else
				if (!stat("./etc/bandwidthd.conf", &StatBuf))
					bd_conf = "./etc/bandwidthd.conf";
				else
					if (!stat(CONFIG_FILE, &StatBuf))
						bd_conf = CONFIG_FILE;
					else
						{
						printf("Cannot find bandwidthd.conf, ./etc/bandwidthd.conf or %s\n", CONFIG_FILE);
						syslog(LOG_ERR, "Cannot find bandwidthd.conf, ./etc/bandwidthd.conf or %s", CONFIG_FILE);
						exit(1);
						}
		}
	bdconfig_in = fopen(bd_conf, "rt");
	if (!bdconfig_in)
		{
		syslog(LOG_ERR, "Cannot open bandwidthd.conf");
		printf("Cannot open ./etc/bandwidthd.conf\n");
		exit(1);
		}
	bdconfig_parse();
	// Log list of monitored subnets
	for (Counter = 0; Counter < SubnetCount; Counter++)
		{
		addr.s_addr = ntohl(SubnetTable[Counter].ip);
		addr2.s_addr = ntohl(SubnetTable[Counter].mask);
		syslog(LOG_INFO, "Monitoring subnet %s with netmask %s", inet_ntoa(addr), inet_ntoa(addr2));
		}
  for (Counter = 0; Counter < NotSubnetCount; Counter++)
    {
    addr.s_addr = ntohl(NotSubnetTable[Counter].ip);
    addr2.s_addr = ntohl(NotSubnetTable[Counter].mask);
    syslog(LOG_INFO, "Ignoring subnet %s with netmask %s", inet_ntoa(addr), inet_ntoa(addr2));
    }
#ifdef HAVE_PCAP_FINDALLDEVS
	pcap_findalldevs(&Devices, Error);
	if (config.dev == NULL && Devices && Devices->name)
		config.dev = strdup(Devices->name);
	if (ListDevices)
		{	
		while(Devices)
			{
			printf("Description: %s\nName: \"%s\"\n\n", Devices->description, Devices->name);
			Devices = Devices->next;
			}
		exit(0);
		}
#else
	if (ListDevices)
		{
		printf("List devices is not supported by you version of libpcap\n");
		exit(0);
		}
#endif
	if (config.graph)
		bd_CollectingData();
	/* detach from console. */
	if (ForkBackground)
		if (fork2())
			exit(0);
	makepidfile(getpid());
	setchildconfig(0); /* initialize first (day graphing) process config */
	if (config.graph || config.output_cdf)
		{
		/* fork processes for week, month and year graphing. */
		for (i=0; i<NR_WORKER_CHILDS; i++) 
			{
			workerchildpids[i] = fork();
			/* initialize children and let them start doing work,
			 * while parent continues to fork children.
			 */
			if (workerchildpids[i] == 0) 
				{ /* child */
				setchildconfig(i+1);
				break;
				}
			if (workerchildpids[i] == -1) 
				{ /* fork failed */
				syslog(LOG_ERR, "Failed to fork graphing child (%d)", i);
				/* i--; ..to retry? -> possible infinite loop */
				continue;
				}
			}
		}	
	signal(SIGHUP, signal_handler);
	signal(SIGTERM, signal_handler);
	if(config.recover_cdf)
		RecoverDataFromCDF();
	IntervalStart = time(NULL);
	syslog(LOG_INFO, "Opening %s", config.dev);
	pd = pcap_open_live(config.dev, SNAPLEN, config.promisc, 1000, Error);
	if (pd == NULL) 
		{
		syslog(LOG_ERR, "%s", Error);
		exit(0);
		}
	if (pcap_compile(pd, &fcode, config.filter, 1, 0) < 0)
		{
		pcap_perror(pd, "Error");
		printf("Malformed libpcap filter string in bandwidthd.conf\n");
		syslog(LOG_ERR, "Malformed libpcap filter string in bandwidthd.conf");
		exit(1);
		}
	if (pcap_setfilter(pd, &fcode) < 0)
		pcap_perror(pd, "Error");
	switch (DataLink = pcap_datalink(pd))
		{
		default:
			if (config.dev)
				printf("Unknown Datalink Type %d, defaulting to ethernet\nPlease forward this error message and a packet sample (captured with \"tcpdump -i %s -s 2000 -n -w capture.cap\") to hinkle@derbyworks.com\n", DataLink, config.dev);
			else
				printf("Unknown Datalink Type %d, defaulting to ethernet\nPlease forward this error message and a packet sample (captured with \"tcpdump -s 2000 -n -w capture.cap\") to hinkle@derbyworks.com\n", DataLink);
			syslog(LOG_INFO, "Unkown datalink type, defaulting to ethernet");
		case DLT_EN10MB:
			syslog(LOG_INFO, "Packet Encoding: Ethernet");
			IP_Offset = 14; //IP_Offset = sizeof(struct ether_header);
			break;
#ifdef DLT_LINUX_SLL 
		case DLT_LINUX_SLL:
			syslog(LOG_INFO, "Packet Encoding: Linux Cooked Socket");
			IP_Offset = 16;
			break;
#endif
#ifdef DLT_RAW
		case DLT_RAW:
			printf("Untested Datalink Type %d\nPlease report to drachs@gmail.com if bandwidthd works for you\non this interface\n", DataLink);
			printf("Packet Encoding:\n\tRaw\n");
			syslog(LOG_INFO, "Untested packet encoding: Raw");
			IP_Offset = 0;
			break;
#endif
		case DLT_IEEE802:
			printf("Untested Datalink Type %d\nPlease report to drachs@gmail.com if bandwidthd works for you\non this interface\n", DataLink);
			printf("Packet Encoding:\nToken Ring\n");
			syslog(LOG_INFO, "Untested packet encoding: Token Ring");
			IP_Offset = 22;
			break;
		}
	if (ForkBackground)
		{
		fclose(stdin);
		fclose(stdout);
		fclose(stderr);
		}
	if (IPDataStore)  // If there is data in the datastore draw some initial graphs
		{
		syslog(LOG_INFO, "Drawing initial graphs");
		pidGraphingChild = WriteOutWebpages(IntervalStart+config.interval);
		}
#ifdef HAVE_PYTHON
	Py_Initialize();
	IpTableDict=PyDict_New();
	Py_Finalize();
#endif
	ResetTrafficCounters();
	// This is also set in CloseInterval because it gets overwritten in some commit modules
	signal(SIGALRM, handle_interval);
	alarm(config.interval);
	nice(1);
	while (1)
		{
		// Bookeeping
		if (IntervalFinished){  // Then write out this intervals data and possibly kick off the grapher
#ifdef HAVE_PYTHON
			PyDict_Clear(IpTableDict);
			Py_Finalize();
#endif
			CloseInterval();
		}
		// Process 1 buffer full of data
		if (pcap_dispatch(pd, -1, PacketCallback, pcap_userdata) == -1) 
			{
			syslog(LOG_ERR, "Bandwidthd: pcap_dispatch: %s",  pcap_geterr(pd));
			exit(1);
			}
		}
	pcap_close(pd);
	exit(0);
	}
   
void CloseInterval(void)
	{
	time_t current_time;
#ifdef PROFILE
	// Exit and write out profiling information before we commit the data
	exit(0);
#endif
	current_time = time(NULL);
	// Sanity check for time getting set forward
	if ((current_time > IntervalStart+(config.interval*100)) || (current_time < IntervalStart-(config.interval*100)))
		{
		IntervalStart = current_time-config.interval;
		ProgramStart = current_time;
		}
	GraphIntervalCount++;
	CommitData(IntervalStart+config.interval);
	BroadcastState(pcap_fileno(pd));
	ResetTrafficCounters();
	// Make sure the signal hasn't been overwritten by one of our CommitData modules
	// pgsql module does overwrite it
	signal(SIGALRM, handle_interval);
	alarm(config.interval);
	}

// Write an ethernet packet describing us out the given socket
void BroadcastState(int fd)
  {
  char buf[SNAPLEN];
  char enet_broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  struct ether_header *eptr;
  if (DataLink != DLT_EN10MB)
    return;
  if ((14+strlen(config.sensor_name)+strlen(config.dev)) > SNAPLEN)
    {
    syslog(LOG_ERR, "Sensor and device name too long for broadcast");
    return;
    }
  bzero(buf, sizeof(buf));
  eptr = (struct ether_header *) buf;    /* start of 16-byte Ethernet header */
  // Always broadcast
  memcpy(&buf[0], enet_broadcast, 6);
  memcpy(&buf[6], enet_broadcast, 6); // Would rather use our own mac address
  eptr->ether_type = htons(1537); // Random packet type we'll use for bandwidthd
  memcpy(buf+14, config.sensor_name, strlen(config.sensor_name)+1);
  memcpy(buf+14+strlen(config.sensor_name)+1, config.dev, strlen(config.dev)+1);
  if (write(fd, buf, SNAPLEN) != SNAPLEN)
    syslog(LOG_ERR, "Write error during bandwidthd broadcast");
  }

void ParseBroadcast(const u_char *in)
  {
  char *p = (char *)in+14; // Skip ethernet header
  struct Broadcast *bc;
  struct Broadcast *bc2;
  char *sensor_name;
  char *interface;
  sensor_name = p;
  interface = p+strlen(p)+1;
  // Sanity check
  if (strlen(sensor_name) > SNAPLEN || strlen(interface) > SNAPLEN)
    {
    syslog(LOG_ERR, "Bandwidthd broadcast packet failed sanity check, discarding");
    return;
    }
  if ((!strcmp(sensor_name, config.sensor_name)) && (!strcmp(interface, config.dev)))
    return; // Our own packet
  for (bc = Broadcasts; bc; bc = bc->next)
    {
    if ((!strcmp(sensor_name, bc->sensor_name)) && (!strcmp(interface, bc->interface)))
      {
      // Found this link
      bc->received = time(NULL);
      return;
      }
    }
  bc2 = malloc(sizeof(struct Broadcast));
  bc2->sensor_name = strdup(sensor_name);
  bc2->interface = strdup(interface);
  bc2->received = time(NULL);
  bc2->next = NULL;
  if (Broadcasts == NULL)
    {
    Broadcasts = bc2;
    return;
    }
  else
    {
    for (bc = Broadcasts; bc->next; bc = bc->next);
    bc->next = bc2;
    return;
    }
  }

void PacketCallback(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
	{
	unsigned int Counter;
	unsigned short SkipSrc = 0;
	unsigned short SkipDst = 0;
	u_int caplen = h->caplen;
	struct ether_header *eptr;
	struct VlanHeader *vlanhdr;
	const struct ip *ip;
	uint32_t srcip;
	uint32_t dstip;
	struct IPData *ptrIPData;
	int AlreadyTotaled = FALSE;
	PacketCallbackLock = TRUE;
	eptr = (struct ether_header *) p;
	vlanhdr = (struct vlanhdr *) p;
	if (eptr->ether_type == htons(1537))
		ParseBroadcast(p);
	if (vlanhdr->ether_type[0]==0x81 && vlanhdr->ether_type[1]==0x00) //Two byte-wise checks instead of 1 word-wise check to avoid word boundary issues on some intel processors
		p+=4;
	caplen -= IP_Offset;  // We're only measuring ip size, so pull off the ethernet header
	p += IP_Offset; // Move the pointer past the datalink header
	ip = (const struct ip *)p; // Point ip at the ip header
	if (ip->ip_v != 4) // then not an ip packet so skip it
		return;
	srcip = ntohl(*(uint32_t *) (&ip->ip_src));
	dstip = ntohl(*(uint32_t *) (&ip->ip_dst));
	for (Counter = 0; Counter < NotSubnetCount; Counter++)
		{
		if (NotSubnetTable[Counter].ip == (srcip & NotSubnetTable[Counter].mask))  //In the list of subnets we're ignoring.
			{
			SkipSrc=1;
			}
		if (NotSubnetTable[Counter].ip == (dstip & NotSubnetTable[Counter].mask))  //In the list of subnets we're ignoring.
			{
			SkipDst=1;
			}
		}
	for (Counter = 0; Counter < SubnetCount; Counter++)
		{
		// Packets from a monitored subnet to a monitored subnet will be
		// credited to both ip's
		// Totals are checked for first to speed up FindIp in the total case
		// That way the 0 entry is always first in the FindIp table
		if ((SubnetTable[Counter].ip == (srcip & SubnetTable[Counter].mask)) && !SkipSrc)
			{
			if (!AlreadyTotaled)
				{
				Credit(&(IpTable[0].Send), ip);
				AlreadyTotaled = TRUE;
				}
			ptrIPData = FindIp(srcip);	// Return or create this ip's data structure
			if (ptrIPData)
				Credit(&(ptrIPData->Send), ip);
			}
		if ((SubnetTable[Counter].ip == (dstip & SubnetTable[Counter].mask)) && !SkipDst)
			{
			if (!AlreadyTotaled)
				{
				Credit(&(IpTable[0].Receive), ip);
				AlreadyTotaled = TRUE;
				}
			ptrIPData = FindIp(dstip);
			if (ptrIPData)
				Credit(&(ptrIPData->Receive), ip);
			}
		}
	PacketCallbackLock = FALSE;
	}

// Eliminates duplicate entries and fully included subnets so packets don't get
// counted multiple times
void MonitorSubnet(unsigned int ip, unsigned int mask)
	{
	unsigned int subnet = ip & mask;
	int Counter, Counter2;
	struct in_addr addr, addr2;
	addr.s_addr = ntohl(subnet);
	addr2.s_addr = ntohl(mask);
	for (Counter = 0; Counter < SubnetCount; Counter++)
		{
		if ((SubnetTable[Counter].ip == subnet) && (SubnetTable[Counter].mask == mask))
			{
			syslog(LOG_ERR, "Subnet %s/%s already exists, skipping.", inet_ntoa(addr), inet_ntoa(addr2));
			return; 
			}
		}
	for (Counter = 0; Counter < SubnetCount; Counter++)
		{
		if ((SubnetTable[Counter].ip == (ip & SubnetTable[Counter].mask)) && (SubnetTable[Counter].mask < mask))
			{
			syslog(LOG_ERR, "Subnet %s/%s is already included, skipping.", inet_ntoa(addr), inet_ntoa(addr2));
			return; 
			}
		}
	for (Counter = 0; Counter < SubnetCount; Counter++)
		{
		if (((SubnetTable[Counter].ip & mask) == subnet) && (SubnetTable[Counter].mask > mask))
			{
			syslog(LOG_ERR, "Subnet %s/%s includes already listed subnet, removing smaller entry", inet_ntoa(addr), inet_ntoa(addr2));
			// Shift everything down
			for (Counter2 = Counter; Counter2 < SubnetCount-1; Counter2++)
				{
				SubnetTable[Counter2].ip = SubnetTable[Counter2+1].ip;
				SubnetTable[Counter2].mask = SubnetTable[Counter2+1].mask;
				}
			SubnetCount--;
			Counter--; // Retest this entry because we replaced it 
			}
		}
	SubnetTable[SubnetCount].mask = mask;
	SubnetTable[SubnetCount].ip = subnet;
	SubnetCount++;
	}

void IgnoreMonitorSubnet(unsigned int ip, unsigned int mask)
  {
  unsigned int subnet = ip & mask;
  int Counter, Counter2;
  struct in_addr addr, addr2;
  addr.s_addr = ntohl(subnet);
  addr2.s_addr = ntohl(mask);
  for (Counter = 0; Counter < NotSubnetCount; Counter++)
    {
    if ((NotSubnetTable[Counter].ip == subnet) && (NotSubnetTable[Counter].mask == mask))
      {
      syslog(LOG_ERR, "Subnet %s/%s already exists, skipping.", inet_ntoa(addr), inet_ntoa(addr2));
      return;
      }
    }
  for (Counter = 0; Counter < NotSubnetCount; Counter++)
    {
    if ((NotSubnetTable[Counter].ip == (ip & NotSubnetTable[Counter].mask)) && (NotSubnetTable[Counter].mask < mask))
      {
      syslog(LOG_ERR, "Subnet %s/%s is already excluded, skipping.", inet_ntoa(addr), inet_ntoa(addr2));
      return;
      }
    }
  for (Counter = 0; Counter < NotSubnetCount; Counter++)
    {
    if (((NotSubnetTable[Counter].ip & mask) == subnet) && (NotSubnetTable[Counter].mask > mask))
      {
      syslog(LOG_ERR, "Subnet %s/%s includes already excluded subnet, removing smaller entry", inet_ntoa(addr), inet_ntoa(addr2));
      // Shift everything down
      for (Counter2 = Counter; Counter2 < NotSubnetCount-1; Counter2++)
        {
        NotSubnetTable[Counter2].ip = NotSubnetTable[Counter2+1].ip;
        NotSubnetTable[Counter2].mask = NotSubnetTable[Counter2+1].mask;
        }
      NotSubnetCount--;
      Counter--; // Retest this entry because we replaced it
      }
    }
  NotSubnetTable[NotSubnetCount].mask = mask;
  NotSubnetTable[NotSubnetCount].ip = subnet;
  NotSubnetCount++;
  }

inline void Credit(struct Statistics *Stats, const struct ip *ip)
	{
	unsigned long size;
	const struct tcphdr *tcp;
	uint16_t port;
	int Counter;
	size = ntohs(ip->ip_len);
	Stats->total += size;
	Stats->packet_count++;
	switch(ip->ip_p)
		{
		case 6:		// TCP
			tcp = (struct tcphdr *)(ip+1);
			tcp = (struct tcphdr *) ( ((char *)tcp) + ((ip->ip_hl-5)*4) ); // Compensate for IP Options
			Stats->tcp += size;
			// This is a wierd way to do this, I know, but the old "if/then" structure burned alot more CPU
			for (port = ntohs(tcp->TCPHDR_DPORT), Counter=2 ; Counter ; port = ntohs(tcp->TCPHDR_SPORT), Counter--)
				{
				switch(port)
					{
					case 110:
					case 25:
					case 143:
					case 587:
						Stats->mail += size;
						return;
					case 80:
					case 443:
						Stats->http += size;
						return;
					case 20:
					case 21:
						Stats->ftp += size;
						return;
					case 1044:	// Direct File Express
					case 1045:	// ''  <- Dito Marks
					case 1214:	// Grokster, Kaza, Morpheus
					case 4661:	// EDonkey 2000
					case 4662:	// ''
					case 4665:	// ''
					case 5190:	// Song Spy
					case 5500:	// Hotline Connect
					case 5501:	// ''
					case 5502:	// ''
					case 5503:	// ''
					case 6346:	// Gnutella Engine
					case 6347:	// ''
					case 6666:	// Yoink
					case 6667:	// ''
					case 7788:	// Budy Share
					case 8888:	// AudioGnome, OpenNap, Swaptor
					case 8889:	// AudioGnome, OpenNap
					case 28864: // hotComm
					case 28865: // hotComm
						Stats->p2p += size;
						return;
					}
				}
			return;
		case 17:
			Stats->udp += size;
			return;
		case 1: 
			Stats->icmp += size;
			return;
		}
	}

// TODO:  Throw away old data!
void DropOldData(long int timestamp)	// Go through the ram datastore and dump old data
	{
	struct IPDataStore *DataStore;
	struct IPDataStore *PrevDataStore;
	struct DataStoreBlock *DeletedBlock;
	PrevDataStore = NULL;
	DataStore = IPDataStore;
	// Progress through the linked list until we reach the end
	while(DataStore)  // we have data
		{
		// If the First block is out of date, purge it, if it is the only block
		// purge the node
		while(DataStore->FirstBlock->LatestTimestamp < timestamp - config.range)
			{
			if ((!DataStore->FirstBlock->Next) && PrevDataStore) // There is no valid block of data for this ip, so unlink the whole ip
				{												// Don't bother unlinking the ip if it's the first one, that's to much
																// Trouble
				PrevDataStore->Next = DataStore->Next;	// Unlink the node
				free(DataStore->FirstBlock->Data);		// Free the memory
				free(DataStore->FirstBlock);
				free(DataStore);
				DataStore = PrevDataStore->Next;	// Go to the next node
				if (!DataStore) return; // We're done
				}
			else if (!DataStore->FirstBlock->Next)
				{
				// There is no valid block of data for this ip, and we are 
				// the first ip, so do nothing 
				break; // break out of this loop so the outside loop increments us
				} 
			else // Just unlink this block
				{
				DeletedBlock = DataStore->FirstBlock;
				DataStore->FirstBlock = DataStore->FirstBlock->Next;	// Unlink the block
				free(DeletedBlock->Data);
				free(DeletedBlock);
				}
			}
		PrevDataStore = DataStore;
		DataStore = DataStore->Next;
		}
	}

void StoreIPDataInDatabase(struct IPData IncData[], struct extensions *extension_data)
	{
	if (config.output_database == DB_PGSQL)
		pgsqlStoreIPData(IncData, extension_data);
	else if(config.output_database == DB_SQLITE)
		sqliteStoreIPData(IncData, extension_data);
	}

void StoreIPDataInCDF(struct IPData IncData[])
	{
	struct IPData *IPData;
	unsigned int Counter;
	FILE *cdf;
	struct Statistics *Stats;
	char IPBuffer[50];
	char logfile[MAX_FILENAME];
	snprintf(logfile, MAX_FILENAME, "%s/log.%c.0.cdf", config.log_dir, config.tag);
	cdf = fopen(logfile, "at");
	for (Counter=0; Counter < IpCount; Counter++)
		{
		IPData = &IncData[Counter];
		HostIp2CharIp(IPData->ip, IPBuffer);
		fprintf(cdf, "%s,%lu,", IPBuffer, IPData->timestamp);
		Stats = &(IPData->Send);
		fprintf(cdf, "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu", Stats->total, Stats->icmp, Stats->udp, Stats->tcp, Stats->ftp, Stats->http, Stats->mail, Stats->p2p); 
		Stats = &(IPData->Receive);
		fprintf(cdf, "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n", Stats->total, Stats->icmp, Stats->udp, Stats->tcp, Stats->ftp, Stats->http, Stats->mail, Stats->p2p); 
		}
	fclose(cdf);
	}

void _StoreIPDataInRam(struct IPData *IPData)
	{
	struct IPDataStore *DataStore;
	struct DataStoreBlock *DataStoreBlock;
	if (!IPDataStore) // we need to create the first entry
		{
		// Allocate Datastore for this IP
		IPDataStore = malloc(sizeof(struct IPDataStore));
		IPDataStore->ip = IPData->ip;
		IPDataStore->Next = NULL;
		// Allocate it's first block of storage
		IPDataStore->FirstBlock = malloc(sizeof(struct DataStoreBlock));
		IPDataStore->FirstBlock->LatestTimestamp = 0;
		IPDataStore->FirstBlock->NumEntries = 0;
		IPDataStore->FirstBlock->Data = calloc(IPDATAALLOCCHUNKS, sizeof(struct IPData));
		IPDataStore->FirstBlock->Next = NULL;
		if (!IPDataStore->FirstBlock || ! IPDataStore->FirstBlock->Data)
			{
			syslog(LOG_ERR, "Could not allocate datastore! Exiting!");
			exit(1);
			}
		}
	DataStore = IPDataStore;
	// Take care of first case
	while (DataStore) // Is not null
		{
		if (DataStore->ip == IPData->ip) // then we have the right store
			{
			DataStoreBlock = DataStore->FirstBlock;
			while(DataStoreBlock) // is not null
				{
				if (DataStoreBlock->NumEntries < IPDATAALLOCCHUNKS) // We have a free spot
					{
					memcpy(&DataStoreBlock->Data[DataStoreBlock->NumEntries++], IPData, sizeof(struct IPData));
					DataStoreBlock->LatestTimestamp = IPData->timestamp;
					return;
					}
				else
					{
					if (!DataStoreBlock->Next) // there isn't another block, add one
						{
						DataStoreBlock->Next = malloc(sizeof(struct DataStoreBlock));
						DataStoreBlock->Next->LatestTimestamp = 0;
						DataStoreBlock->Next->NumEntries = 0;
						DataStoreBlock->Next->Data = calloc(IPDATAALLOCCHUNKS, sizeof(struct IPData));
						DataStoreBlock->Next->Next = NULL;																				
						}
					DataStoreBlock = DataStoreBlock->Next;
					}
				}
			return;
			}
		else
			{
			if (!DataStore->Next) // there is no entry for this ip, so lets make one.
				{
				// Allocate Datastore for this IP
				DataStore->Next = malloc(sizeof(struct IPDataStore));
				DataStore->Next->ip = IPData->ip;
				DataStore->Next->Next = NULL;
				// Allocate it's first block of storage
				DataStore->Next->FirstBlock = malloc(sizeof(struct DataStoreBlock));
				DataStore->Next->FirstBlock->LatestTimestamp = 0;
				DataStore->Next->FirstBlock->NumEntries = 0;
				DataStore->Next->FirstBlock->Data = calloc(IPDATAALLOCCHUNKS, sizeof(struct IPData));
				DataStore->Next->FirstBlock->Next = NULL;
				}
			DataStore = DataStore->Next;
			}
		}
	}

void StoreIPDataInRam(struct IPData IncData[])
	{
	unsigned int Counter;
	for (Counter=0; Counter < IpCount; Counter++)
		_StoreIPDataInRam(&IncData[Counter]);
	}

void CommitData(time_t timestamp)
	{
	int MayGraph = FALSE;
	unsigned int Counter;
	struct stat StatBuf;
	char logname1[MAX_FILENAME];
	char logname2[MAX_FILENAME];
	int offset;
	struct extensions *extension_data;
	// Set the timestamps
	for (Counter=0; Counter < IpCount; Counter++)
		IpTable[Counter].timestamp = timestamp;
	// Call extensions
	if (config.extensions)
		extension_data = execute_extensions();
	else
		extension_data = NULL;
#ifdef HAVE_PYTHON
	//Run python plugin script on IpTable
	Py_Initialize();
	initbandwidthd();
	iptable_Transform(IpCount);
	Py_Finalize();
#endif
	// Output modules
	// Only call this from first thread
	if (config.output_database && config.tag == '1')
		StoreIPDataInDatabase(IpTable, extension_data);
	if (config.output_cdf)
		{
		// TODO: This needs to be moved into the forked section, but I don't want to 
		//	deal with that right now (Heavy disk io may make us drop packets)
		StoreIPDataInCDF(IpTable);
		if (RotateLogs >= config.range/RANGE1) // We set this++ on HUP
			{
			snprintf(logname1, MAX_FILENAME, "%s/log.%c.%n4.cdf", config.log_dir, config.tag, &offset); 
			snprintf(logname2, MAX_FILENAME, "%s/log.%c.5.cdf", config.log_dir, config.tag); 
			if (!stat(logname2, &StatBuf)) // File exists
				unlink(logname2);
			if (!stat(logname1, &StatBuf)) // File exists
				rename(logname1, logname2);
			logname1[offset] = '3';
			logname2[offset] = '4';
			if (!stat(logname1, &StatBuf)) // File exists
				rename(logname1, logname2);
			logname1[offset] = '2';
			logname2[offset] = '3';
			if (!stat(logname1, &StatBuf)) // File exists
				rename(logname1, logname2);
			logname1[offset] = '1';
			logname2[offset] = '2';
			if (!stat(logname1, &StatBuf)) // File exists
				rename(logname1, logname2);
			logname1[offset] = '0';
			logname2[offset] = '1';
			if (!stat(logname1, &StatBuf)) // File exists
				rename(logname1, logname2); 
			fclose(fopen(logname1, "at")); // Touch file
			RotateLogs = FALSE;
			}
		}
	if (config.graph)
		{
		StoreIPDataInRam(IpTable);
		// If there is no child to wait on or child returned or waitpid produced an error
		if (pidGraphingChild < 1 || waitpid(pidGraphingChild, NULL, WNOHANG))
			MayGraph = TRUE;
		if (GraphIntervalCount%config.skip_intervals == 0 && MayGraph)
			pidGraphingChild = WriteOutWebpages(timestamp);
		else if (GraphIntervalCount%config.skip_intervals == 0)
			syslog(LOG_INFO, "Previouse graphing run not complete... Skipping current run");
		DropOldData(timestamp);
		}
	destroy_extension_data(extension_data);
	}


int RCDF_Test(char *filename)
	{
	// Determine if the first date in the file is before the cutoff
	// return FALSE on error
	FILE *cdf;
	char ipaddrBuffer[16];
	time_t timestamp;
	if (!(cdf = fopen(filename, "rt"))) 
		return FALSE;
	fseek(cdf, 10, SEEK_END); // fseek to near end of file
	while (fgetc(cdf) != '\n') // rewind to last newline
		{
		if (fseek(cdf, -2, SEEK_CUR) == -1)
			break;
		}
	if(fscanf(cdf, " %15[0-9.],%lu,", ipaddrBuffer, &timestamp) != 2)
		{
		syslog(LOG_ERR, "%s is corrupted, skipping", filename); 
		return FALSE;
		}
	fclose(cdf);
	if (timestamp < time(NULL) - config.range)
		return FALSE; // There is no data in this file from after the cutoff
	else
		return TRUE; // This file has data from after the cutoff
	}


void RCDF_PositionStream(FILE *cdf)
	{
	time_t timestamp;
	time_t current_timestamp;
	char ipaddrBuffer[16];
	current_timestamp = time(NULL);
	fseek(cdf, 0, SEEK_END);
	timestamp = current_timestamp;
	while (timestamp > current_timestamp - config.range)
		{
		// What happenes if we seek past the beginning of the file?
		if (fseek(cdf, -512*1024,SEEK_CUR) == -1 || ferror(cdf))
			{ // fseek returned error, just seek to beginning
			clearerr(cdf);
			fseek(cdf, 0, SEEK_SET);
			return;
			}
		while (fgetc(cdf) != '\n' && !feof(cdf)); // Read to next line
		ungetc('\n', cdf);	// Just so the fscanf mask stays identical
		if(fscanf(cdf, " %15[0-9.],%lu,", ipaddrBuffer, &timestamp) != 2)
			{
			syslog(LOG_ERR, "Unknown error while scanning for beginning of data...\n");
			return;
			}
		}
	while (fgetc(cdf) != '\n' && !feof(cdf));
	ungetc('\n', cdf); 
	}

void ResetTrafficCounters(void)
	{
	// Set to 1 because "totals" are the 0 slot
	IpCount = 1;
	IntervalStart=time(NULL);
	IntervalFinished = FALSE;
	memset(IpTable, 0, sizeof(struct IPData)*IP_NUM);
#ifdef HAVE_PYTHON
	Py_Initialize();
#endif
	}

void RCDF_Load(FILE *cdf)
	{
	time_t timestamp;
	time_t current_timestamp = 0;
	struct in_addr ipaddr;
	struct IPData *ip=NULL;
	char ipaddrBuffer[16];
	unsigned long int Counter = 0;
	unsigned long int IntervalsRead = 0;
	for(Counter = 0; !feof(cdf) && !ferror(cdf); Counter++)
		{
		if(fscanf(cdf, " %15[0-9.],%lu,", ipaddrBuffer, &timestamp) != 2) 
			goto End_RecoverDataFromCdf;
		if (!timestamp) // First run through loop
			current_timestamp = timestamp;
		if (timestamp != current_timestamp)
			{ // Dump to datastore
			StoreIPDataInRam(IpTable);
			IpCount = 0;
			current_timestamp = timestamp;
			IntervalsRead++;
			}
		inet_aton(ipaddrBuffer, &ipaddr);
		ip = FindIp(ntohl(ipaddr.s_addr));
		ip->timestamp = timestamp;
		if (fscanf(cdf, "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,",
			&ip->Send.total, &ip->Send.icmp, &ip->Send.udp,
			&ip->Send.tcp, &ip->Send.ftp, &ip->Send.http, &ip->Send.mail, &ip->Send.p2p) != 7
		  || fscanf(cdf, "%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
			&ip->Receive.total, &ip->Receive.icmp, &ip->Receive.udp,
			&ip->Receive.tcp, &ip->Receive.ftp, &ip->Receive.http, &ip->Receive.mail, &ip->Receive.p2p) != 7)
			goto End_RecoverDataFromCdf;
		}
End_RecoverDataFromCdf:
	StoreIPDataInRam(IpTable);
	syslog(LOG_INFO, "Finished recovering %lu records", Counter);	
	DropOldData(time(NULL)); // Dump the extra data
	if(!feof(cdf))
	   syslog(LOG_ERR, "Failed to parse part of log file. Giving up on the file");
	IpCount = 0; // Reset traffic Counters
	fclose(cdf);
	}

void RecoverDataFromCDF(void)
	{
	FILE *cdf;
	char index[] = "012345";
	char logname[MAX_FILENAME];
	int offset;
	int Counter;
	int First = FALSE;
	snprintf(logname, MAX_FILENAME, "%s/log.%c.%n0.cdf", config.log_dir, config.tag, &offset);
	for (Counter = 5; Counter >= 0; Counter--)
		{
		logname[offset] = index[Counter];
		if (RCDF_Test(logname))
			break;
		}
	First = TRUE;
	for (; Counter >= 0; Counter--)
		{
		logname[offset] = index[Counter];
		if ((cdf = fopen(logname, "rt")))
			{
			syslog(LOG_INFO, "Recovering from %s", logname);
			if (First)
				{
				RCDF_PositionStream(cdf);
				First = FALSE;
				}
			RCDF_Load(cdf);
			}
		}
	}

// ****** FindIp **********
// ****** Returns or allocates an Ip's data structure

inline struct IPData *FindIp(uint32_t ipaddr)
	{
	unsigned int Counter;
	static time_t last_error = 0;
#ifndef HAVE_PYTHON
	for (Counter=0; Counter < IpCount; Counter++)
		if (IpTable[Counter].ip == ipaddr)
			return (&IpTable[Counter]);
#else
	PyObject *oIP;
	PyObject *oCounter;
	oIP=PyInt_FromLong(ipaddr);
	Counter=PyDict_Contains(IpTableDict, oIP);
	if (Counter==-1){
		printf("PyDict_Contains had an error.\n");
		abort();
	}
	if (Counter){
		Counter=PyInt_AsLong(PyDict_GetItem(IpTableDict, oIP));
		Py_DECREF(oIP);
		return &IpTable[Counter];
	}
#endif
	if (IpCount >= IP_NUM)
		{
		time_t current_error = time(NULL);
		if (current_error > last_error + 30)
			{
			last_error = current_error;
			syslog(LOG_ERR, "Maximum tracked IP's (%d) is too low, some ips will be ignored.  Possible network scan?", IP_NUM);
			}
		return(NULL);
		}
#ifndef HAVE_PYTHON
	IpTable[IpCount].ip = ipaddr;
	return (&IpTable[IpCount++]);
#else
	oCounter=PyInt_FromLong(IpCount);
  IpTable[IpCount].ip = ipaddr;
  PyDict_SetItem(IpTableDict, oIP, oCounter);
  Py_DECREF(oCounter);
  Py_DECREF(oIP);
  return (&IpTable[IpCount++]);
#endif
	}

char inline *HostIp2CharIp(unsigned long ipaddr, char *buffer)
	{
	struct in_addr in_addr;
	char *s;
	in_addr.s_addr = htonl(ipaddr);	
	s = inet_ntoa(in_addr);
	strncpy(buffer, s, 16);
	buffer[15] = '\0';
	return(buffer);
/*	uint32_t ip = *(uint32_t *)ipaddr;
	sprintf(buffer, "%d.%d.%d.%d", (ip << 24)  >> 24, (ip << 16) >> 24, (ip << 8) >> 24, (ip << 0) >> 24);
*/
	}


// Add better error checking
int fork2()
	{
	pid_t pid;
	if (!(pid = fork()))
		{
		if (!fork())
			{
#ifdef PROFILE
				// Got this incantation from a message board.  Don't forget to set
				// GMON_OUT_PREFIX in the shell
				extern void _start(void), etext(void);
				syslog(LOG_INFO, "Calling profiler startup...");
				monstartup((u_long) &_start, (u_long) &etext);
#endif
			return(0);
			}		 
		_exit(0);
		}
	waitpid(pid, NULL, 0);
	return(1);
	}
#ifdef HAVE_PYTHON
void iptable_Transform(uint32_t Counter)
  {
  PyObject *pName, *pModule, *pFunc;
  PyObject *pArgs, *pValue;
  pName = PyString_FromString("IPTableTransform");
  PyRun_SimpleString("import sys\nsys.path.append('.')\n");
  pModule = PyImport_Import(pName);
  Py_DECREF(pName);
  if (pModule != NULL)
    {
    pFunc = PyObject_GetAttrString(pModule, "main");
    if (pFunc && PyCallable_Check(pFunc))
      {
      pArgs= PyTuple_New(2);
      PyTuple_SetItem(pArgs, 0, PyLong_FromUnsignedLong(Counter));
      PyTuple_SetItem(pArgs, 1, PyString_FromString(config.sensor_name));
      pValue = PyObject_CallObject(pFunc, pArgs);
      Py_DECREF(pArgs);
      if (pValue != NULL)
        {
        printf("Result of call: %s\n", PyString_AsString(pValue));
        Py_DECREF(pValue);
        }
      else
        {
        Py_DECREF(pFunc);
        Py_DECREF(pModule);
        PyErr_Print();
        fprintf(stderr,"Call failed\n");
        Py_Finalize();
        return;
        }
      }
    }
  return;
  }

static PyObject *bandwidthd_get_entry_as_dict(PyObject *self, PyObject* args)
{
  uint32_t iCounter;
  struct IPData ip_entry;
  if (!PyArg_ParseTuple(args, "k", &iCounter))
    return NULL;
  ip_entry=IpTable[iCounter];
  return Py_BuildValue("{s:k,s:s,s:l,s:{s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K},s:{s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K}}",
    "ip"    , ip_entry.ip,
    "mac"    , ip_entry.mac,
    "timestamp" , ip_entry.timestamp,
    "send"    ,
    "total"  , ip_entry.Send.total,
    "tcp"    , ip_entry.Send.tcp,
    "udp"    , ip_entry.Send.udp,
    "p2p"    , ip_entry.Send.p2p,
    "mail"    , ip_entry.Send.mail,
    "icmp"    , ip_entry.Send.icmp,
    "http"    , ip_entry.Send.http,
    "ftp"    , ip_entry.Send.ftp,
    "packets"   , ip_entry.Send.packet_count,
    "receive"   ,
    "total"  , ip_entry.Receive.total,
    "tcp"    , ip_entry.Receive.tcp,
    "udp"    , ip_entry.Receive.udp,
    "p2p"    , ip_entry.Receive.p2p,
    "mail"    , ip_entry.Receive.mail,
    "icmp"    , ip_entry.Receive.icmp,
    "http"    , ip_entry.Receive.http,
    "ftp"    , ip_entry.Receive.ftp,
    "packets"   , ip_entry.Receive.packet_count
    );
}
static PyObject *bandwidthd_set_entry_by_dict(PyObject *self, PyObject* args)
{
#if 0
  uint32_t iCounter;
  int *len;
  char *mac;
  struct IPData ip_entry;
  if (!PyArg_ParseTuple(args,
   "k(ks#lKKKKKKKKKKKKKKKKKK)", &iCounter, &ip_entry.ip, &mac,&len, &ip_entry.timestamp, &ip_entry.Send.total, &ip_entry.Send.tcp, &ip_entry.Send.udp, &ip_entry.Send.p2p, &ip_entry.Send.mail, &ip_entry.Send.icmp, &ip_entry.Send.http, &ip_entry.Send.ftp, &ip_entry.Send.packet_count, &ip_entry.Receive.total, &ip_entry.Receive.tcp, &ip_entry.Receive.udp, &ip_entry.Receive.p2p, &ip_entry.Receive.mail, &ip_entry.Receive.icmp, &ip_entry.Receive.http, &ip_entry.Receive.ftp, &ip_entry.Receive.packet_count))
    return NULL;
  strncpy(IpTable[iCounter].mac[0], mac, 17);
  IpTable[iCounter].mac[0][17] = '\0';
  printf("ip: %i\nnew_ip: %i\n", IpTable[iCounter].ip,ip_entry.ip);
  return Py_BuildValue("s",IpTable[iCounter].mac[0]);
#else
  uint32_t iCounter;
  int *len;
  char *mac;
  if (!PyArg_ParseTuple(args, "ks#", &iCounter, &mac,&len))
     return NULL;
  strncpy(IpTable[iCounter].mac[0], mac, 17);
  IpTable[iCounter].mac[0][17] = '\0';
  return Py_BuildValue("s",IpTable[iCounter].mac[0]);
#endif
}


static PyMethodDef bandwidthd_methods[] = {
  {"get_entry",      bandwidthd_get_entry_as_dict,    METH_VARARGS, "Return dictionary of IpTable entry for the given ip and timestamp."},
  {"set_entry",      bandwidthd_set_entry_by_dict,    METH_VARARGS, "Set dictionary of IpTable entry for the given ip and timestamp."},
  {NULL,        NULL}      /* sentinel */
};

void initbandwidthd(void)
{
  PyImport_AddModule("bandwidthd");
  Py_InitModule("bandwidthd", bandwidthd_methods);
}
#endif //HAVE_PYTHON
