CREATE TABLE extension_log ( 
	sensor_id int, 
	timestamp timestamp with time zone DEFAULT now(), 
	errors varchar,
	loadavg varchar,
	signal varchar,
	uptime varchar,
	wireless varchar);
create index extension_log_sensor_id_idx on extension_log(sensor_id);
