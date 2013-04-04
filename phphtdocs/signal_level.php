<HTML>
<HEAD>
<TITLE>Signal Level Report</TITLE>
<link href="bandwidthd.css" rel="stylesheet" type="text/css">
</HEAD>
<BODY>
<?
include("include.php");
include("header.php");

$db = ConnectDb();
?>
<h3>Signal Level Report</h3>
<?
// NOTE: This result set is used several times below
//"select * from extension_log where sensor_id = ".$interface['sensor_id']." order by timestamp desc limit 1;"

$res = pg_query("
SELECT name as loc_name, sensor_name, management_url, interface,
	date_trunc('seconds', max(now()-last_connection))- interval '260 seconds' as checkin,
	min(minlatency) as minlatency, max(maxlatency) as maxlatency, avg(avglatency) as avglatency, 
avg(loadavg) as loadavg, avg(signal) as signal
FROM sensors, locations, extension_log
WHERE location = id and sensors.sensor_id = extension_log.sensor_id and signal is not null 
GROUP BY loc_name, sensor_name, management_url, interface
order by loc_name, sensor_name, interface;");
if (!$res)
    echo "<TR><TD>No routers reporting uptime</center>";
else
	{
	echo "<TABLE width=1000 cellpadding=0 cellspacing=0>";                                                                                                                             
	echo "<TR><TH class=row-header-left>Location<TH class=row-header-middle>Router<TH class=row-header-middle>Interface
		<TH class=row-header-middle>Min Latency<TH class=row-header-middle>Avg Latency
		<TH class=row-header-middle>Max Latency<TH class=row-header-middle>Load Avg<TH class=row-header-middle>Signal
		<TH class=row-header-right>Checkin Overdue";
	while ($r = @pg_fetch_array($res))
    	{
    	echo("<TR><TD>".$r['loc_name'].
			"<TD><a href=".$r['management_url'].">".$r['sensor_name']."</a>
			<TD align=center>".$r['interface'].
			"<TD align=right>".$r['minlatency'].
			"<TD align=right>".round($r['avglatency'], 1).
			"<TD align=right>".$r['maxlatency'].
			"<TD align=right>".round($r['loadavg'], 2).
			"<TD align=right>".round($r['signal'],1).
			"<TD align=right>");
		if ($r['checkin'] > 0)
			echo($r['checkin']);
    	}
	echo "</TABLE>";
	}
?>
</BODY>