<HEAD>
<TITLE>Location Statistics</TITLE>
</HEAD>
<?
include("include.php");
include("header.php");
?></CENTER>
<HEAD>
<link href="bandwidthd.css" rel="stylesheet" type="text/css">
</head>
<center><img src=legend.gif></center>
<?
$db = ConnectDb();

$location_id = $HTTP_GET_VARS['location_id'];
$locations = pg_query("SELECT * from locations where id = $location_id");
if (!$locations)
    echo "<TR><TD>No matching locations in database...</center>";

$r = @pg_fetch_array($locations);

$sensors = pg_query("select distinct sensor_name from sensors where location = ".$r['id']." order by sensor_name;");

while ($sensor = @pg_fetch_array($sensors))
	{
	echo("<H3>".$sensor['sensor_name']."</H3>");
	$interfaces = pg_query("select * from sensors where sensor_name = '".$sensor['sensor_name']."' order by interface;");
	$First = true;
	while ($interface = @pg_fetch_array($interfaces))
		{
		$extension_log = pg_query("select * from extension_log where sensor_id = ".$interface['sensor_id']." order by timestamp desc limit 1;");
		$extension = @pg_fetch_array($extension_log);
		if ($First)
			{
			$First = false;
			echo("<TABLE width=100%><TR>");	
			if ($extension['version'] != "")
				echo("<TD>Version: ".$extension['version']);
			if ($extension['uptime'] != "")
				{
				$uptime_seconds = $extension['uptime'];
				$uptime_text = "";
				if ($uptime_seconds > 60*60*24)
					{
					$value = floor($uptime_seconds/(60*60*24));
					$uptime_text .= $value." Days ";
					$uptime_seconds -= $value*60*60*24;
					}
				if ($uptime_seconds > 60*60)
					{
					$value = floor($uptime_seconds/(60*60));
					$uptime_text .= $value." Hours ";
					$uptime_seconds -= $value*60*60;
					}
				$value = floor($uptime_seconds/60);
				$uptime_text .= $value." Minutes ";
				$uptime_seconds -= $value*60*60;
				echo("<TD>Uptime: ".$uptime_text);
				}
			if ($extension['loadavg'] != "")
				echo("<TR><TD>5 Min load Avg: ".$extension['loadavg']);
			echo("</TABLE>");
			}
		echo "<h4><a onclick=\"window.moveTo(0,0); window.resizeTo(screen.width,screen.height);\" href=sensors.php?sensor_id=".$interface['sensor_id'].">".$interface['interface']." - ".$interface['description']."</a></h4>\n";
		if ($extension['minlatency'] != "")
			{
			echo("<TABLE><TR><TD>Latency:<TD>Min<TD>".$extension['minlatency']."ms<TD>Avg<TD>".$extension['avglatency']."ms<TD>Max<TD>".$extension['maxlatency']."ms</TABLE>");
			}
		if ($extension['ipaddr'] != "")
			echo "<PRE>".$extension['ipaddr']."</PRE>\n";
		if ($extension['errors'] != "")
			echo "<PRE>".$extension['errors']."</PRE>\n";	
		if ($extension['wireless'] != "")
			echo "<PRE>".preg_replace("/Encryption key:[0-9A-F-]*/", "Encryption key:HIDDEN", $extension['wireless'])."</PRE>\n";
		echo "Transmit<br><img src=graph.php?ip=0.0.0.0/0&sensor_id=".$interface['sensor_id']."&table=bd_tx_total_log&width=525&height=150><BR>";
		echo "Receive<BR><img src=graph.php?ip=0.0.0.0/0&sensor_id=".$interface['sensor_id']."&table=bd_rx_total_log&width=525&height=150><BR>";
		}
    }

// Add some blank lines to the bottom so we can always scroll all the way down to the bottom graph
?>
<BR>
<BR>
<BR>
<BR>
<BR>
<BR>
<BR>
<BR>
<BR>
<BR>
<BR>
<BR>
<BR>
<BR>
<BR>