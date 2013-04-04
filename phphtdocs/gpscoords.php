<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:v="urn:schemas-microsoft-com:vml">
<head>
<style type="text/css">
v\:* {
  behavior:url(#default#VML);
  }
</style>
<script src="http://maps.google.com/maps?file=api&v=1&key=ABQIAAAANV8M8s128eP55wUqCqvWyBQVMYhLIeywhJ0puM4BK8Tp-d4myhTvfxey30oeLPqs-t72AGpR7hBa9w" type="text/javascript"></script>


<script type="text/javascript">
//<![CDATA[
function OnLoad()
    {

	if (GBrowserIsCompatible()) {

	var pntDerbyTech = new GPoint(-90.44445276260376, 41.51460278611576);

	// Display Map

	var map = new GMap(document.getElementById("map"));
	map.addControl(new GSmallMapControl());
	map.addControl(new GMapTypeControl());
	map.centerAndZoom(pntDerbyTech, 4);

	GEvent.addListener(map, 'click', function(overlay, point) {
  		if (overlay) {
    		map.removeOverlay(overlay);
  		} else if (point) {
			marker = new GMarker(point);
    		map.addOverlay(marker);
			marker.openInfoWindowHtml("GPS: "+point);
  		}
		});
	}

// End Compatible
}

//]]></script>

</head>

<body onload="OnLoad()">
<center><div id="map" style="width: 950px; height: 500px"></div>
</body>
</html>
