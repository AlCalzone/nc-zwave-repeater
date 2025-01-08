#include "app_led_task.h"
#include "FreeRTOS.h" // needed for queue.h
#include "queue.h"	  // for QueueHandle_t
#include "drivers/ws2812.h"
#include <SizeOf.h>
#include <string.h>
#include "zaf_event_distributor_soc.h"
#include "events.h"

QueueHandle_t xQueue;
// Transmitting a solid color to the LEDs takes about 660µs, so wait a millisecond before issuing the next command
#define SOLID_COLOR_WAIT_TIME_MS 1

void board_indicator_queue_init()
{
	xQueue = xQueueCreate(2, sizeof(IndicatorCommand_t));
}

bool board_indicator_queue_command(IndicatorCommand_t *command)
{
	return pdPASS == xQueueSend(xQueue, command, 0);
}

void Board_IndicatorTask(
	__attribute__((unused)) void *pTaskParam)
{
	IndicatorCommand_t receivedCommand;

	for (;;)
	{
		// Warten Sie, bis eine Struktur aus der Warteschlange empfangen wird
		if (xQueueReceive(
				xQueue,
				&receivedCommand,
				portMAX_DELAY) == pdPASS)
		{
			switch (receivedCommand.mode)
			{
			case INDICATOR_SOLID:
			{
				IndicatorSolid_t solid = receivedCommand.mode_data.solid;
				// copy color to each of colors' entries
				for (int i = 0; i < NUMBER_OF_LEDS; i++)
				{
					LED_CONTROL[i].R = solid.color.R;
					LED_CONTROL[i].G = solid.color.G;
					LED_CONTROL[i].B = solid.color.B;
				}

				zaf_event_distributor_enqueue_app_event(EVENT_APP_LED_CONTROL);
				vTaskDelay(pdMS_TO_TICKS(SOLID_COLOR_WAIT_TIME_MS));
				break;
			}

			case INDICATOR_BLINK:
			{
				IndicatorBlink_t blink = receivedCommand.mode_data.blink;

				// copy color to each of colors' entries
				for (int i = 0; i < NUMBER_OF_LEDS; i++)
				{
					LED_CONTROL[i].R = blink.color.R;
					LED_CONTROL[i].G = blink.color.G;
					LED_CONTROL[i].B = blink.color.B;
				}

				for (uint32_t i = 0; i < blink.num_cycles; i++)
				{
					// set_color_buffer(colors);
					zaf_event_distributor_enqueue_app_event(EVENT_APP_LED_CONTROL);
					vTaskDelay(pdMS_TO_TICKS(blink.on_time_ms));

					// set_color_buffer(off);
					zaf_event_distributor_enqueue_app_event(EVENT_APP_LED_OFF);
					vTaskDelay(pdMS_TO_TICKS(blink.off_time_ms));

					// When blinking indefinitely, abort when the next command is received
					if (blink.num_cycles == BLINK_INDEFINITELY && uxQueueMessagesWaiting(xQueue) > 0)
					{
						break;
					}
				}
				if (blink.stay_on && blink.num_cycles != BLINK_INDEFINITELY)
				{
					// set_color_buffer(colors);
					zaf_event_distributor_enqueue_app_event(EVENT_APP_LED_CONTROL);
					vTaskDelay(pdMS_TO_TICKS(SOLID_COLOR_WAIT_TIME_MS));
				}

				break;
			}
			}
		}
	}
}
