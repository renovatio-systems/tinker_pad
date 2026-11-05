#include "led.h"

#include "hardware/pio.h"
#include "ws2812.pio.h"

static PIO led_pio = pio0;
static uint led_sm;

void led_init(uint pin) {
    uint offset = pio_add_program(led_pio, &ws2812_program);
    led_sm = pio_claim_unused_sm(led_pio, true);
    ws2812_program_init(led_pio, led_sm, offset, pin, 800000, false);
}

void led_set(uint8_t r, uint8_t g, uint8_t b) {
    // The PIO shifts the top 24 bits out first. This board's LED reads red, green,
    // blue in that order (the usual WS2812B reads green first, so the first and
    // second colours swapped on the first test).
    uint32_t rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    pio_sm_put_blocking(led_pio, led_sm, rgb << 8u);
}
