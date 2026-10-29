// Blank canvas: each tick, game.c updates from the buttons and draws the frame in
// strips. Only the strips that changed are packed and sent to the panel.
//
// Hold MID while powering on to show the display diagnostic instead of the game.
// Use it to check the wiring and the panel settings in config.h.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "config.h"
#include "game.h"
#include "ili9488.h"
#include "input.h"
#include "pico/stdlib.h"

#define STRIPS (GAME_H / GAME_STRIP_ROWS)
#define STRIP_PIXELS (GAME_W * GAME_STRIP_ROWS)
#define TILE_W GAME_TILE_W // a strip is cut into tiles this wide, and only changed tiles are sent
#define TILES (GAME_W / TILE_W)

// Set to 1 to print the frame rate and the tiles sent per frame over USB serial.
#define FPS_REPORT 0

static uint16_t strip_rgb[STRIP_PIXELS];                 // the strip being drawn
static uint8_t packed[2][STRIP_PIXELS * ILI9488_BYTES_PER_PIXEL]; // two buffers for DMA
static uint32_t tile_sum[STRIPS][TILES];                 // checksum of each tile last sent
static uint32_t tiles_sent;                              // tiles sent since the last report
#if FPS_REPORT
static uint64_t draw_us, update_us;                      // time spent since the last report
#endif

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

// FNV-style hash of one tile (TILE_W x GAME_STRIP_ROWS pixels, read two pixels at a time):
// a cheap change detector, so unchanged tiles aren't sent.
static uint32_t tile_checksum(const uint16_t *strip, int tile) {
    uint32_t h = 2166136261u;
    for (int r = 0; r < GAME_STRIP_ROWS; r++) {
        const uint32_t *w = (const uint32_t *)(strip + r * GAME_W + tile * TILE_W);
        for (int i = 0; i < TILE_W / 2; i++) h = (h ^ w[i]) * 16777619u;
    }
    return h;
}

// Draws the frame strip by strip, skipping strips and tiles the game says are unchanged. Within a strip, runs of changed tiles are packed
// and sent as one block each (a single unchanged tile between two changed ones is sent too,
// which is cheaper than another command). Everything is sent on the first frame.
static void draw_frame(bool first) {
    int cur = 0;
    const uint32_t *dirty = game_dirty_tiles();
    for (int s = 0; s < STRIPS; s++) {
        uint32_t mask = first ? (1u << TILES) - 1 : dirty[s];
        if (!mask) continue;
        int y0 = s * GAME_STRIP_ROWS;
        game_draw(strip_rgb, y0, GAME_STRIP_ROWS);

        bool changed[TILES];
        for (int t = 0; t < TILES; t++) {
            changed[t] = false;
            if (!((mask >> t) & 1)) continue;
            uint32_t sum = tile_checksum(strip_rgb, t);
            changed[t] = first || sum != tile_sum[s][t];
            tile_sum[s][t] = sum;
        }

        for (int t = 0; t < TILES; t++) {
            if (!changed[t]) continue;
            int end = t;
            for (int u = t + 1; u < TILES && u <= end + 2; u++)
                if (changed[u]) end = u;
            int w = (end - t + 1) * TILE_W;

            // Pack into the buffer the previous DMA transfer is not reading.
            for (int r = 0; r < GAME_STRIP_ROWS; r++) {
                const uint16_t *src = strip_rgb + r * GAME_W + t * TILE_W;
                uint8_t *dst = &packed[cur][r * w * ILI9488_BYTES_PER_PIXEL];
                for (int i = 0; i < w; i++) ili9488_pack(src[i], &dst[i * ILI9488_BYTES_PER_PIXEL]);
            }
            ili9488_blit_dma(t * TILE_W, y0, w, GAME_STRIP_ROWS, packed[cur]);
            cur ^= 1;
            tiles_sent += (uint32_t)(end - t + 1);
            t = end;
        }
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
    stdio_init_all(); // starts USB serial, which also gives the 1200-baud reboot into flash mode
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
#if FPS_REPORT
    uint64_t report_at = next_tick + 2000000;
    uint32_t frames = 0;
#endif
    while (true) {
        uint64_t now = time_us_64();
        input_poll((uint32_t)(now / 1000));

        if (now >= next_tick) {
            uint64_t t0 = time_us_64();
            game_update(input_held(), input_pressed());
            uint64_t t1 = time_us_64();
            next_tick += tick_us;
            if (now >= next_tick) next_tick = now + tick_us;
            draw_frame(false);
#if FPS_REPORT
            update_us += t1 - t0;
            draw_us += time_us_64() - t1;
#else
            (void)t0;
            (void)t1;
#endif
#if FPS_REPORT
            frames++;
#endif
        }
#if FPS_REPORT
        if (now >= report_at) {
            printf("fps %lu  tiles/frame %lu  update %lu us  draw %lu us\n", (unsigned long)(frames / 2),
                   (unsigned long)(frames ? tiles_sent / frames : 0),
                   (unsigned long)(frames ? update_us / frames : 0),
                   (unsigned long)(frames ? draw_us / frames : 0));
            frames = 0;
            tiles_sent = 0;
            update_us = draw_us = 0;
            report_at = now + 2000000;
        }
#endif
    }
}
