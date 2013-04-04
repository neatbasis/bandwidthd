#include <unistd.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <time.h>
#include "bandwidthd.h"

#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif

extern unsigned int SubnetCount;
extern struct config config;

jmp_buf dnsjump;

static void rdnslngjmp(int signal);

void rdns(char *Buffer, unsigned long IP)  // This takes over sigalarm!
	{
#ifdef HAVE_RESOLV_H
	char DNSError[] = "DNS Timeout: Correct to speed up graphing";
	char None[] = "Configure DNS to reverse this IP";
	char TooManyDNSTimeouts[] = "Too many dns timeouts, reverse lookups suspended";
	struct hostent *hostent;
	char chrIP[50];
	static int Init = TRUE;
	static int DNSTimeouts = 0;  // This is reset for each run because we're forked
	unsigned long addr = htonl(IP);

	_res.retrans = 1;
	_res.retry = 2;

	if (Init)
		{
		signal(SIGALRM, rdnslngjmp);
		Init = FALSE;
		}

	if (DNSTimeouts > 100)
		{
		syslog(LOG_ERR, "Too many dns timeouts, reverse lookups suspended");
		strncpy(Buffer, TooManyDNSTimeouts, 253);
		Buffer[254] = '\0';
		return;
		}		

	if (setjmp(dnsjump) == 0)
		{
		alarm(10);	// Don't let gethostbyaddr hold us up too long
		hostent = gethostbyaddr((char *) &addr, 4, AF_INET); // (char *)&Data->IP				
		alarm(0);
		
		if (hostent)
			sprintf(Buffer, "%s", hostent->h_name);
		else
			{
			strncpy(Buffer, None, 253);
			Buffer[254] = '\0';
			}
		}
	else  // Our alarm timed out
		{
		HostIp2CharIp(IP, chrIP);
		syslog(LOG_ERR, "DNS timeout for %s: This problem reduces graphing performance", chrIP);
		DNSTimeouts++;
		strncpy(Buffer, DNSError, 253);
		Buffer[254] = '\0';
		}
#else
	Buffer[0] = '\0';
#endif
	}

static void rdnslngjmp(int signal)
	{
	longjmp(dnsjump, 1);
	}

void swap(struct SummaryData **a, struct SummaryData **b) {
	struct SummaryData *temp;
	temp = *a; *a = *b; *b = temp;
}
void QuickSortSummaryData(struct SummaryData *SummaryData[], int left, int right) {
	int i,j,center;
	unsigned long long pivot;
	if (left==right) return;
	if (left+1==right) {
		if (SummaryData[left]->Total < SummaryData[right]->Total)
			swap(&SummaryData[left],&SummaryData[right]);
		return;
	}
	/* use the median-of-three method for picking pivot */
	center = (left+right)/2;
	if (SummaryData[left]->Total < SummaryData[center]->Total)
		swap(&SummaryData[left],&SummaryData[center]);
	if (SummaryData[left]->Total < SummaryData[right]->Total)
		swap(&SummaryData[left],&SummaryData[right]);
	if (SummaryData[center]->Total < SummaryData[right]->Total)
		swap(&SummaryData[center],&SummaryData[right]);
	pivot = SummaryData[center]->Total;
	swap(&SummaryData[center],&SummaryData[right-1]); /* hide the pivot */
	i = left; j = right - 1;
	do {
		do { ++i; } while (SummaryData[i]->Total > pivot);
		do { --j; } while (SummaryData[j]->Total < pivot);
		swap(&SummaryData[i],&SummaryData[j]);
	} while (j > i);
	swap(&SummaryData[i],&SummaryData[j]); /* undo last swap */
	swap(&SummaryData[i],&SummaryData[right-1]); /* restore pivot */
	QuickSortSummaryData(SummaryData,left,i-1);
	QuickSortSummaryData(SummaryData,i+1,right);
}

#define NumFactor 1024
static void FormatNum(unsigned long long n, char *buf, int len) {
	double f;
	if (n<NumFactor) { snprintf(buf,len,"<td align=\"right\"><tt>%i&nbsp;</tt></td>",(int)n); return; }
	f = n;
	f /= NumFactor; if (f<NumFactor) { snprintf(buf,len,"<td align=\"right\"><tt>%.1fK</tt></td>",f); return; }
	f /= NumFactor; if (f<NumFactor) { snprintf(buf,len,"<td align=\"right\"><tt>%.1fM</tt></td>",f); return; }
	f /= NumFactor; if (f<NumFactor) { snprintf(buf,len,"<td align=\"right\"><tt>%.1fG</tt></td>",f); return; }
	f /= NumFactor; snprintf(buf,len,"<td align=\"right\"><tt>%.1fT</tt></td>\n",f);
}

void PrintTableLine(FILE *stream, struct SummaryData *Data, int Counter)
	{
	char Buffer1[50];
	char Buffer2[50];
	char Buffer3[50];
	char Buffer4[50];
	char Buffer4b[50];
	char Buffer5[50];
	char Buffer5b[50];
	char Buffer5c[50];
	char Buffer6[50];
	char Buffer7[50];
	char Buffer8[50];

	// First convert the info to nice, human readable stuff
	if (Data->IP == 0)
		strcpy(Buffer1, "Total");
	else
		HostIp2CharIp(Data->IP, Buffer1);

	FormatNum(Data->Total,		   Buffer2,  50);
	FormatNum(Data->TotalSent,	   Buffer3,  50);
	FormatNum(Data->TotalReceived, Buffer4,  50);
	FormatNum(Data->FTP,		   Buffer4b, 50);
	FormatNum(Data->HTTP,		   Buffer5,  50);
	FormatNum(Data->MAIL,		   Buffer5c, 50);
	FormatNum(Data->P2P,		   Buffer5b, 50);
	FormatNum(Data->TCP,		   Buffer6,  50);
	FormatNum(Data->UDP,		   Buffer7,  50);
	FormatNum(Data->ICMP,		   Buffer8,  50);

	if (Counter%4 == 0 || (Counter-1)%4 == 0)
		fprintf(stream, "<TR>");
	else
		fprintf(stream, "<TR bgcolor=lightblue>");

	if (Data->Graph)
		fprintf(stream, "<TD><a href=\"#%s-%c\">%s</a></TD>%s%s%s%s%s%s%s%s%s%s</TR>\n",
			Buffer1, // Ip
			config.tag,
			Buffer1, // Ip
			Buffer2, // Total
			Buffer3, // TotalSent
			Buffer4, // TotalReceived
			Buffer4b, // FTP
			Buffer5, // HTTP
			Buffer5c, // Mail
			Buffer5b, // P2P
			Buffer6, // TCP
			Buffer7, // UDP
			Buffer8); // ICMP
	else
		fprintf(stream, "<TD>%s</TD>%s%s%s%s%s%s%s%s%s%s</TR>\n",
			Buffer1, // Ip
			Buffer2, // Total
			Buffer3, // TotalSent
			Buffer4, // TotalReceived
			Buffer4b, // FTP
			Buffer5, // HTTP
			Buffer5c, // Mail
			Buffer5b, // P2P		
			Buffer6, // TCP
			Buffer7, // UDP
			Buffer8); // ICMP
	}

void MakeIndexPages(int NumIps, struct SummaryData *SummaryData[])
	{
	int SubnetCounter;
	int Counter, tCounter;
	time_t WriteTime;
	char filename[MAX_FILENAME];
	char *PeriodDesc;
	
	FILE *file;

	char Buffer1[50];
	char Buffer2[50];
	char HostName[255];

	WriteTime = time(NULL);
	
	QuickSortSummaryData(SummaryData, 0, NumIps-1);

	////////////////////////////////////////////////
	// Print main index page
	
	if (config.tag == '1')
		{
		snprintf(filename, MAX_FILENAME, "%s/index.html", config.htdocs_dir);
		if ((file = fopen(filename, "wt")) == NULL)
			{
			syslog(LOG_ERR, "Failed to open %s", filename);
			exit(1);
			}
		}
	else
		{
		snprintf(filename, MAX_FILENAME, "%s/index%c.html", config.htdocs_dir, config.tag);
		if ((file = fopen(filename, "wt")) == NULL)
			{
			syslog(LOG_ERR, "Failed to open %s", filename);
			exit(1);
			}
		}

	switch (config.tag)
		{
		case '1': PeriodDesc = "Daily"; break;
		case '2': PeriodDesc = "Weekly"; break;
		case '3': PeriodDesc = "Monthly"; break;
		case '4': PeriodDesc = "Yearly"; break;
		default: PeriodDesc = ""; break;
		}
	
	fprintf(file, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n");
	fprintf(file, "<HTML>\n<HEAD>\n<TITLE>Bandwidthd</TITLE>\n");

	if (config.meta_refresh)
		fprintf(file, "<META HTTP-EQUIV=\"REFRESH\" content=\"%u\">\n",
				config.meta_refresh);
	fprintf(file, "<META HTTP-EQUIV=\"EXPIRES\" content=\"-1\">\n");
	fprintf(file, "<META HTTP-EQUIV=\"PRAGMA\" content=\"no-cache\">\n");
	fprintf(file, "</HEAD>\n<BODY vlink=blue>\n%s<br>\n<center><img src=\"logo.gif\" ALT=\"Logo\"><BR>\n", ctime(&WriteTime));
	fprintf(file, "Programmed by David Hinkle, Commissioned by <a href=\"http://www.derbytech.com\">DerbyTech</a> wireless networking.<BR>");
	fprintf(file, "<BR>\n - <a href=\"index.html\">Daily</a> -- <a href=\"index2.html\">Weekly</a> -- ");
	fprintf(file, "<a href=\"index3.html\">Monthly</a> -- <a href=\"index4.html\">Yearly</a> - <BR>\n");

	fprintf(file, "<BR>\nPick a Subnet:<BR>\n");	
	if (config.tag == '1')
		fprintf(file, "- <a href=\"index.html\">Top20</a> -");
	else
		fprintf(file, "- <a href=\"index%c.html\">Top20</a> -", config.tag);

	for (Counter = 0; Counter < SubnetCount; Counter++)			   
		{
		HostIp2CharIp(SubnetTable[Counter].ip, Buffer1);
		fprintf(file, "- <a href=\"Subnet-%c-%s.html\">%s</a> -", config.tag, Buffer1, Buffer1);
		}

	/////  TOP 20

	fprintf(file, "<H1>Top 20 IPs by Traffic - %s</H1></center>", PeriodDesc);
	fprintf(file, "<center>\n<table width=\"100%%\" border=1 cellspacing=0>\n");

	// PASS 1:	Write out the table

	fprintf(file, "<TR bgcolor=lightblue><TD>Ip and Name<TD align=center>Total<TD align=center>Total Sent<TD align=center>Total Received<TD align=center>FTP<TD align=center>HTTP<TD align=center>MAIL<TD align=center>P2P<TD align=center>TCP<TD align=center>UDP<TD align=center>ICMP\n");
	for (Counter=0; Counter < 21 && Counter < NumIps; Counter++)
		PrintTableLine(file, SummaryData[Counter], Counter);

	fprintf(file, "</table></center>\n");

	// PASS 2: The graphs
	for (Counter=0; Counter < 21 && Counter < NumIps; Counter++)
		if (SummaryData[Counter]->Graph)
			{
			if (SummaryData[Counter]->IP == 0)
				{
				strcpy(Buffer1, "Total");	
				strcpy(HostName, "Total of all subnets");
				}
			else
				{	
				HostIp2CharIp(SummaryData[Counter]->IP, Buffer1);
				rdns(HostName, SummaryData[Counter]->IP);
				}
			fprintf(file, "<a name=\"%s-%c\"></a><H1><a href=\"#top\">(Top)</a> %s - %s</H1><BR>\nSend:<br>\n<img src=%s-%c-S.png ALT=\"Sent traffic for %s\"><BR>\n<img src=legend.gif ALT=\"Legend\"><br>\nReceived:<br>\n<img src=%s-%c-R.png ALT=\"Sent traffic for %s\"><BR>\n<img src=legend.gif ALT=\"Legend\"><br>\n<BR>\n", Buffer1, config.tag, Buffer1, HostName, Buffer1, config.tag, Buffer1, Buffer1, config.tag, Buffer1);
			}

	fprintf(file, "</BODY></HTML>\n");

	fclose(file);

	////////////////////////////////////////////////
	// Print each subnet page

	for (SubnetCounter = 0; SubnetCounter < SubnetCount; SubnetCounter++)
		{
		HostIp2CharIp(SubnetTable[SubnetCounter].ip, Buffer1);
		snprintf(filename, MAX_FILENAME, "%s/Subnet-%c-%s.html", config.htdocs_dir, config.tag, Buffer1);
		if ((file = fopen(filename, "wt")) == NULL)
			{
			syslog(LOG_ERR, "Failed to open %s", filename);
			exit(1);
			}
		fprintf(file, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n");
		fprintf(file, "<HTML>\n<HEAD><TITLE>Bandwidthd - Subnet %s</TITLE>\n", Buffer1);

		if (config.meta_refresh)
			fprintf(file, "<META HTTP-EQUIV=\"REFRESH\" content=\"%u\">\n",
					config.meta_refresh);
		fprintf(file, "<META HTTP-EQUIV=\"EXPIRES\" content=\"-1\">\n");
		fprintf(file, "<META HTTP-EQUIV=\"PRAGMA\" content=\"no-cache\">\n");
		fprintf(file, "</HEAD>\n<BODY vlink=blue>\n%s<br>\n<CENTER><a name=\"Top\"></a>", ctime(&WriteTime));
		fprintf(file, "<img src=\"logo.gif\" ALT=\"Logo\"><BR>");
		fprintf(file, "Programmed by David Hinkle, Commissioned by <a href=\"http://www.derbytech.com\">DerbyTech</a> wireless networking.<BR>\n");

		fprintf(file, "<BR>\n - <a href=\"index.html\">Daily</a> -- <a href=\"index2.html\">Weekly</a> -- ");
		fprintf(file, "<a href=\"index3.html\">Monthly</a> -- <a href=\"index4.html\">Yearly</a> - <BR>\n");

		fprintf(file, "<BR>\nPick a Subnet:<BR>\n");
		if (config.tag == '1')
			fprintf(file, "- <a href=\"index.html\">Top20</a> -");
		else
			fprintf(file, "- <a href=\"index%c.html\">Top20</a> -", config.tag);

		for (Counter = 0; Counter < SubnetCount; Counter++)
			{
			HostIp2CharIp(SubnetTable[Counter].ip, Buffer2);
			fprintf(file, "- <a href=\"Subnet-%c-%s.html\">%s</a> -", config.tag, Buffer2, Buffer2);
			}

		fprintf(file, "<H1>%s - %s</H1></center>", Buffer1, PeriodDesc);
		fprintf(file, "<table width=\"100%%\" border=1 cellspacing=0>\n");

		// PASS 1:	Write out the table

		fprintf(file, "<TR bgcolor=lightblue><TD>Ip and Name<TD align=center>Total<TD align=center>Total Sent<TD align=center>Total Received<TD align=center>FTP<TD align=center>HTTP<TD align=center>MAIL<TD align=center>P2P<TD align=center>TCP<TD align=center>UDP<TD align=center>ICMP\n");
		for (tCounter=0, Counter=0; Counter < NumIps; Counter++)
			{
			if (SubnetTable[SubnetCounter].ip == (SummaryData[Counter]->IP & SubnetTable[SubnetCounter].mask))
				{ // The ip belongs to this subnet
				PrintTableLine(file, SummaryData[Counter], tCounter++);
				}
			}

		fprintf(file, "</table>\n");

		// PASS 2: The graphs
		for (Counter=0; Counter < NumIps; Counter++)
			{
			if (SubnetTable[SubnetCounter].ip == (SummaryData[Counter]->IP & SubnetTable[SubnetCounter].mask))
				{ // The ip belongs to this subnet
				if (SummaryData[Counter]->Graph)
					{
					HostIp2CharIp(SummaryData[Counter]->IP, Buffer1);
					rdns(HostName, SummaryData[Counter]->IP);
					fprintf(file, "<a name=\"%s-%c\"></a><H1><a href=\"#top\">(Top)</a> %s - %s</H1><BR>\nSend:<br>\n<img src=%s-%c-S.png ALT=\"Sent traffic graph for %s\"><BR>\n<img src=legend.gif ALT=\"Legend\"><br>\nReceived:<br>\n<img src=%s-%c-R.png ALT=\"Received traffic for %s\"><BR>\n<img src=legend.gif ALT=\"Legend\"><br>\n<BR>\n", Buffer1, config.tag, Buffer1, HostName, Buffer1, config.tag, Buffer1, Buffer1, config.tag, Buffer1);
					}
				}
			}

		fprintf(file, "</BODY></HTML>\n");
		fclose(file);
		}

	free(SummaryData);
	}

void GraphIp(struct IPDataStore *DataStore, struct SummaryData *SummaryData, time_t timestamp)
	{
	FILE *OutputFile;
	char filename[MAX_FILENAME];
	gdImagePtr im, im2;
	int white;
	unsigned long long int YMax;
	char CharIp[20];

	time_t GraphBeginTime;

	// TODO: First determine if graph will be printed before creating image and drawing backround, etc

	if (DataStore->ip == 0)
		strcpy(CharIp, "Total");
	else
		HostIp2CharIp(DataStore->ip, CharIp);

	GraphBeginTime = timestamp - config.range;

	im = gdImageCreate(XWIDTH, YHEIGHT);
	white = gdImageColorAllocate(im, 255, 255, 255);
	//gdImageFill(im, 10, 10, white);

	im2 = gdImageCreate(XWIDTH, YHEIGHT);
	white = gdImageColorAllocate(im2, 255, 255, 255);
	//gdImageFill(im2, 10, 10, white);

	YMax = GraphData(im, im2, DataStore, GraphBeginTime, SummaryData);
	if (YMax != 0)
		{
		// Finish the graph
		PrepareXAxis(im, timestamp);
		PrepareYAxis(im, YMax);

		PrepareXAxis(im2, timestamp);
		PrepareYAxis(im2, YMax);

		snprintf(filename, MAX_FILENAME, "%s/%s-%c-S.png", config.htdocs_dir, CharIp, config.tag);
		if ((OutputFile = fopen(filename, "wb")) == NULL)
			{
			syslog(LOG_ERR, "Failed to open %s", filename);
			exit(1);
			}				
		gdImagePng(im, OutputFile);
		fclose(OutputFile);

		snprintf(filename, MAX_FILENAME, "%s/%s-%c-R.png", config.htdocs_dir, CharIp, config.tag);
		if ((OutputFile = fopen(filename, "wb")) == NULL)
			{
			syslog(LOG_ERR, "Failed to open %s", filename);
			exit(1);
			}
		gdImagePng(im2, OutputFile);
		fclose(OutputFile);
		}
	else
		{
		// The graph isn't worth clutering up the web pages with
		sprintf(filename, "%s/htdocs/%s-%c-R.png", config.htdocs_dir, CharIp, config.tag);
		unlink(filename);
		sprintf(filename, "%s/htdocs/%s-%c-S.png", config.htdocs_dir, CharIp, config.tag);
		unlink(filename);
		}

	gdImageDestroy(im);
	gdImageDestroy(im2);
	}

// Returns YMax
unsigned long long int GraphData(gdImagePtr im, gdImagePtr im2, struct IPDataStore *DataStore, time_t timestamp, struct SummaryData *SummaryData)
	{
	unsigned long long int YMax=0;
	
	struct DataStoreBlock *CurrentBlock;
	struct IPData *Data;

	// TODO: These should be a structure!!!!
	// TODO: This is an awfull lot of data to be allocated on the stack

	unsigned long long total[XWIDTH];
	unsigned long long icmp[XWIDTH];
	unsigned long long udp[XWIDTH];
	unsigned long long tcp[XWIDTH];
	unsigned long long ftp[XWIDTH];
	unsigned long long http[XWIDTH];
	unsigned long long mail[XWIDTH];
	unsigned long long p2p[XWIDTH];
	int Count[XWIDTH];

	unsigned long long total2[XWIDTH];
	unsigned long long icmp2[XWIDTH];
	unsigned long long udp2[XWIDTH];
	unsigned long long tcp2[XWIDTH];
	unsigned long long ftp2[XWIDTH];
	unsigned long long mail2[XWIDTH];
	unsigned long long http2[XWIDTH];
	unsigned long long p2p2[XWIDTH];

	size_t DataPoints;
	double x;
	int xint;
	int Counter;
	char Buffer[30];
	char Buffer2[50];
	
	int blue, lblue, orange, red, yellow, purple, green, brown, black;
	int blue2, lblue2, orange2, red2, yellow2, purple2, green2, brown2, black2;

	unsigned long long int SentPeak = 0;
	unsigned long long int ReceivedPeak = 0;

	yellow	 = gdImageColorAllocate(im, 255, 255, 0);
	purple	 = gdImageColorAllocate(im, 255, 0, 255);
	green	 = gdImageColorAllocate(im, 0, 255, 0);
	blue	 = gdImageColorAllocate(im, 0, 0, 255);
	lblue	 = gdImageColorAllocate(im, 128, 128, 255);
	orange	 = gdImageColorAllocate(im, 255, 128, 0);
	brown	 = gdImageColorAllocate(im, 128, 0, 0);
	red		 = gdImageColorAllocate(im, 255, 0, 0);
	black	 = gdImageColorAllocate(im, 0, 0, 0);
	
	yellow2  = gdImageColorAllocate(im2, 255, 255, 0);
	purple2   = gdImageColorAllocate(im2, 255, 0, 255);
	green2	 = gdImageColorAllocate(im2, 0, 255, 0);
	blue2	 = gdImageColorAllocate(im2, 0, 0, 255);
	lblue2	 = gdImageColorAllocate(im2, 128, 128, 255);
	orange2  = gdImageColorAllocate(im2, 255, 128, 0);
	brown2	 = gdImageColorAllocate(im2, 128, 0, 0);
	red2	 = gdImageColorAllocate(im2, 255, 0, 0);
	black2	 = gdImageColorAllocate(im2, 0, 0, 0);

	CurrentBlock = DataStore->FirstBlock;
	Data = CurrentBlock->Data;
	DataPoints = CurrentBlock->NumEntries;

	memset(SummaryData, 0, sizeof(struct SummaryData));
	SummaryData->IP = Data[0].ip;
	
	memset(Count, 0, sizeof(Count[0])*XWIDTH);

	memset(total, 0, sizeof(total[0])*XWIDTH);
	memset(icmp, 0, sizeof(total[0])*XWIDTH);
	memset(udp, 0, sizeof(total[0])*XWIDTH);
	memset(tcp, 0, sizeof(total[0])*XWIDTH);
	memset(ftp, 0, sizeof(total[0])*XWIDTH);
	memset(http, 0, sizeof(total[0])*XWIDTH);
	memset(mail, 0, sizeof(total[0])*XWIDTH);
	memset(p2p, 0, sizeof(total[0])*XWIDTH);

	memset(total2, 0, sizeof(total[0])*XWIDTH);
	memset(icmp2, 0, sizeof(total[0])*XWIDTH);
	memset(udp2, 0, sizeof(total[0])*XWIDTH);
	memset(tcp2, 0, sizeof(total[0])*XWIDTH);
	memset(ftp2, 0, sizeof(total[0])*XWIDTH);
	memset(http2, 0, sizeof(total[0])*XWIDTH);
	memset(mail2, 0, sizeof(total[0])*XWIDTH);
	memset(p2p2, 0, sizeof(total[0])*XWIDTH);

	// Change this to just run through all the datapoints we have stored in ram

	// Sum up the bytes/second
	while(DataPoints > 0)  // We have data to graph
		{
		for (Counter = 0; Counter < DataPoints; Counter++)	// Graph it all
			{
			x = (Data[Counter].timestamp-timestamp)*((XWIDTH-XOFFSET)/config.range)+XOFFSET;		
			xint = x;

			if (xint >= 0 && xint < XWIDTH)
				{
				Count[xint]++;
				
				if (Data[Counter].Send.total > SentPeak)
					SentPeak = Data[Counter].Send.total;
				total[xint] += Data[Counter].Send.total;
				icmp[xint] += Data[Counter].Send.icmp;
				udp[xint] += Data[Counter].Send.udp;
				tcp[xint] += Data[Counter].Send.tcp;
				ftp[xint] += Data[Counter].Send.ftp;
				http[xint] += Data[Counter].Send.http;
				mail[xint] += Data[Counter].Send.mail;
				p2p[xint] += Data[Counter].Send.p2p;

				if (Data[Counter].Receive.total > ReceivedPeak)
					ReceivedPeak = Data[Counter].Receive.total;
				total2[xint] += Data[Counter].Receive.total;
				icmp2[xint] += Data[Counter].Receive.icmp;
				udp2[xint] += Data[Counter].Receive.udp;
				tcp2[xint] += Data[Counter].Receive.tcp;
				ftp2[xint] += Data[Counter].Receive.ftp;
				http2[xint] += Data[Counter].Receive.http;
				mail2[xint] += Data[Counter].Receive.mail;
				p2p2[xint] += Data[Counter].Receive.p2p;
				}
			}

		CurrentBlock = CurrentBlock->Next;
			
		if (CurrentBlock)
			{
			Data = CurrentBlock->Data;
			DataPoints = CurrentBlock->NumEntries;
			}
		else
			DataPoints = 0;		
		}

	// Convert SentPeak and ReceivedPeak from bytes to bytes/second
	SentPeak /= config.interval; ReceivedPeak /= config.interval;

	// Preform the Average
	for(Counter=XOFFSET+1; Counter < XWIDTH; Counter++)
			{
			if (Count[Counter] > 0)
				{
				SummaryData->Total += total[Counter] + total2[Counter];
				SummaryData->TotalSent += total[Counter];
				SummaryData->TotalReceived += total2[Counter];
				SummaryData->TCP += tcp[Counter] + tcp2[Counter];
				SummaryData->FTP += ftp[Counter] + ftp2[Counter];
				SummaryData->HTTP += http[Counter] + http2[Counter];
				SummaryData->MAIL += mail[Counter] + mail2[Counter];
				SummaryData->P2P += p2p[Counter] + p2p2[Counter];
				SummaryData->UDP += udp[Counter] + udp2[Counter];
				SummaryData->ICMP += icmp[Counter] + icmp2[Counter];

				// Preform the average
				total[Counter] /= (Count[Counter]*config.interval);
				tcp[Counter] /= (Count[Counter]*config.interval);
				ftp[Counter] /= (Count[Counter]*config.interval);
				http[Counter] /= (Count[Counter]*config.interval);
				mail[Counter] /= (Count[Counter]*config.interval);
				p2p[Counter] /= (Count[Counter]*config.interval);
				udp[Counter] /= (Count[Counter]*config.interval);
				icmp[Counter] /= (Count[Counter]*config.interval);
								
				total2[Counter] /= (Count[Counter]*config.interval);
				tcp2[Counter] /= (Count[Counter]*config.interval);
				ftp2[Counter] /= (Count[Counter]*config.interval);
				http2[Counter] /= (Count[Counter]*config.interval);
				mail2[Counter] /= (Count[Counter]*config.interval);
				p2p2[Counter] /= (Count[Counter]*config.interval);
				udp2[Counter] /= (Count[Counter]*config.interval);
				icmp2[Counter] /= (Count[Counter]*config.interval);


				if (total[Counter] > YMax)
					YMax = total[Counter];
				
				if (total2[Counter] > YMax)
					YMax = total2[Counter];
				}
			}

	YMax += YMax*0.05;	  // Add an extra 5%
	
	if ((SummaryData->IP != 0 && SummaryData->Total < config.graph_cutoff))
		{
		SummaryData->Graph = FALSE;
		return(0);
		}
	else
		SummaryData->Graph = TRUE;

	// Plot the points
	for(Counter=XOFFSET+1; Counter < XWIDTH; Counter++)    
			{
			if (Count[Counter] > 0)
				{
				// Convert the bytes/sec to y coords
				total[Counter] = (total[Counter]*(YHEIGHT-YOFFSET))/YMax;
				tcp[Counter] = (tcp[Counter]*(YHEIGHT-YOFFSET))/YMax;
				ftp[Counter] = (ftp[Counter]*(YHEIGHT-YOFFSET))/YMax;
				http[Counter] = (http[Counter]*(YHEIGHT-YOFFSET))/YMax;
				mail[Counter] = (mail[Counter]*(YHEIGHT-YOFFSET))/YMax;
				p2p[Counter] = (p2p[Counter]*(YHEIGHT-YOFFSET))/YMax;
				udp[Counter] = (udp[Counter]*(YHEIGHT-YOFFSET))/YMax;
				icmp[Counter] = (icmp[Counter]*(YHEIGHT-YOFFSET))/YMax;

				total2[Counter] = (total2[Counter]*(YHEIGHT-YOFFSET))/YMax;
				tcp2[Counter] = (tcp2[Counter]*(YHEIGHT-YOFFSET))/YMax;
				ftp2[Counter] = (ftp2[Counter]*(YHEIGHT-YOFFSET))/YMax;
				http2[Counter] = (http2[Counter]*(YHEIGHT-YOFFSET))/YMax;
				mail2[Counter] = (mail2[Counter]*(YHEIGHT-YOFFSET))/YMax;
				p2p2[Counter] = (p2p2[Counter]*(YHEIGHT-YOFFSET))/YMax;
				udp2[Counter] = (udp2[Counter]*(YHEIGHT-YOFFSET))/YMax;
				icmp2[Counter] = (icmp2[Counter]*(YHEIGHT-YOFFSET))/YMax;

				// Stack 'em up!
				// Total is stacked from the bottom
				// Icmp is on the bottom too
				// Udp is stacked on top of icmp
				udp[Counter] += icmp[Counter];
				udp2[Counter] += icmp2[Counter];
				// TCP and p2p are stacked on top of Udp
				tcp[Counter] += udp[Counter];
				tcp2[Counter] += udp2[Counter];
				p2p[Counter] += udp[Counter];
				p2p2[Counter] += udp2[Counter];
				// Http is stacked on top of p2p
				http[Counter] += p2p[Counter];
				http2[Counter] += p2p2[Counter];
				// Mail is stacked on top of http
				mail[Counter] += http[Counter];
				mail2[Counter] += http2[Counter];				
				// Ftp is stacked on top of mail
				ftp[Counter] += mail[Counter];
				ftp2[Counter] += mail2[Counter];

				// Plot them!
				// Sent
				gdImageLine(im, Counter, (YHEIGHT-YOFFSET) - total[Counter], Counter, YHEIGHT-YOFFSET-1, yellow);
				gdImageLine(im, Counter, (YHEIGHT-YOFFSET) - icmp[Counter], Counter, YHEIGHT-YOFFSET-1, red);
				gdImageLine(im, Counter, (YHEIGHT-YOFFSET) - udp[Counter], Counter, (YHEIGHT-YOFFSET) - icmp[Counter] - 1, brown);
				gdImageLine(im, Counter, (YHEIGHT-YOFFSET) - tcp[Counter], Counter, (YHEIGHT-YOFFSET) - udp[Counter] - 1, green);
				gdImageLine(im, Counter, (YHEIGHT-YOFFSET) - p2p[Counter], Counter, (YHEIGHT-YOFFSET) - udp[Counter] - 1, purple);
				gdImageLine(im, Counter, (YHEIGHT-YOFFSET) - http[Counter], Counter, (YHEIGHT-YOFFSET) - p2p[Counter] - 1, blue);
				gdImageLine(im, Counter, (YHEIGHT-YOFFSET) - mail[Counter], Counter, (YHEIGHT-YOFFSET) - http[Counter] - 1, orange);
				gdImageLine(im, Counter, (YHEIGHT-YOFFSET) - ftp[Counter], Counter, (YHEIGHT-YOFFSET) - mail[Counter] - 1, lblue);
								
				// Receive
				gdImageLine(im2, Counter, (YHEIGHT-YOFFSET) - total2[Counter], Counter, YHEIGHT-YOFFSET-1, yellow2);
				gdImageLine(im2, Counter, (YHEIGHT-YOFFSET) - icmp2[Counter], Counter, YHEIGHT-YOFFSET-1, red2);
				gdImageLine(im2, Counter, (YHEIGHT-YOFFSET) - udp2[Counter], Counter, (YHEIGHT-YOFFSET) - icmp2[Counter] - 1, brown2);
				gdImageLine(im2, Counter, (YHEIGHT-YOFFSET) - tcp2[Counter], Counter, (YHEIGHT-YOFFSET) - udp2[Counter] - 1, green2);
				gdImageLine(im2, Counter, (YHEIGHT-YOFFSET) - p2p2[Counter], Counter, (YHEIGHT-YOFFSET) - udp2[Counter] - 1, purple2);
				gdImageLine(im2, Counter, (YHEIGHT-YOFFSET) - http2[Counter], Counter, (YHEIGHT-YOFFSET) - p2p2[Counter] - 1, blue2);
				gdImageLine(im2, Counter, (YHEIGHT-YOFFSET) - mail2[Counter], Counter, (YHEIGHT-YOFFSET) - http2[Counter] - 1, orange);
				gdImageLine(im2, Counter, (YHEIGHT-YOFFSET) - ftp2[Counter], Counter, (YHEIGHT-YOFFSET) - mail2[Counter] - 1, lblue2);
				}
			}

	if (SentPeak < 1024/8)
		snprintf(Buffer2, 50, "Peak Send Rate: %.1f Bits/sec", (double)SentPeak*8);
	else if (SentPeak < (1024*1024)/8)
		snprintf(Buffer2, 50, "Peak Send Rate: %.1f KBits/sec", ((double)SentPeak*8.0)/1024.0);
	else snprintf(Buffer2, 50, "Peak Send Rate: %.1f MBits/sec", ((double)SentPeak*8.0)/(1024.0*1024.0));
								
	if (SummaryData->TotalSent < 1024)
		snprintf(Buffer, 30, "Sent %.1f Bytes", (double)SummaryData->TotalSent);					
	else if (SummaryData->TotalSent < 1024*1024)
		snprintf(Buffer, 30, "Sent %.1f KBytes", (double)SummaryData->TotalSent/1024.0);
	else snprintf(Buffer, 30, "Sent %.1f MBytes", (double)SummaryData->TotalSent/(1024.0*1024.0));

	gdImageString(im, gdFontSmall, XOFFSET+5,  YHEIGHT-20, (unsigned char *) Buffer, black);
	gdImageString(im, gdFontSmall, XWIDTH/2+XOFFSET/2,	YHEIGHT-20, (unsigned char *) Buffer2, black);				

	if (ReceivedPeak < 1024/8)
		snprintf(Buffer2, 50, "Peak Receive Rate: %.1f Bits/sec", (double)ReceivedPeak*8);
	else if (ReceivedPeak < (1024*1024)/8)
		snprintf(Buffer2, 50, "Peak Receive Rate: %.1f KBits/sec", ((double)ReceivedPeak*8.0)/1024.0);				 
	else snprintf(Buffer2, 50, "Peak Receive Rate: %.1f MBits/sec", ((double)ReceivedPeak*8.0)/(1024.0*1024.0));

	if (SummaryData->TotalReceived < 1024)
		snprintf(Buffer, 30, "Received %.1f Bytes", (double)SummaryData->TotalReceived);
	else if (SummaryData->TotalReceived < 1024*1024)
		snprintf(Buffer, 30, "Received %.1f KBytes", (double)SummaryData->TotalReceived/1024.0);
	else snprintf(Buffer, 30, "Received %.1f MBytes", (double)SummaryData->TotalReceived/(1024.0*1024.0));
																											  
	gdImageString(im2, gdFontSmall, XOFFSET+5,	YHEIGHT-20, (unsigned char *) Buffer, black2);				
	gdImageString(im2, gdFontSmall, XWIDTH/2+XOFFSET/2,  YHEIGHT-20, (unsigned char *) Buffer2, black2);

	return(YMax);
	}

void PrepareYAxis(gdImagePtr im, unsigned long long int YMax)
	{
	char buffer[20];

	char YLegend;
	long long int Divisor;

	int black;
	float YTic = 0;
	double y;
	long int YStep;
	
	black = gdImageColorAllocate(im, 0, 0, 0);
	gdImageLine(im, XOFFSET, 0, XOFFSET, YHEIGHT, black);

	YLegend = ' ';
	Divisor = 1;
	if (YMax*8 > 1024*2)
		{
		Divisor = 1024;    // Display in K
		YLegend = 'k';
		}
	if (YMax*8 > 1024*1024*2)
		{
		Divisor = 1024*1024; // Display in M
		YLegend = 'm';
		}
	if (YMax*8 > (long long)1024*1024*1024*2)
		{
		Divisor = 1024*1024*1024; // Display in G
		YLegend = 'g';
		}

	YStep = YMax/10;
	if (YStep < 1)
		YStep=1;
	YTic=YStep;

	while (YTic < (YMax - YMax/10))
		{
		y = (YHEIGHT-YOFFSET)-((YTic*(YHEIGHT-YOFFSET))/YMax);		  

		gdImageLine(im, XOFFSET, y, XWIDTH, y, black);		  
		snprintf(buffer, 20, "%4.1f %cbits/s", (float)(8.0*YTic)/Divisor, YLegend);
		gdImageString(im, gdFontSmall, 3, y-7, (unsigned char *) buffer, black);		  

		YTic += YStep;
		}
	} 

void PrepareXAxis(gdImagePtr im, time_t timestamp)
	{
	char buffer[100];
	int black, red;
	time_t sample_begin, sample_end;	
	struct tm *timestruct;
	long int MarkTime;
	long int MarkTimeStep;
	double x;
	
	sample_begin=timestamp-config.range;
	sample_end=sample_begin+config.interval;

	black = gdImageColorAllocate(im, 0, 0, 0);
	red   = gdImageColorAllocate(im, 255, 0, 0);

	gdImageLine(im, 0, YHEIGHT-YOFFSET, XWIDTH, YHEIGHT-YOFFSET, black);

	// ********************************************************************
	// ****  Write the red day/month seperator bars
	// ********************************************************************

	if ((24*60*60*(XWIDTH-XOFFSET))/config.range > (XWIDTH-XOFFSET)/10)
		{
		// Day bars
		timestruct = localtime((time_t *)&sample_begin);
		timestruct->tm_sec = 0;
		timestruct->tm_min = 0;
		timestruct->tm_hour = 0;
		MarkTime = mktime(timestruct);
			
		x = (MarkTime-sample_begin)*( ((double)(XWIDTH-XOFFSET)) / config.range) + XOFFSET;
		while (x < XOFFSET)
			{
			MarkTime += (24*60*60);
			x = (MarkTime-sample_begin)*((XWIDTH-XOFFSET)/config.range) + XOFFSET;
			}

		while (x < (XWIDTH-10))
			{
			// Day Lines
			gdImageLine(im, x, 0, x, YHEIGHT-YOFFSET, red);
			gdImageLine(im, x+1, 0, x+1, YHEIGHT-YOFFSET, red);
	
			timestruct = localtime((time_t *)&MarkTime);
			strftime(buffer, 100, "%a, %b %d", timestruct);
			gdImageString(im, gdFontSmall, x-30,  YHEIGHT-YOFFSET+10, (unsigned char *) buffer, black);		 

			// Calculate Next x
			MarkTime += (24*60*60);
			x = (MarkTime-sample_begin)*((XWIDTH-XOFFSET)/config.range) + XOFFSET;
			}
		}
	else
		{
		// Month Bars
		timestruct = localtime((time_t *)&sample_begin);
		timestruct->tm_sec = 0;
		timestruct->tm_min = 0;
		timestruct->tm_hour = 0;
		timestruct->tm_mday = 1;
		timestruct->tm_mon--; // Start the month before the sample
		MarkTime = mktime(timestruct);

		x = (MarkTime-sample_begin)*( ((double)(XWIDTH-XOFFSET)) / config.range) + XOFFSET;
		while (x < XOFFSET)
			{
			timestruct->tm_mon++;
			MarkTime = mktime(timestruct);
			x = (MarkTime-sample_begin)*((XWIDTH-XOFFSET)/config.range) + XOFFSET;
			}

		while (x < (XWIDTH-10))
			{
			// Month Lines
			gdImageLine(im, x, 0, x, YHEIGHT-YOFFSET, red);
			gdImageLine(im, x+1, 0, x+1, YHEIGHT-YOFFSET, red);
	
			timestruct = localtime((time_t *)&MarkTime);
			strftime(buffer, 100, "%b", timestruct);
			gdImageString(im, gdFontSmall, x-6,  YHEIGHT-YOFFSET+10, (unsigned char *) buffer, black);		

			// Calculate Next x
			timestruct->tm_mon++;
			MarkTime = mktime(timestruct);
			x = (MarkTime-sample_begin)*((XWIDTH-XOFFSET)/config.range) + XOFFSET;
			}				
		}

	// ********************************************************************
	// ****  Write the tic marks
	// ********************************************************************

	timestruct = localtime((time_t *)&sample_begin);
	timestruct->tm_sec = 0;
	timestruct->tm_min = 0;
	timestruct->tm_hour = 0;
	MarkTime = mktime(timestruct);

	if ((6*60*60*(XWIDTH-XOFFSET))/config.range > 10) // pixels per 6 hours is more than 2
		MarkTimeStep = 6*60*60; // Major ticks are 6 hours
	else if ((24*60*60*(XWIDTH-XOFFSET))/config.range > 10)
		MarkTimeStep = 24*60*60; // Major ticks are 24 hours;
	else
		return; // Done		

	x = (MarkTime-sample_begin)*((XWIDTH-XOFFSET)/config.range) + XOFFSET;
	while (x < XOFFSET)
		{
		MarkTime += MarkTimeStep;
		x = (MarkTime-sample_begin)*((XWIDTH-XOFFSET)/config.range) + XOFFSET;
		}

	while (x < (XWIDTH-10))
		{
		if (x > XOFFSET) {
			gdImageLine(im, x, YHEIGHT-YOFFSET-5, x, YHEIGHT-YOFFSET+5, black);
			gdImageLine(im, x+1, YHEIGHT-YOFFSET-5, x+1, YHEIGHT-YOFFSET+5, black);
			}
		MarkTime += MarkTimeStep;
		x = (MarkTime-sample_begin)*((XWIDTH-XOFFSET)/config.range) + XOFFSET;
		}

	timestruct = localtime((time_t *)&sample_begin);
	timestruct->tm_sec = 0;
	timestruct->tm_min = 0;
	timestruct->tm_hour = 0;
	MarkTime = mktime(timestruct);

	if ((60*60*(XWIDTH-XOFFSET))/config.range > 2) // pixels per hour is more than 2
		MarkTimeStep = 60*60;  // Minor ticks are 1 hour
	else if ((6*60*60*(XWIDTH-XOFFSET))/config.range > 2)
		MarkTimeStep = 6*60*60; // Minor ticks are 6 hours
	else if ((24*60*60*(XWIDTH-XOFFSET))/config.range > 2)
		MarkTimeStep = 24*60*60;
	else
		return; // Done

	// Draw Minor Tic Marks
	x = (MarkTime-sample_begin)*((XWIDTH-XOFFSET)/config.range) + XOFFSET;

	while (x < XOFFSET)
		{
		MarkTime += MarkTimeStep;
		x = (MarkTime-sample_begin)*((XWIDTH-XOFFSET)/config.range) + XOFFSET;
		}

	while (x < (XWIDTH-10))
		{
		if (x > XOFFSET) {
			gdImageLine(im, x, YHEIGHT-YOFFSET, x, YHEIGHT-YOFFSET+5, black);
			gdImageLine(im, x+1, YHEIGHT-YOFFSET, x+1, YHEIGHT-YOFFSET+5, black);
			}
		MarkTime+=MarkTimeStep;
		x = (MarkTime-sample_begin)*((XWIDTH-XOFFSET)/config.range) + XOFFSET;
		}
	}


