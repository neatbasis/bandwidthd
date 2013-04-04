<HTML>
<HEAD>
<TITLE>Uptime Report</TITLE>
<link href="bandwidthd.css" rel="stylesheet" type="text/css">
</HEAD>
<BODY>
<?
include("include.php");
include("header.php");

$db = ConnectDb();
?>
<h3>Uptime Report</h3>
<?
// NOTE: This result set is used several times below
//"select * from extension_log where sensor_id = ".$interface['sensor_id']." order by timestamp desc limit 1;"

$res = pg_query("
SELECT sensor_name, max(sensor_id) as sensor_id, management_url, build, date_trunc('seconds', max(uptime)) as uptime, max(reboots) as reboots, name as loc_name, date_trunc('seconds', max(now()-last_connection))- interval '260 seconds' as checkin
FROM sensors, locations
WHERE location = id
GROUP BY sensor_name, management_url, build, loc_name
order by uptime desc;");
if (!$res)
    echo "<TR><TD>No routers reporting uptime</center>";
else
	{
	echo "<TABLE width=1000 cellpadding=0 cellspacing=0>";                                                                                                                             
	echo "<TR><TH class=row-header-left>Router<TH class=row-header-middle>Location<TH class=row-header-middle>Version<TH class=row-header-middle>Load Avg<TH class=row-header-middle>Reboots<TH class=row-header-middle>Uptime<TH class=row-header-right>Checkin Overdue";
	while ($r = @pg_fetch_array($res))
    	{
		$extension_log = pg_query("select * from extension_log where sensor_id = ".$r['sensor_id']." order by timestamp desc limit 1;");
		$extension = @pg_fetch_array($extension_log);
    	echo("<TR><TD><a href=".$r['management_url'].">".$r['sensor_name']."</a><TD align=center>".$r['loc_name']."<TD align=right>".$extension['version']."<TD align=right>".$extension['loadavg']."<TD align=right>".$r['reboots']."<TD align=right>".$r['uptime']."<TD align=right>");
		if ($r['checkin'] > 0)
			echo($r['checkin']);
    	}
	echo "</TABLE>";
	}
?>
</BODY>