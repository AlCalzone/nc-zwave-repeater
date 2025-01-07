/**
 * @file
 * Command Class Multilevel Switch Configuration
 * @remarks This file is auto generated
 * @copyright 2022 Silicon Laboratories Inc.
 */
#include <cc_multilevel_switch_support_config.h>

#include <CC_MultilevelSwitch_Support.h>

#include <ZW_typedefs.h>
#include <SizeOf.h>
#include <assert.h>

static cc_multilevel_switch_t s_switches[] = {
  {
    .endpoint = 0,
  },
};

_Static_assert((sizeof_array(s_switches) > 0), "STATIC_ASSERT_FAILED_size_mismatch");

cc_multilevel_switch_t *cc_multilevel_switch_support_config_get_switches(void)
{
  return s_switches;
}

uint8_t cc_multilevel_switch_support_config_get_length_switches(void)
{
  return sizeof_array(s_switches);
}

uint8_t cc_multilevel_switch_support_config_get_default_duration(void)
{
  return CC_MULTILEVEL_SWITCH_CONFIG_DEFAULT_DURATION_SEC;
}

ZAF_CC_REGISTER_CONFIG(COMMAND_CLASS_SWITCH_MULTILEVEL, &s_switches[0], 0);
