# Agent Jatayu: Level 1

A Ramayana arcade game for the 480 x 320 ILI9488 display on the RP2040 USB stick. Jatayu
fights three Rakshasas with his magical golden claws. The title picture and the hero sprite
come from `jatayu.png`.

## Controls

| Control | Action |
|---|---|
| Joystick | Fly in 8 directions (two directions at once) |
| MID | Magical claw. Left and right swings alternate; a third quick swing is the double-claw X slash (2 damage) |
| SET | Divine claw burst: golden shockwave, 2 damage to every enemy in range, costs 40 divine energy. On the title and end screens, SET starts or restarts the level |
| Double-tap a direction | Dash, with brief invulnerability |
| RST | Back to the title |

## Level 1

Wave 1 sends three Rakshasas down from the top: the **Blood Warrior** (2 hits, dives at
you), the **Shadow Spearman** (3 hits, zigzags and throws dark orbs) and the **Demon
Commander** (5 hits, charges after a glowing wind-up). When they are all down, **wave 2**
brings four more (two Blood Warriors, a Shadow Spearman and a Demon Commander), each with one
extra hit point. Defeat all seven to win. Jatayu has 200 health; if it reaches 0 it is game
over, and SET tries again.

## Score

Each kill scores points (Blood Warrior 100, Shadow Spearman 200, Demon Commander 500).
When the level is won the final score adds three bonuses:

| Bonus | How it is worked out |
|---|---|
| Time | 15 points for every second under 100 seconds of fighting |
| Attacks | 15 points for every attack under 80 (claw swings and bursts both count) |
| Health | 10 points for every health point left |

The numbers are `TIME_PAR_S`, `TIME_PTS`, `ATTACK_PAR`, `ATTACK_PTS` and `HEALTH_PTS` at the top
of `src/game.c`.

## Artwork

`src/assets.c` and `src/assets.h` are generated. To rebuild them from `jatayu.png`:

```sh
python3 -I tools/gen_assets.py path/to/jatayu.png [preview.png]
```

Needs Python with Pillow, NumPy and SciPy.

## Display speed

The panel is slow, so the game tells the display code which 16-pixel tiles changed each
frame (`game_dirty_tiles()`), and only those are sent. That is why the background doesn't
scroll and the screen doesn't shake. Set `FPS_REPORT` to 1 in `src/main.c` to print the frame
rate over USB serial.

Flashing: if the firmware is running, the stick reboots into flash mode when its USB serial
port is opened at 1200 baud (it may take a few tries). Otherwise hold **BOOT** while plugging in.

---

# Original starter kit notes

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
