create table groups (
	group_id serial,
	name varchar,
	color varchar);
CREATE TABLE locations ( 
	id serial PRIMARY KEY, 
	name varchar, 
	group_id int default 0,
	latitude varchar,
	longitude varchar );
CREATE TABLE links_ignorelist (
	sensor_id int );
CREATE TABLE slices (
	id serial,
	sensor_id int,
	direction float,
	angle float,
	radius float);