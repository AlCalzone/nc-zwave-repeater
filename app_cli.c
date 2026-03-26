/***************************************************************************//**
 * @file
 * @brief cli_led_bulb.c
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <strings.h>
#include "sl_component_catalog.h"

#ifdef SL_CATALOG_ZW_CLI_COMMON_PRESENT

#include "ZAF_Common_interface.h"
#include "repeater_config_nvm.h"
#include "zaf_event_distributor_soc.h"
#include "sl_cli.h"
#include "sl_sleeptimer.h"
#include "app_log.h"
#include "ev_man.h"
#include "events.h"
#include "zaf_config.h"
#include "zaf_protocol_config.h"
#include "zpal_misc.h"
#include "zpal_radio.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
typedef struct {
  const char *name;
  zpal_radio_region_t region;
} cli_region_name_t;

#define TX_POWER_LIMIT_MIN_DDBM     (-100)
#define TX_POWER_ADJUST_LIMIT_DDBM  (100)
#define CLI_REBOOT_DELAY_MS         (50)

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
static bool sli_try_parse_region_name(const char *value, zpal_radio_region_t *region);
static bool sli_try_parse_tx_power(const char *value, zpal_tx_power_t *tx_power);
static const char *sli_get_region_name(zpal_radio_region_t region);
static void sli_reboot_after_cli_delay(void);
static bool sli_validate_powerlevel(zpal_tx_power_t tx_power_level,
                                    zpal_tx_power_t tx_power_adjust,
                                    zpal_tx_power_t max_tx_power_lr);
static void sli_log_supported_regions(void);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
static const cli_region_name_t sli_region_names[] = {
  { "EU", REGION_EU },
  { "US", REGION_US },
  { "ANZ", REGION_ANZ },
  { "HK", REGION_HK },
  { "IN", REGION_IN },
  { "IL", REGION_IL },
  { "RU", REGION_RU },
  { "CN", REGION_CN },
  { "US_LR", REGION_US_LR },
  { "US-LR", REGION_US_LR },
  { "EU_LR", REGION_EU_LR },
  { "EU-LR", REGION_EU_LR },
  { "JP", REGION_JP },
  { "KR", REGION_KR },
};

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

/******************************************************************************
 * CLI - bootloader: Reboot into bootloader
 *****************************************************************************/
void cli_bootloader(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
  app_log_info("Rebooting into bootloader\r\n");
  zaf_event_distributor_enqueue_app_event(EVENT_APP_BOOTLOADER);
}

/******************************************************************************
 * CLI - set_region: Update the configured RF region token
 *****************************************************************************/
void cli_set_region(sl_cli_command_arg_t *arguments)
{
  zpal_radio_region_t active_region;
  zpal_radio_region_t region;
  const char *region_name;

  if (sl_cli_get_argument_count(arguments) != 1) {
    app_log_info("Usage: set_region <region>\r\n");
    sli_log_supported_regions();
    return;
  }

  region_name = sl_cli_get_argument_string(arguments, 0);
  if (!sli_try_parse_region_name(region_name, &region)) {
    app_log_info("Unknown region '%s'\r\n", region_name);
    sli_log_supported_regions();
    return;
  }

  if (!isRfRegionValid(region)) {
    app_log_info("Region '%s' is not supported by this firmware\r\n", sli_get_region_name(region));
    sli_log_supported_regions();
    return;
  }

  active_region = zpal_radio_get_region();
  if (!SaveApplicationRfRegion(region)) {
    app_log_info("Failed to persist region %s.\r\n", sli_get_region_name(region));
    return;
  }

  if (region == active_region) {
    app_log_info("Configured region set to %s in NVM. Region already active, no reboot required.\r\n",
                 sli_get_region_name(region));
    return;
  }

  app_log_info("Configured region set to %s in NVM. Rebooting to apply the new region.\r\n",
               sli_get_region_name(region));
  sli_reboot_after_cli_delay();
}

/******************************************************************************
 * CLI - get_powerlevel: Read the persisted RF power configuration
 *****************************************************************************/
void cli_get_powerlevel(sl_cli_command_arg_t *arguments)
{
  zpal_tx_power_t tx_power_level;
  zpal_tx_power_t tx_power_adjust;
  zpal_tx_power_t max_tx_power_lr;

  (void) arguments;

  if (!ReadApplicationTxPowerlevel(&tx_power_level, &tx_power_adjust)
      || !ReadApplicationMaxLRTxPwr(&max_tx_power_lr)) {
    app_log_info("Power configuration is not available in NVM.\r\n");
    return;
  }

  app_log_info("iTxPowerLevelMax=%d iTxPowerLevelAdjust=%d iTxPowerLevelMaxLR=%d\r\n",
               (int)tx_power_level,
               (int)tx_power_adjust,
               (int)max_tx_power_lr);
}

/******************************************************************************
 * CLI - set_powerlevel: Update the persisted RF power configuration
 *****************************************************************************/
void cli_set_powerlevel(sl_cli_command_arg_t *arguments)
{
  SRadioConfig_t *radio_config = zaf_get_radio_config();
  zpal_tx_power_t tx_power_level;
  zpal_tx_power_t tx_power_adjust;
  zpal_tx_power_t max_tx_power_lr;
  const char *tx_power_level_str;
  const char *tx_power_adjust_str;
  const char *max_tx_power_lr_str;
  bool requires_reboot;

  if (sl_cli_get_argument_count(arguments) != 3) {
    app_log_info("Usage: set_powerlevel <iTxPowerLevelMax> <iTxPowerLevelAdjust> <iTxPowerLevelMaxLR>\r\n");
    return;
  }

  tx_power_level_str = sl_cli_get_argument_string(arguments, 0);
  tx_power_adjust_str = sl_cli_get_argument_string(arguments, 1);
  max_tx_power_lr_str = sl_cli_get_argument_string(arguments, 2);

  if (!sli_try_parse_tx_power(tx_power_level_str, &tx_power_level)
      || !sli_try_parse_tx_power(tx_power_adjust_str, &tx_power_adjust)
      || !sli_try_parse_tx_power(max_tx_power_lr_str, &max_tx_power_lr)) {
    app_log_info("Invalid integer input. Usage: set_powerlevel <iTxPowerLevelMax> <iTxPowerLevelAdjust> <iTxPowerLevelMaxLR>\r\n");
    return;
  }

  if (!sli_validate_powerlevel(tx_power_level, tx_power_adjust, max_tx_power_lr)) {
    app_log_info("Invalid power levels. iTxPowerLevelMax range: %d..%d, iTxPowerLevelAdjust range: %d..%d, iTxPowerLevelMaxLR range: %d..%d\r\n",
                 TX_POWER_LIMIT_MIN_DDBM,
                 (int)zpal_radio_get_maximum_tx_power(),
                 TX_POWER_LIMIT_MIN_DDBM,
                 TX_POWER_ADJUST_LIMIT_DDBM,
                 (int)zpal_radio_get_minimum_lr_tx_power(),
                 (int)zpal_radio_get_maximum_lr_tx_power());
    return;
  }

  if (!SaveApplicationTxPowerlevel(tx_power_level, tx_power_adjust)
      || !SaveApplicationMaxLRTxPwr(max_tx_power_lr)) {
    app_log_info("Failed to persist power configuration.\r\n");
    return;
  }

  requires_reboot = (radio_config->iTxPowerLevelMax != tx_power_level)
                 || (radio_config->iTxPowerLevelAdjust != tx_power_adjust)
                 || (radio_config->iTxPowerLevelMaxLR != max_tx_power_lr);

  if (!requires_reboot) {
    app_log_info("Power configuration stored in NVM. Values are already active, no reboot required.\r\n");
    return;
  }

  app_log_info("Power configuration stored in NVM. Rebooting to apply the new settings.\r\n");
  sli_reboot_after_cli_delay();
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------

static bool sli_try_parse_region_name(const char *value, zpal_radio_region_t *region)
{
  size_t i;

  for (i = 0; i < (sizeof(sli_region_names) / sizeof(sli_region_names[0])); i++) {
    if (strcasecmp(value, sli_region_names[i].name) == 0) {
      *region = sli_region_names[i].region;
      return true;
    }
  }

  return false;
}

static bool sli_try_parse_tx_power(const char *value, zpal_tx_power_t *tx_power)
{
  char *end_ptr;
  long parsed_value;

  errno = 0;
  parsed_value = strtol(value, &end_ptr, 0);
  if ((0 != errno) || ('\0' != *end_ptr)) {
    return false;
  }

  if ((parsed_value < INT16_MIN) || (parsed_value > INT16_MAX)) {
    return false;
  }

  *tx_power = (zpal_tx_power_t)parsed_value;
  return true;
}

static const char *sli_get_region_name(zpal_radio_region_t region)
{
  switch (region) {
    case REGION_EU:    return "EU";
    case REGION_US:    return "US";
    case REGION_ANZ:   return "ANZ";
    case REGION_HK:    return "HK";
    case REGION_IN:    return "IN";
    case REGION_IL:    return "IL";
    case REGION_RU:    return "RU";
    case REGION_CN:    return "CN";
    case REGION_US_LR: return "US_LR";
    case REGION_EU_LR: return "EU_LR";
    case REGION_JP:    return "JP";
    case REGION_KR:    return "KR";
    default:           return "Unknown";
  }
}

static void sli_reboot_after_cli_delay(void)
{
  sl_sleeptimer_delay_millisecond(CLI_REBOOT_DELAY_MS);
  zpal_reboot_with_info(ZAF_CONFIG_MANUFACTURER_ID, ZPAL_RESET_INFO_DEFAULT);
}

static bool sli_validate_powerlevel(zpal_tx_power_t tx_power_level,
                                    zpal_tx_power_t tx_power_adjust,
                                    zpal_tx_power_t max_tx_power_lr)
{
  return (tx_power_level >= TX_POWER_LIMIT_MIN_DDBM)
      && (tx_power_level <= zpal_radio_get_maximum_tx_power())
      && (tx_power_adjust >= TX_POWER_LIMIT_MIN_DDBM)
      && (tx_power_adjust <= TX_POWER_ADJUST_LIMIT_DDBM)
  && (max_tx_power_lr >= zpal_radio_get_minimum_lr_tx_power())
  && (max_tx_power_lr <= zpal_radio_get_maximum_lr_tx_power());
}

static void sli_log_supported_regions(void)
{
  app_log_info("Accepted region names: EU US ANZ HK IN IL RU CN US_LR EU_LR JP KR\r\n");
}

#endif // SL_CATALOG_ZW_CLI_COMMON_PRESENT
