#include "bandwidthd.h"

extern struct config config;
extern unsigned int IpCount;
extern time_t ProgramStart;

#ifdef HAVE_LIBSQLITE3

#include <sqlite3.h>

// Check that tables exist and create them if not
sqlite3 *sqliteCheckTables(sqlite3 *conn)
	{
	char **results;
	int rows, columns;
	char *error;

	if (sqlite3_get_table(conn, "select name from sqlite_master where name='sensors';", &results, &rows, &columns, &error) != SQLITE_OK)
		{
		syslog(LOG_ERR, "SQLite select failed: %s", error);
		sqlite3_free_table(results);
		sqlite3_free(error);
		sqlite3_close(conn);
		return(NULL);
		}
	sqlite3_free_table(results);

	if (rows != 1)
		{	
		if (sqlite3_exec(conn, "CREATE TABLE bd_rx_log (sensor_id int, mac varchar, ip int, timestamp int, sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); create index bd_rx_log_sensor_id_ip_timestamp_idx on bd_rx_log (sensor_id, ip, timestamp); create index bd_rx_log_sensor_id_timestamp_idx on bd_rx_log(sensor_id, timestamp);", 
			NULL, NULL, &error) != SQLITE_OK)
        	{
	        syslog(LOG_ERR, "SQLite rrror creating table: %s", error);
			sqlite3_free_table(results);
			sqlite3_free(error);
			sqlite3_close(conn);
			return(NULL);
        	}

		if (sqlite3_exec(conn, "CREATE TABLE bd_tx_log (sensor_id int, mac varchar, ip int, timestamp int, sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); create index bd_tx_log_sensor_id_ip_timestamp_idx on bd_tx_log (sensor_id, ip, timestamp); create index bd_tx_log_sensor_id_timestamp_idx on bd_tx_log(sensor_id, timestamp);",
			NULL, NULL, &error) != SQLITE_OK)
        	{
	        syslog(LOG_ERR, "SQLite rrror creating table: %s", error);
			sqlite3_free_table(results);
			sqlite3_free(error);
			sqlite3_close(conn);
			return(NULL);
        	}

		if (sqlite3_exec(conn, "CREATE TABLE bd_rx_total_log (sensor_id int, ip int, timestamp int, sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); create index bd_rx_total_log_sensor_id_timestamp_ip_idx on bd_rx_total_log (sensor_id, timestamp);", 
			NULL, NULL, &error) != SQLITE_OK)
        	{
	        syslog(LOG_ERR, "SQLite rrror creating table: %s", error);
			sqlite3_free_table(results);
			sqlite3_free(error);
			sqlite3_close(conn);
			return(NULL);
        	}

		if (sqlite3_exec(conn, "CREATE TABLE bd_tx_total_log (sensor_id int, ip int, timestamp int, sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int); create index bd_tx_total_log_sensor_id_timestamp_ip_idx on bd_tx_total_log (sensor_id, timestamp);", 
			NULL, NULL, &error) != SQLITE_OK)
        	{
	        syslog(LOG_ERR, "SQLite rrror creating table: %s", error);
			sqlite3_free_table(results);
			sqlite3_free(error);
			sqlite3_close(conn);
			return(NULL);
        	}		

		if (sqlite3_exec(conn, "CREATE TABLE sensors ( sensor_id INTEGER PRIMARY KEY, sensor_name varchar, location int, build int default 0, uptime int, reboots int default 0, interface varchar, description varchar, management_url varchar, last_connection int );",
			NULL, NULL, &error) != SQLITE_OK)
        	{
	        syslog(LOG_ERR, "SQLite rrror creating table: %s", error);
			sqlite3_free_table(results);
			sqlite3_free(error);
			sqlite3_close(conn);
			return(NULL);
        	}		

		/* Link support is getting pulled I beleive 
		"CREATE TABLE links (id1 int, id2 int, plot boolean default TRUE, last_update timestamp with time zone);");
		*/
		}
	return(conn);
	}

sqlite3* sqliteInit(void)
	{
	sqlite3* conn;
	int ret;
	
	while((ret = sqlite3_open(config.db_connect_string, &conn)) != SQLITE_OK)
		{
		if (ret == SQLITE_BUSY || ret == SQLITE_LOCKED)
			syslog(LOG_ERR, "Database is busy, waiting...");
		else
			{
			syslog(LOG_ERR, "Connection to database '%s' was unsuccessfull.  Error Code: %d", config.db_connect_string, ret);
			return(NULL);
			}
		sleep(rand()%5);
		}

	return(conn);
	}

sqlite3* sqliteDetermineSensorID(sqlite3* conn, int *sensor_id, char *sensor_name, char *interface)
	{
	sqlite3_stmt* stmt;	
	int ret;

	if (sqlite3_prepare_v2(conn, 
		"select sensor_id from sensors where sensor_name = :one and interface = :two;",
		-1, &stmt, NULL) != SQLITE_OK)
		{
		syslog(LOG_ERR, "Error compiling SQL Statement to determine sensorid");
		sqlite3_close(conn);
		return(NULL);
		}

	sqlite3_bind_text(stmt, 1, sensor_name, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, interface, -1, SQLITE_TRANSIENT);

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE && ret != SQLITE_ROW)
    	{
        syslog(LOG_ERR, "SQLite select failed");
		sqlite3_finalize(stmt);
		sqlite3_close(conn);
		return(NULL);
        }

	if (ret == SQLITE_ROW)
		*sensor_id = sqlite3_column_int(stmt, 0);
	else
		*sensor_id = -1;

	sqlite3_finalize(stmt);
	return(conn);
	}		

sqlite3* sqliteCreateSensorID(sqlite3* conn, int *sensor_id)
	{
	sqlite3_stmt* stmt;
	int ret;
	
	if (sqlite3_prepare_v2(conn,
		"insert into sensors (sensor_name, interface) VALUES (:one, :two);",
		-1, &stmt, NULL) != SQLITE_OK)
		{
		syslog(LOG_ERR, "Error compiling SQL Statement to create new sensor_id");
		sqlite3_close(conn);
		return(NULL);
		}

	sqlite3_bind_text(stmt, 1, config.sensor_name, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, config.dev, -1, SQLITE_TRANSIENT);

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE)
    	{
        syslog(LOG_ERR, "SQLite select failed");
		sqlite3_finalize(stmt);
		sqlite3_close(conn);
		return(NULL);
        }

	sqlite3_finalize(stmt);
	*sensor_id = sqlite3_last_insert_rowid(conn);
	return(conn);
	}

sqlite3* sqliteUpdateSensorStatus(sqlite3* conn, int sensor_id, int timestamp)
	{
	sqlite3_stmt* stmt;
	int ret;
	
	if (sqlite3_prepare_v2(conn,
		"update sensors set description = :one, management_url = :two, last_connection = :three, build = :four, uptime = :five where sensor_id = :six;",
		-1, &stmt, NULL) != SQLITE_OK)
		{
		syslog(LOG_ERR, "Error compiling SQL Statement to create new sensor_id");
		sqlite3_close(conn);
		return(NULL);
		}

	sqlite3_bind_text(stmt, 1, config.description, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, config.management_url, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, timestamp);
	sqlite3_bind_int(stmt, 4, BUILD);
	sqlite3_bind_int(stmt, 5, timestamp-ProgramStart);
	sqlite3_bind_int(stmt, 6, sensor_id);

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE)
    	{
        syslog(LOG_ERR, "SQLite select failed");
		sqlite3_finalize(stmt);
		sqlite3_close(conn);
		return(NULL);
        }

	sqlite3_finalize(stmt);
	return(conn);	
	}

sqlite3* sqliteIncReboots(sqlite3* conn, int sensor_id)
	{
	sqlite3_stmt* stmt;
	int ret;
	
	if (sqlite3_prepare_v2(conn,
		"update sensors set reboots = reboots+1 where sensor_id = $1;",
		-1, &stmt, NULL) != SQLITE_OK)
		{
		syslog(LOG_ERR, "Error compiling SQL Statement to create new sensor_id");
		sqlite3_close(conn);
		return(NULL);
		}

	sqlite3_bind_int(stmt, 1, sensor_id);

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE)
    	{
        syslog(LOG_ERR, "SQLite sensor reboot update failed");
		sqlite3_finalize(stmt);
		sqlite3_close(conn);
		return(NULL);
        }

	sqlite3_finalize(stmt);
	return(conn);	
	}
#endif

void sqliteStoreIPData(struct IPData IncData[], struct extensions *extension_data)
	{
#ifdef HAVE_LIBSQLITE3
	static int sensor_id = -1;
	static pid_t child = 0;

	struct IPData *IPData;
	unsigned int Counter;
	struct Statistics *Stats;

	// SQLite variables
	sqlite3* conn = NULL;
	char *zErrMsg = 0;

	time_t now;

	if (!config.output_database == DB_SQLITE)
		return;

	// ************ Inititialize the db if it's not already

	// Determine Now
	now = time(NULL);

	// Do initialization in main thread in order to prevent doing sensor_id work repeatedly
	if (sensor_id < 0)	// Determine numeric sensor ID
		{
		syslog(LOG_INFO, "Initializing database info");
		conn = sqliteInit();

		if (!conn)
			{
			syslog(LOG_ERR, "Could not connect to database");
			return;
			}

		if (!(conn = sqliteCheckTables(conn))) // Create tables if neccisary
			{
			syslog(LOG_ERR, "Failed to check or create database tables");
			return;
			}

		if (!(conn = sqliteDetermineSensorID(conn, &sensor_id, config.sensor_name, config.dev)))
			{
			syslog(LOG_ERR, "Failed to determine sensor_id");
			return;
			}

		if (sensor_id < 0) // Create a new sensor ID
			{
			syslog(LOG_ERR, "Registering new sensor_id");
			if (!(conn = sqliteCreateSensorID(conn, &sensor_id)))
				{
				syslog(LOG_ERR, "Failed to create new sensor_id");
				return;
				}
			}

		sqliteIncReboots(conn, sensor_id);
		
		sqlite3_close(conn);
		conn = NULL;

		syslog(LOG_INFO, "Sensor ID: %d", sensor_id);
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

	conn = sqliteInit();

	if (!conn)
		{
		syslog(LOG_ERR, "Could not connect to database");
		_exit(2);
		}

	// Update sensor state
	if (!(conn = sqliteUpdateSensorStatus(conn, sensor_id, now)))
		{
		syslog(LOG_ERR, "Could not update sensor status");
		_exit(2);
		}

	/*
	// Update link state
	if (!(conn = sqliteUpdateLinkStatus(conn, sensor_id)))
		{
		syslog(LOG_ERR, "Count not update link status");
		_exit(2);
		}
	*/



	// Begin transaction

	if (sqlite3_exec(conn, "begin;", NULL, NULL, &zErrMsg) != SQLITE_OK)
		{
		syslog(LOG_ERR, "Error beginning transaction: %s", zErrMsg);
		_exit(2);
		}

	// Compile sql statements
	sqlite3_stmt* sqlStandardTXInsert;
	sqlite3_stmt* sqlStandardRXInsert;
	sqlite3_stmt* sqlTotalTXInsert;
	sqlite3_stmt* sqlTotalRXInsert;

	if (sqlite3_prepare_v2(conn, 
		"INSERT INTO bd_tx_total_log (sensor_id, timestamp, sample_duration, ip, packet_count,total, icmp, udp, tcp, ftp, http, mail, p2p) VALUES(:one, :two, :three, :four, :five, :six, :seven, :eight, :nine, :ten, :eleven, :twelve, :thirteen);",
		-1, &sqlTotalTXInsert, NULL) != SQLITE_OK)
		{
		syslog(LOG_ERR, "Error compiling SQL Statement");
		_exit(2);
		}

	if (sqlite3_prepare_v2(conn, 
		"INSERT INTO bd_rx_total_log (sensor_id, timestamp, sample_duration, ip, packet_count,total, icmp, udp, tcp, ftp, http, mail, p2p) VALUES(:one, :two, :three, :four, :five, :six, :seven, :eight, :nine, :ten, :eleven, :twelve, :thirteen);",
		-1, &sqlTotalRXInsert, NULL) != SQLITE_OK)
		{
		syslog(LOG_ERR, "Error compiling SQL Statement");
		_exit(2);
		}

	if (sqlite3_prepare_v2(conn, 
		"INSERT INTO bd_tx_log (sensor_id, timestamp, sample_duration, mac, ip, packet_count,total, icmp, udp, tcp, ftp, http, mail, p2p) VALUES(:one, :two, :three, :four, :five, :six, :seven, :eight, :nine, :ten, :eleven, :twelve, :thirteen, :fourteen);",
		-1, &sqlStandardTXInsert, NULL) != SQLITE_OK)
		{
		syslog(LOG_ERR, "Error compiling SQL Statement");
		_exit(2);
		}

	if (sqlite3_prepare_v2(conn, 
		"INSERT INTO bd_rx_log (sensor_id, timestamp, sample_duration, mac, ip, packet_count,total, icmp, udp, tcp, ftp, http, mail, p2p) VALUES(:one, :two, :three, :four, :five, :six, :seven, :eight, :nine, :ten, :eleven, :twelve, :thirteen, :fourteen);",
		-1, &sqlStandardRXInsert, NULL) != SQLITE_OK)
		{
		syslog(LOG_ERR, "Error compiling SQL Statement");
		_exit(2);
		}

	// **** Prepare bindings that never change
	sqlite3_bind_int(sqlStandardTXInsert, 1, sensor_id);
	sqlite3_bind_int(sqlStandardRXInsert, 1, sensor_id);
	sqlite3_bind_int(sqlTotalTXInsert, 1, sensor_id);
	sqlite3_bind_int(sqlTotalRXInsert, 1, sensor_id);

	sqlite3_bind_int(sqlStandardTXInsert, 2, now);
	sqlite3_bind_int(sqlStandardRXInsert, 2, now);
	sqlite3_bind_int(sqlTotalTXInsert, 2, now);
	sqlite3_bind_int(sqlTotalRXInsert, 2, now);

	sqlite3_bind_int(sqlStandardTXInsert, 3, config.interval);
	sqlite3_bind_int(sqlStandardRXInsert, 3, config.interval);
	sqlite3_bind_int(sqlTotalTXInsert, 3, config.interval);
	sqlite3_bind_int(sqlTotalRXInsert, 3, config.interval);

	// Preform Inserts
	for (Counter=0; Counter < IpCount; Counter++)
		{
		IPData = &IncData[Counter];
		sqlite3_stmt* sqlTXInsert;
		sqlite3_stmt* sqlRXInsert;
		int i=0;
		if (IPData->ip == 0)
			{
			// This optimization allows us to quickly draw totals graphs for a sensor
			sqlTXInsert = sqlTotalTXInsert;
			sqlRXInsert = sqlTotalRXInsert;
			}
		else
			{
			sqlTXInsert = sqlStandardTXInsert;
			sqlRXInsert = sqlStandardRXInsert;
			printf("423, %s\n", IPData->mac[0]);
			sqlite3_bind_text(sqlTXInsert, 4, IPData->mac, -1, SQLITE_STATIC);
			sqlite3_bind_text(sqlRXInsert, 4, IPData->mac, -1, SQLITE_STATIC);
			i=1;
			}
		sqlite3_bind_int(sqlTXInsert, 4+i, IPData->ip);
		sqlite3_bind_int(sqlRXInsert, 4+i, IPData->ip);
		
		Stats = &(IPData->Send);
		if (Stats->total > 512) // Don't log empty sets
			{
			// Log data in kilobytes
			sqlite3_bind_int64(sqlTXInsert, 5+i, Stats->packet_count);
			sqlite3_bind_int64(sqlTXInsert, 6+i, (long long unsigned int)((((double)Stats->total)/1024.0) + 0.5));
			sqlite3_bind_int64(sqlTXInsert, 7+i, (long long unsigned int)((((double)Stats->icmp)/1024.0) + 0.5));
			sqlite3_bind_int64(sqlTXInsert, 8+i, (long long unsigned int)((((double)Stats->udp)/1024.0) + 0.5));
			sqlite3_bind_int64(sqlTXInsert, 9+i, (long long unsigned int)((((double)Stats->tcp)/1024.0) + 0.5));
			sqlite3_bind_int64(sqlTXInsert, 10+i, (long long unsigned int)((((double)Stats->ftp)/1024.0) + 0.5)); 			
			sqlite3_bind_int64(sqlTXInsert, 11+i, (long long unsigned int)((((double)Stats->http)/1024.0) + 0.5));
			sqlite3_bind_int64(sqlTXInsert, 12+i, (long long unsigned int)((((double)Stats->mail)/1024.0) + 0.5));
			sqlite3_bind_int64(sqlTXInsert, 13+i, (long long unsigned int)((((double)Stats->p2p)/1024.0) + 0.5));

			if (sqlite3_step(sqlTXInsert) != SQLITE_DONE)
				{
				syslog(LOG_ERR, "SQLite Insert failed");
				_exit(2);
				}

			sqlite3_reset(sqlTXInsert);
			}

		Stats = &(IPData->Receive);
		if (Stats->total > 512) // Don't log empty sets
			{
			// Log data in kilobytes
			sqlite3_bind_int64(sqlRXInsert, 5+i, Stats->packet_count);
			sqlite3_bind_int64(sqlRXInsert, 6+i, (long long unsigned int)((((double)Stats->total)/1024.0) + 0.5));
			sqlite3_bind_int64(sqlRXInsert, 7+i, (long long unsigned int)((((double)Stats->icmp)/1024.0) + 0.5));
			sqlite3_bind_int64(sqlRXInsert, 8+i, (long long unsigned int)((((double)Stats->udp)/1024.0) + 0.5));
			sqlite3_bind_int64(sqlRXInsert, 9+i, (long long unsigned int)((((double)Stats->tcp)/1024.0) + 0.5));
			sqlite3_bind_int64(sqlRXInsert, 10+i, (long long unsigned int)((((double)Stats->ftp)/1024.0) + 0.5)); 			
			sqlite3_bind_int64(sqlRXInsert, 11+i, (long long unsigned int)((((double)Stats->http)/1024.0) + 0.5));
			sqlite3_bind_int64(sqlRXInsert, 12+i, (long long unsigned int)((((double)Stats->mail)/1024.0) + 0.5));
			sqlite3_bind_int64(sqlRXInsert, 13+i, (long long unsigned int)((((double)Stats->p2p)/1024.0) + 0.5));

			if (sqlite3_step(sqlRXInsert) != SQLITE_DONE)
				{
				syslog(LOG_ERR, "SQLite Insert failed");
				_exit(2);
				}

			sqlite3_reset(sqlRXInsert);
			}		
		}

	if (sqlite3_exec(conn, "commit;", NULL, NULL, &zErrMsg) != SQLITE_OK)
		{
		syslog(LOG_ERR, "Error commiting transaction");
		_exit(2);
		}

	sqlite3_finalize(sqlStandardTXInsert);
	sqlite3_finalize(sqlStandardRXInsert);
	sqlite3_finalize(sqlTotalTXInsert);
	sqlite3_finalize(sqlTotalRXInsert);

	if (sqlite3_exec(conn, "begin;", NULL, NULL, &zErrMsg) != SQLITE_OK)
		{
		syslog(LOG_ERR, "Error beginning transaction");
		_exit(2);
		}

	// Insert extension data
	if (extension_data)
		{
		char *sql;
		char *ptr;
		int len, Counter;
		int fields;
		struct extensions *extptr;
		sqlite3_stmt* stmt;
		int ret;

		// Allocate SQL buffer;
		for(extptr = extension_data, len=200, fields=0; extptr; extptr = extptr->next)
			{
			len += strlen(extptr->name)+6;
			fields++;
			}

		if (fields > 20)
			{
			fields = 20;
			syslog(LOG_ERR, "sqlite: This module limited to 20 extension fields");
			}

		sql = malloc(len);
		if (!sql)
			{
			syslog(LOG_ERR, "Could not allocate RAM for extension SQL statement. Size: %d", len);
			sqlite3_close(conn);
			_exit(2);
			}

		// Write SQL statement
		ptr = sql;
		ptr += sprintf(ptr, "INSERT INTO extension_log (sensor_id, timestamp, "); 
		for(extptr = extension_data, Counter=0; Counter < fields; extptr = extptr->next, Counter++)
			ptr += sprintf(ptr, "%s, ", extptr->name);			

		ptr -= 2;
	
		ptr += sprintf(ptr, ") VALUES (?1, ?2, ");

		for(extptr = extension_data, Counter=0; Counter < fields; extptr = extptr->next, Counter++)
			ptr += sprintf(ptr, "?%d, ", Counter+3);			
		ptr -= 2;

		ptr += sprintf(ptr, ");");

		if (sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL) != SQLITE_OK)
			{
			syslog(LOG_ERR, "Error compiling SQL Statement to create new sensor_id");
			sqlite3_close(conn);
			_exit(2);
			}	

		for(extptr = extension_data, Counter=0; Counter < fields; extptr = extptr->next, Counter++)
			sqlite3_bind_text(stmt, Counter+3, extptr->value, -1, SQLITE_TRANSIENT);

		ret = sqlite3_step(stmt);
		if (ret != SQLITE_DONE)
    		{
	        syslog(LOG_ERR, "SQLite extension insert failed");
			sqlite3_finalize(stmt);
			sqlite3_close(conn);
			_exit(2);
    	    }

		sqlite3_finalize(stmt);

		free(sql);
		}

	if (sqlite3_exec(conn, "commit;", NULL, NULL, &zErrMsg) != SQLITE_OK)
		{
		syslog(LOG_ERR, "Error commiting transaction");
		_exit(2);
		}

	sqlite3_close(conn);
	_exit(0);
#else
	syslog(LOG_ERR, "SQLite logging selected but SQLite support is not compiled into binary.  Please check the documentation in README, distributed with this software.");
#endif
	}

