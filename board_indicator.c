// A modification of protocol/z-wave/platform/SiliconLabs/AppsHw/src/common/board_indicator.c
// to work with our custom LEDs

#include "board_indicator.h"
#include "drivers/ws2812.h"
#include "app_led_task.h"

static bool m_indicator_active_from_cc = false;
rgb_t IDLE_COLOR = {4, 0, 0};
rgb_t OFF_COLOR = {0, 0, 0};
rgb_t LEARNMODE_COLOR = {255, 0, 255};

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
	return board_indicator_queue_command(&command);
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
	// TODO
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
	// TODO run the LED stuff on background task
	(void)called_from_indicator_cc;
	// TODO: m_indicator_active_from_cc = called_from_indicator_cc;

	rgb_t colors[NUMBER_OF_LEDS] = {};
	get_color_buffer(colors);

	return indicator_blink(&colors[0], on_time_ms, off_time_ms, num_cycles, false);
}

bool Board_IsIndicatorActive(void)
{
	return m_indicator_active_from_cc;
}
