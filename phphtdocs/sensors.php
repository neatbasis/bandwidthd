<?
include("include.php");
include("header.php");

// Get variables from url

if (isset($_GET['sensor_id']) && $_GET['sensor_id'] != "none")
    $sensor_id = $_GET['sensor_id'];

if (isset($_GET['interval']) && $_GET['interval'] != "none")
    $interval = $_GET['interval'];

if (isset($_GET['timestamp']) && $_GET['timestamp'] != "none")
    $timestamp = $_GET['timestamp'];

if (isset($_GET['subnet']) && $_GET['subnet'] != "none")
    $subnet = $_GET['subnet'];

if (isset($_GET['limit']) && $_GET['limit'] != "none")
	$limit = $_GET['limit'];

if (isset($_GET['graphs']))
	$graphs = $_GET['graphs'];

$db = ConnectDb();
?>
<HEAD>
<link href="bandwidthd.css" rel="stylesheet" type="text/css">
</head>
<FORM name="navigation" method=get action=<?=$PHP_SELF?>>
<table width=100% cellspacing=0 cellpadding=5 border=1>
<tr>
<td>
<?
$sql = "SELECT sensor_name, interface, sensor_id from sensors order by sensor_name, interface;";
$result = @pg_query($sql);
if (!$result)
	{
	echo "<center>Collecting data...</center>";
	exit;
	}
?>
<SELECT name="sensor_id">
<OPTION value="none">--Select A Sensor--
<?
while ($r = pg_fetch_array($result))
    echo "<option value=\"".$r['sensor_id']."\" ".($sensor_id==$r['sensor_id']?"SELECTED":"").">".$r['sensor_name']." - ".$r['interface']."\n";
?>
</SELECT>
<td><SELECT name="interval">
<OPTION value="none">--Select An Interval--
<OPTION value=<?=INT_DAILY?> <?=$interval==INT_DAILY?"SELECTED":""?>>Daily
<OPTION value=<?=INT_WEEKLY?> <?=$interval==INT_WEEKLY?"SELECTED":""?>>Weekly
<OPTION value=<?=INT_MONTHLY?> <?=$interval==INT_MONTHLY?"SELECTED":""?>>Monthly
<OPTION value=<?=INT_YEARLY?> <?=$interval==INT_YEARLY?"SELECTED":""?>>Yearly
<OPTION value=<?=24*60*60?> <?=$interval==24*60*60?"SELECTED":""?>>24hrs
<OPTION value=<?=30*24*60*60?> <?=$interval==30*24*60*60?"SELECTED":""?>>30days
</select>

<td><SELECT name="limit">
<OPTION value="none">--How Many Results--
<OPTION value=20 <?=$limit==20?"SELECTED":""?>>20
<OPTION value=50 <?=$limit==50?"SELECTED":""?>>50
<OPTION value=100 <?=$limit==100?"SELECTED":""?>>100
<OPTION value=all <?=$limit=="all"?"SELECTED":""?>>All
</select>

<? if ($graphs != "") $GraphsChecked = "Checked"; else $GraphsChecked = ""; ?>
<td><input type="checkbox" name="graphs" <?=$GraphsChecked?>>Display Graphs</a>

<td>Subnet Filter:<input name=subnet value="<?=isset($subnet)?$subnet:"0.0.0.0/0"?>">
<input type=submit value="Go">
</table>
</FORM>
<?
// Set defaults
if (!isset($interval))
	$interval = DFLT_INTERVAL;

if (!isset($timestamp))
	$timestamp = time() - $interval + (0.05*$interval);

if (!isset($limit))
	$limit = 20;

// Validation
if (!isset($sensor_id))
	exit(0);

$sql = "SELECT sensor_name, interface, sensor_id from sensors where sensor_id = $sensor_id;";
$result = @pg_query($sql);
$r = pg_fetch_array($result);
$sensor_name = $r['sensor_name'];
$interface = $r['interface'];

// Print Title

if (isset($limit))
	echo "<h2>Top $limit - $sensor_name - $interface</h2>";
else
	echo "<h2>All Records - $sensor_name - $interface</h2>";

// Sqlize the incomming variables
if (isset($subnet))
	$sql_subnet = "and ip <<= '$subnet'";

// Sql Statement
$sql = "select tx.ip, rx.scale as rxscale, tx.scale as txscale, tx.total+rx.total as total, tx.total as sent,
rx.total as received, tx.tcp+rx.tcp as tcp, tx.udp+rx.udp as udp,
tx.icmp+rx.icmp as icmp, tx.http+rx.http as http,
tx.mail+rx.mail as mail,
tx.p2p+rx.p2p as p2p, tx.ftp+rx.ftp as ftp
from

(SELECT ip, max(total/sample_duration)*8 as scale, sum(total) as total, sum(tcp) as tcp, sum(udp) as udp, sum(icmp) as icmp,
sum(http) as http, sum(mail) as mail, sum(p2p) as p2p, sum(ftp) as ftp
from sensors, bd_tx_log
where sensors.sensor_id = '$sensor_id'
and sensors.sensor_id = bd_tx_log.sensor_id
$sql_subnet
and timestamp > $timestamp::abstime and timestamp < ".($timestamp+$interval)."::abstime
group by ip) as tx,

(SELECT ip, max(total/sample_duration)*8 as scale, sum(total) as total, sum(tcp) as tcp, sum(udp) as udp, sum(icmp) as icmp,
sum(http) as http, sum(mail) as mail, sum(p2p) as p2p, sum(ftp) as ftp
from sensors, bd_rx_log
where sensors.sensor_id = '$sensor_id'
and sensors.sensor_id = bd_rx_log.sensor_id
$sql_subnet
and timestamp > $timestamp::abstime and timestamp < ".($timestamp+$interval)."::abstime
group by ip) as rx

where tx.ip = rx.ip
order by total desc;";

//echo "</center><pre>$sql</pre><center>"; exit(0);
pg_query("SET sort_mem TO 30000;");

pg_send_query($db, $sql);

$query_start = time();
Echo "<strong>Query In Progress.";
flush();
while (pg_connection_busy($db)) {
	sleep(2);
    Echo ".";
    flush();
    }

echo((time()-$query_start)." seconds </strong>");

$result = pg_get_result($db);

pg_query("set sort_mem to default;");

if ($limit == "all")
	$limit = pg_num_rows($result);

echo "<a name=top><table width=100% border=1 cellspacing=0><tr><td>Ip<td>Name<td>Total<td>Sent<td>Received<td>tcp<td>udp<td>icmp<td>http<td>mail<td>p2p<td>ftp";

if (!isset($subnet)) // Set this now for total graphs
	$subnet = "0.0.0.0/0";

// Output Total Line
if ($graphs == "")
	$url = "<a href=# onclick=\"window.open('details.php?sensor_id=$sensor_id&ip=$subnet','_blank', 'scrollbars=yes,width=930,height=768,resizable=yes,left=20,top=20')\">";
else
	$url = "<a href=#Total>";

echo "\n<TR><TD>".$url."Total</a><TD>$subnet";
foreach (array("total", "sent", "received", "tcp", "udp", "icmp", "http", "mail", "p2p", "ftp") as $key)
	{
	for($Counter=0, $Total = 0; $Counter < pg_num_rows($result); $Counter++)
		{
		$r = pg_fetch_array($result, $Counter);
		$Total += $r[$key];
		}
	echo fmtb($Total);
	}
echo "\n";

// Output Other Lines
for($Counter=0; $Counter < pg_num_rows($result) && $Counter < $limit; $Counter++)
	{
	$r = pg_fetch_array($result, $Counter);
	if ($graphs == "")
		$url = "<a href=# onclick=\"window.open('details.php?sensor_id=$sensor_id&ip=".$r['ip']."','_blank', 'scrollbars=yes,width=930,height=768,resizable=yes,left=20,top=20')\">";
	else
		$url = "<a href=#".$r['ip'].">";
	echo "<tr><td>".$url;
	echo $r['ip']."<td>".gethostbyaddr($r['ip']);
	echo "</a>";
	echo fmtb($r['total']).fmtb($r['sent']).fmtb($r['received']).
		fmtb($r['tcp']).fmtb($r['udp']).fmtb($r['icmp']).fmtb($r['http']).fmtb($r['mail']).
		fmtb($r['p2p']).fmtb($r['ftp'])."\n";
	}
echo "</table></center>";

// Stop here
if ($graphs == "")
	exit();

// Output Total Graph
for($Counter=0, $Total = 0; $Counter < pg_num_rows($result); $Counter++)
	{
	$r = pg_fetch_array($result, $Counter);
	$scale = max($r['txscale'], $scale);
	$scale = max($r['rxscale'], $scale);
	}

if ($subnet == "0.0.0.0/0")
	$total_table = "bd_tx_total_log";
else
	$total_table = "bd_tx_log";

$sn = str_replace("/", "_", $subnet);

echo "<a name=Total><h3><a href=details.php?sensor_id=$sensor_id&ip=$subnet>";
echo "Total - Total of $subnet</h3>";
echo "</a>";
echo "Send:<br><img src=graph.php?ip=$sn&interval=$interval&sensor_id=".$sensor_id."&table=$total_table><br>";
echo "<img src=legend.gif><br>\n";
if ($subnet == "0.0.0.0/0")
	$total_table = "bd_rx_total_log";
else
	$total_table = "bd_rx_log";
echo "Receive:<br><img src=graph.php?ip=$sn&interval=$interval&sensor_id=".$sensor_id."&table=$total_table><br>";
echo "<img src=legend.gif><br>\n";
echo "<a href=#top>[Return to Top]</a>";

// Output Other Graphs
for($Counter=0; $Counter < pg_num_rows($result) && $Counter < $limit; $Counter++)
	{
	$r = pg_fetch_array($result, $Counter);
	echo "<a name=".$r['ip']."><h3><a href=details.php?sensor_id=$sensor_id&ip=".$r['ip'].">";
	if ($r['ip'] == "0.0.0.0")
		echo "Total - Total of all subnets</h3>";
	else
		echo $r['ip']." - ".gethostbyaddr($r['ip'])."</h3>";
	echo "</a>";
	echo "Send:<br><img src=graph.php?ip=".$r['ip']."&interval=$interval&sensor_id=".$sensor_id."&table=bd_tx_log&yscale=".(max($r['txscale'], $r['rxscale']))."><br>";
	echo "<img src=legend.gif><br>\n";
	echo "Receive:<br><img src=graph.php?ip=".$r['ip']."&interval=$interval&sensor_id=".$sensor_id."&table=bd_rx_log&yscale=".(max($r['txscale'], $r['rxscale']))."><br>";
	echo "<img src=legend.gif><br>\n";
	echo "<a href=#top>[Return to Top]</a>";
	}

include('footer.php');
