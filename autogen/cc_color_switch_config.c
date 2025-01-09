/**
 * @file
 * Command Class Color Switch Configuration
 * @remarks This file is auto generated
 * @copyright 2022 Silicon Laboratories Inc.
 */
#include <cc_color_switch_config.h>

#include <cc_color_switch_config_api.h>
#include <CC_ColorSwitch.h>
#include <ZW_typedefs.h>
#include <SizeOf.h>
#include <assert.h>

void cc_color_switch_cb(struct colorComponent_ *color);

static s_colorComponent s_colorComponents[] = {
  {
    .ep = 0,
    .colorId = ECOLORCOMPONENT_RED,
    .cb = cc_color_switch_cb
  },
  {
    .ep = 0,
    .colorId = ECOLORCOMPONENT_GREEN,
    .cb = cc_color_switch_cb
  },
  {
    .ep = 0,
    .colorId = ECOLORCOMPONENT_BLUE,
    .cb = cc_color_switch_cb
  },
};

_Static_assert((sizeof_array(s_colorComponents) > 0), "STATIC_ASSERT_FAILED_size_mismatch");

ZW_WEAK void cc_color_switch_cb(__attribute__((unused)) s_colorComponent * colorComponent)
{
}

s_colorComponent *cc_color_switch_get_colorComponents(void)
{
  return s_colorComponents;
}

uint8_t cc_color_switch_get_length_colorComponents(void)
{
  return sizeof_array(s_colorComponents);
}

uint8_t cc_color_switch_get_default_duration(void)
{
  return CC_COLOR_SWITCH_CONFIG_DEFAULT_DURATION_SEC;
}

ZAF_CC_REGISTER_CONFIG(COMMAND_CLASS_SWITCH_COLOR, &s_colorComponents[0], 0);
ZAF_CC_REGISTER_CONFIG(COMMAND_CLASS_SWITCH_COLOR, &s_colorComponents[1], 1);
ZAF_CC_REGISTER_CONFIG(COMMAND_CLASS_SWITCH_COLOR, &s_colorComponents[2], 2);
