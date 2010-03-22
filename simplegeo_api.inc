<?php
require_once('lib/gmaptile.php');

function simplegeo_api_lon_to_x($lon) {
  return round(SIMPLEGEOTILES_OFFSET + SIMPLEGEOTILES_RADIUS * $lon * pi() / 180);
}

function simplegeo_api_lat_to_y($lat) {
  return round(SIMPLEGEOTILES_OFFSET - SIMPLEGEOTILES_RADIUS *
    log((1 + sin($lat * pi() / 180)) /
    (1 - sin($lat * pi() / 180))) / 2);
}

function simplegeo_api_pixel_distance($lat1, $lon1, $lat2, $lon2, $zoom) {
  $x1 = simplegeo_api_lon_to_x($lon1);
  $y1 = simplegeo_api_lat_to_y($lat1);

  $x2 = simplegeo_api_lon_to_x($lon2);
  $y2 = simplegeo_api_lat_to_y($lat2);

  // Love the use of bitwise op here. A bitwise shift by one doubles a value
  // and that's the principle behind gmaps zoom levels. Each zoom-level divides
  // the each tile into four tiles, thus providing tiles with half the width
  // and height and one fourth of the area.
  return sqrt(pow(($x1-$x2), 2) + pow(($y1-$y2), 2)) >> (21 - $zoom);
}

function simplegeo_api_cluster($markers, $distance, $zoom) {
  $clustered = array();
  /* Loop until all markers have been compared. */
  while (count($markers)) {
    $marker  = array_pop($markers);
    $cluster = array();

    /* Compare against all markers which are left. */
    foreach ($markers as $key => $target) {
      $pixels = simplegeo_api_pixel_distance($marker['lat'], $marker['lon'],
        $target['lat'], $target['lon'],
        $zoom);
      /* If two markers are closer than given distance remove */
      /* target marker from array and add it to cluster.      */
      if ($distance > $pixels) {
        unset($markers[$key]);
        $cluster[] = $target;
      }
    }

    /* If a marker has been added to cluster, add also the one  */
    /* we were comparing to and remove the original from array. */
    if (count($cluster) > 0) {
      $cluster[] = $marker;
      $clustered[] = _simplegeo_api_prepare_cluster($cluster);
    }
    else {
      $marker['nid'] = array($marker['nid']);
      $clustered[] = $marker;
    }
  }
  return $clustered;
}

function _simplegeo_api_prepare_cluster($cluster) {
  // Save the postion from the first marker to have something to compare against
  $min_lat = $cluster[0]['lat'];
  $min_lon = $cluster[0]['lon'];
  $max_lat = $cluster[0]['lat'];
  $max_lon = $cluster[0]['lon'];
  $i = 0;
  $max_nodes = variable_get('simplegeo_api_max_nodes', SIMPLEGEO_API_MAX_NODE_INFO);
  $nids = array();

  foreach ($cluster as $marker) {
    if ($marker['lat'] < $min_lat) {
      $min_lat = $marker['lat'];
    }
    else if ($marker['lat'] > $max_lat) {
      $max_lat = $marker['lat'];
    }

    if ($marker['lon'] < $min_lon) {
      $min_lon = $marker['lon'];
    }
    else if ($marker['lon'] > $max_lon) {
      $max_lon = $marker['lon'];
    }
    // Only save the nodes ids up to max;
    if ($i < $max_nodes) {
      $nids[] = $marker['nid'];
    }
    $i++;
  }
  // Calculate center of cluster
  $center_lat = ($max_lat + $min_lat) / 2;
  $center_lon = ($max_lon + $min_lon) / 2;

  return array('lat' => $center_lat, 'lon' => $center_lon, 'count' => count($cluster), 'nid' => $nids, 'NW' => array($max_lat, $min_lon), 'SE' => array($min_lat, $max_lon));
}

function simplegeo_api_tile2coord($x, $y, $zoom) {
  $gmaptile = GMapTile::fromTileCoord($x, $y, $zoom);

  return array('lat' => $gmaptile->getLat(), 'long' => $gmaptile->getLong());
}

function simplegeo_api_coord2tile($coords, $zoom) {
  // Switch zoom level (gmaptile users the old 17-0 levels instead of 0-17) TODO: fix this in gmaptile.php
  $zoom = 17 - $zoom;
  $point = simple_geo_coordinates_to_array($coords);
  $tile = new GMapTile($point[0], $point[1], $zoom);
  return $tile->getTileCoord();
}
