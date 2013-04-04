import bandwidthd

try:
	import MySQLdb
except Exception as e:
	MySQLdb=None

if MySQLdb:
	MySQLconn = MySQLdb.connect (host = "localhost",
                           user = "test",
                           passwd = "test",
                           db = "test")
	MySQLcurs = MySQLconn.cursor()
	try:
		MySQLcurs.execute("use bandwidthd")
	except:
		MySQLcurs.execute("create database bandwidthd")
		MySQLcurs.execute("use bandwidthd")
	try:
		MySQLcurs.execute("describe bd_tx_log;")
	except:
		MySQLcurs.execute('CREATE TABLE bd_tx_log (sensor_id int,mac character (17), ip int unsigned, timestamp timestamp, sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int);')
		MySQLcurs.execute('CREATE TABLE bd_rx_log (sensor_id int,mac character (17), ip int unsigned, timestamp timestamp, sample_duration int, packet_count int, total int, icmp int, udp int, tcp int, ftp int, http int, mail int, p2p int);')
		MySQLcurs.execute('CREATE TABLE sensors (sensor_id int,sensor_name character (255));')


def LookupMac(ip):
	return "no mac found"

def main(IpCount, sensor_name):
	if MySQLdb:
		MySQLcurs.execute('select * from sensors where sensor_name=%s',''.join(list(sensor_name)))
		SensorID=MySQLcurs.fetchall()
		if SensorID:
			SensorID=SensorID[0][0]
		else:
			MySQLcurs.execute('select * from sensors order by sensor_id desc') 
			SensorID=MySQLcurs.fetchall()
			if SensorID:
				SensorID=SensorID[0][0]+1
				MySQLcurs.execute('insert into sensors values ( %s, %s)', [SensorID, sensor_name])
			else:
				SensorID=1
				MySQLcurs.execute('insert into sensors values ( %s, %s)', [SensorID, sensor_name])
	for Counter in range(0,IpCount):
		res=bandwidthd.get_entry(Counter)
		res['mac']=LookupMac(res['ip'])
		if MySQLdb:
			MySQLcurs.execute('insert into bd_tx_log values (%s, %s, %s, FROM_UNIXTIME(%s), %s, %s, %s, %s, %s, %s, %s, %s, %s, %s);', [SensorID, res['mac'], res['ip'], res['timestamp'], 200, res['send']['packets'],res['send']['total'],res['send']['icmp'],res['send']['udp'],res['send']['tcp'],res['send']['ftp'],res['send']['http'],res['send']['mail'],res['send']['p2p']])
			MySQLcurs.execute('insert into bd_rx_log values (%s, %s, %s, FROM_UNIXTIME(%s), %s, %s, %s, %s, %s, %s, %s, %s, %s, %s);', [SensorID, res['mac'], res['ip'], res['timestamp'], 200, res['receive']['packets'],res['receive']['total'],res['receive']['icmp'],res['receive']['udp'],res['receive']['tcp'],res['receive']['ftp'],res['receive']['http'],res['receive']['mail'],res['receive']['p2p']])
	if MySQLdb:
		MySQLcurs.execute('commit;')
	return "Done"
