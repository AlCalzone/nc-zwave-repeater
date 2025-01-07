/*
 * ws2812.c
 *
 *  Created on: 2024年11月1日
 *      Author: qian
 */


// BSP for board controller pin macros
#include "ws2812.h"
#include "pin_config.h"
#include <string.h>
#include "dmadrv.h"

// Frequency for the protocol in Hz, 800 kHz gives a 1.25uS duty cycle
#define PROTOCOL_FREQUENCY       800000

// 3 USART bits are required to make a full 1.25uS color bit
// USART frequency should therefore be 3x the protocol frequency
#define REQUIRED_USART_FREQUENCY (PROTOCOL_FREQUENCY * 3)

// 3 color channels, 8 bits each
#define NUMBER_OF_COLOR_BITS     (NUMBER_OF_LEDS * 3 * 8)

// 3 USART bits are required to make a full 1.25uS color bit,
// each USART bit is 416nS
#define USART_NUMBER_OF_BITS     (NUMBER_OF_COLOR_BITS * 3)

#define LATCH_BYTES 15

// How big the USART buffer should be,
// the last 15 bytes should be empty to provide a 50uS reset signal.
// For whatever reason, the first bit is not recognized if we don't also send zeroes
// before it, so we have an additional 15 bytes of zeroes at the beginning.
#define USART_BUFFER_SIZE_BYTES  (LATCH_BYTES + (USART_NUMBER_OF_BITS / 8) + LATCH_BYTES)

// Output buffer for USART
static uint8_t USART_tx_buffer[USART_BUFFER_SIZE_BYTES] = {0};
static rgb_t rgb_color_buffer[NUMBER_OF_LEDS];

unsigned int channel;

// The WS2812 protocol interprets a signal that is 2/3 high 1/3 low as 1
// and 1/3 high 2/3 low as 0. This can be done by encoding each bit as 3 bits,
// where 110 is high and 100 is low.
// Therefore, the full 3-byte sequence for a color byte is
// 0x1_01 _01_ 01_0 1_01 _01_ 01_0
#define FIRST_BYTE_DEFAULT  0b10010010;
//                     bits:   7  6  5
#define SECOND_BYTE_DEFAULT 0b01001001;
//                     bits:    4  3
#define THIRD_BYTE_DEFAULT  0b00100100;
//                     bits:  2  1  0

/**************************************************************************//**
 * @brief
 *    CMU initialization
 *****************************************************************************/
static void initCMU(void)
{
  // Enable clock to GPIO and EUSART1
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_EUSART1, true);
}

/**************************************************************************//**
 * @brief
 *    GPIO initialization
 *****************************************************************************/
static void initGPIO(void)
{
  // Configure MOSI (TX) pin as an output, idle low (0)
  GPIO_PinModeSet(EUS1MOSI_PORT, EUS1MOSI_PIN, gpioModePushPull, 0);

  // Configure SCLK pin as an output low (CPOL = 0)
  GPIO_PinModeSet(EUS1SCLK_PORT, EUS1SCLK_PIN, gpioModePushPull, 0);

  // Pull enable pin high to enable the WS2812
  GPIO_PinModeSet(WS2812_EN_PORT, WS2812_EN_PIN, gpioModePushPull, 1);
}

/**************************************************************************//**
 * @brief
 *    EUSART1 initialization
 *****************************************************************************/
static void initEUSART1(void)
{
  // SPI advanced configuration (part of the initializer)
  EUSART_SpiAdvancedInit_TypeDef adv = EUSART_SPI_ADVANCED_INIT_DEFAULT;

  adv.msbFirst = true;        // SPI standard MSB first
//  adv.invertIO = eusartInvertTxEnable;
//  adv.autoInterFrameTime = 7; // 7 bit times of delay between frames
//                              // to accommodate non-DMA secondaries

  // Default asynchronous initializer (main/master mode and 8-bit data)
  EUSART_SpiInit_TypeDef init = EUSART_SPI_MASTER_INIT_DEFAULT_HF;

  init.bitRate = REQUIRED_USART_FREQUENCY;
  // init.loopbackEnable = eusartLoopbackEnable;
  init.advancedSettings = &adv;   // Advanced settings structure

  /*
   * Route EUSART1 MOSI, MISO, and SCLK to the specified pins.  CS is
   * not controlled by EUSART1 so there is no write to the corresponding
   * EUSARTROUTE register to do this.
   */
  GPIO->EUSARTROUTE[1].TXROUTE = (EUS1MOSI_PORT << _GPIO_EUSART_TXROUTE_PORT_SHIFT)
      | (EUS1MOSI_PIN << _GPIO_EUSART_TXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[1].SCLKROUTE = (EUS1SCLK_PORT << _GPIO_EUSART_SCLKROUTE_PORT_SHIFT)
      | (EUS1SCLK_PIN << _GPIO_EUSART_SCLKROUTE_PIN_SHIFT);

  // Enable EUSART interface pins
  GPIO->EUSARTROUTE[1].ROUTEEN = GPIO_EUSART_ROUTEEN_TXPEN |    // MOSI
                                 GPIO_EUSART_ROUTEEN_SCLKPEN;

  // Configure and enable EUSART1
  EUSART_SpiInit(EUSART1, &init);
}

// /**************************************************************************//**
//  * @brief
//  *    LDMA initialization
//  *****************************************************************************/
void initLDMA(void)
{
  // Initialize the LDMA unit itself and request a channel
  DMADRV_Init();
  DMADRV_AllocateChannel(&channel, NULL);
}

void initWs2812(void)
{
  // Initialize GPIO and USART0
  initCMU();
  initGPIO();
  initEUSART1();
  initLDMA();
}

void get_color_buffer(rgb_t *output_colors)
{
  // copy the contents of rgb_color_buffer to output_colors
  memcpy(output_colors, rgb_color_buffer, sizeof(rgb_color_buffer));
}

void set_color_buffer(const rgb_t *input_colors)
{
  // Remember the current color for later querying
  memcpy(rgb_color_buffer, input_colors, sizeof(rgb_color_buffer));

  // See above for a more detailed description of the protocol and bit order
  const uint8_t *input_color_byte = (uint8_t *)input_colors;
  uint32_t usart_buffer_index = LATCH_BYTES; // start filling the buffer after the first zero segment
  while (usart_buffer_index < USART_BUFFER_SIZE_BYTES - 2) {
    // FIRST BYTE
    // Isolate bit 7 and shift to position 6
    uint8_t bit_7 = (uint8_t)((*input_color_byte & 0x80) >> 1);
    // Isolate bit 6 and shift to position 3
    uint8_t bit_6 = (uint8_t)((*input_color_byte & 0x40) >> 3);
    // Isolate bit 5 and shift to position 0
    uint8_t bit_5 = (uint8_t)((*input_color_byte & 0x20) >> 5);
    // Load byte into the TX buffer
    USART_tx_buffer[usart_buffer_index] = bit_7 | bit_6 | bit_5 | FIRST_BYTE_DEFAULT;
    usart_buffer_index++;  // Increment USART_tx_buffer pointer

    // SECOND BYTE
    // Isolate bit 4 and shift to position 5
    uint8_t bit_4 = (uint8_t)((*input_color_byte & 0x10) << 1);
    // Isolate bit 3 and shift to position 2
    uint8_t bit_3 = (uint8_t)((*input_color_byte & 0x08) >> 1);
    // Load byte into the TX buffer
    USART_tx_buffer[usart_buffer_index] = bit_4 | bit_3 | SECOND_BYTE_DEFAULT;
    usart_buffer_index++; // Increment USART_tx_buffer pointer

    // THIRD BYTE
    // Isolate bit 2 and shift to position 7
    uint8_t bit_2 = (uint8_t)((*input_color_byte & 0x04) << 5);
    // Isolate bit 1 and shift to position 4
    uint8_t bit_1 = (uint8_t)((*input_color_byte & 0x02) << 3);
    // Isolate bit 0 and shift to position 1
    uint8_t bit_0 = (uint8_t)((*input_color_byte & 0x01) << 1);
    // Load byte into the TX buffer
    USART_tx_buffer[usart_buffer_index] = bit_2 | bit_1 | bit_0 | THIRD_BYTE_DEFAULT;
    usart_buffer_index++; // Increment USART_tx_buffer pointer
    
    input_color_byte++; // move to the next color byte
  }

  DMADRV_MemoryPeripheral(
    channel,
    ldmaPeripheralSignal_EUSART1_TXFL,
    (void*) &(EUSART1->TXDATA),
    USART_tx_buffer,
    true,
    USART_BUFFER_SIZE_BYTES,
    dmadrvDataSize1,
    NULL,
    NULL
  );
}
