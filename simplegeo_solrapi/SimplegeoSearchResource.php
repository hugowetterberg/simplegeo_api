<?php
// $Id$

/**
 * Class that defines the SimplegeoSearch resource
 */
class SimplegeoSearchResource {
  /**
   * Callback for the SimplegeoSearchResource index.
   *
   * @param int $z
   * @param int $x
   * @param int $y
   * @param string $query
   * @param int $page
   * @param string $fields
   * @param array $params
   * @return array
   */
  public static function index($z, $x, $y, $query='*:*', $page=0, $fields='', $params=array()) {
    // Get required files
    module_load_include('inc', 'simplegeo_api');

    $z = $z-1;

    $params = $params + array(
      'cluster' => TRUE,
      // Height and width is used to request a set of tiles at once.
      'width' => 1,
      'height' => 1,
      // Cluster offset can be used to cluster the markers for a different
      // zoom level than the one that's been requested.
      'cluster_offset' => 0,
    );
    $params['cluster'] = (bool)$params['cluster'];

    // Enforce some defaults for the fields.
    if (empty($fields) || $params['cluster']) {
      $fields = array('nid', 'ss_simple_geo_position');
    }
    else if (is_string($fields)) {
      $fields = split(',', $fields);
    }

    $tile_q = array();
    for ($i=0; $i<$params['width']; $i++) {
      for ($j=0; $j<$params['height']; $j++) {
        $tile_q[] = sprintf('sm_simple_geo_tile:%02d.%05d.%05d', $z, $x+$i, $y+$j);
      }
    }
    $query .= ' (' . join($tile_q, ' OR ') . ')';

    $res = SolrResource::index($query, $page, $fields, $params, array(
      'rows' => 500,
    ));

    // Get area name for area facets.
    if (isset($res['facets'])) {
      foreach ($res['facets'] as $facet_field => $counts) {
        if (!empty($counts)) {
          switch ($facet_field) {
            case 'im_simple_geo_area':
              self::getAreaInfo($res['facets'][$facet_field], $areaFacets);
              break;
          }
        }
      }
    }

    // Cluster result
    if ($params['cluster']) {
      $nodes = array();
      foreach ($res['items'] as $item) {
        if(!empty($item['ss_simple_geo_position'])) {
          $points = explode(" ", $item['ss_simple_geo_position']);
          $nodes[] = array('lat' => (float)$points[0], 'lon' => (float)$points[1], 'count' => 1, 'nid' => (int)$item['nid']);
        }
      }

      $res['items'] = simplegeo_api_cluster($nodes, 25, $z + $params['cluster_offset']);
    }

    return $res;
  }

  /**
   * Get area info
   */
  private static function getAreaInfo(&$facets, $defaultAreas) {
    $nids = array_keys($facets);
    // Added check to avoid errors if no facets returned
    if(count($nids) > 0) {
      $placeholders = join(array_fill(0, count($nids), '%d'), ', ');
      $res = db_query("SELECT nid, title FROM {node} WHERE nid IN({$placeholders})", $nids);

      $titles = array();
      while($t = db_fetch_object($res)) {
        // Filter out map defined tags
        if (in_array($t->nid, $defaultAreas)) {
          unset($facets[$t->nid]);
          continue;
        }
        $facets[$t->nid] = array('name' => $t->title, 'count' => $facets[$t->nid]);
        $titles[] = $t->title;
      }
    }
  }
}