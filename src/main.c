// Blank canvas: each tick, game.c updates from the buttons and draws a frame,
// which is sent to the panel.
//
// Hold MID while powering on to show the display diagnostic instead of the game.
// Use it to check the wiring and the panel settings in config.h.

#include <stdint.h>

#include "config.h"
#include "game.h"
#include "input.h"
#include "pico/stdlib.h"
#include "st7789.h"

// Double-buffered: the game draws one frame while DMA sends the other.
static uint16_t frames[2][GAME_W * GAME_H];

static st7789_config_t display_config(void) {
    return (st7789_config_t){
        .spi = TFT_SPI,
        .pin_sck = PIN_SCK,
        .pin_mosi = PIN_MOSI,
        .pin_cs = PIN_CS,
        .pin_dc = PIN_DC,
        .pin_rst = PIN_RST,
        .pin_bl = PIN_BL,
        .baud = SPI_BAUD,
        .x_off = ST7789_X_OFFSET,
        .y_off = ST7789_Y_OFFSET,
        .madctl = ST7789_MADCTL,
        .invert = ST7789_INVERT,
    };
}

// Red, green and blue bands top to bottom, with a white border, so orientation
// and colour order can be checked at a glance.
static void diag_pattern(uint16_t *fb) {
    const uint16_t bands[3] = {RGB565(255, 0, 0), RGB565(0, 255, 0), RGB565(0, 0, 255)};
    const int band_h = GAME_H / 3;
    for (int y = 0; y < GAME_H; y++) {
        uint16_t c = bands[y < band_h ? 0 : y < 2 * band_h ? 1 : 2];
        for (int x = 0; x < GAME_W; x++) fb[y * GAME_W + x] = c;
    }
    for (int x = 0; x < GAME_W; x++) {
        fb[x] = RGB565(255, 255, 255);
        fb[(GAME_H - 1) * GAME_W + x] = RGB565(255, 255, 255);
    }
    for (int y = 0; y < GAME_H; y++) {
        fb[y * GAME_W] = RGB565(255, 255, 255);
        fb[y * GAME_W + GAME_W - 1] = RGB565(255, 255, 255);
    }
}

// Alternates the pattern with solid red, green, blue, white and black. Runs until reset.
static void diagnostic(void) {
    uint16_t *const fb = frames[0];
    const uint16_t solids[5] = {
        RGB565(255, 0, 0), RGB565(0, 255, 0), RGB565(0, 0, 255), RGB565(255, 255, 255), 0,
    };
    for (unsigned i = 0;; i = (i + 1) % 5) {
        diag_pattern(fb);
        st7789_send_frame(fb);
        sleep_ms(1500);

        for (int p = 0; p < GAME_W * GAME_H; p++) fb[p] = solids[i];
        st7789_send_frame(fb);
        sleep_ms(1500);
    }
}

int main(void) {
    input_init();
    st7789_config_t cfg = display_config();
    st7789_init(&cfg);

    // Debounce needs the level to hold for a few ms, so sample MID twice.
    input_poll(0);
    sleep_ms(30);
    input_poll(30);
    if (input_held() & JOY_MID) diagnostic();

    game_init();

    // Fixed timestep on an absolute grid. After a long stall the grid is re-anchored.
    const uint64_t tick_us = 1000000 / GAME_TICK_HZ;
    uint64_t next_tick = time_us_64();
    int cur = 0;
    while (true) {
        uint64_t now = time_us_64();
        input_poll((uint32_t)(now / 1000));

        if (now >= next_tick) {
            game_update(input_held(), input_pressed());
            next_tick += tick_us;
            if (now >= next_tick) next_tick = now + tick_us;

            // Draw into the buffer DMA is not reading, then send it. The send waits
            // for the previous frame to finish, so the buffers never overlap.
            game_draw(frames[cur]);
            st7789_send_frame(frames[cur]);
            cur ^= 1;
        }
    }
}
