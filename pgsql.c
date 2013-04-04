#include "bandwidthd.h"
#include <setjmp.h>

extern struct config config;
extern unsigned int IpCount;
extern struct Broadcast *Broadcasts;
extern time_t ProgramStart;

jmp_buf pgsqljmp;

#ifdef HAVE_LIBPQ

#include "postgresql/libpq-fe.h"

#define MAX_PARAM_SIZE 200

// Check that tables exist and create them if not
PGconn *pgsqlCheckTables(PGconn *conn)
	{
	PGresult   *res;
	
	res = PQexec(conn, "select tablename from pg_tables where tablename='sensors';");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
		syslog(LOG_ERR, "Postresql Select failed: %s", PQerrorMessage(conn));
		PQclear(res);
		PQfinish(conn);
		return(NULL);
		}

	if (PQntuples(res) != 1)
		{
		PQclear(res);
		res = PQexec(conn,  "CREATE TABLE bd_rx_log (sensor_id int,mac character varying(20), ip inet, timestamp timestamp with time zone DEFAULT now(), sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); create index bd_rx_log_sensor_id_ip_timestamp_idx on bd_rx_log (sensor_id, ip, timestamp); create index bd_rx_log_sensor_id_timestamp_idx on bd_rx_log(sensor_id, timestamp);");
		if (PQresultStatus(res) != PGRES_COMMAND_OK)
			{
			syslog(LOG_ERR, "Postresql create table bd_rx_log failed: %s", PQerrorMessage(conn));
			PQclear(res);
			PQfinish(conn);
			return(NULL);
			}
		PQclear(res);
		
		res = PQexec(conn, "CREATE TABLE bd_tx_log (sensor_id int,mac character varying(20), ip inet, timestamp timestamp with time zone DEFAULT now(), sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); create index bd_tx_log_sensor_id_ip_timestamp_idx on bd_tx_log (sensor_id, ip, timestamp); create index bd_tx_log_sensor_id_timestamp_idx on bd_tx_log(sensor_id, timestamp);");
		if (PQresultStatus(res) != PGRES_COMMAND_OK)
			{
			syslog(LOG_ERR, "Postresql create table bd_tx_log failed: %s", PQerrorMessage(conn));
			PQclear(res);
			PQfinish(conn);
			return(NULL);
			}
		PQclear(res);

		res = PQexec(conn, "CREATE TABLE bd_rx_total_log (sensor_id int,mac character varying(20), ip inet, timestamp timestamp with time zone DEFAULT now(), sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); create index bd_rx_total_log_sensor_id_timestamp_ip_idx on bd_rx_total_log (sensor_id, timestamp);");
		if (PQresultStatus(res) != PGRES_COMMAND_OK)
			{
			syslog(LOG_ERR, "Postresql create table bd_rx_total_log failed: %s", PQerrorMessage(conn));
			PQclear(res);
			PQfinish(conn);
			return(NULL);
			}
		PQclear(res);

		res = PQexec(conn, "CREATE TABLE bd_tx_total_log (sensor_id int,mac character varying(20), ip inet, timestamp timestamp with time zone DEFAULT now(), sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); create index bd_tx_total_log_sensor_id_timestamp_ip_idx on bd_tx_total_log (sensor_id, timestamp);");
		if (PQresultStatus(res) != PGRES_COMMAND_OK)
			{
			syslog(LOG_ERR, "Postresql create table bd_tx_total_log failed: %s", PQerrorMessage(conn));
			PQclear(res);
			PQfinish(conn);
			return(NULL);
			}
		PQclear(res);

		res = PQexec(conn, "CREATE TABLE sensors ( sensor_id serial PRIMARY KEY, sensor_name varchar, location int, build int default 0, uptime interval, reboots int default 0, interface varchar, description varchar, management_url varchar, last_connection timestamp with time zone );");
		if (PQresultStatus(res) != PGRES_COMMAND_OK)
			{
			syslog(LOG_ERR, "Postresql create table sensors failed: %s", PQerrorMessage(conn));
			PQclear(res);
			PQfinish(conn);
			return(NULL);
			}
		PQclear(res);

		res = PQexec(conn, "CREATE TABLE links (id1 int, id2 int, plot boolean default TRUE, last_update timestamp with time zone);");
		if (PQresultStatus(res) != PGRES_COMMAND_OK)
			{
			syslog(LOG_ERR, "Postresql create table links failed: %s", PQerrorMessage(conn));
			PQclear(res);
			PQfinish(conn);
			return(NULL);
			}
		PQclear(res);
		return(conn);
		}
	else
		{		
		PQclear(res);

		// Patch database to include mail column if it doesn't alread exist
		res = PQexec(conn, "SELECT table_name, column_name from information_schema.columns where table_name = 'bd_rx_log' and column_name = 'mail';");

		if (PQresultStatus(res) != PGRES_TUPLES_OK)
			{
			syslog(LOG_ERR, "Postresql Select failed: %s", PQerrorMessage(conn));
			PQclear(res);
			PQfinish(conn);
			return(NULL);
			}

		if (PQntuples(res) != 1)
			{
			PQclear(res);

			res = PQexec(conn,  "alter table bd_rx_log add column mail int;");
			if (PQresultStatus(res) != PGRES_COMMAND_OK)
				{
				syslog(LOG_ERR, "Add column failed: %s", PQerrorMessage(conn));
				PQclear(res);
				PQfinish(conn);
				return(NULL);
				}
			PQclear(res);

			res = PQexec(conn,  "alter table bd_tx_log add column mail int;");
			if (PQresultStatus(res) != PGRES_COMMAND_OK)
				{
				syslog(LOG_ERR, "Add column failed: %s", PQerrorMessage(conn));
				PQclear(res);
				PQfinish(conn);
				return(NULL);
				}
			PQclear(res);

			res = PQexec(conn,  "alter table bd_rx_total_log add column mail int;");
			if (PQresultStatus(res) != PGRES_COMMAND_OK)
				{
				syslog(LOG_ERR, "Add column failed: %s", PQerrorMessage(conn));
				PQclear(res);
				PQfinish(conn);
				return(NULL);
				}
			PQclear(res);

			res = PQexec(conn,  "alter table bd_tx_total_log add column mail int;");
			if (PQresultStatus(res) != PGRES_COMMAND_OK)
				{
				syslog(LOG_ERR, "Add column failed: %s", PQerrorMessage(conn));
				PQclear(res);
				PQfinish(conn);
				return(NULL);
				}
			PQclear(res);
			}
		else
			{
			PQclear(res);
			}
		res = PQexec(conn, "SELECT table_name, column_name from information_schema.columns where table_name = 'bd_rx_log' and column_name = 'mac';");

		if (PQresultStatus(res) != PGRES_TUPLES_OK)
			{
			syslog(LOG_ERR, "Postresql Select failed: %s", PQerrorMessage(conn));
			PQclear(res);
			PQfinish(conn);
			return(NULL);
			}

		if (PQntuples(res) != 1)
			{
			PQclear(res);

			res = PQexec(conn,  "alter table bd_rx_log add column mac character varying(20);");
			if (PQresultStatus(res) != PGRES_COMMAND_OK)
				{
				syslog(LOG_ERR, "Add column failed: %s", PQerrorMessage(conn));
				PQclear(res);
				PQfinish(conn);
				return(NULL);
				}
			PQclear(res);

			res = PQexec(conn,  "alter table bd_tx_log add column mac character varying(20);");
			if (PQresultStatus(res) != PGRES_COMMAND_OK)
				{
				syslog(LOG_ERR, "Add column failed: %s", PQerrorMessage(conn));
				PQclear(res);
				PQfinish(conn);
				return(NULL);
				}
			PQclear(res);

			res = PQexec(conn,  "alter table bd_rx_total_log add column mac character varying(20);");
			if (PQresultStatus(res) != PGRES_COMMAND_OK)
				{
				syslog(LOG_ERR, "Add column failed: %s", PQerrorMessage(conn));
				PQclear(res);
				PQfinish(conn);
				return(NULL);
				}
			PQclear(res);

			res = PQexec(conn,  "alter table bd_tx_total_log add column mac character varying(20);");
			if (PQresultStatus(res) != PGRES_COMMAND_OK)
				{
				syslog(LOG_ERR, "Add column failed: %s", PQerrorMessage(conn));
				PQclear(res);
				PQfinish(conn);
				return(NULL);
				}
			PQclear(res);
			}
		else
			{
			PQclear(res);
			}

		return(conn);
		}
	}

PGconn *pgsqlInit(void)
	{
	PGconn *conn = NULL;

	conn = PQconnectdb(config.db_connect_string);

	/* Check to see that the backend connection was successfully made */
	if (PQstatus(conn) != CONNECTION_OK)
		{
		syslog(LOG_ERR, "Connection to database '%s' failed: %s", config.db_connect_string, PQerrorMessage(conn));
		PQfinish(conn);
		return(NULL);
		}

	return(conn);
	}

PGconn *pgsqlDetermineSensorID(PGconn *conn, char *sensor_id, char *sensor_name, char *interface)
	{
	const char *paramValues[3];
	char Values[2][MAX_PARAM_SIZE];
	PGresult   *res;

	paramValues[0] = Values[0];
	paramValues[1] = Values[1];

	strncpy(Values[0], sensor_name, MAX_PARAM_SIZE);
	strncpy(Values[1], interface, MAX_PARAM_SIZE);
	res = PQexecParams(conn, "select sensor_id from sensors where sensor_name = $1 and interface = $2;",
		2,       /* number of params */
		NULL,    /* let the backend deduce param type */
		paramValues,
		NULL,    /* don't need param lengths since text */
		NULL,    /* default to all text params */
		0);      /* ask for binary results */
		
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
		syslog(LOG_ERR, "Postresql SELECT failed: %s", PQerrorMessage(conn));
		PQclear(res);
		PQfinish(conn);
		return(NULL);
		}

	if (PQntuples(res))
		{
		strncpy(sensor_id, PQgetvalue(res, 0, 0), MAX_PARAM_SIZE);
		PQclear(res);
		return(conn);
		}
	else
		{
		sensor_id[0] = '\0';
		return(conn);
		}
	}		

PGconn *pgsqlCreateSensorID(PGconn *conn, char *sensor_id)
	{
	const char *paramValues[3];
	char Values[3][MAX_PARAM_SIZE];
	PGresult   *res;
																															
	paramValues[0] = Values[0];
	paramValues[1] = Values[1];
	paramValues[2] = Values[2];

	res = PQexec(conn, "select nextval('sensors_sensor_id_seq'::Text);");
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
		syslog(LOG_ERR, "Postresql select failed: %s", PQerrorMessage(conn));
		PQclear(res);
		PQfinish(conn);
		return(NULL);
		}

	strncpy(sensor_id, PQgetvalue(res, 0, 0), MAX_PARAM_SIZE);
	PQclear(res);

	strncpy(Values[0], config.sensor_name, MAX_PARAM_SIZE);
	strncpy(Values[1], config.dev, MAX_PARAM_SIZE);
	strncpy(Values[2], sensor_id, MAX_PARAM_SIZE);
	// Insert new sensor id
	res = PQexecParams(conn, "insert into sensors (sensor_name, interface, sensor_id) VALUES ($1, $2, $3);",
		3,       /* number of parameters */
		NULL,    /* let the backend deduce param type */
		paramValues,
		NULL,    /* don't need param lengths since text */
		NULL,    /* default to all text params */
		0);      /* ask for binary results */

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
		{
		syslog(LOG_ERR, "Postresql INSERT failed: %s", PQerrorMessage(conn));
		PQclear(res);
		PQfinish(conn);
		return(NULL);
		}
	else
		{
		PQclear(res);
		return(conn);
		}
	}

PGconn *pgsqlUpdateLinkStatus(PGconn *conn, char *sensor_id)
	{
	struct Broadcast *bc;
	unsigned long diff;
	const char *paramValues[3];
	char Values[3][MAX_PARAM_SIZE];
	PGresult   *res;
						
	paramValues[0] = Values[0];
	paramValues[1] = Values[1];
	paramValues[2] = Values[2];

	for (bc = Broadcasts; bc; bc = bc->next)
		{
		strncpy(Values[0], sensor_id, MAX_PARAM_SIZE);
		// Determine numeric sensor ID of other sensor
		if (!(conn = pgsqlDetermineSensorID(conn, Values[1], bc->sensor_name, bc->interface)))
			return(NULL);		
		
		if (!Values[1][0])
			{
			syslog(LOG_ERR, "Sensor '%s - %s' does not exist in database, skiping link update", bc->sensor_name, bc->interface);
			continue;
			}

		diff = time(NULL) - bc->received;
		snprintf(Values[2], MAX_PARAM_SIZE, "%lu", diff);
		res = PQexecParams(conn, "update links set last_update = now()-($3*interval '1 second') where (id1 = $1 and id2 = $2) or (id1 = $2 and id2 = $1);",
				3,       /* number of parameters */
				NULL,    /* let the backend deduce param type */
				paramValues,
				NULL,    /* don't need param lengths since text */
				NULL,    /* default to all text params */
				0);      /* ask for binary results */

		if (PQresultStatus(res) != PGRES_COMMAND_OK)
			{
			syslog(LOG_ERR, "Postresql Update of links table failed: %s", PQerrorMessage(conn));
			PQclear(res);
			PQfinish(conn);
			return(NULL);
			}
		
		// There may be duplicate rows
		if (atol(PQcmdTuples(res)) > 0) 
			{
			PQclear(res); // Sucess, allow loop to fall through
			}
		else
			{
			PQclear(res);
			
			// Link doesn't exist so we must add it
			diff = time(NULL) - bc->received;
			snprintf(Values[2], MAX_PARAM_SIZE, "%lu", diff);
			res = PQexecParams(conn, "insert into links (id1, id2, last_update) values ($1, $2, now()-($3*interval '1 second'));",
					3,       /* number of parameters */
					NULL,    /* let the backend deduce param type */
					paramValues,
					NULL,    /* don't need param lengths since text */
					NULL,    /* default to all text params */
					0);      /* ask for binary results */

			if (PQresultStatus(res) != PGRES_COMMAND_OK)
				{
				syslog(LOG_ERR, "Postresql Insert into links table failed: %s", PQerrorMessage(conn));
				PQclear(res);
				PQfinish(conn);
				return(NULL);
				}
			PQclear(res);
			}
		}
	return(conn);
	}

PGconn *pgsqlUpdateSensorStatus(PGconn *conn, char *sensor_id)
	{
	const char *paramValues[5];
	char Values[5][MAX_PARAM_SIZE];
	PGresult   *res;

	paramValues[0] = Values[0];
	paramValues[1] = Values[1];
	paramValues[2] = Values[2];
	paramValues[3] = Values[3];
	paramValues[4] = Values[4];

	strncpy(Values[0], sensor_id, MAX_PARAM_SIZE);
	strncpy(Values[1], config.description, MAX_PARAM_SIZE);	
	strncpy(Values[2], config.management_url, MAX_PARAM_SIZE);	
	snprintf(Values[3], MAX_PARAM_SIZE, "%u", BUILD);
	snprintf(Values[4], MAX_PARAM_SIZE, "%lu", time(NULL)-ProgramStart);	
	res = PQexecParams(conn, "update sensors set description = $2, management_url = $3, last_connection = now(), build = $4, uptime = $5*interval '1 second' where sensor_id = $1;",
			5,       /* number of parameters */
			NULL,    /* let the backend deduce param type */
			paramValues,
			NULL,    /* don't need param lengths since text */
			NULL,    /* default to all text params */
			0);      /* ask for binary results */
		
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
		{
		syslog(LOG_ERR, "Postresql UPDATE description and management_url failed: %s", PQerrorMessage(conn));
		PQclear(res);
		PQfinish(conn);
		return(NULL);
		}
	else
		{
		PQclear(res);
		return(conn);
		}
	}

PGconn *pgsqlIncReboots(PGconn *conn, char *sensor_id)
	{
	const char *paramValues[1];
	char Values[1][MAX_PARAM_SIZE];
	PGresult   *res;

	paramValues[0] = Values[0];

	strncpy(Values[0], sensor_id, MAX_PARAM_SIZE);
	res = PQexecParams(conn, "update sensors set reboots = reboots+1 where sensor_id = $1;",
			1,       /* number of parameters */
			NULL,    /* let the backend deduce param type */
			paramValues,
			NULL,    /* don't need param lengths since text */
			NULL,    /* default to all text params */
			0);      /* ask for binary results */
		
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
		{
		syslog(LOG_ERR, "Postresql UPDATE reboots failed: %s", PQerrorMessage(conn));
		PQclear(res);
		PQfinish(conn);
		return(NULL);
		}
	else
		{
		PQclear(res);
		return(conn);
		}
	}
#endif

static void pgsqllngjmp(int signal)
	{
	longjmp(pgsqljmp, 1);	
	}

void pgsqlStoreIPData(struct IPData IncData[], struct extensions *extension_data)
	{
#ifdef HAVE_LIBPQ
	static char sensor_id[MAX_PARAM_SIZE] = { '\0' };
	static pid_t child = 0;

	struct IPData *IPData;
	unsigned int Counter;
	struct Statistics *Stats;
	PGresult   *res;
	PGconn *conn = NULL;
	char now[MAX_PARAM_SIZE];
	// This is higher than Values below for the extensions code
	const char *paramValues[22];
	char *sql1; 
	char *sql2;
	char Values[14][MAX_PARAM_SIZE];

	if (!config.output_database == DB_PGSQL)
		return;

	// Start of actual work
	paramValues[0] = Values[0];
	paramValues[1] = Values[1];
	paramValues[2] = Values[2];
	paramValues[3] = Values[3];	
	paramValues[4] = Values[4];
	paramValues[5] = Values[5];
	paramValues[6] = Values[6];
	paramValues[7] = Values[7];
	paramValues[8] = Values[8];
	paramValues[9] = Values[9];
	paramValues[10] = Values[10];
	paramValues[11] = Values[11];
	paramValues[12] = Values[12];
	paramValues[13] = Values[13];

	// ************ Inititialize the db if it's not already

	// Do initialization in main thread in order to prevent doing sensor_id work repeatedly
	if (!sensor_id[0])	// Determine numeric sensor ID
		{
		/* I don't know how safe interupting pgsql's calls to the database 
		are, and since we're working in the main process here I picked a higher interval
		soas to interupt less frequently.  If I don't do this, the whole process gets locked up
		when the database is unreachable so it can only help. */
		alarm(config.interval*5);
		signal(SIGALRM, pgsqllngjmp);

		if (setjmp(pgsqljmp))
			{
			syslog(LOG_ERR, "Timeout when working with database");
			return;
			}
	
		syslog(LOG_INFO, "Initializing database info");
		conn = pgsqlInit();

		if (!conn)
			{
			syslog(LOG_ERR, "Could not connect to database");
			alarm(0);
			return;
			}

		if (!(conn = pgsqlCheckTables(conn))) // Create tables if neccisary
			{
			syslog(LOG_ERR, "Failed to check or create database tables");
			alarm(0);
			return;
			}

		if (!(conn = pgsqlDetermineSensorID(conn, sensor_id, config.sensor_name, config.dev)))
			{
			syslog(LOG_ERR, "Failed to determine sensor_id");
			alarm(0);
			return;
			}

		if (!sensor_id[0]) // Create a new sensor ID
			if (!(conn = pgsqlCreateSensorID(conn, sensor_id)))
				{
				syslog(LOG_ERR, "Failed to create new sensor_id");
				alarm(0);
				return;
				}

		pgsqlIncReboots(conn, sensor_id);
		
		PQfinish(conn);
		conn = NULL;

		syslog(LOG_INFO, "Sensor ID: %s", sensor_id);
		alarm(0);
		}


	// If we have a valid child see if he has exited
	if (child > 0)
		{
		if (waitpid(child, NULL, WNOHANG) == 0)
			{
			syslog(LOG_ERR, "Logging child still active: No response or slow database? Killing child.");
			kill(child, SIGKILL);
			waitpid(child, NULL, 0);
			}
		}

	// Fork to allow bandwidthd to operate un-interupted
	if ((child = fork()))
		return;

	conn = pgsqlInit();

	if (!conn)
		{
		syslog(LOG_ERR, "Could not connect to database");
		_exit(2);
		}

	// Update sensor state
	if (!(conn = pgsqlUpdateSensorStatus(conn, sensor_id)))
		{
		syslog(LOG_ERR, "Could not update sensor status");
		_exit(2);
		}

	// Update link state
	if (!(conn = pgsqlUpdateLinkStatus(conn, sensor_id)))
		{
		syslog(LOG_ERR, "Count not update link status");
		_exit(2);
		}

	// Determine Now
	res = PQexecParams(conn, "select now();",
		0,       /* number of params */
		NULL,    /* let the backend deduce param type */
		NULL,
		NULL,    /* don't need param lengths since text */
		NULL,    /* default to all text params */
		0);      /* ask for binary results */
		
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
		syslog(LOG_ERR, "Postresql SELECT failed: %s", PQerrorMessage(conn));
		PQclear(res);
		PQfinish(conn);
		_exit(2);
		}

	strncpy(now, PQgetvalue(res, 0, 0), MAX_PARAM_SIZE);
	PQclear(res);

	// Begin transaction

	// **** Perform inserts
	strncpy(Values[0], sensor_id, MAX_PARAM_SIZE);
	strncpy(Values[1], now, MAX_PARAM_SIZE);
	snprintf(Values[2], MAX_PARAM_SIZE, "%llu", config.interval);
	for (Counter=0; Counter < IpCount; Counter++)
		{
		IPData = &IncData[Counter];

		if (IPData->ip == 0)
			{
			// This optimization allows us to quickly draw totals graphs for a sensor
			sql1 = "INSERT INTO bd_tx_total_log (sensor_id, timestamp, sample_duration, mac, ip, packet_count,total, icmp, udp, tcp, ftp, http, mail, p2p) VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13,$14);";
			sql2 = "INSERT INTO bd_rx_total_log (sensor_id, timestamp, sample_duration, mac, ip, packet_count,total, icmp, udp, tcp, ftp, http, mail, p2p) VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13,$14);";
			}
		else
			{
			sql1 = "INSERT INTO bd_tx_log (sensor_id, timestamp, sample_duration, mac, ip, packet_count, total, icmp, udp, tcp, ftp, http, mail, p2p) VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13,$14);";
			sql2 = "INSERT INTO bd_rx_log (sensor_id, timestamp, sample_duration, mac, ip, packet_count, total, icmp, udp, tcp, ftp, http, mail, p2p) VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13,$14);";
			}
		snprintf(Values[3], MAX_PARAM_SIZE, "%s", IPData->mac[0]);

		HostIp2CharIp(IPData->ip, Values[4]);

		Stats = &(IPData->Send);
		if (Stats->total > 512) // Don't log empty sets
			{
			// Log data in kilobytes
			snprintf(Values[5], MAX_PARAM_SIZE, "%llu", Stats->packet_count);
			snprintf(Values[6], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->total)/1024.0) + 0.5));
			snprintf(Values[7], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->icmp)/1024.0) + 0.5));
			snprintf(Values[8], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->udp)/1024.0) + 0.5));
			snprintf(Values[9], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->tcp)/1024.0) + 0.5));
			snprintf(Values[10], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->ftp)/1024.0) + 0.5));
			snprintf(Values[11], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->http)/1024.0) + 0.5));
			snprintf(Values[12], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->mail)/1024.0) + 0.5));
			snprintf(Values[13], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->p2p)/1024.0) + 0.5));

			res = PQexecParams(conn, sql1,
				14,       
				NULL,    /* let the backend deduce param type */
				paramValues,
				NULL,    /* don't need param lengths since text */
				NULL,    /* default to all text params */
				1);      /* ask for binary results */

			if (PQresultStatus(res) != PGRES_COMMAND_OK)
				{
				syslog(LOG_ERR, "Postresql INSERT failed: %s", PQerrorMessage(conn));
				PQclear(res);
				PQfinish(conn);
				conn = NULL;
				_exit(2);
				}
			PQclear(res);
			}
		Stats = &(IPData->Receive);
		if (Stats->total > 512) // Don't log empty sets
			{
			snprintf(Values[5], MAX_PARAM_SIZE, "%llu", Stats->packet_count);
			snprintf(Values[6], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->total)/1024.0) + 0.5));
			snprintf(Values[7], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->icmp)/1024.0) + 0.5));
			snprintf(Values[8], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->udp)/1024.0) + 0.5));
			snprintf(Values[9], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->tcp)/1024.0) + 0.5));
			snprintf(Values[10], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->ftp)/1024.0) + 0.5));
			snprintf(Values[11], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->http)/1024.0) + 0.5));
			snprintf(Values[12], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->mail)/1024.0) + 0.5));
			snprintf(Values[13], MAX_PARAM_SIZE, "%llu", (long long unsigned int)((((double)Stats->p2p)/1024.0) + 0.5));

			res = PQexecParams(conn, sql2,
				14,       
				NULL,    /* let the backend deduce param type */
				paramValues,
				NULL,    /* don't need param lengths since text */
				NULL,    /* default to all text params */
				1);      /* ask for binary results */

			if (PQresultStatus(res) != PGRES_COMMAND_OK)
				{
				syslog(LOG_ERR, "Postresql INSERT failed: %s", PQerrorMessage(conn));
				PQclear(res);
				PQfinish(conn);
				conn = NULL;
				_exit(2);
				}
			PQclear(res);
			}
		}

	// Insert extension data
	if (extension_data)
		{
		char *sql;
		char *ptr;
		int len, Counter;
		int fields;
		struct extensions *extptr;

		// Allocate SQL buffer;
		for(extptr = extension_data, len=200, fields=0; extptr; extptr = extptr->next)
			{
			len += strlen(extptr->name)+6;
			fields++;
			}

		if (fields > 20)
			{
			fields = 20;
			syslog(LOG_ERR, "pgsql: This module limited to 20 extension fields");
			}

		sql = malloc(len);
		if (!sql)
			{
			syslog(LOG_ERR, "Could not allocate RAM for extension SQL statement. Size: %d", len);
			PQfinish(conn);
			conn = NULL;
			_exit(2);
			}

		// Write SQL statement
		ptr = sql;
		ptr += sprintf(ptr, "INSERT INTO extension_log (sensor_id, timestamp, "); 
		for(extptr = extension_data, Counter=0; Counter < fields; extptr = extptr->next, Counter++)
			{
			ptr += sprintf(ptr, "%s, ", extptr->name);			
			paramValues[Counter+2] = extptr->value;
			}
		ptr -= 2;
	
		ptr += sprintf(ptr, ") VALUES ($1, $2, ");

		for(extptr = extension_data, Counter=0; Counter < fields; extptr = extptr->next, Counter++)
			ptr += sprintf(ptr, "$%d, ", Counter+3);			
		ptr -= 2;

		ptr += sprintf(ptr, ");");

		res = PQexecParams(conn, sql,
			fields+2,       
			NULL,    /* let the backend deduce param type */
			paramValues,
			NULL,    /* don't need param lengths since text */
			NULL,    /* default to all text params */
			1);      /* ask for binary results */

		if (PQresultStatus(res) != PGRES_COMMAND_OK)
			{
			syslog(LOG_ERR, "Postresql INSERT failed: %s", PQerrorMessage(conn));
			PQclear(res);
			PQfinish(conn);
			conn = NULL;
			_exit(2);
			}
		PQclear(res);
		free(sql);
		}

	PQfinish(conn);
	_exit(0);
#else
	syslog(LOG_ERR, "Postgresql logging selected but postgresql support is not compiled into binary.  Please check the documentation in README, distributed with this software.");
#endif
	}

