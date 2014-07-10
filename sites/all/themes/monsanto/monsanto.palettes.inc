<?php

/**
 * Returns an array of color palettes keyed on class name.
 */
function monsanto_get_palettes() {
  $palettes = array(
    'default' => 'Default',
    'palette-gray-lightgray-blue' => 'Grey/light grey/blue',
    'palette-blue-blue' => 'Blue/blue',
    'palette-magenta-blue' => 'Magenta/blue',
    'palette-black-green' => 'Black/green',
    'palette-brown-burnt-orange' => 'Brown/burnt orange',
    'palette-brown-yellow' => 'Brown/yellow',
    'palette-seafoam-orange' => 'Seafom orange',
    'palette-orange-red' => 'Orange/red',
    'palette-black-blue-yellow' => 'Black blue yellow',
    'palette-black-white-red' => 'Black/white/red',
    'palette-brick-brick' => 'Brick',
  );

  foreach ($palettes as $key => $name) {
    $palettes[$key] = theme('image', array('alt' => t($name), 'title' => t($name), 'path' => drupal_get_path('theme', 'monsanto') . "/images/palettes/$key.png"));
  }
  return $palettes;
}

/**
 * Add a color palette selection to a form.
 */
function monsanto_palettes_form(&$form) {
  $form['monsanto_palette_fieldset'] = array(
    '#type' => 'fieldset',
    '#title' => t('Color palette'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  $form['monsanto_palette_fieldset']['monsanto_palette'] = array(
    '#type' => 'radios',
    '#options' => monsanto_get_palettes(),
    '#default_value' => variable_get('monsanto_palette', 'default'),
  );
  $form['#submit'][] = 'monsanto_color_palettes_form_submit';
}

/**
 * Submit handler for the Commons Origins palette selection form.
 */
function monsanto_color_palettes_form_submit($form, &$form_state) {
  variable_set('monsanto_palette', $form_state['values']['monsanto_palette']);
}