<?
include("include.php");
header("Content-Type: text/xml");

if (isset($_GET['group_id']))
    $group_id = $_GET['group_id'];

                                                                                                                                               
$db = ConnectDb();

echo("<links>\n");
// Display links

$sql = "
SELECT distinct on (a_long, a_lat, b_long, b_lat) 
loc_a.longitude as a_long, loc_a.latitude as a_lat, loc_b.longitude as b_long, loc_b.latitude as b_lat, id1, 
sens_a.sensor_name, sens_b.sensor_name, extract (epoch from (now() - last_update))/60 as age from links, sensors as sens_a, sensors as sens_b, locations as loc_a, 
locations as loc_b 
WHERE id1 not in (select * from links_ignorelist ) and id2 not in (select * from links_ignorelist ) 
and sens_a.location != sens_b.location and id1 = sens_a.sensor_id and id2 = sens_b.sensor_id 
and sens_a.location = loc_a.id and sens_b.location = loc_b.id";

if (isset($group_id))
    $sql .= " and (loc_a.group_id = $group_id or loc_b.group_id = $group_id)";
                                                                                                                                               
$links = pg_query($sql);

$lnk_colors[0] = "0000FF"; // Bright Blue
$lnk_colors[1] = "7700FF"; // Violet
$lnk_colors[2] = "CC00CC"; // Purple
$lnk_colors[3] = "FF6600"; // Orange
$lnk_colors[4] = "FF0000"; // Red

while ($link = @pg_fetch_array($links))
	{
	$rate = 0;
	if ($link['age'] < 10)
		{
		$res = pg_query("SELECT total, sample_duration from bd_rx_total_log 
							where sensor_id = ".$link['id1']." and timestamp > now()-interval '20 minutes' 
							order by timestamp desc limit 1;");
		if ($res && pg_num_rows($res) == 1)
			{
			$r = @pg_fetch_array($res);
			$rate += ($r['total']*8)/$r['sample_duration'];
			}

		$res = pg_query("SELECT total, sample_duration from bd_tx_total_log 
							where sensor_id = ".$link['id1']." and timestamp > now()-interval '20 minutes' 
							order by timestamp desc limit 1;");
		if ($res && pg_num_rows($res) == 1)
			{
			$r = @pg_fetch_array($res);
			$rate += ($r['total']*8)/$r['sample_duration'];
			}

		if ($rate < 256)
			$color = $lnk_colors[0];		
		elseif ($rate < 1500)
			$color = $lnk_colors[1];
		elseif ($rate < 3000)
			$color = $lnk_colors[2];
		elseif ($rate < 4500)
			$color = $lnk_colors[3];
		else 
			$color = $lnk_colors[4];
		//$color = str_pad(dechex($red), 2, '0', STR_PAD_LEFT).str_pad(dechex($green), 2, '0', STR_PAD_LEFT)."00";
		}
	else
		$color = "000000";
	echo("<link a_lat=\"".$link['a_lat']."\" a_lng=\"".$link['a_long']."\" b_lat=\"".$link['b_lat']."\" b_lng=\"".$link['b_long']."\" color=\"#".$color."\"/>\n");
	}
echo("</links>\n");
?>
