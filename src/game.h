#ifndef GAME_API_H
#define GAME_API_H

#include <stdint.h>

#include "input.h"
#include "st7789.h"

// Native RGB565 colour from 8-bit red, green and blue.
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#define GAME_W       ST7789_WIDTH  // 76 pixels across
#define GAME_H       ST7789_HEIGHT // 284 pixels down
#define GAME_TICK_HZ 60            // game logic runs this many times a second

// The only functions the rest of the firmware calls. Edit game.c to build your game.
void game_init(void);

// Runs once per tick. `held` is every button down now, `pressed` the ones that went
// down since the last tick. Bits are JOY_UP, JOY_DOWN, JOY_LEFT, JOY_RIGHT,
// JOY_MID, JOY_SET and JOY_RESET from input.h.
void game_update(uint8_t held, uint8_t pressed);

// Draws the whole frame into fb: GAME_W * GAME_H native RGB565 pixels, row by row.
void game_draw(uint16_t *fb);

#endif
