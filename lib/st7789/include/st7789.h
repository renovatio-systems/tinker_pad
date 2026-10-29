#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>
#include "hardware/spi.h"

// Logical panel size: the 2.25" module is 76 x 284 in portrait.
#define ST7789_WIDTH  76
#define ST7789_HEIGHT 284

typedef struct {
    spi_inst_t *spi;
    uint pin_sck;
    uint pin_mosi;
    uint pin_cs;
    uint pin_dc;
    uint pin_rst;
    int pin_bl;      // backlight GPIO, or -1 if LED is tied to 3V3
    uint baud;       // requested SPI clock in Hz
    uint16_t x_off; // RAM column of the first visible pixel (unverified, see README)
    uint16_t y_off; // RAM row of the first visible pixel (unverified, see README)
    uint8_t madctl; // memory access control, 0x00 = portrait RGB, 0x08 = BGR
    bool invert;    // send INVON: most IPS modules need it for correct colours
} st7789_config_t;

// Initialises SPI, GPIOs and the panel. Returns the actual SPI clock in Hz.
uint st7789_init(const st7789_config_t *cfg);

// Sends a full ST7789_WIDTH x ST7789_HEIGHT frame of native RGB565 pixels by
// DMA and returns once it is queued. `pixels` must stay untouched until
// st7789_wait() returns, and every other call waits for the transfer first.
void st7789_send_frame(const uint16_t *pixels);
void st7789_wait(void);

// Blocking fill of a rectangle, for splash and diagnostic screens.
void st7789_fill_rect(int x, int y, int w, int h, uint16_t color);

// Blocking fill in controller RAM coordinates, ignoring the panel offsets. Covers
// the whole 240 x 320 RAM, for finding where the visible area sits.
void st7789_fill_ram(int x, int y, int w, int h, uint16_t color);

#endif
