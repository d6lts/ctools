<?php

/**
 * @file
 * Contains Drupal\ctools\Form\ConditionConfigure.
 */

namespace Drupal\ctools\Form;


use Drupal\Component\Plugin\PluginManagerInterface;
use Drupal\Core\Ajax\AjaxResponse;
use Drupal\Core\Ajax\CloseModalDialogCommand;
use Drupal\Core\Ajax\RedirectCommand;
use Drupal\Core\Form\FormBase;
use Drupal\Core\Form\FormStateInterface;
use Drupal\Core\Plugin\Context\Context;
use Drupal\Core\Plugin\Context\ContextDefinition;
use Drupal\Core\Plugin\ContextAwarePluginInterface;
use Drupal\user\SharedTempStoreFactory;
use Symfony\Component\DependencyInjection\ContainerInterface;

/**
 * Base class for condition configur operations.
 */
abstract class ConditionConfigure extends FormBase {

  /**
   * @var \Drupal\user\SharedTempStoreFactory
   */
  protected $tempstore;

  /**
   * @var \Drupal\Core\Condition\ConditionManager
   */
  protected $manager;

  /**
   * @var string
   */
  protected $tempstore_id;

  /**
   * @var string;
   */
  protected $machine_name;

  /**
   * {@inheritdoc}
   */
  public static function create(ContainerInterface $container) {
    return new static($container->get('user.shared_tempstore'), $container->get('plugin.manager.condition'));
  }

  function __construct(SharedTempStoreFactory $tempstore, PluginManagerInterface $manager) {
    $this->tempstore = $tempstore;
    $this->manager = $manager;
  }

  /**
   * {@inheritdoc}
   */
  public function getFormId() {
    return 'ctools_context_configure';
  }

  /**
   * {@inheritdoc}
   */
  public function buildForm(array $form, FormStateInterface $form_state, $condition = NULL, $tempstore_id = NULL, $machine_name = NULL) {
    $this->tempstore_id = $tempstore_id;
    $this->machine_name = $machine_name;
    $cached_values = $this->tempstore->get($this->tempstore_id)->get($this->machine_name);
    if (is_numeric($condition)) {
      $id = $condition;
      $condition = $this->getConditions($cached_values)[$id];
      $instance = $this->manager->createInstance($condition['id'], $condition);
    }
    else {
      $instance = $this->manager->createInstance($condition, []);
    }
    // Build the contexts array so that we can get an internal mapping.
    $contexts = [];
    foreach ($this->getContexts($cached_values) as $context) {
      $contexts[$context['machine_name']] = new Context(ContextDefinition::create($context['context']));
    }
    $form_state->setTemporaryValue('gathered_contexts', $contexts);
    /** @var $instance \Drupal\Core\Condition\ConditionInterface */
    $form = $instance->buildConfigurationForm($form, $form_state);
    if (isset($id)) {
      // Conditionally set this form element so that we can update or add.
      $form['id'] = [
        '#type' => 'value',
        '#value' => $id
      ];
    }
    $form['instance'] = [
      '#type' => 'value',
      '#value' => $instance
    ];
    $form['submit'] = [
      '#type' => 'submit',
      '#value' => $this->t('Save'),
      '#ajax' => [
        'callback' => [$this, 'ajaxSave'],
      ]
    ];
    return $form;
  }

  /**
   * {@inheritdoc}
   */
  public function submitForm(array &$form, FormStateInterface $form_state) {
    $cached_values = $this->tempstore->get($this->tempstore_id)->get($this->machine_name);
    /** @var $instance \Drupal\Core\Condition\ConditionInterface */
    $instance = $form_state->getValue('instance');
    $instance->submitConfigurationForm($form, $form_state);
    $conditions = $this->getConditions($cached_values);
    if ($instance instanceof ContextAwarePluginInterface) {
      $context_mapping = $form_state->hasValue('context_mapping')? $form_state->getValue('context_mapping') : [];
      $instance->setContextMapping($context_mapping);
    }
    if ($form_state->hasValue('id')) {
      $conditions[$form_state->getValue('id')] = $instance->getConfiguration();
    }
    else {
      $conditions[] = $instance->getConfiguration();
    }
    $cached_values = $this->setConditions($cached_values, $conditions);
    $this->tempstore->get($this->tempstore_id)->set($this->machine_name, $cached_values);
    list($route_name, $route_parameters) = $this->getRouteInfo();
    $form_state->setRedirect($route_name, $route_parameters);
  }

  public function ajaxSave(array &$form, FormStateInterface $form_state) {
    $response = new AjaxResponse();
    list($route_name, $route_parameters) = $this->getRouteInfo();
    $response->addCommand(new RedirectCommand($this->url($route_name, $route_parameters)));
    $response->addCommand(new CloseModalDialogCommand());
    return $response;
  }

  /**
   * Document the route name and parameters for redirect after submission.
   *
   * @return array
   *   In the format of
   *   return ['route.name', ['machine_name' => $this->machine_name, 'step' => 'step_name]];
   */
  abstract protected function getRouteInfo();

  /**
   * Custom logic for retrieving the conditions array from cached_values.
   *
   * @param $cached_values
   *
   * @return array
   */
  abstract protected function getConditions($cached_values);

  /**
   * Custom logic for setting the conditions array in cached_values.
   *
   * @param $cached_values
   *
   * @param $conditions
   *   The conditions to set within the cached values.
   *
   * @return mixed
   *   Return the $cached_values
   */
  abstract protected function setConditions($cached_values, $conditions);

  /**
   * Custom logic for retrieving the contexts array from cached_values.
   *
   * @param $cached_values
   *
   * @return array
   */
  abstract protected function getContexts($cached_values);

}