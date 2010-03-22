<?php
// $Id$

/**
 * @file
 * Administrative functions for the simplegeo_tileservice module.
 *
 * This provides the UI to list, create, edit and delete layers.
 */

/**
 * Output a list of layers.
 */
function simplegeo_tileservice_list_layer($js = NULL) {
  $header = array(
    array('data' => t('Name'),    'class' => 'simplegeo_tileservice-layer-name'),
    array('data' => t('Storage'), 'class' => 'simplegeo_tileservice-layer-storage'),
  );

  $header[] = array('data' => t('Operations'), 'class' => 'simplegeo_tileservice-layer-operations');

  $layers = simplegeo_tileservice_layer_load_all();
  $rows = array();

  foreach ($layers as $layer) {
    $operations = array();

    if (empty($layer->disabled)) {
      $operations[] = array(
        'title' => t('Edit'),
        'href'  => 'admin/build/simplegeo-tileservice/' . $layer->name . '/edit',
      );
      $operations[] = array(
        'title' => t('Export'),
        'href'  => 'admin/build/simplegeo-tileservice/' . $layer->name . '/export',
      );
    }

    if ($layer->export_type == (EXPORT_IN_CODE | EXPORT_IN_DATABASE)) {
      $operations[] = array(
        'title' => t('Revert'),
        'href'  => 'admin/build/simplegeo-tileservice/' . $layer->name . '/delete',
      );
    }
    elseif ($layer->export_type != EXPORT_IN_CODE) {
      $operations[] = array(
        'title' => t('Delete'),
        'href'  => 'admin/build/simplegeo-tileservice/' . $layer->name . '/delete',
      );
    }
    elseif (empty($layer->disabled)) {
      $operations[] = array(
        'title' => t('Disable'),
        'href'  => 'admin/build/simplegeo-tileservice/' . $layer->name . '/disable',
        'query' => drupal_get_destination(),
      );
    }
    else {
      $operations[] = array(
        'title' => t('Enable'),
        'href'  => 'admin/build/simplegeo-tileservice/' . $layer->name . '/enable',
        'query' => drupal_get_destination(),
      );
    }

    $rows[$layer->name] = array(
      'data' => array(
        'name' => array(
          'data'  => $layer->name,
          'class' => 'simplegeo_tileservice-layer-name',
        ),
        'storage' => array(
          'data'  => ($layer->export_type == EXPORT_IN_CODE) ? t('In code') : t('In database'),
          'class' => 'simplegeo_tileservice-layer-storage',
        ),
        'operations' => array(
          'data'  => theme('links', $operations),
          'class' => 'simplegeo_tileservice-layer-operations',
        ),
      ),
      'class' => 'simplegeo_tileservice-layer-' . $layer->name . (!empty($layer->disabled) ? ' simplegeo_tileservice-layer-disabled' : ''),
    );
  }

  $table = theme('table', $header, $rows, array('id' => 'simplegeo_tileservice-list-layer'));

  drupal_add_css(drupal_get_path('module', 'simplegeo_tileservice') . '/simplegeo_tileservice.admin.css');

  return $table;
}

function simplegeo_tileservice_add_layer_page() {
  $layer_types = simplegeo_tileservice_get_layer_types();
  $items = array();
  foreach ($layer_types as $layer_type_name => $layer_type) {
    $type_url_str = str_replace('_', '-', $layer_type_name);
    $items[] = l($layer_type['title'], 'admin/build/simplegeo-tileservice/add/' . $type_url_str);
  }
  if (empty($items)) {
    $output .= t('No available layer types.');
  }
  else {
    $output .= theme('item_list', $items);
  }
  return $output;
}

/**
 * Handle the add layer page.
 */
function simplegeo_tileservice_add_layer($layer_type_name) {
  $layer = simplegeo_tileservice_layer_new();
  $layer->layer_type = str_replace('-', '_', $layer_type_name);
  drupal_set_title(t('Add layer')); //TODO: Isn't this redundant?
  return simplegeo_tileservice_edit_layer($layer);
}

/**
 * Edit a layer.
 *
 * Called from both the add and edit points to provide for common flow.
 */
function simplegeo_tileservice_edit_layer($layer) {
  if (!is_object($layer)) {
    $layer = simplegeo_tileservice_layer_load($layer);
  }
  if ($layer) {
    drupal_set_title(check_plain($layer->name));
  }

  return drupal_get_form('simplegeo_tileservice_edit_form_layer', $layer);
}

/**
 * Form to edit the settings of a layer.
 */
function simplegeo_tileservice_edit_form_layer(&$form_state, $layer) {
  $form = array();
  $type = simplegeo_tileservice_get_layer_type($layer->layer_type);

  $form['lid'] = array(
    '#type'  => 'value',
    '#value' => isset($layer->lid) ? $layer->lid : '',
  );
  $form['layer'] = array(
    '#type'  => 'value',
    '#value' => $layer,
  );
  
  $form['layer_type'] = array(
    '#type' => 'item',
    '#value' => t('Layer type: @layer_type', array('@layer_type' => $type['title'])),
  );

  $form['name'] = array(
    '#type'          => 'textfield',
    '#size'          => 24,
    '#default_value' => $layer->name,
    '#title'         => t('Name'),
    '#description'   => t('A unique name used to identify this layer internally. It can only contain alphanumeric characters, underscores (_), and hyphens (-).'),
    '#required'      => TRUE,
  );

  $form['title'] = array(
    '#type'          => 'textfield',
    '#size'          => 24,
    '#default_value' => $layer->title,
    '#title'         => t('Title'),
    '#description'   => t('A title that describes this layer.'),
    '#required'      => TRUE,
  );

  if (isset($type['file'])) {
    require_once($type['file']);
  }

  if ($type_settings = call_user_func($type['settings callback'], $layer)) {
    $form = array_merge($form, $type_settings);
  }

  drupal_alter('simplegeo_tileservice_layer_settings', $form, $layer);

  $label = empty($layer->lid) ? t('Save and proceed') : t('Save');
  $form['submit'] = array(
    '#type'  => 'submit',
    '#value' => $label,
  );

  return $form;
}

/**
 * Validate submission of the layer edit form.
 */
function simplegeo_tileservice_edit_form_layer_validate($form, &$form_state) {
  // Test uniqueness of name:
  if (!$form_state['values']['name']) {
    form_error($form['name'], t('Name is required.'));
  }
  elseif (preg_match("/[^A-Za-z0-9_]/", $form_state['values']['name'])) {
    form_error($form['name'], t('Name must be alphanumeric or underscores only.'));
  }
  else {
    $query = "SELECT lid FROM {simplegeo_tileservice_layer} WHERE name = '%s'";
    if (!empty($form_state['values']['lid']) && is_numeric($form_state['values']['lid'])) {
      $query .= ' AND lid != ' . $form_state['values']['lid'];
    }
    if (db_result(db_query($query, $form_state['values']['name']))) {
      form_error($form['name'], t('Name must be unique.'));
    }
  }
}

/**
 * Process submission of the mini panel edit form.
 */
function simplegeo_tileservice_edit_form_layer_submit($form, &$form_state) {
  $layer = $form_state['values']['layer'];
  $layer->title  = $form_state['values']['title'];
  $layer->name   = $form_state['values']['name'];

  // Let the layer type add data to $layer->conf
  $type = simplegeo_tileservice_get_layer_type($layer->layer_type);
  if (isset($type['file'])) {
    require_once($type['file']);
  }
  $conf = array();
  $conf = call_user_func($type['settings submit callback'], $form_state['values'], $layer);

  // Let modules alter $conf and $layer.
  $conf['__drupal_alter_by_ref'] = array($layer);
  drupal_alter('simplegeo_tileservice_layer_settings_submit', $conf, $form_state);

  $layer->conf = $conf;

  if (empty($layer->lid)) {
    drupal_set_message(t('Your new layer %name has been saved.', array('%name' => $layer->name)));
    simplegeo_tileservice_layer_save($layer);
    $form_state['values']['lid'] = $layer->lid;
  }
  else {
    drupal_set_message(t('Your changes have been saved.'));
    simplegeo_tileservice_layer_save($layer);
  }

  $form_state['redirect'] = 'admin/build/simplegeo-tileservice';
}

/**
 * Page callback to export a layer to PHP code.
 */
function simplegeo_tileservice_export_layer(&$form_state, $layer) {
  if (!is_object($layer)) {
    $layer = simplegeo_tileservice_layer_load($layer);
  }
  drupal_set_title(check_plain($layer->name));
  $code = simplegeo_tileservice_layer_export($layer);

  $lines = substr_count($code, "\n");
  $form['code'] = array(
    '#type'          => 'textarea',
    '#title'         => $layer->name,
    '#default_value' => $code,
    '#rows'          => $lines,
  );
  return $form;
}

/**
 * Provide a form to confirm deletion of a layer.
 */
function simplegeo_tileservice_delete_confirm_layer(&$form_state, $layer) {
  if (!is_object($layer)) {
    $layer = simplegeo_tileservice_layer_load($layer);
  }
  if ($layer->export_type == (EXPORT_IN_CODE | EXPORT_IN_DATABASE)) {
    $submit = t('Revert');
  }
  elseif ($layer->export_type != EXPORT_IN_CODE) {
    $submit = t('Delete');
  }
  else {
    drupal_not_found();
    die;
  }
  $form['name'] = array('#type' => 'value', '#value' => $layer->name);
  $form['lid']  = array('#type' => 'value', '#value' => $layer->lid);
  return confirm_form($form,
    t('Are you sure you want to delete the layer "@name"?', array('@name' => $layer->name)),
    !empty($_GET['destination']) ? $_GET['destination'] : 'admin/build/simplegeo-tileservice',
    t('This action cannot be undone.'),
    $submit, t('Cancel')
  );
}

/**
 * Handle the submit button to delete a mini panel.
 */
function simplegeo_tileservice_delete_confirm_layer_submit($form, &$form_state) {
  $layer = simplegeo_tileservice_layer_load($form_state['values']['name']);
  if ($layer->lid == $form_state['values']['lid']) {
    simplegeo_tileservice_layer_delete($layer);
    $form_state['redirect'] = 'admin/build/simplegeo-tileservice';
  }
}

/**
 * Enable a default layer.
 */
function simplegeo_tileservice_enable_layer($layer) {
  if (!is_object($layer)) {
    $layer = simplegeo_tileservice_layer_load($layer);
  }
  ctools_include('export');
  ctools_export_set_status('simplegeo_tileservice_layer', $layer->name, FALSE);
  //TODO: Extract this and move to oembedfield
  if (module_exists('content')) {
    content_clear_type_cache();
    drupal_rebuild_theme_registry();
  }
  drupal_goto();
}

/**
 * Disable a default layer.
 */
function simplegeo_tileservice_disable_layer($layer) {
  if (!is_object($layer)) {
    $layer = simplegeo_tileservice_layer_load($layer);
  }
  ctools_include('export');
  ctools_export_set_status('simplegeo_tileservice_layer', $layer->name, TRUE);
  //TODO: Extract this and move to oembedfield
  if (module_exists('content')) {
    content_clear_type_cache();
    drupal_rebuild_theme_registry();
  }
  drupal_goto();
}