#include "drivers/ws2812.h"

#ifndef BOARD_INDICATOR_CONTROL_H
#define BOARD_INDICATOR_CONTROL_H

void set_idle_color(rgb_t *color);

bool indicator_solid(rgb_t *color);
bool indicator_blink(rgb_t *color, uint32_t on_time_ms, uint32_t off_time_ms, uint32_t num_cycles, bool stay_on);

#endif // BOARD_INDICATOR_CONTROL_H