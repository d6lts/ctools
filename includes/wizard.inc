<?php
// $Id$

/**
 * @file
 * CTools' multi-step form wizard tool.
 *
 * This tool enables the creation of multi-step forms that go from one
 * form to another. The forms themselves can allow branching if they
 * like, and there are a number of configurable options to how
 * the wizard operates.
 *
 * The wizard can also be friendly to ajax forms, such as when used
 * with the modal tool.
 *
 * The wizard provides callbacks throughout the process, allowing the
 * owner to control the flow. The general flow of what happens is:
 *
 * Generate a form
 * submit a form
 * based upon button clicked, 'finished', 'next form', 'cancel' or 'return'.
 *
 * Each action has its own callback, so cached objects can be modifed and or
 * turned into real objects. Each callback can make decisions about where to
 * go next if it wishes to override the default flow.
 */

/**
 * Display a multi-step form.
 *
 * Aside from the addition of the $form_info which contains an array of
 * information and configuration so the multi-step wizard can do its thing,
 * this function works a lot like ctools_build_form.
 *
 * Remember that the form builders for this form will receive
 * &$form, &$form_state, NOT just &$form_state and no additional args.
 *
 * Do NOT use #required => TRUE with these forms as that validation
 * cannot be skipped for the CANCEL button.
 *
 * @param $form_info
 *   An array of form info. @todo document the array.
 * @param $step
 *   The current form step.
 * @param &$form_state
 *   The form state array; this is a reference so the caller can get back
 *   whatever information the form(s) involved left for it.
 */
function ctools_wizard_multistep_form($form_info, $step, &$form_state) {
  $form_state['step'] = $step;
  $form_state['form_info'] = $form_info;

  // Ensure we have form information for the current step.
  if (!isset($form_info['forms'][$step])) {
    return;
  }

  // Ensure that whatever include file(s) were requested by the form info are
  // actually included.
  $info = $form_info['forms'][$step];

  if (!empty($info['include'])) {
    if (is_array($info['include'])) {
      foreach ($info['include'] as $file) {
        require_once './' . $file;
      }
    }
    else {
      require_once './' . $info['include'];
    }
  }

  // This tells ctools_build_form to apply our wrapper to the form. It
  // will give it buttons and the like.
  $form_state['wrapper callback'] = 'ctools_wizard_wrapper';
  $form_state['re_render'] = FALSE;
  $form_state['no_redirect'] = TRUE;

  ctools_include('form');
  $output = ctools_build_form($info['form id'], $form_state);

  if (empty($form_state['executed'])) {
    if (!empty($form_state['ajax render'])) {
      // Any include files should already be included by this point:
      return $form_state['ajax render']($form_state, $output);
    }

    // Automatically use the modal tool if set to true.
    if (!empty($form_state['modal'])) {
      ctools_include('modal');
      $title = isset($form_state['title']) ? $form_state['title'] : $info['title'];
      $form_state['commands'][] = ctools_modal_command_display($title, $output);
    }
  }
  else  {
    // We use the plugins get_function format because it's powerful and
    // not limited to just functions.
    ctools_include('plugins');

    if (isset($form_state['clicked_button']['#wizard type'])) {
      $type = $form_state['clicked_button']['#wizard type'];
      // If we have a callback depending upon the type of button that was
      // clicked, call it.
      if ($function = ctools_plugin_get_function($form_info, "$type callback")) {
        $function($form_state);
      }

      // If the modal is in use, some special code for it:
      if (!empty($form_state['modal'])) {
        if ($type != 'next') {
          // Automatically dismiss the modal if we're not going to another form.
          ctools_include('modal');
          $form_state['commands'][] = ctools_modal_command_dismiss();
        }
      }
    }

    if (empty($form_state['ajax'])) {
      // redirect, if one is set.
      return drupal_redirect_form(array(), $form_state['redirect']);
    }
    else if (isset($form_state['ajax next'])) {
      // Clear a few items off the form state so we don't double post:
      $next = $form_state['ajax next'];
      unset($form_state['ajax next']);
      unset($form_state['executed']);
      unset($form_state['post']);
      unset($form_state['next']);
      return ctools_wizard_multistep_form($form_info, $next, $form_state);
    }
    // If the callbacks wanted to do something besides go to the next form,
    // it needs to have set $form_state['commands'] with something that can
    // be rendered.
  }

  // Render ajax commands if we have any.
  if (isset($form_state['ajax']) && isset($form_state['commands'])) {
    return ctools_ajax_render($form_state['commands']);
  }

  // Otherwise, return the output.
  return $output;
}

/**
 * Provide a wrapper around another form for adding multi-step information.
 */
function ctools_wizard_wrapper(&$form, &$form_state) {
  $form_info = &$form_state['form_info'];
  $info = $form_info['forms'][$form_state['step']];

  // Determine the next form from this step.
  // Create a form trail if we're supposed to have one.
  $trail = array();
  $previous = TRUE;
  foreach ($form_info['order'] as $id => $title) {
    if ($id == $form_state['step']) {
      $previous = FALSE;
      $class = 'wizard-trail-current';
    }
    elseif ($previous) {
      $not_first = TRUE;
      $class = 'wizard-trail-previous';
      $form_state['previous'] = $id;
    }
    else {
      $class = 'wizard-trail-next';
      if (!isset($form_state['next'])) {
        $form_state['next'] = $id;
      }
      if (empty($form_info['show trail'])) {
        break;
      }
    }

    if (!empty($form_info['show trail'])) {
      $trail[] = '<span class="' . $class . '">' . $title . '</span>';
    }
  }

  // Display the trail if instructed to do so.
  if (!empty($form_info['show trail'])) {
    ctools_add_css('wizard');
    $form['ctools_trail'] = array(
      '#value' => theme(array('ctools_wizard_trail__' . $form_info['id'], 'ctools_wizard_trail'), $trail),
      '#weight' => -1000,
    );
  }

  // Ensure buttons stay on the bottom.
  $form['buttons'] = array(
    '#prefix' => '<div class="clear-block">',
    '#suffix' => '</div>',
    '#weight' => 1000,
  );

  if (!empty($form_info['show back']) && isset($form_state['previous'])) {
    $form['buttons']['previous'] = array(
      '#type' => 'submit',
      '#value' => isset($form_info['back text']) ? $form_info['back text'] : t('Back'),
      '#next' => $form_state['previous'],
      '#wizard type' => 'next',
      '#weight' => -2000,
      // hardcode the submit so that it doesn't try to save data.
      '#submit' => array('ctools_wizard_submit'),
    );
  }

  // If there is a next form, place the next button.
  if (isset($form_state['next'])) {
    $form['buttons']['next'] = array(
      '#type' => 'submit',
      '#value' => isset($form_info['next text']) ? $form_info['next text'] : t('Continue'),
      '#next' => $form_state['next'],
      '#wizard type' => 'next',
      '#weight' => -1000,
    );
  }

  // There are two ways the return button can appear. If this is not the
  // end of the form list (i.e, there is a next) then it's "update and return"
  // to be clear. If this is the end of the path and there is no next, we
  // call it 'Finish'.

  // Even if there is no direct return path (some forms may not want you
  // leaving in the middle) the final button is always a Finish and it does
  // whatever the return action is.
  if (!empty($form_info['show return']) && !empty($form_state['next'])) {
    $form['buttons']['return'] = array(
      '#type' => 'submit',
      '#value' => isset($form_info['return text']) ? $form_info['return text'] : t('Update and return'),
      '#wizard type' => 'return',
    );
  }
  else if (empty($form_state['next'])) {
    $form['buttons']['return'] = array(
      '#type' => 'submit',
      '#value' => isset($form_info['finish text']) ? $form_info['finish text'] : t('Finish'),
      '#wizard type' => 'finish',
    );
  }

  // If we are allowed to cancel, place a cancel button.
  if (isset($form_info['cancel path']) || !empty($form_info['show cancel'])) {
    $form['buttons']['cancel'] = array(
      '#type' => 'submit',
      '#value' => isset($form_info['cancel text']) ? $form_info['cancel text'] : t('Cancel'),
      '#wizard type' => 'cancel',
      // hardcode the submit so that it doesn't try to save data.
      '#submit' => array('ctools_wizard_submit'),
    );
  }

  // Set up optional validate handlers.
  $form['#validate'] = array();
  if (function_exists($info['form id'] . '_validate')) {
    $form['#validate'][] = $info['form id'] . '_validate';
  }
  if (isset($info['validate']) && function_exists($info['validate'])) {
    $form['#validate'][] = $info['validate'];
  }

  // Set up our submit handler after theirs. Since putting something here will
  // skip Drupal's autodetect, we autodetect for it.

  // We make sure ours is after theirs so that they get to change #next if
  // the want to.
  $form['#submit'] = array();
  if (function_exists($info['form id'] . '_submit')) {
    $form['#submit'][] = $info['form id'] . '_submit';
  }
  if (isset($info['submit']) && function_exists($info['submit'])) {
    $form['#submit'][] = $info['submit'];
  }
  $form['#submit'][] = 'ctools_wizard_submit';

  if (!empty($form_state['modal'])) {
    $form['#action'] = url(ctools_wizard_get_path($form_state['form_info'], $form_state['step']));
  }
}

/**
 * On a submit, go to the next form.
 */
function ctools_wizard_submit(&$form, &$form_state) {
  if (isset($form_state['clicked_button']['#wizard type'])) {
    $type = $form_state['clicked_button']['#wizard type'];

    // if AJAX enabled, we proceed slightly differently here.
    if (!empty($form_state['ajax'])) {
      if ($type == 'next') {
        $form_state['ajax next'] = $form_state['clicked_button']['#next'];
      }
    }
    else {
      if ($type == 'cancel' && isset($form_state['form_info']['cancel path'])) {
        $form_state['redirect'] = $form_state['form_info']['return path'];
      }
      else if ($type == 'next') {
        $form_state['redirect'] = ctools_wizard_get_path($form_state['form_info'], $form_state['clicked_button']['#next']);
      }
      else if (isset($form_state['form_info']['return path'])) {
        $form_state['redirect'] = $form_state['form_info']['return path'];
      }
    }
  }
}

/**
 * Create a path from the form info and a given step.
 */
function ctools_wizard_get_path($form_info, $step) {
  return str_replace('%step', $step, $form_info['path']);
}