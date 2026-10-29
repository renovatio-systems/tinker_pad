# Blank canvas

A starter kit for a 3.5" ILI9488 display (480 × 320, landscape) on an RP2040 USB stick,
written in C with the Raspberry Pi Pico SDK. It has no game in it yet. It shows a
title, a box you move with the joystick, and a button legend that lights up as you
press each button. Change `src/game.c` to build your own game.

## What's on screen

- **HELLO** at the top.
- A box in the middle that moves with the joystick. MID changes its colour. SET
  pauses it.
- A legend at the bottom. Each label lights up while its button is held.

## Controls

| Button | Action in the canvas |
|---|---|
| UP / DN / LT / RT | Move the box |
| MID (centre) | Change the box's colour |
| SET | Pause and resume |
| RST | Reset the canvas |

The stick's own RST button resets the microcontroller, which restarts the firmware.

## Building

You need CMake ≥ 3.13, the ARM GCC toolchain and the Pico SDK (2.x; tested with 2.3.1).

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j       # -> build/src/blank_canvas.uf2
```

## Flashing

Hold **BOOT** while plugging the stick in, then copy `build/src/blank_canvas.uf2`
to the `RPI-RP2` drive. Or use `picotool load -x build/src/blank_canvas.uf2`.

## Display diagnostic

Hold **MID** while powering on. The stick shows red, green and blue bands with a
white border, then solid colours. Use it to check the wiring and the panel settings
in `src/config.h`.

## Wiring

See [docs/hardware.md](docs/hardware.md). Summary: SCK GP2, SDI GP3, SDO GP4,
CS GP5, DC GP6, RST GP7, VCC 3V3, GND, and BL to 3V3 (or GND, see docs/hardware.md).
The buttons go to GP14, GP15, GP26, GP27, GP28, GP0 and GP1, each to GND.

## Files

```
src/game.c       your game: edit this
src/game.h       the three functions the firmware calls
src/main.c       the loop and the diagnostic screen: leave alone
src/config.h     pins, panel offsets and the SPI clock
src/input.c/h    buttons, debounced
lib/ili9488/      display driver (3.5" ILI9488)
lib/font5x7/     5x7 text font
docs/hardware.md wiring and panel notes
```

## Writing your game

`game.h` has three functions, and `main.c` calls them in this order every tick:

1. `game_init()` once at power-up, and again when RST is pressed.
2. `game_update(held, pressed)` once per tick. `held` lists the buttons down now.
   `pressed` lists the buttons that went down since the last tick.
3. `game_draw(buf, y0, rows)` several times per tick, once for each 8-row strip. Draw everything into `buf`, which is `rows` rows of the frame starting at row `y0`. Anything outside those rows is clipped away.

Coordinates are in pixels: x from 0 to 479, y from 0 to 319. `buf` is a flat array of
RGB565 pixels, row by row. Use `RGB565(r, g, b)` for colours.

Start by changing the colours, the speed and the text. Then add your own variables
next to the box's, and draw them in `game_draw()`.
