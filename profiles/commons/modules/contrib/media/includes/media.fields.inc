<?php

/**
 * @file
 * Provide the media selector widget and media field formatters to the Fields
 * API.
 */

/**
 * Implements hook_field_widget_info().
 */
function media_field_widget_info() {
  return array(
    'media_generic' => array(
      'label' => t('Media browser'),
      'field types' => array('file', 'image'),
      'settings' => array(
        'allowed_types' => array('image'),
        'browser_plugins' => array(),
        'allowed_schemes' => array('public', 'private'),
      ),
      'behaviors' => array(
        'multiple values' => FIELD_BEHAVIOR_CUSTOM,
        'default value' => FIELD_BEHAVIOR_NONE,
      ),
    ),
  );
}

/**
 * Implements hook_field_widget_settings_form().
 */
function media_field_widget_settings_form($field, $instance) {
  $widget = $instance['widget'];
  $settings = $widget['settings'];

  $plugins = media_get_browser_plugin_info();
  $options = array();
  foreach ($plugins as $key => $plugin) {
    $options[$key] = check_plain($plugin['title']);
  }

  $form['browser_plugins'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Enabled browser plugins'),
    '#options' => $options,
    '#default_value' => $settings['browser_plugins'],
    '#description' => t('Media browser plugins which are allowed for this field. If no plugins are selected, they will all be available.'),
  );

  $form['allowed_types'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Allowed file types'),
    '#options' => file_entity_type_get_names(),
    '#default_value' => $settings['allowed_types'],
    '#description' => t('File types which are allowed for this field. If no file types are selected, they will all be available.'),
  );

  $visible_steam_wrappers = file_get_stream_wrappers(STREAM_WRAPPERS_VISIBLE);
  $options = array();
  foreach ($visible_steam_wrappers as $scheme => $information) {
    $options[$scheme] = check_plain($information['name']);
  }

  $form['allowed_schemes'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Allowed URI schemes'),
    '#options' => $options,
    '#default_value' => $settings['allowed_schemes'],
    '#description' => t('URI schemes which are allowed for this field. If no schemes are selected, they will all be available.'),
  );

  return $form;
}

/**
 * Implements hook_field_widget_form().
 */
function media_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {

  // Add display_field setting to field because media_field_widget_form() assumes it is set.
  if (!isset($field['settings']['display_field'])) {
    $field['settings']['display_field'] = 0;
  }

  $defaults = array(
    'fid' => 0,
    'display' => !empty($field['settings']['display_default']),
    'description' => '',
  );

  // Load the items for form rebuilds from the field state as they might not be
  // in $form_state['values'] because of validation limitations. Also, they are
  // only passed in as $items when editing existing entities.
  $field_state = field_form_get_state($element['#field_parents'], $field['field_name'], $langcode, $form_state);
  if (isset($field_state['items'])) {
    $items = $field_state['items'];
  }

  $field_settings = $instance['settings'];
  $widget_settings = $instance['widget']['settings'];

  // Essentially we use the media type, extended with some enhancements.
  $element_info = element_info('media');
  $element += array(
    '#type' => 'media',
    '#value_callback' => 'media_field_widget_value',
    '#process' => array_merge($element_info['#process'], array('media_field_widget_process')),
    '#media_options' => array(
      'global' => array(
        'types' => array_filter($widget_settings['allowed_types']),
        'enabledPlugins' => array_filter($instance['widget']['settings']['browser_plugins']),
        'schemes' => array_filter($widget_settings['allowed_schemes']),
        'file_directory' => isset($field_settings['file_directory']) ? $field_settings['file_directory'] : '',
        'file_extensions' => isset($field_settings['file_extensions']) ? $field_settings['file_extensions'] : variable_get('file_entity_default_allowed_extensions', 'jpg jpeg gif png txt doc docx xls xlsx pdf ppt pptx pps ppsx odt ods odp mp3 mov mp4 m4a m4v mpeg avi ogg oga ogv weba webp webm'),
        'max_filesize' => isset($field_settings['max_filesize']) ? $field_settings['max_filesize'] : 0,
        'uri_scheme' => !empty($field['settings']['uri_scheme']) ? $field['settings']['uri_scheme'] : file_default_scheme(),
      ),
    ),
    // Allows this field to return an array instead of a single value.
    '#extended' => TRUE,
  );

  // Add image field specific validators.
  if ($field['type'] == 'image') {
    if ($field_settings['min_resolution'] || $field_settings['max_resolution']) {
      $element['#media_options']['global']['min_resolution'] = $field_settings['min_resolution'];
      $element['#media_options']['global']['max_resolution'] = $field_settings['max_resolution'];
    }
  }

  if ($field['cardinality'] == 1) {
    // Set the default value.
    $element['#default_value'] = !empty($items) ? $items[0] : $defaults;
    // If there's only one field, return it as delta 0.
    if (empty($element['#default_value']['fid'])) {
      $element['#description'] = theme('media_upload_help', array('description' => $element['#description']));
    }
    $elements = array($element);
  }
  else {
    // If there are multiple values, add an element for each existing one.
    foreach ($items as $item) {
      $elements[$delta] = $element;
      $elements[$delta]['#default_value'] = $item;
      $elements[$delta]['#weight'] = $delta;
      $delta++;
    }
    // And then add one more empty row for new uploads except when this is a
    // programmed form as it is not necessary.
    if (($field['cardinality'] == FIELD_CARDINALITY_UNLIMITED || $delta < $field['cardinality']) && empty($form_state['programmed'])) {
      $elements[$delta] = $element;
      $elements[$delta]['#default_value'] = $defaults;
      $elements[$delta]['#weight'] = $delta;
      $elements[$delta]['#required'] = ($element['#required'] && $delta == 0);
    }
    // The group of elements all-together need some extra functionality
    // after building up the full list (like draggable table rows).
    $elements['#file_upload_delta'] = $delta;
    $elements['#theme'] = 'media_widget_multiple';
    $elements['#theme_wrappers'] = array('fieldset');
    $elements['#process'] = array('media_field_widget_process_multiple');
    $elements['#title'] = $element['#title'];
    $elements['#description'] = $element['#description'];
    $elements['#field_name'] = $element['#field_name'];
    $elements['#language'] = $element['#language'];
    $elements['#display_field'] = $field['settings']['display_field'];

    // Add some properties that will eventually be added to the media upload
    // field. These are added here so that they may be referenced easily through
    // a hook_form_alter().
    $elements['#file_upload_title'] = t('Attach media');
    $elements['#file_upload_description'] = theme('media_upload_help', array('description' => ''));
  }

  return $elements;
}

/**
 * The #value_callback for the media field element.
 */
function media_field_widget_value($element, $input = FALSE, $form_state) {
  if ($input) {
    // Checkboxes lose their value when empty.
    // If the display field is present make sure its unchecked value is saved.
    $field = field_widget_field($element, $form_state);
    if (empty($input['display'])) {
      $input['display'] = $field['settings']['display_field'] ? 0 : 1;
    }
  }

  // We depend on the media element to handle uploads.
  $return = media_file_value($element, $input, $form_state);

  // Ensure that all the required properties are returned even if empty.
  $return += array(
    'fid' => 0,
    'display' => 1,
    'description' => '',
  );

  return $return;
}

/**
 * An element #process callback for the media field type.
 *
 * Expands the media type to include the description and display fields.
 */
function media_field_widget_process($element, &$form_state, $form) {
  $item = $element['#value'];
  $item['fid'] = $element['fid']['#value'];

  $field = field_widget_field($element, $form_state);
  $instance = field_widget_instance($element, $form_state);
  $settings = $instance['widget']['settings'];

  $element['#theme'] = 'media_widget';

  // Add the display field if enabled.
  if (!empty($field['settings']['display_field']) && $item['fid']) {
    $element['display'] = array(
      '#type' => empty($item['fid']) ? 'hidden' : 'checkbox',
      '#title' => t('Include file in display'),
      '#value' => isset($item['display']) ? $item['display'] : $field['settings']['display_default'],
      '#attributes' => array('class' => array('file-display')),
    );
  }
  else {
    $element['display'] = array(
      '#type' => 'hidden',
      '#value' => '1',
    );
  }

  // Add the description field if enabled.
  if (!empty($instance['settings']['description_field']) && $item['fid']) {
    $element['description'] = array(
      '#type' => variable_get('file_description_type', 'textfield'),
      '#title' => t('Description'),
      '#value' => isset($item['description']) ? $item['description'] : '',
      '#maxlength' => variable_get('file_description_length', 128),
      '#description' => t('The description may be used as the label of the link to the file.'),
    );
  }

  // Adjust the Ajax settings so that on upload and remove of any individual
  // file, the entire group of file fields is updated together.
  if ($field['cardinality'] != 1) {
    $parents = array_slice($element['#array_parents'], 0, -1);
    $new_path = 'media/ajax/' . implode('/', $parents) . '/' . $form['form_build_id']['#value'];
    $field_element = drupal_array_get_nested_value($form, $parents);
    $new_wrapper = $field_element['#id'] . '-ajax-wrapper';
    foreach (element_children($element) as $key) {
      if (isset($element[$key]['#ajax'])) {
        $element[$key]['#ajax']['path'] = $new_path;
        $element[$key]['#ajax']['wrapper'] = $new_wrapper;
      }
    }
    unset($element['#prefix'], $element['#suffix']);
  }

  // Add another submit handler to the upload and remove buttons, to implement
  // functionality needed by the field widget. This submit handler, along with
  // the rebuild logic in media_field_widget_form() requires the entire field,
  // not just the individual item, to be valid.
  foreach (array('attach_button', 'remove_button') as $key) {
    $element[$key]['#submit'][] = 'media_field_widget_submit';
    $element[$key]['#limit_validation_errors'] = array(array_slice($element['#parents'], 0, -1));
  }

  return $element;
}

/**
 * An element #process callback for a group of media fields.
 *
 * Adds the weight field to each row so it can be ordered and adds a new Ajax
 * wrapper around the entire group so it can be replaced all at once.
 */
function media_field_widget_process_multiple($element, &$form_state, $form) {
  $element_children = element_children($element, TRUE);
  $count = count($element_children);

  foreach ($element_children as $delta => $key) {
    if ($key != $element['#file_upload_delta']) {
      $description = _media_field_get_description_from_element($element[$key]);
      $element[$key]['_weight'] = array(
        '#type' => 'weight',
        '#title' => $description ? t('Weight for @title', array('@title' => $description)) : t('Weight for new file'),
        '#title_display' => 'invisible',
        '#delta' => $count,
        '#default_value' => $delta,
      );
    }
    else {
      // The title needs to be assigned to the attach field so that validation
      // errors include the correct widget label.
      $element[$key]['#title'] = $element['#title'];
      $element[$key]['_weight'] = array(
        '#type' => 'hidden',
        '#default_value' => $delta,
      );
    }
  }

  // Add a new wrapper around all the elements for Ajax replacement.
  $element['#prefix'] = '<div id="' . $element['#id'] . '-ajax-wrapper">';
  $element['#suffix'] = '</div>';

  return $element;
}

/**
 * Retrieves the file description from a media field element.
 *
 * This helper function is used by media_field_widget_process_multiple().
 *
 * @param $element
 *   The element being processed.
 *
 * @return
 *   A description of the file suitable for use in the administrative interface.
 */
function _media_field_get_description_from_element($element) {
  // Use the actual file description, if it's available.
  if (!empty($element['#default_value']['description'])) {
    return $element['#default_value']['description'];
  }
  // Otherwise, fall back to the filename.
  if (!empty($element['#default_value']['filename'])) {
    return $element['#default_value']['filename'];
  }
  // This is probably a newly uploaded file; no description is available.
  return FALSE;
}

/**
 * Form submission handler for attach/remove button of media_field_widget_form().
 *
 * This runs in addition to and after media_field_widget_submit().
 *
 * @see media_field_widget_submit()
 * @see media_field_widget_form()
 * @see media_field_widget_process()
 */
function media_field_widget_submit($form, &$form_state) {
  // During the form rebuild, media_field_widget_form() will create field item
  // widget elements using re-indexed deltas, so clear out $form_state['input']
  // to avoid a mismatch between old and new deltas. The rebuilt elements will
  // have #default_value set appropriately for the current state of the field,
  // so nothing is lost in doing this.
  $parents = array_slice($form_state['triggering_element']['#parents'], 0, -2);
  drupal_array_set_nested_value($form_state['input'], $parents, NULL);

  $button = $form_state['triggering_element'];

  // Go one level up in the form, to the widgets container.
  $element = drupal_array_get_nested_value($form, array_slice($button['#array_parents'], 0, -1));
  $field_name = $element['#field_name'];
  $langcode = $element['#language'];
  $parents = $element['#field_parents'];

  $submitted_values = drupal_array_get_nested_value($form_state['values'], array_slice($button['#array_parents'], 0, -2));
  foreach ($submitted_values as $delta => $submitted_value) {
    if (!$submitted_value['fid']) {
      unset($submitted_values[$delta]);
    }
  }

  // Re-index deltas after removing empty items.
  $submitted_values = array_values($submitted_values);

  // Update form_state values.
  drupal_array_set_nested_value($form_state['values'], array_slice($button['#array_parents'], 0, -2), $submitted_values);

  // Update items.
  $field_state = field_form_get_state($parents, $field_name, $langcode, $form_state);
  $field_state['items'] = $submitted_values;
  field_form_set_state($parents, $field_name, $langcode, $form_state, $field_state);
}

/**
 * Returns HTML for an individual media widget.
 *
 * @param $variables
 *   An associative array containing:
 *   - element: A render element representing the widget.
 *
 * @ingroup themeable
 */
function theme_media_widget($variables) {
  $element = $variables['element'];
  $output = '';

  // The "form-media" class is required for proper Ajax functionality.
  $output .= '<div id="' . $element['#id'] . '" class="media-widget form-media clearfix">';
  $output .= drupal_render_children($element);
  $output .= '</div>';

  return $output;
}

/**
 * Returns HTML for a group of media widgets.
 *
 * @param $variables
 *   An associative array containing:
 *   - element: A render element representing the widgets.
 *
 * @ingroup themeable
 */
function theme_media_widget_multiple($variables) {
  $element = $variables['element'];

  // Special ID and classes for draggable tables.
  $weight_class = $element['#id'] . '-weight';
  $table_id = $element['#id'] . '-table';

  // Build up a table of applicable fields.
  $headers = array();
  $headers[] = t('File information');
  if ($element['#display_field']) {
    $headers[] = array(
      'data' => t('Display'),
      'class' => array('checkbox'),
    );
  }
  $headers[] = t('Weight');
  $headers[] = t('Operations');

  // Get our list of widgets in order (needed when the form comes back after
  // preview or failed validation).
  $widgets = array();
  foreach (element_children($element) as $key) {
    $widgets[] = &$element[$key];
  }
  usort($widgets, '_field_sort_items_value_helper');

  $rows = array();
  foreach ($widgets as $key => &$widget) {
    // Save the uploading row for last.
    if ($widget['#file'] == FALSE) {
      $widget['#title'] = $element['#file_upload_title'];
      $widget['#description'] = $element['#file_upload_description'];
      continue;
    }

    // Delay rendering of the buttons, so that they can be rendered later in the
    // "operations" column.
    $operations_elements = array();
    foreach (element_children($widget) as $sub_key) {
      if (isset($widget[$sub_key]['#type']) && $widget[$sub_key]['#type'] == 'submit') {
        hide($widget[$sub_key]);
        $operations_elements[] = &$widget[$sub_key];
      }
    }

    // Delay rendering of the "Display" option and the weight selector, so that
    // each can be rendered later in its own column.
    if ($element['#display_field']) {
      hide($widget['display']);
    }
    hide($widget['_weight']);

    // Render everything else together in a column, without the normal wrappers.
    $widget['#theme_wrappers'] = array();
    $information = drupal_render($widget);

    // Render the previously hidden elements, using render() instead of
    // drupal_render(), to undo the earlier hide().
    $operations = '';
    foreach ($operations_elements as $operation_element) {
      $operations .= render($operation_element);
    }
    $display = '';
    if ($element['#display_field']) {
      unset($widget['display']['#title']);
      $display = array(
        'data' => render($widget['display']),
        'class' => array('checkbox'),
      );
    }
    $widget['_weight']['#attributes']['class'] = array($weight_class);
    $weight = render($widget['_weight']);

    // Arrange the row with all of the rendered columns.
    $row = array();
    $row[] = $information;
    if ($element['#display_field']) {
      $row[] = $display;
    }
    $row[] = $weight;
    $row[] = $operations;
    $rows[] = array(
      'data' => $row,
      'class' => isset($widget['#attributes']['class']) ? array_merge($widget['#attributes']['class'], array('draggable')) : array('draggable'),
    );
  }

  drupal_add_tabledrag($table_id, 'order', 'sibling', $weight_class);

  $output = '';
  $output = empty($rows) ? '' : theme('table', array('header' => $headers, 'rows' => $rows, 'attributes' => array('id' => $table_id)));
  $output .= drupal_render_children($element);
  return $output;
}

/**
 * Returns HTML for help text.
 *
 * @param $variables
 *   An associative array containing:
 *   - description: The normal description for this field, specified by the
 *     user.
 *
 * @ingroup themeable
 */
function theme_media_upload_help($variables) {
  $description = $variables['description'];

  $descriptions = array();

  if (strlen($description)) {
    $descriptions[] = $description;
  }

  return implode('<br />', $descriptions);
}

/**
 * Implements hook_field_formatter_info().
 *
 * Provides legacy support for the "Large filetype icon" file field formatter.
 * This was originally used when media entities contained file fields. The
 * current file entity architecture no longer needs this, but people may
 * have used this formatter for other file fields on their website.
 *
 * @todo Some day, remove this.
 */
function media_field_formatter_info() {
  $formatters = array(
    'media_large_icon' => array(
      'label' => t('Large filetype icon'),
      'field types' => array('file'),
    ),
  );
  return $formatters;
}

/**
 * Implements hook_field_formatter_view().
 *
 * Legacy support for the "Large filetype icon" file field formatter.
 * @see media_field_formatter_info()
 */
function media_field_formatter_view($entity_type, $entity, $field, $instance, $langcode, $items, $display) {
  $element = array();

  if ($display['type'] == 'media_large_icon') {
    // Use the media_thumbnail image style so that the output in media browser
    // is consistent.
    foreach ($items as $delta => $item) {
      $element[$delta] = array(
        '#theme' => 'media_formatter_large_icon',
        '#file' => (object) $item,
      );
    }
  }

  return $element;
}
