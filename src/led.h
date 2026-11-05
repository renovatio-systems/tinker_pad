#ifndef LED_H
#define LED_H

#include <stdint.h>

#include "pico/types.h"

// Drives one WS2812B-style RGB LED on `pin` through PIO. Must be called before led_set().
void led_init(uint pin);

// Sets the LED colour, 0 to 255 per channel.
void led_set(uint8_t r, uint8_t g, uint8_t b);

#endif
