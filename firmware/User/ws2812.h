#ifndef __WS2812_H
#define __WS2812_H

#include "py32f0xx_hal.h"

// Number of LEDs in the chain (adjust as needed)
#define WS2812_NUM_LEDS 1

// We use PA5 for WS2812 Data
#define WS2812_PORT GPIOA
#define WS2812_PIN  GPIO_PIN_5

void WS2812_Init(void);
void WS2812_SendPixel(uint8_t r, uint8_t g, uint8_t b);
void WS2812_Show(void);

#endif /* __WS2812_H */
