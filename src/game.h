#ifndef GAME_API_H
#define GAME_API_H

#include <stdint.h>

#include "ili9488.h"
#include "input.h"

#define GAME_W       ILI9488_WIDTH  // 480 pixels across (landscape)
#define GAME_H       ILI9488_HEIGHT // 320 pixels down
#define GAME_TICK_HZ 30             // game logic runs this many times a second
#define GAME_STRIP_ROWS 8           // the frame is drawn 8 rows at a time, to save RAM

// The only functions the rest of the firmware calls. Edit game.c to build your game.
void game_init(void);

// Runs once per tick. `held` is every button down now, `pressed` the ones that went
// down since the last tick. Bits are JOY_UP, JOY_DOWN, JOY_LEFT, JOY_RIGHT,
// JOY_MID, JOY_SET and JOY_RESET from input.h.
void game_update(uint8_t held, uint8_t pressed);

// Draws rows y0 to y0 + rows - 1 of the frame into buf: rows * GAME_W RGB565 pixels,
// row by row. Anything outside those rows is clipped away, so the game draws every
// strip in turn with the same code.
void game_draw(uint16_t *buf, int y0, int rows);

#endif
