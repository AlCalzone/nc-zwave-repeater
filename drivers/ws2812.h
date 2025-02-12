/*
 * ws2812.h
 *
 *  Created on: 2024年11月1日
 *      Author: qian
 */

#ifndef WS2812_H_
#define WS2812_H_

#include <stdint.h>
#include <stdlib.h>
#include "em_eusart.h"
#include "em_ldma.h"
#include "em_gpio.h"
#include "em_cmu.h"
#include "pin_config.h"

#define EUS1MOSI_PORT   EUSART1_TX_PORT
#define EUS1MOSI_PIN    EUSART1_TX_PIN
#define EUS1SCLK_PORT   EUSART1_SCLK_PORT
#define EUS1SCLK_PIN    EUSART1_SCLK_PIN
// #define WS2812_EN_PORT   WS2812_EN_PORT
// #define WS2812_EN_PIN    WS2812_EN_PIN

// NEEDS TO BE USER DEFINED
#ifndef NUMBER_OF_LEDS
#define NUMBER_OF_LEDS         4
#endif

typedef struct rgb_t{
  uint8_t G, R, B;
}rgb_t;

static const rgb_t red = { 0x00, 0xFF, 0x00 };
static const rgb_t green = { 0xFF, 0x00, 0x00 };
static const rgb_t blue = { 0x00, 0x00, 0xFF };
static const rgb_t yellow = { 0xFF, 0xFF, 0x00 };
static const rgb_t magenta = { 0x00, 0xFF, 0xFF };
static const rgb_t cyan = { 0xFF, 0x00, 0xFF };
static const rgb_t white = { 0xFF, 0xFF, 0xFF };
static const rgb_t black = { 0x00, 0x00, 0x00 };

void get_color_buffer(rgb_t *output_colors);
void set_color_buffer(const rgb_t *input_colors);
void initWs2812(void);
// void color_test();

#endif /* WS2812_H_ */
