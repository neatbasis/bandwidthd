<?
include("include.php");

$db = ConnectDb();

$PHP_SELF = "failures.php";

if ($HTTP_GET_VARS['reset_links'] != "")
	{
	pg_query("delete from links where last_update < now()-interval '10 minutes';");
	header("Location: $PHP_SELF");
	}
?>
<HTML>
<HEAD>
<TITLE>Failure Report</TITLE>
<link href="bandwidthd.css" rel="stylesheet" type="text/css">
</HEAD>
<BODY>
<?
include("header.php");
?>
<h3>Failed Routers</h3>
<?
$res = pg_query("
SELECT sensor_name, management_url, name as loc_name, date_trunc('seconds', min(now()-last_connection))- interval '4 minutes' as missing 
FROM sensors, locations 
WHERE location = id and last_connection < now()-interval '5 minutes' 
GROUP BY sensor_name, management_url, loc_name
order by missing desc;");
if (!$res)
    echo "<TR><TD>All routers have checked in</center>";
else
	{
	echo "<TABLE width=100% cellpadding=0 cellspacing=0>";                                                                                                                             
	echo "<TR><TH class=row-header-left>Router<TH class=row-header-middle>Location<TH class=row-header-right>Checkin Overdue";
	while ($r = @pg_fetch_array($res))
    	{
    	echo("<TR><TD><a href=".$r['management_url'].">".$r['sensor_name']."</a><TD>".$r['loc_name']."<TD align=right>".$r['missing']);
    	}
	echo "</TABLE>";
	}
?>
<h3>Failed Links</h3>
</CENTER>
<a href=<?=$PHP_SELF?>?reset_links=1 class=text-button>Reset Broken Links</a>
<BR>
<CENTER>
<TABLE width=100% cellpadding=0 cellspacing=0>
<TR><TH class=row-header-left>First Tower<TH class=row-header-middle>Second Tower<TH class=row-header-middle>First Router
<TH class=row-header-middle>Second Router<TH class=row-header-right>Last Update
<?
$links = pg_query("
SELECT loc_a.name as loc_a_name, loc_b.name as loc_b_name, sens_a.sensor_id as sens_a_id, sens_a.sensor_name as sens_a_name,
    sens_a.interface as sens_a_interface, sens_b.sensor_id as sens_b_id, sens_b.sensor_name as sens_b_name,
    sens_b.interface as sens_b_interface, to_char(last_update, 'HH24:MI Mon, DD YY') as last_update, 
	sens_a.management_url as sens_a_url, sens_b.management_url as sens_b_url
FROM links, sensors as sens_a, sensors as sens_b, locations as loc_a, locations as loc_b
WHERE id1 not in (select * from links_ignorelist ) and id2 not in (select * from links_ignorelist )
	and last_update < now()-interval '10 minutes'
    and sens_a.location != sens_b.location and id1 = sens_a.sensor_id and id2 = sens_b.sensor_id and sens_a.location = loc_a.id
    and sens_b.location = loc_b.id
ORDER BY last_update;");
if (!$links)
    echo "<TR><TD>No links in database...</center>";
                                                                                                                             
while ($r = @pg_fetch_array($links))
    {
    echo("<TR><TD>".$r['loc_a_name']."<TD>".$r['loc_b_name']);
    echo("<TD><A href=".$r['sens_a_url'].">".ereg_replace("\..*$", "", $r['sens_a_name'])."</a> - ".$r['sens_a_interface']);
    echo("<TD><A href=".$r['sens_b_url'].">".ereg_replace("\..*$", "", $r['sens_b_name'])."</a> - ".$r['sens_b_interface']);
    echo("<TD>".$r['last_update']);
    }
?>
</table>
<h3>Low Rates</h3>
</CENTER>
<?
/*$res = pg_query("SELECT sensor_name, interface, management_url, signal, wireless from sensors, extension_log 
	WHERE sensors.sensor_id = extension_log.sensor_id and wireless like '%Frequency:5.%'	
		and wireless not like '%Bit Rate:0kb/s%' order by signal
;");*/
//		and wireless like '%Bit Rate:6Mb/s%'
//;");
$res = pg_query("SELECT sensor_name, interface, sensors.sensor_id, management_url, 95+avg(signal) as signal from sensors, extension_log 
		WHERE sensors.sensor_id = extension_log.sensor_id 
		and wireless like '%Frequency:5.%' 
		and wireless not like '%Bit Rate:0kb/s%' 
		group by sensor_name, interface, sensors.sensor_id, management_url order by signal;
;");
if (!$res)
    echo "<TR><TD>No low rates on record</center>";
else	
{
	echo "<TABLE>";
	while ($r = @pg_fetch_array($res))
    	{
    	echo("<TR><TD><a href=".$r['management_url'].">".$r['sensor_name']."</A><TD>".$r['interface']."<TD>".$r['signal']);
		$res2 = pg_query("SELECT timestamp, wireless from extension_log where sensor_id = ".$r['sensor_id']." order by timestamp desc limit 3"); 
		echo("<TR><TD colspan=3><PRE>");
		while($r2 = @pg_fetch_array($res2))
			{
			echo($r2['timestamp']."\n".preg_replace("/Encryption key:[0-9A-F-]+/", "Encryption key:HIDDEN", $r2['wireless']));
			}
    	}
	echo "</TABLE>";
	}
?>
</BODY>