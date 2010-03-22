<?php
// $Id$

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

  $layer_definitions = simplegeo_tileservice_layer_load_all();

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
      foreach ($tiles_result as $r) {
        $layer_result[$layer][] = json_decode($r);
      }
    }
  }
  return $layer_result;
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
  $layerdef = simplegeo_tileservice_layer_load_all();

  foreach ($layerdef as $layer => $layer_info ) {
    if ($layer_info->layer_type == 'node_sql' && isset($layer_info->conf['node_type']) && $layer_info->conf['node_type'] == $type) {
      // Clear the cache for all zoom levels (we currently use 7-17);
      for ($zoom=7; $zoom <= 17; $zoom++) {
        $coords = _simplegeo_tileservice_coord2tile($position, $zoom);
        simplegeo_tileservice_clear_cache($coords->x, $coords->y, $zoom, $layer);
      }
    }
  }
}

function _simplegeo_tileservice_tile2coord($x, $y, $zoom) {
  module_load_include('inc', 'simplegeo_api');
  $gmaptile = GMapTile::fromTileCoord($x, $y, $zoom);

  return array('lat' => $gmaptile->getLat(), 'long' => $gmaptile->getLong());
}

function _simplegeo_tileservice_coord2tile($coords, $zoom) {
  module_load_include('inc', 'simplegeo_api');
  // Switch zoom level (gmaptile users the old 17-0 levels instead of 0-17) TODO: fix this in gmaptile.php
  $zoom = 17 - $zoom;
  $point = simple_geo_coordinates_to_array($coords);
  $tile = new GMapTile($point[0], $point[1], $zoom);
  return $tile->getTileCoord();
}

function _simplegeo_tileservice_get_cluster($x, $y, $zoom, $layer_definition) {
  module_load_include('inc', 'simplegeo_api');
  $type = simplegeo_tileservice_get_layer_type($layer_definition->layer_type);

  if (isset($type['file'])) {
    require_once($type['file']);
  }

  $data = call_user_func($type['load callback'], $x, $y, $zoom, $layer_definition, $type);
  if (!empty($data)) {
    // Specifie the max distance between markers to cluster on
    $distance = 50;
    $cluster = simplegeo_api_cluster($data, 50, $zoom);
    return json_encode($cluster);
  }
  return NULL;
}

function _simplegeo_tileservice_get_nodes($x, $y, $zoom, $layer, $type) {
  // Switch zoom level (gmaptile users the old 17-0 levels instead of 0-17) TODO: fix this in gmaptile.php
  $zoom = 17 - $zoom;
  $top_left = _simplegeo_tileservice_tile2coord($x, $y, $zoom);
  $bottom_right = _simplegeo_tileservice_tile2coord($x+1, $y+1, $zoom);
  $nodes = array();
  // Use views as query builder if a view is specified.
  if (module_exists('views') && $layer->conf['view'] && $view = views_get_view($layer->conf['view'])) {
    // Sanity check; make sure the user added the bounding_box argument to the view.
    // TODO: Add support for other displays than "default"?.
    $argument_setting = $view->display['default']->display_options['arguments'];
    if (is_array($argument_setting)) {
      $first_argument_setting = current($argument_setting);
      if ($first_argument_setting['id'] == 'bounding_box') {
        // Create the string expected by the bounding box argument.
        $box = $top_left['lat'] . ',' . $top_left['long'] . ',' . $bottom_right['lat'] . ',' . $bottom_right['long'];
        $view->set_arguments(array($box));
        $view->execute();
        foreach ($view->result as $node) {
          $point = explode(' ', simple_geo_clean_wkt('point', $node->simple_geo_point));
          $nodes[] = array('lat' => (float)$point[0], 'lon' => (float)$point[1], 'count' => 1, 'nid' => (int)$node->nid);
        }
      }
    }
  }
  // Build our own query based on layer settings.
  else {
    $sql = "SELECT n.nid, AsText(ps.position) AS simple_geo_point
            FROM {node} n
            INNER JOIN {simple_geo_position} ps ON n.nid = ps.nid AND ps.type = 'node' ";

    // Define the WHERE part of the query. We first define some defaults.
    $wheres = array(
      'n.status <> 0',
      "Contains(Envelope(GeomFromText('LineString(%s %s,%s %s)')), ps.position)",
    );
    if (!empty($layer->conf['node_type'])) {
      $wheres[] = "n.type = '%s'";
    }

    // If max age is defined check so the node isn't older than the specified age.
    if (!empty($layer->conf['max_age'])) {
      $wheres[] = 'n.created >= ' . strtotime('-' . $layer->conf['max_age']);
    }

    // If update since is defined check so the node has been updated since the specified time.
    if (!empty($layer->conf['updated_since'])) {
      $wheres[] = 'n.changed >= ' . strtotime('-' . $layer->conf['updated_since']);
    }

    // Add the WHEREs to the query.
    $sql .= ' WHERE ' . implode(' AND ', $wheres);

    $sql .= " ORDER BY n.created";

    $params = array($top_left['lat'], $top_left['long'], $bottom_right['lat'], $bottom_right['long']);
    if (isset($layer->conf['node_type'])) {
      $params[] = $layer->conf['node_type'];
    }

    $res = db_query(db_rewrite_sql($sql), $params);

    while ($node = db_fetch_object($res)) {
      $point = explode(' ', simple_geo_clean_wkt('point', $node->simple_geo_point));
      $nodes[] = array('lat' => (float)$point[0], 'lon' => (float)$point[1], 'count' => 1, 'nid' => (int)$node->nid);
    }
  }

  return $nodes;
}

function _simplegeo_tileservice_clear_all_cache($redirect = TRUE) {
  db_query("DELETE FROM {simplegeo_tileservice_tilecache}");
  if ($redirect) {
    drupal_goto('');
  }
}