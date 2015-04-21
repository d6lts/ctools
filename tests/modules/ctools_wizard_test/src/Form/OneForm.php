<?php

/**
 * @file
 * Contains \Drupal\ctools_wizard_test\Form\OneForm.
 */

namespace Drupal\ctools_wizard_test\Form;


use Drupal\Core\Form\FormBase;
use Drupal\Core\Form\FormStateInterface;

/**
 * Simple wizard step form.
 */
class OneForm extends FormBase {

  /**
   * Returns a unique string identifying the form.
   *
   * @return string
   *   The unique string identifying the form.
   */
  public function getFormId() {
    return 'ctools_wizard_test_one_form';
  }

  /**
   * Form constructor.
   *
   * @param array $form
   *   An associative array containing the structure of the form.
   * @param \Drupal\Core\Form\FormStateInterface $form_state
   *   The current state of the form.
   *
   * @return array
   *   The form structure.
   */
  public function buildForm(array $form, FormStateInterface $form_state) {
    $cached_values = $form_state->get('wizard');
    $form['name']['id']['#machine_name']['exists'] = '\Drupal\ctools_wizard_test\Form\OneForm::load';
    $form['one'] = array(
      '#title' => t('One'),
      '#type' => 'textfield',
      '#default_value' => !empty($cached_values['one']) ? $cached_values['one'] : '',
    );
    return $form;
  }

  /**
   * Form submission handler.
   *
   * @param array $form
   *   An associative array containing the structure of the form.
   * @param \Drupal\Core\Form\FormStateInterface $form_state
   *   The current state of the form.
   */
  public function submitForm(array &$form, FormStateInterface $form_state) {
    $keys = array(
      'one',
    );
    $cached_values = $form_state->get('wizard');
    foreach ($keys as $key) {
      $cached_values[$key] = $form_state->getValue($key);
    }
    $form_state->set('wizard', $cached_values);
  }

  /**
   * Simple load method for the machine name exist check.
   *
   * @param $id
   *
   * @return bool
   */
  public static function load($id) {
    return FALSE;
  }

}
