/**
 * Z-Wave Application LED Bulb
 *
 * @copyright 2020 Silicon Laboratories Inc.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "MfgTokens.h"
#include "DebugPrintConfig.h"
//#define DEBUGPRINT
#include "DebugPrint.h"
#include "ZW_system_startup_api.h"
#include "ZAF_Common_helper.h"
#include "ZAF_Common_interface.h"
#include "ZAF_network_learn.h"
#include "events.h"
#include "zpal_watchdog.h"
#include "app_hw.h"
#include "board_indicator.h"
#include "zw_region_config.h"
#include "ZAF_ApplicationEvents.h"
#include "zaf_event_distributor_soc.h"
#include "zpal_misc.h"
#include "zaf_protocol_config.h"
#ifdef DEBUGPRINT
#include "ZAF_PrintAppInfo.h"
#endif
#include "ZW_UserTask.h"
#include "app_led_task.h"
#include "CC_ColorSwitch.h"
#include "cc_color_switch_config_api.h"
#include "cc_color_switch_io.h"
#include "board_indicator_control.h"

#ifdef SL_CATALOG_ZW_CLI_COMMON_PRESENT
#include "zw_cli_common.h"
#endif

#if (!defined(SL_CATALOG_SILICON_LABS_ZWAVE_APPLICATION_PRESENT) && !defined(UNIT_TEST))
#include "app_hw.h"
#endif

static void ApplicationTask(SApplicationHandles* pAppHandles);

#define LED_TASK_STACK_SIZE           200  // [bytes]
static TaskHandle_t m_xTaskHandleLED   = NULL;
// Task and stack buffer allocation for the default/main application task!
static StaticTask_t LEDTaskBuffer;
static uint8_t LEDStackBuffer[LED_TASK_STACK_SIZE];

rgb_t LED_CONTROL[NUMBER_OF_LEDS] = {0};
bool m_indicator_active_from_cc = false;

bool restore_color_switch_cc_state() {
  s_colorComponent* components = cc_color_switch_get_colorComponents();
  for (int i = 0; i < cc_color_switch_get_length_colorComponents(); i++) {
    bool result = cc_color_switch_read(i, &components[i]);
    if (!result) {
      return false;
    }
  }

  uint8_t red = ZAF_Actuator_GetCurrentValue(&components[0].obj);
  uint8_t green = ZAF_Actuator_GetCurrentValue(&components[1].obj);
  uint8_t blue = ZAF_Actuator_GetCurrentValue(&components[2].obj);

  if (red != 0 || green != 0 || blue != 0) {
    rgb_t color = {
        .R = red,
        .G = green,
        .B = blue
    };
    set_idle_color(&color);
  }

  return true;
}


/**
 * @brief See description for function prototype in ZW_basis_api.h.
 */
ZW_APPLICATION_STATUS ApplicationInit(__attribute__((unused)) zpal_reset_reason_t eResetReason)
{
  SRadioConfig_t* RadioConfig;

  zpal_enable_watchdog(true);

#ifdef DEBUGPRINT
  static uint8_t m_aDebugPrintBuffer[96];
  DebugPrintConfig(m_aDebugPrintBuffer, sizeof(m_aDebugPrintBuffer), zpal_debug_output);
  DebugPrintf("ApplicationInit eResetReason = %d\n", eResetReason);
#endif

  RadioConfig = zaf_get_radio_config();

  // Read Rf region from MFG_ZWAVE_COUNTRY_FREQ
  zpal_radio_region_t regionMfg;
  ZW_GetMfgTokenDataCountryFreq((void*) &regionMfg);
  if (isRfRegionValid(regionMfg)) {
    RadioConfig->eRegion = regionMfg;
  } else {
    ZW_SetMfgTokenDataCountryRegion((void*) &RadioConfig->eRegion);
  }

  /*
   * Register the main application task.
   *
   * Attention: this is the only FreeRTOS task that can invoke the ZAF API.
   *
   * ZW_UserTask_CreateTask() can be used to create additional application tasks. See the
   * Sensor PIR application for an example use of ZW_UserTask_CreateTask().
   */
  __attribute__((unused)) bool bWasTaskCreated = ZW_ApplicationRegisterTask(
    ApplicationTask,
    EAPPLICATIONEVENT_ZWRX,
    EAPPLICATIONEVENT_ZWCOMMANDSTATUS,
    zaf_get_protocol_config()
    );
  assert(bWasTaskCreated);

  //
  // Create LED background task
  //
  board_indicator_queue_init();

  // Interact with the hardware in a background task
  ZW_UserTask_Buffer_t ledTaskBuffer;
  ledTaskBuffer.taskBuffer = &LEDTaskBuffer;
  ledTaskBuffer.stackBuffer = LEDStackBuffer;
  ledTaskBuffer.stackBufferLength = LED_TASK_STACK_SIZE;

  // Create the task setting-structure!
  ZW_UserTask_t task;
  task.pTaskFunc = (TaskFunction_t)Board_IndicatorTask;
  task.pTaskName = "LED";
  task.pUserTaskParam = NULL;
  task.priority = USERTASK_PRIORITY_NORMAL;
  task.taskBuffer = &ledTaskBuffer;

  // Create the task!
  ZW_UserTask_CreateTask(&task, &m_xTaskHandleLED);

  return (APPLICATION_RUNNING);
}

/**
 * A pointer to this function is passed to ZW_ApplicationRegisterTask() making it the FreeRTOS
 * application task.
 */
static void ApplicationTask(SApplicationHandles* pAppHandles)
{
  uint32_t unhandledEvents = 0;
  ZAF_Init(xTaskGetCurrentTaskHandle(), pAppHandles);

#ifdef DEBUGPRINT
  ZAF_PrintAppInfo();
#endif

  // Restore Color Switch CC state from NVM - if that fails, use the default idle color
  if (!restore_color_switch_cc_state()) {
    Board_IndicateStatus(BOARD_STATUS_IDLE);
  }

  // Initialize other NC-specific hardware

  /* Enter SmartStart*/
  /* Protocol will commence SmartStart only if the node is NOT already included in the network */
  ZAF_setNetworkLearnMode(E_NETWORK_LEARN_MODE_INCLUSION_SMARTSTART);

  // Wait for and process events
  DPRINT("LED Bulb Event processor Started\r\n");
  for (;;)
  {
    unhandledEvents = zaf_event_distributor_distribute();
    if (0 != unhandledEvents)
    {
      DPRINTF("Unhandled Events: 0x%08lx\n", unhandledEvents);
#ifdef UNIT_TEST
      return;
#endif
    }
  }
}

/**
 * @brief The core state machine of this sample application.
 * @param event The event that triggered the call of zaf_event_distributor_app_event_manager.
 */
void zaf_event_distributor_app_event_manager(const uint8_t event)
{
  DPRINTF("zaf_event_distributor_app_event_manager Ev: %d\r\n", event);

  switch (event)
  {
  case EVENT_APP_LED_CONTROL:
    set_color_buffer(LED_CONTROL);
    break;

  case EVENT_APP_LED_OFF:
    // For some reason we cannot use a static here, because its last byte gets overwritten somehow
    set_color_buffer((rgb_t[NUMBER_OF_LEDS]){0});
    break;

  case EVENT_APP_CLEAR_INDICATOR_FLAG:
    m_indicator_active_from_cc = false;
    break;

  case EVENT_APP_BOOTLOADER:

    bootloader_rebootAndInstall();
    break;

  default:
    // Unknown event - do nothing.
    break;
  }

#ifdef SL_CATALOG_ZW_CLI_COMMON_PRESENT
  cli_log_system_events(event);
#endif
}

// Gets called when the current color was changed through Color Switch CC
void cc_color_switch_cb(s_colorComponent *colorComponent)
{

  // Get the current color
  s_colorComponent *components = cc_color_switch_get_colorComponents();
  uint8_t red = ZAF_Actuator_GetTargetValue(&components[0].obj);
  uint8_t green = ZAF_Actuator_GetTargetValue(&components[1].obj);
  uint8_t blue = ZAF_Actuator_GetTargetValue(&components[2].obj);

  // And merge it with the new value
  uint8_t new_value = ZAF_Actuator_GetTargetValue(&colorComponent->obj);
  switch (colorComponent->colorId)
  {
  case ECOLORCOMPONENT_RED:
    red = new_value;
    break;
  case ECOLORCOMPONENT_GREEN:
    green = new_value;
    break;
  case ECOLORCOMPONENT_BLUE:
    blue = new_value;
    break;
  default:
    // Unsupported color changed
    return;
  }

  rgb_t color = {
      green, red, blue};
  indicator_solid(&color);
}