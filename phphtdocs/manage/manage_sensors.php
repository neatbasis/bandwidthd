<?
include("../include.php");

$db = ConnectDb();

$PHP_SELF = "manage_sensors.php";

if ($HTTP_POST_VARS['submit'] == "Update Locations")
    {
    foreach($HTTP_POST_VARS as $key => $value)
        {
        if (preg_match ("/(^SensorLocation)([0-9]*)$/i", $key, $match))
            {
			$result = pg_query("Select sensor_name from sensors where sensor_id = '$match[2]';");
			$row = pg_fetch_array($result);
			pg_query("UPDATE sensors set location = $value where sensor_name = '".$row['sensor_name']."';");
            }
        }
	header("Location: $PHP_SELF");
	exit(0);
    }

include("manage_header.php");

if ($HTTP_GET_VARS['del_sensor'] != "")
    {
	$result = pg_query("select * from sensors where sensor_name = '".$HTTP_GET_VARS['del_sensor']."';");
	while ($r = @pg_fetch_array($result))
		{
		echo("Please wait while deleting records from tables...<BR>\n");
		$sql = "delete from bd_rx_log where sensor_id = ".$r['sensor_id'];
		echo($sql."<BR>\n");
		flush();
		pg_query($sql);
		$sql = "delete from bd_tx_log where sensor_id = ".$r['sensor_id'];
		echo($sql."<BR>\n");
		flush();
		pg_query($sql);
		$sql = "delete from bd_tx_total_log where sensor_id = ".$r['sensor_id'];
		echo($sql."<BR>\n");
		flush();
		pg_query($sql);
		$sql = "delete from bd_rx_total_log where sensor_id = ".$r['sensor_id'];
		echo($sql."<BR>\n");
		flush();
		pg_query($sql);
		$sql = "delete from links where id1 = ".$r['sensor_id'];
		echo($sql."<BR>\n");
		flush();
		pg_query($sql);
		$sql = "delete from links where id2 = ".$r['sensor_id'];
		echo($sql."<BR>\n");
		flush();
		pg_query($sql);
		$sql = "delete from sensors where sensor_id = ".$r['sensor_id'];
		echo($sql."<BR>\n");
		flush();
		pg_query($sql);
		}
	echo("Done<BR>\n");
    exit(0);
    }

?>

<BR>
<FORM name=UnassignedSensors method=post action=<?=$PHP_SELF?>>
<a name=sensors><h3>Un-assigned sensors</h3>

<TABLE width=100% cellpadding=0 cellspacing=0>
<?
$locations = pg_query("SELECT * from locations");
$sql = "SELECT distinct sensor_name, max(sensor_id) as sensor_id, max(last_connection) as last_connection from sensors where location is null group by sensor_name order by sensor_name ";
$result = pg_query($sql);
if (!$result)
    echo "<center>No un-assigned sensors in database...</center>";
?>
<TR><TH class=row-header-left>&nbsp<TH class=row-header-middle>Sensor Name<TH class=row-header-middle>Assign Location<TH class=row-header-right>Last Checkin
<?
while ($r = @pg_fetch_array($result))
	{
    echo("<TR><TD><a href=$PHP_SELF?del_sensor=".$r['sensor_name']."><img border=0 src=x.gif></a><TD>".$r['sensor_name']."<TD align=center><select name=\"SensorLocation".$r['sensor_id']."\">");
	echo("<option value=\"NULL\" SELECTED>Unknown</option>");
	pg_result_seek($locations, 0);
	while ($location = pg_fetch_array($locations))
		echo("<option value=".$location['id'].">".$location['name']."</option>");
	echo "<TD align=center>".$r['last_connection'];
	}
?>
<TR><TD>&nbsp<TD align=center><INPUT type=submit name=submit value="Update Locations">
</TABLE>
</form>

<BR>
<FORM name=AssignedSensors method=post action=<?=$PHP_SELF?>>
<h3>Assigned sensors</h3>

<TABLE width=100% cellpadding=0 cellspacing=0>
<?
$sql = "SELECT distinct sensor_name, location, max(sensor_id) as sensor_id, max(last_connection) as last_connection from sensors where location is not null group by sensor_name, location order by sensor_name ";
$result = pg_query($sql);
if (!$result)
    echo "<center>No un-assigned sensors in database...</center>";
?>
<TR><TH class=row-header-left>&nbsp<TH class=row-header-middle>Sensor Name<TH class=row-header-middle>Assign Location<TH class=row-header-right>Last Checkin
<?
while ($r = @pg_fetch_array($result))
	{
    echo "<TR><TD><a href=$PHP_SELF?del_sensor=".$r['sensor_name']."><img border=0 src=x.gif></a><TD>".$r['sensor_name']."<TD align=center><select name=\"SensorLocation".$r['sensor_id']."\">";
	echo("<option value=\"NULL\">Unknown</option>");
	pg_result_seek($locations, 0);
	while ($location = pg_fetch_array($locations))
		{
		if ($location['id'] == $r['location'])
			$selected = "SELECTED";
		else
			$selected = "";

		echo("<option value=".$location['id']." $selected>".$location['name']."</option>");
		}
	echo "<TD align=center>".$r['last_connection'];
	}
?>
<TR><TD>&nbsp<TD align=center><INPUT type=submit name=submit value="Update Locations">
</TABLE>
</form>
</BODY>