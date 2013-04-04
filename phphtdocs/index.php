<?
include("include.php");

if (isset($_GET['group_id']))
	$group_id = $_GET['group_id'];

$db = ConnectDb();
?>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:v="urn:schemas-microsoft-com:vml">
<head>
<style type="text/css">
v\:* {
  behavior:url(#default#VML);
  }
</style>
<title>DerbyTech Wireless Network</title>
<script src="http://maps.google.com/maps?file=api&v=1&key=ABQIAAAANV8M8s128eP55wUqCqvWyBQVMYhLIeywhJ0puM4BK8Tp-d4myhTvfxey30oeLPqs-t72AGpR7hBa9w" type="text/javascript"></script>
<link href="bandwidthd.css" rel="stylesheet" type="text/css">
<script type="text/javascript">
//<![CDATA[

var request;  	// XML Request
var map; 		// Our google map

function drawCircle(map,lng,lat,radius) 
	{
	var color = '#0000ff';    // color blue
	var width = 1;             // width pixels
	var d2r = Math.PI/180;   // degrees to radians
	var r2d = 180/Math.PI;   // radians to degrees
	var Clat = (radius/3963)*r2d;   //  using 3963 as earth's radius
	
	var Clng = Clat/Math.cos(lat*d2r);
   	var Cpoints = [];
   	for (var i=0; i < 65; i++) 
		{
		var theta = Math.PI * (i/32);
      	Cx = lng + (Clng * Math.cos(theta));
      	Cy = lat + (Clat * Math.sin(theta));
      	Cpoints.push(new GPoint(Cx,Cy));
   		};
   	map.addOverlay(new GPolyline(Cpoints,color,width));
	}

function drawSlice(map,lng,lat,orientation,degrees,radius) 
	{
	if (degrees == 360)
		{
		drawCircle(map,lng,lat,radius);
		return;
		}

	var color = '#0000ff';    // color blue
	var width = 1;             // width pixels

	var d2r = Math.PI/180;   // degrees to radians
	var r2d = 180/Math.PI;   // radians to degrees

	var Clat = (radius/3963)*r2d;   //  using 3963 as earth's radius	
	var Clng = Clat/Math.cos(lat*d2r);

	var start = (90-orientation-degrees/2)*d2r;
	var end = (90-orientation+degrees/2)*d2r;

   	var Cpoints = [];
	Cpoints.push(new GPoint(lng,lat));

	theta = 1/64;
	for (var deg=start; deg < end; deg += theta)
		{
		Cx = lng + (Clng * Math.cos(deg));
		Cy = lat + (Clat * Math.sin(deg));
		Cpoints.push(new GPoint(Cx,Cy));
		}

	deg = end;
	Cx = lng + (Clng * Math.cos(deg));
    Cy = lat + (Clat * Math.sin(deg));
    Cpoints.push(new GPoint(Cx,Cy));

	Cpoints.push(new GPoint(lng,lat));

   	map.addOverlay(new GPolyline(Cpoints,color,width));
	}

// Creates a marker whith tower info
function createTower(point, text, icon) 
	{
 	var marker = new GMarker(point, icon);

  	// Show this marker's index in the info window when it is clicked
  	GEvent.addListener(marker, "click", function() { marker.openInfoWindowHtml(text); });

  	return marker;
	}

var polylines = new Array();

<?
if (isset($group_id))
	$xml_url = "xml.php?group_id=$group_id";
else
	$xml_url = "xml.php";
?>

function LoadLines()
	{
    request = GXmlHttp.create();
    request.open("GET", "<?=$xml_url?>", true);
    request.onreadystatechange = ProcessXml;
    request.send(null);
	}

function ProcessXml() 
		{
	  	if (request.readyState == 4) 
			{
			if (request.status == 200) 
				{
				while(polylines.length > 0)
					{
					polyline = polylines.pop();
					map.removeOverlay(polyline);
					delete polyline;
					}

	 		   	//var xmlDoc = request.responseXML;
    			//var links = xmlDoc.documentElement.getElementsByTagName("link");
				var links = request.responseXML.getElementsByTagName("link");
    			for (var i = 0; i < links.length; i++) 
					{
					a_lng = parseFloat(links[i].getAttribute("a_lng"));
					a_lat = parseFloat(links[i].getAttribute("a_lat"));
					b_lng = parseFloat(links[i].getAttribute("b_lng"));
					b_lat = parseFloat(links[i].getAttribute("b_lat"));
					color = links[i].getAttribute("color");				
					var polyline = new GPolyline([new GPoint(a_lng, a_lat), new GPoint(b_lng, b_lat)], color, 5);
					map.addOverlay(polyline);
					polylines.push(polyline);
    				}
				}
			else
				alert("There was a problem retrieving the XML data:\n" + request.statusText);

			window.setTimeout('LoadLines()', 120000);
  			}
		}

function OnLoad() 
	{
	if (parseInt(navigator.appVersion)>3) 
		{
	 	if (navigator.appName=="Netscape") 
			{
			document.getElementById("map").style.height = (window.innerHeight-75)+"px";
	 		}
 		if (navigator.appName.indexOf("Microsoft")!=-1) 
			{
			document.getElementById("map").style.height = (document.body.document.documentElement.clientHeight-90)+"px";
			}
		}
	
	//if (!GBrowserIsCompatible()) 
	//	return;
	//var pntDerbyTech = new GPoint(-90.44474244117737, 41.5146750885744);
	//var pntDerbyTech = new GPoint(-90.44466733932495, 41.51341380076779);

	// Build Icon
	var icon_base = new GIcon();
	icon_base.shadow = "http://labs.google.com/ridefinder/images/mm_20_shadow.png";
	icon_base.iconSize = new GSize(12, 20);
	icon_base.shadowSize = new GSize(22, 20);
	icon_base.iconAnchor = new GPoint(6, 20);
	icon_base.infoWindowAnchor = new GPoint(5, 1);

	icon_green = new GIcon(icon_base);
	icon_green.image = "http://labs.google.com/ridefinder/images/mm_20_green.png";

	icon_blue = new GIcon(icon_base);
	icon_blue.image = "http://labs.google.com/ridefinder/images/mm_20_blue.png";

	icon_red = new GIcon(icon_base);
	icon_red.image = "http://labs.google.com/ridefinder/images/mm_20_red.png";
	
	// Display Map

	map = new GMap(document.getElementById("map"));
	//map.setMapType(G_SATELLITE_TYPE)
	map.addControl(new GLargeMapControl());
	map.addControl(new GMapTypeControl());
	map.centerAndZoom(new GPoint(<?=ZOOM_LONG?>, <?=ZOOM_LAT?>), <?=ZOOM_LEVEL?>);

	LoadLines();

// Displays Towers
<?
$sql = "SELECT locations.*, groups.color from locations, groups where locations.group_id = groups.group_id";
if (isset($group_id))
	$sql .= " and locations.group_id = $group_id";

$locations = pg_query($sql);

while ($r = @pg_fetch_array($locations))
    {
	$Statistics = "window.open(\'location_statistics.php?location_id=".$r['id']."\',\'statistics\',\'scrollbars=yes,height=\'+screen.height+\',width=550,resizable=yes,top=0 \')"; 
	$html = $r['name']."<BR><a href=# onclick=\"$Statistics\">Statistics</a><BR>Management<BR>";

	$sensors = pg_query("select distinct on (sensor_name) sensor_name, management_url from sensors where location = ".$r['id'].";");
	while ($sensor = @pg_fetch_array($sensors))
		{
		//$html .= "<a href=".$sensor['management_url'].">X</a> - <a href=# onclick=\"window.open(\'".$sensor['management_url']."\')\">".$sensor['sensor_name']."</a><BR>";
		$html .= "<a target=".$sensor['management_url']." href=".$sensor['management_url'].">".$sensor['sensor_name']."</a><BR>";
		}
	$html .= "</TABLE>";
	echo("map.addOverlay(createTower(new GPoint(".$r['longitude'].", ".$r['latitude']."), '$html', icon_".$r['color']."));\n");
    }

// Display Coverage Slices
$sql = "SELECT longitude, latitude, direction, angle, radius from slices, sensors, locations where slices.sensor_id = sensors.sensor_id and sensors.location = locations.id";
if (isset($group_id))
    $sql .= " and locations.group_id = $group_id";

$res = pg_query($sql);

while ($r = @pg_fetch_array($res))
    {
	echo("drawSlice(map, ".$r['longitude'].", ".$r['latitude'].", ".$r['direction'].", ".$r['angle'].", ".$r['radius'].");\n");	
	}

// Implement Search
if (isset($_GET['search']))
	{
	//$locations = pg_query("SELECT id, name, longitude, latitude, count(sensors.sensor_name) from sensors, locations where sensors.location = locations.id and sensor_name ~* '".$_GET['search']."' group by id, name, longitude, latitude order by count desc limit 1;");
	// Very wierd query... Pulls all towers that have sensors who's name matches the query and pulls towers who's name matches the query and artificially ranks the towers very high, right now limit 1 but in future can be used to create a weighted list of matches
	$locations = pg_query("SELECT id, name, longitude, latitude, count(sensors.sensor_name) from sensors, locations where locations.id = sensors.location and sensor_name ~* '".$_GET['search']."' or name ~* '".$_GET['search']."' group by id, name, longitude, latitude order by count desc limit 1;");
	while ($r = @pg_fetch_array($locations))
	    {
		$Statistics = "window.open(\'location_statistics.php?location_id=".$r['id']."\',\'statistics\',\'scrollbars=yes,height=900,width=550\')"; 
		$html = $r['name']."<BR><a href=# onclick=\"$Statistics\">Statistics</a><BR>Management<BR>";

		$sensors = pg_query("select distinct on (sensor_name) sensor_name, management_url from sensors where location = ".$r['id'].";");
		while ($sensor = @pg_fetch_array($sensors))
			{
			$html .= "<a href=# onclick=\"window.open(\'".$sensor['management_url']."\')\">".$sensor['sensor_name']."</a><BR>";
			}
		echo("map.centerAndZoom(new GPoint(".$r['longitude'].", ".$r['latitude']."), 7);\n");
		echo("var marker;\n");
		echo("marker = createTower(new GPoint(".$r['longitude'].", ".$r['latitude']."), '$html', icon_green);\n");
		echo("map.addOverlay(marker);\n");
		echo("marker.openInfoWindowHtml('$html');\n");
   	 	}	
	}
	
?>
	}



                          
//]]></script>

</head>
<body onload="OnLoad()">
<FORM name=search method=get action=<?=$_SERVER['PHP_SELF']?>>

<?
$locations = pg_query("SELECT count(name) as num from locations");
$r = @pg_fetch_array($locations);
echo("Monitoring: ".$r['num']." Towers");

$locations = pg_query("SELECT count(sensor_name) as num from (select distinct sensor_name from sensors) as sens;");
$r = @pg_fetch_array($locations);
echo(" ".$r['num']." Routers");

$locations = pg_query("SELECT count(interface) as num from sensors;");
$r = @pg_fetch_array($locations);
echo(" ".$r['num']." Interfaces");

$locations = pg_query("SELECT count(id1) as num from links;");
$r = @pg_fetch_array($locations);
echo(" ".$r['num']." Links");

?><BR>
<a href=# onclick="document.search.submit()" class=text-button>Search</a> &nbsp <INPUT name=search size=50 value="<?=$_GET['search']?>"><br>
<a href=# onclick="window.open('sensors.php', 'sensors')" class=text-button>Usage Reports</a>
<a href=# onclick="window.open('failures.php', 'failures')" class=text-button>Failure Report</a>
<a href=# onclick="window.open('uptime.php', 'uptime')" class=text-button>Uptime Report</a>
<a href=# onclick="window.open('manage/manage.php', 'manage')" class=text-button>Management</a>
<a href=<?=$_SERVER['PHP_SELF']?> class=text-button>All Towers</a>
<?
$locations = pg_query("SELECT * from groups");
                                                                                                                                               
while ($r = @pg_fetch_array($locations))
    {
	echo("<a href=".$_SERVER['PHP_SELF']."?group_id=".$r['group_id']." class=text-button color=".$r['color'].">".$r['name']."</a> ");
	}
?>


<center><div id="map" style="width: 100%; height: 100px"></div>
</body>
</html>
