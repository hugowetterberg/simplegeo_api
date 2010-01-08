<?php
// $Id$

require_once('gmaptile.php');

define('SIMPLEGEOTILES_OFFSET', 268435456);
define('SIMPLEGEOTILES_RADIUS', 85445659.4471); /* $offset / pi() */

function _simplegeo_tileservice_index($z, $x, $y, $layers, $width=1, $height=1) {
  if (is_string($layers)) {
    $layers = explode(',', $layers);
  }
  return _simplegeo_tileservice_serve_tiles($z, $x, $y, $layers, $width, $height);
}

function _simplegeo_tileservice_serve_tiles($zoom, $fromx, $fromy, $layers, $width, $height) {

  // TODO: Implement allowed bounds and zoom check! /Anders
  $allowed_bounds = simple_geo_coordinates_to_array(variable_get('simplegeo_tileservice_allowed_bounds', SIMPLEGEOTILES_ALLOWED_BOUNDS));
  $max_zomm = variable_get('simplegeo_tileservice_max_zoom', SIMPLEGEOTILES_ALLOWED_BOUNDS);
  $min_zoom = variable_get('simplegeo_tileservice_min_zom', SIMPLEGEOTILES_ALLOWED_BOUNDS);

  $json = array();

  $layer_definitions = simplegeo_tileservice_get_layers();

  $tox = $fromx + $width;
  $toy = $fromy + $height;

  $layer_result = array();

  foreach ($layers as $layer) {
    if (isset($layer_definitions[$layer])) {
      $tiles_result = array();
      $layerdef = $layer_definitions[$layer];

      for ($y = $fromy; $y <= $toy; $y++) {
        for ($x = $fromx; $x <= $tox; $x++) {
          $data = simplegeo_tileservice_read_cache($x, $y, $zoom, $layer);
          if (!$data) {
            $data = _simplegeo_tileservice_get_cluster($x, $y, $zoom, $layerdef);
            simplegeo_tileservice_write_cache($x, $y, $zoom, $layer, $data);
          }
          if (!empty($data)) {
            $tiles_result[] = $data;
          }
        }
      }

      $json[] =  $layer . ':[' . join(",", $tiles_result) . ']';
    }
  }

  return array('{'. join(",", $json) .'}');
}


function simplegeo_tileservice_read_cache($x, $y, $zoom, $layer) {
  $sql =  "SELECT data FROM {simplegeo_tileservice_tilecache}
    WHERE x = %d AND y = %d AND zoom = %d AND layer = '%s'";

  $res = db_query($sql, $x, $y, $zoom, $layer);
  if ($obj = db_fetch_object($res)) {
    return $obj->data;
  }

  return NULL;
}

function simplegeo_tileservice_write_cache($x, $y, $zoom, $layer, $data) {
  $cache_row = db_result(db_query("SELECT COUNT(x) FROM {simplegeo_tileservice_tilecache}
    WHERE x = %d AND y = %d AND zoom = %d AND layer = '%s'", $x, $y, $zoom, $layer));

  if ($cache_row) {
    $sql =  "UPDATE {simplegeo_tileservice_tilecache}
      SET data = '%s'
      WHERE x = %d AND y = %d AND zoom = %d AND layer = '%s'";
    return db_query($sql, $data, $x, $y, $zoom, $layer);
  }
  else {
    $sql =  "INSERT INTO {simplegeo_tileservice_tilecache} (x,y, zoom, layer, data)
      VALUES (%d, %d, %d, '%s', '%s')";
    return db_query($sql, $x, $y, $zoom, $layer, $data);
  }
}

function simplegeo_tileservice_clear_cache($x, $y, $zoom, $layer) {
  $sql =  "DELETE FROM {simplegeo_tileservice_tilecache}
    WHERE x = %d AND y = %d AND zoom = %d AND layer = '%s'";

  return db_query($sql, $x, $y, $zoom, $layer);
}

function simplegeo_tileservice_clear_node_cache($type, $position) {
  $layerdef = simplegeo_tileservice_get_layers();

  foreach ($layerdef as $layer => $layer_info ) {
    if ($layer_info['nodetype'] == $type) {
      // Clear the cache for all zoom levels (we currently use 7-17);
      for ($zoom=7; $zoom <= 17; $zoom++) {
        $coords = _simplegeo_tileservice_coord2tile($position, $zoom);
        simplegeo_tileservice_clear_cache($coords->x, $coords->y, $zoom, $layer);
      }
    }
  }
}

function _simplegeo_tileservice_tile2coord($x, $y, $zoom) {
  $gmaptile = GMapTile::fromTileCoord($x, $y, $zoom);

  return array('lat' => $gmaptile->getLat(), 'long' => $gmaptile->getLong());
}
function _simplegeo_tileservice_coord2tile($coords, $zoom) {
  // Switch zoom level (gmaptile users the old 17-0 levels instead of 0-17) TODO: fix this in gmaptile.php
  $zoom = 17 - $zoom;
  $point = simple_geo_coordinates_to_array($coords);
  $tile = new GMapTile($point[0], $point[1], $zoom);
  return $tile->getTileCoord();
}

function _simplegeo_tileservice_get_cluster($x, $y, $zoom, $layer) {
  $type = simplegeo_tileservice_get_layer_type($layer['type']);

  if (isset($type['file'])) {
    require_once($type['file']);
  }

  $data = call_user_func($type['callback'], $x, $y, $zoom, $layer, $type);
  if (!empty($data)) {
    // Specifie the max distance between markers to cluster on
    $distance = 50;
    $cluster = _simplegeo_tileservice_cluster($data, 50, $zoom);
    return json_encode($cluster);
  }
  return NULL;
}

function _simplegeo_tileservice_get_nodes($x, $y, $zoom, $layer, $type) {
  // Switch zoom level (gmaptile users the old 17-0 levels instead of 0-17) TODO: fix this in gmaptile.php
  $zoom = 17 - $zoom;
  $top_left = _simplegeo_tileservice_tile2coord($x, $y, $zoom);
  $bottom_right = _simplegeo_tileservice_tile2coord($x+1, $y+1, $zoom);

  // Build the query.
  $sql = "SELECT n.type, n.nid, AsText(ps.position) AS position
          FROM {node} n
          INNER JOIN {simple_geo_position} ps ON n.nid = ps.nid AND ps.type = 'node' ";

  if(isset($layer['joins'])) {
    $sql .=  implode($layer['joins'], ' ');
  }

  // Define the WHERE part of the query. We first define some defaults.
  $wheres = array(
    'n.status <> 0',
    "Contains(Envelope(GeomFromText('LineString(%s %s,%s %s)')), ps.position)",
  );
  if (isset($layer['nodetype'])) {
    $wheres[] = "n.type = '%s'";
  }

  // Add any WHEREs defined in the layer definition.
  if (is_array($layer['wheres'])) {
    $wheres = array_merge($layer['wheres'], $wheres);
  }

  // If max age is defined check so the node isn't older than the specified age.
  if (isset($layer['max_age'])) {
    $wheres[] = 'n.created >= ' . strtotime('-' . $layer['max_age']);
  }

  // Add the WHEREs to the query.
  $sql .= ' WHERE ' . implode($wheres, ' AND ');

  $sql .= " ORDER BY n.created";

  $params = array($top_left['lat'], $top_left['long'], $bottom_right['lat'], $bottom_right['long']);
  if (isset($layer['nodetype'])) {
    $params[] = $layer['nodetype'];
  }

  $res = db_query($sql, $params);

  $nodes = array();

  while ($node = db_fetch_object($res)) {
    $point = explode(' ', simple_geo_clean_wkt('point', $node->position));
    $nodes[] = array('lat' => (float)$point[0], 'lon' => (float)$point[1], 'count' => 1, 'nid' => (int)$node->nid);

  }

  return $nodes;

}

function simplegeo_tileservice_lon_to_x($lon) {
  return round(SIMPLEGEOTILES_OFFSET + SIMPLEGEOTILES_RADIUS * $lon * pi() / 180);
}

function simplegeo_tileservice_lat_to_y($lat) {
  return round(SIMPLEGEOTILES_OFFSET - SIMPLEGEOTILES_RADIUS *
    log((1 + sin($lat * pi() / 180)) /
    (1 - sin($lat * pi() / 180))) / 2);
}

function simplegeo_tileservice_pixel_distance($lat1, $lon1, $lat2, $lon2, $zoom) {
  $x1 = simplegeo_tileservice_lon_to_x($lon1);
  $y1 = simplegeo_tileservice_lat_to_y($lat1);

  $x2 = simplegeo_tileservice_lon_to_x($lon2);
  $y2 = simplegeo_tileservice_lat_to_y($lat2);

  // Love the use of bitwise op here. A bitwise shift by one doubles a value
  // and that's the principle behind gmaps zoom levels. Each zoom-level divides
  // the each tile into four tiles, thus providing tiles with half the width
  // and height and one fourth of the area.
  return sqrt(pow(($x1-$x2), 2) + pow(($y1-$y2), 2)) >> (21 - $zoom);
}

function  _simplegeo_tileservice_cluster($markers, $distance, $zoom) {
  $clustered = array();
  /* Loop until all markers have been compared. */
  while (count($markers)) {
    $marker  = array_pop($markers);
    $cluster = array();

    /* Compare against all markers which are left. */
    foreach ($markers as $key => $target) {
      $pixels = simplegeo_tileservice_pixel_distance($marker['lat'], $marker['lon'],
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
      $clustered[] = _simplegeo_tileservice_prepare_cluster($cluster);
    }
    else {
      $marker['nid'] = array($marker['nid']);
      $clustered[] = $marker;
    }
  }
  return $clustered;
}

function _simplegeo_tileservice_prepare_cluster($cluster) {
  // Save the postion from the first marker to have something to compare against
  $min_lat = $cluster[0]['lat'];
  $min_lon = $cluster[0]['lon'];
  $max_lat = $cluster[0]['lat'];
  $max_lon = $cluster[0]['lon'];
  $i = 0;
  $max_nodes = variable_get('simplegeo_tileservice_max_nodes', SIMPLEGEOTILES_MAX_NODES);
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

function _simplegeo_tileservice_clear_all_cache($redirect = TRUE) {
  db_query("DELETE FROM {simplegeo_tileservice_tilecache}");
  if ($redirect) {
    drupal_goto('');
  }
}