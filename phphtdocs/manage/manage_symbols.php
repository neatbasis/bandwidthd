<?
include("../include.php");

$db = ConnectDb();

$PHP_SELF = "manage_symbols.php";

if ($HTTP_POST_VARS['submit'] == "Add Slice")
	{
	pg_query("INSERT INTO slices (sensor_id, direction, angle, radius) SELECT sensor_id, ".$HTTP_POST_VARS['direction']." as direction, ".$HTTP_POST_VARS['angle']." as angle, ".$HTTP_POST_VARS['radius']." as radius from sensors where sensor_name like '".$HTTP_POST_VARS['sensor_name']."%' and interface = '".$HTTP_POST_VARS['interface']."';");
	header("Location: ".$PHP_SELF);
	exit(0);
	}

if ($HTTP_GET_VARS['del_slice'] != "")
	{
	pg_query("delete from slices where id = ".$HTTP_GET_VARS['del_slice'].";");
	header("Location: ".$PHP_SELF);
	exit(0);	
	}

include("manage_header.php");

?>
<BR>
<h3>Pie Slices</h3>
<FORM name=slices method=post action=<?=$PHP_SELF?>>
<TABLE width=100% cellpadding=0 cellspacing=0>
<TR><TH class=row-header-left>&nbsp<TH class=row-header-middle>Sensor Name<TH class=row-header-middle>Interface<TH class=row-header-middle>Direction<TH class=row-header-middle>Angle<TH class=row-header-right>Radius
<?
// NOTE: This result set is used several times below
$res = pg_query("select id, sensor_name, interface, slices.sensor_id, direction, angle, radius from slices, sensors where slices.sensor_id = sensors.sensor_id order by sensor_name;");
if (!$res)
    echo "<TR><TD>No links in database...</center>";

while ($r = @pg_fetch_array($res))
	{
	echo("<TR><TD><a href=$PHP_SELF?del_slice=".$r['id']."><img border=0 src=x.gif></a><TD>".$r['sensor_name']."<TD>".$r['interface']."<TD>".$r['direction']."<TD>".$r['angle']."<TD>".$r['radius']);
	}	
?>
<TR><TD>&nbsp<TD><INPUT name=sensor_name><TD><INPUT name=interface><TD><INPUT name=direction><TD><INPUT name=angle><TD><INPUT name=radius>
<TR><TD><TD>&nbsp<input type=submit name=submit value="Add Slice">
</table>
</FORM>
</BODY>