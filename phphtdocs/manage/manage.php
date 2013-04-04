<?
include("../include.php");

$db = ConnectDb();

if ($HTTP_POST_VARS['submit'] == "Update Locations")
    {
	$name = array();
	$latitude = array();
	$longitude = array();
	$group = array();

    foreach($HTTP_POST_VARS as $key => $value)
        {
        if (preg_match ("/(^LocationGroup)([0-9]*)$/i", $key, $match))
            {
			$group[$match[2]] = $value;
            }
        elseif (preg_match ("/(^LocationLatitude)([0-9]*)$/i", $key, $match))
            {
			$latitude[$match[2]] = $value;
            }
        elseif (preg_match ("/(^LocationLongitude)([0-9]*)$/i", $key, $match))
            {
			$longitude[$match[2]] = $value;
            }
        elseif (preg_match ("/(^LocationName)([0-9]*)$/i", $key, $match))
            {
			$name[$match[2]] = $value;
            }
        }

	foreach($name as $key => $value)
		{
        pg_query("UPDATE locations set name = '".$name[$key]."', group_id = ".$group[$key].", latitude = '".$latitude[$key]."', longitude = '".$longitude[$key]."' where id = '".$key."';");
		}

	pg_query("vacuum locations;");
    header("Location: manage.php");
    exit(0);
    }

if ($HTTP_POST_VARS['submit'] == "Add Location")
    {
	pg_query("INSERT INTO locations (name, group_id, longitude, latitude) values ('".$HTTP_POST_VARS['name']."', ".$HTTP_POST_VARS['group_id'].", '".$HTTP_POST_VARS['longitude']."', '".$HTTP_POST_VARS['latitude']."');"); 
	header("Location: manage.php");
	exit(0);
    }

if ($HTTP_POST_VARS['submit'] == "Add to Ignored Links")
	{
	pg_query("INSERT INTO links_ignorelist (sensor_id) SELECT sensor_id from sensors where sensor_name like '".$HTTP_POST_VARS['sensor_name']."%' and interface = '".$HTTP_POST_VARS['interface']."';");
	header("Location: manage.php");
	exit(0);
	}

include("manage_header.php");

?>
<a name=locations><h3>Locations</h3>

<TABLE width=800 cellpadding=0 cellspacing=0>
<FORM name=UpdateLocations method=post action=<?=$PHP_SELF?>>
<TR><TH class=row-header-left>Name<TH class=row-header-middle>Color<TH class=row-header-middle>Longitude<TH class=row-header-right>Latitude
<?
// NOTE: Used several times below
$groups = pg_query("SELECT * from groups");
// NOTE: This result set is used several times below
$locations = pg_query("SELECT * from locations");
if (!$locations)
    echo "<TR><TD>No locations in database...</center>";

while ($r = @pg_fetch_array($locations))
	{
	echo("<TR><TD><input name=LocationName".$r['id']." value=\"".$r['name']."\" size=32>");
	pg_result_seek($groups, 0);
	echo("<TD align=center><select name=LocationGroup".$r['id'].">");
	while ($g = @pg_fetch_array($groups))
		{
		($r['group_id'] == $g['group_id']) ? $Selected = "SELECTED" : $Selected = "";
		echo("<option value=".$g['group_id']." $Selected>".$g['name']."</option>");
		}
	echo("</select>");
	echo("<TD align=center><input name=LocationLongitude".$r['id']." value=\"".$r['longitude']."\">");
	echo("<TD align=center><input name=LocationLatitude".$r['id']." value=\"".$r['latitude']."\">");
	}	
?>
<TR><TD><input type=submit name=submit value="Update Locations">
</FORM>
<TR><TD><FORM name=AddLocation method=post action=<?=$PHP_SELF?>>
<INPUT name=name>
<?
echo("<TD align=center><select name=group_id>");
pg_result_seek($groups, 0);
while ($g = @pg_fetch_array($groups))
	{
	echo("<option value=".$g['group_id'].">".$g['name']."</option>");
	}
echo("</select>");
?>
<TD align=center><INPUT name=longitude><TD align=center><INPUT name=latitude>
<TR><TD><input type=submit name=submit value="Add Location"></FORM>
</table>

<a name=links><h3>Links</h3>
<FORM name=UpdateLinks method=post action=<?=$PHP_SELF?>>
<TABLE width=100% cellpadding=0 cellspacing=0>
<TR><TH class=row-header-left>First Tower<TH class=row-header-middle>Second Tower<TH class=row-header-middle>First Router
<TH class=row-header-middle>Second Router<TH class=row-header-right>Last Update
<?
// NOTE: This result set is used several times below
$links = pg_query("
SELECT loc_a.name as loc_a_name, loc_b.name as loc_b_name, sens_a.sensor_id as sens_a_id, sens_a.sensor_name as sens_a_name, 
	sens_a.interface as sens_a_interface, sens_b.sensor_id as sens_b_id, sens_b.sensor_name as sens_b_name, 
	sens_b.interface as sens_b_interface, to_char(last_update, 'HH24:MI Mon, DD YY') as last_update 
FROM links, sensors as sens_a, sensors as sens_b, locations as loc_a, locations as loc_b 
WHERE id1 not in (select * from links_ignorelist ) and id2 not in (select * from links_ignorelist ) and 
	sens_a.location != sens_b.location and id1 = sens_a.sensor_id and id2 = sens_b.sensor_id and sens_a.location = loc_a.id 
	and sens_b.location = loc_b.id order by loc_a.name;");
if (!$links)
    echo "<TR><TD>No links in database...</center>";

while ($r = @pg_fetch_array($links))
	{
	echo("<TR><TD>".$r['loc_a_name']."<TD>".$r['loc_b_name']);
	echo("<TD>".ereg_replace("\..*$", "", $r['sens_a_name'])." - ".$r['sens_a_interface']);
	echo("<TD>".ereg_replace("\..*$", "", $r['sens_b_name'])." - ".$r['sens_b_interface']);
	echo("<TD>".$r['last_update']);
	}	
?>
<TR><TD>&nbsp<TD align=center><input type=submit name=submit value="Update Links">
</table>
</FORM>

<BR>
<h3>Hide Links To</h3>
<FORM name=HideLinks method=post action=<?=$PHP_SELF?>>
<TABLE width=100% cellpadding=0 cellspacing=0>
<TR><TH class=row-header-left>Sensor Name<TH class=row-header-middle>Interface<TH class=row-header-middle>Far Location<TH class=row-header-right>Far Router
<?
// NOTE: This result set is used several times below
$res = pg_query("select sensor_name, interface, sensors.sensor_id from links_ignorelist, sensors where links_ignorelist.sensor_id = sensors.sensor_id;");
if (!$res)
    echo "<TR><TD>No links in database...</center>";

while ($r = @pg_fetch_array($res))
	{
	echo("<TR><TD>".$r['sensor_name']."<TD>".$r['interface']);
	$res2 = pg_query("select locations.name as loc_name, sensor_name from links, locations, sensors where id2 = ".$r['sensor_id']." and id1 = sensor_id and location = locations.id;");		
	while ($r2 = @pg_fetch_array($res2))
		{
		echo("<TR><TD>&nbsp<TD><TD>".$r2['loc_name']."<TD>".$r2['sensor_name']);
		}
	$res2 = pg_query("select locations.name as loc_name, sensor_name from links, locations, sensors where id1 = ".$r['sensor_id']." and id2 = sensor_id and location = locations.id;");		
	while ($r2 = @pg_fetch_array($res2))
		{
		echo("<TR><TD>&nbsp<TD><TD>".$r2['loc_name']."<TD>".$r2['sensor_name']);
		}

	}	
?>
<TR><TD><INPUT name=sensor_name><TD align=center><INPUT name=interface>
<TR><TD><input type=submit name=submit value="Add to Ignored Links">
</table>
</FORM>

</BODY>