#include <stdint.h>
#include <stdbool.h>
#include "drivers/ws2812.h"

typedef struct {
	rgb_t color;
} IndicatorSolid_t;

typedef struct {
	rgb_t color;
	uint32_t on_time_ms;
	uint32_t off_time_ms;
	uint32_t num_cycles;
	bool stay_on;
} IndicatorBlink_t;

#define BLINK_INDEFINITELY UINT32_MAX

typedef enum {
	INDICATOR_SOLID,
	INDICATOR_BLINK,
} IndicatorMode_t;

typedef struct {
	IndicatorMode_t mode;
	union {
		IndicatorBlink_t blink;
		IndicatorSolid_t solid;
	} mode_data;
} IndicatorCommand_t;

void board_indicator_queue_init();
bool board_indicator_queue_command(IndicatorCommand_t *command);
void Board_IndicatorTask(void *pTaskParam);

extern rgb_t LED_CONTROL[NUMBER_OF_LEDS];
