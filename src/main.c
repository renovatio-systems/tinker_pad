// Blank canvas: each tick, game.c updates from the buttons and draws the frame in
// strips. Only the strips that changed are packed and sent to the panel.
//
// Hold MID while powering on to show the display diagnostic instead of the game.
// Use it to check the wiring and the panel settings in config.h.

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "game.h"
#include "ili9488.h"
#include "input.h"
#include "pico/stdlib.h"

#define STRIPS (GAME_H / GAME_STRIP_ROWS)
#define STRIP_PIXELS (GAME_W * GAME_STRIP_ROWS)

static uint16_t strip_rgb[STRIP_PIXELS];                 // the strip being drawn
static uint8_t packed[2][STRIP_PIXELS * ILI9488_BYTES_PER_PIXEL]; // two buffers for DMA
static uint32_t strip_sum[STRIPS];                       // checksum of each strip last sent

static ili9488_config_t display_config(void) {
    return (ili9488_config_t){
        .spi = TFT_SPI,
        .pin_sck = PIN_SCK,
        .pin_mosi = PIN_MOSI,
        .pin_miso = PIN_MISO,
        .pin_cs = PIN_CS,
        .pin_dc = PIN_DC,
        .pin_rst = PIN_RST,
        .pin_bl = PIN_BL,
        .baud = SPI_BAUD,
    };
}

// FNV-1a over a strip's pixels: a cheap change detector, so unchanged strips aren't sent.
static uint32_t checksum(const uint16_t *p, int n) {
    const uint8_t *b = (const uint8_t *)p;
    uint32_t h = 2166136261u;
    for (int i = 0; i < n * 2; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}

// Draws the whole frame strip by strip. A strip is sent only if its checksum changed
// since the last frame, or if it's the first frame.
static void draw_frame(bool first) {
    int cur = 0;
    for (int s = 0; s < STRIPS; s++) {
        int y0 = s * GAME_STRIP_ROWS;
        game_draw(strip_rgb, y0, GAME_STRIP_ROWS);

        uint32_t sum = checksum(strip_rgb, STRIP_PIXELS);
        if (!first && sum == strip_sum[s]) continue;
        strip_sum[s] = sum;

        // Pack into the buffer the previous DMA transfer is not reading.
        for (int i = 0; i < STRIP_PIXELS; i++)
            ili9488_pack(strip_rgb[i], &packed[cur][i * ILI9488_BYTES_PER_PIXEL]);
        ili9488_blit_dma(0, y0, GAME_W, GAME_STRIP_ROWS, packed[cur]);
        cur ^= 1;
    }
}

// Solid-colour bands, a white border and the colour sequence, to check the wiring,
// orientation and colour order. Runs until reset.
static void diagnostic(void) {
    const uint16_t solids[5] = {
        RGB565(255, 0, 0), RGB565(0, 255, 0), RGB565(0, 0, 255), RGB565(255, 255, 255), 0,
    };
    for (unsigned i = 0;; i = (i + 1) % 5) {
        ili9488_fill_rect(0, 0, GAME_W, GAME_H / 3, RGB565(255, 0, 0));
        ili9488_fill_rect(0, GAME_H / 3, GAME_W, GAME_H / 3, RGB565(0, 255, 0));
        ili9488_fill_rect(0, 2 * GAME_H / 3, GAME_W, GAME_H - 2 * GAME_H / 3, RGB565(0, 0, 255));
        ili9488_fill_rect(0, 0, GAME_W, 2, RGB565(255, 255, 255));
        ili9488_fill_rect(0, GAME_H - 2, GAME_W, 2, RGB565(255, 255, 255));
        ili9488_fill_rect(0, 0, 2, GAME_H, RGB565(255, 255, 255));
        ili9488_fill_rect(GAME_W - 2, 0, 2, GAME_H, RGB565(255, 255, 255));
        sleep_ms(1500);

        ili9488_fill_rect(0, 0, GAME_W, GAME_H, solids[i]);
        sleep_ms(1500);
    }
}

int main(void) {
    input_init();
    ili9488_config_t cfg = display_config();
    ili9488_init(&cfg);

    // Debounce needs the level to hold for a few ms, so sample MID twice.
    input_poll(0);
    sleep_ms(30);
    input_poll(30);
    if (input_held() & JOY_MID) diagnostic();

    game_init();
    draw_frame(true);

    // Fixed timestep on an absolute grid. After a long stall the grid is re-anchored.
    const uint64_t tick_us = 1000000 / GAME_TICK_HZ;
    uint64_t next_tick = time_us_64();
    while (true) {
        uint64_t now = time_us_64();
        input_poll((uint32_t)(now / 1000));

        if (now >= next_tick) {
            game_update(input_held(), input_pressed());
            next_tick += tick_us;
            if (now >= next_tick) next_tick = now + tick_us;
            draw_frame(false);
        }
    }
}
