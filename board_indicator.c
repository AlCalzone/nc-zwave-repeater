// A modification of protocol/z-wave/platform/SiliconLabs/AppsHw/src/common/board_indicator.c
// to work with our custom LEDs

#include "board_indicator.h"
#include "board_indicator_control.h"
#include "drivers/ws2812.h"
#include "app_led_task.h"
#include "CC_ColorSwitch.h"

extern bool m_indicator_active_from_cc;
rgb_t IDLE_COLOR = {4, 0, 0};
rgb_t OFF_COLOR = {0, 0, 0};
rgb_t LEARNMODE_COLOR = {255, 0, 255};
rgb_t DEFAULT_COLOR = {255, 0, 0};

void set_idle_color(rgb_t *color)
{
	IDLE_COLOR.R = color->R;
	IDLE_COLOR.G = color->G;
	IDLE_COLOR.B = color->B;
}

uint8_t cc_color_switch_get_default_value(s_colorComponent* colorComponent) {
	switch (colorComponent->colorId) {
		case ECOLORCOMPONENT_RED:
			return IDLE_COLOR.R;
		case ECOLORCOMPONENT_GREEN:
			return IDLE_COLOR.G;
		case ECOLORCOMPONENT_BLUE:
			return IDLE_COLOR.B;
		default:
			return 0;
	}
}

bool indicator_solid(rgb_t *color)
{
	IndicatorSolid_t solid = {
		.color = {
			.R = color->R,
			.G = color->G,
			.B = color->B,
		},
	};
	IndicatorCommand_t command = {
		.mode = INDICATOR_SOLID,
		.mode_data.solid = solid,
	};

	bool result = board_indicator_queue_command(&command);
	m_indicator_active_from_cc = false;
	return result;
}

bool indicator_blink(rgb_t *color, uint32_t on_time_ms, uint32_t off_time_ms, uint32_t num_cycles, bool stay_on)
{
	IndicatorBlink_t blink = {
		.color = {
			.R = color->R,
			.G = color->G,
			.B = color->B,
		},
		.on_time_ms = on_time_ms,
		.off_time_ms = off_time_ms,
		.num_cycles = num_cycles,
		.stay_on = stay_on,
	};
	IndicatorCommand_t command = {
		.mode = INDICATOR_BLINK,
		.mode_data.blink = blink,
	};
	return board_indicator_queue_command(&command);
}

void Board_IndicateStatus(board_status_t status)
{
	if (status == BOARD_STATUS_POWER_DOWN)
	{
		indicator_solid(&OFF_COLOR);
	}
	else if (status == BOARD_STATUS_LEARNMODE_ACTIVE)
	{
		indicator_blink(
			&LEARNMODE_COLOR,
			500,
			500,
			BLINK_INDEFINITELY,
			false);
	}
	else
	{
		indicator_solid(&IDLE_COLOR);
	}
}

void Board_IndicatorInit(void)
{
	initWs2812();
}

bool Board_IndicatorControl(uint32_t on_time_ms,
							uint32_t off_time_ms,
							uint32_t num_cycles,
							bool called_from_indicator_cc)
{

	// Remember original color to be restored later
	rgb_t colors[NUMBER_OF_LEDS] = {0};
	get_color_buffer(colors);

	// Indicator CC indicates indefinite blinking with 0
	if (num_cycles == 0)
	{
		num_cycles = BLINK_INDEFINITELY;
	}

	bool result;
	if (called_from_indicator_cc || (colors[0].R == 0 && colors[0].G == 0 && colors[0].B == 0))
	{
		result = indicator_blink(&DEFAULT_COLOR, on_time_ms, off_time_ms, num_cycles, false);
	}
	else
	{
		result = indicator_blink(&colors[0], on_time_ms, off_time_ms, num_cycles, false);
	}
	if (result)
	{
		m_indicator_active_from_cc = called_from_indicator_cc;
	}

	if (m_indicator_active_from_cc && num_cycles != BLINK_INDEFINITELY)
	{
		indicator_solid(&colors[0]);
	}

	return result;
}

bool Board_IsIndicatorActive(void)
{
	return m_indicator_active_from_cc;
}

void cc_indicator_handler(uint32_t on_time_ms, uint32_t off_time_ms, uint32_t num_cycles)
{
	// V1 indicator handling
	if (num_cycles == 0 && on_time_ms == 0 && off_time_ms == 0) {
		indicator_solid(&IDLE_COLOR);
		return;
	} else if (num_cycles == 0xff && on_time_ms == 0xff && off_time_ms == 0xff) {
		indicator_solid(&DEFAULT_COLOR);
		return;
	}
	
	if (num_cycles == 0)
	{
		num_cycles = BLINK_INDEFINITELY;
	}

	// Restore original color after blinking
	rgb_t colors[NUMBER_OF_LEDS] = {0};
	get_color_buffer(colors);

	m_indicator_active_from_cc = indicator_blink(&DEFAULT_COLOR, on_time_ms, off_time_ms, num_cycles, false);

	if (num_cycles != BLINK_INDEFINITELY)
	{
		indicator_solid(&colors[0]);
	}
}
