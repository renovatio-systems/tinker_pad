# tinker_pad

A starter kit for a microcontroller game or gadget. It runs on an RP2040 or RP2350 USB stick
with a 3.5" ILI9488 display (480 × 320, landscape) and a 5-button joystick. The code is C with the
Raspberry Pi Pico SDK. There is no sound hardware and no operating system.

Out of the box it runs a small demo, the blank canvas: a title, a box you move with the joystick,
and a button legend that lights up while you hold each button. Replace `src/game.c` with your own
game. `agent_jatayu` is a full game built on this kit.

## Controls (blank canvas demo)

| Button | Action |
|---|---|
| UP / DOWN / LEFT / RIGHT | Move the box |
| MID (centre) | Change the box's colour |
| SET | Pause and resume |
| RST | Reset the canvas |

The stick's own RST button resets the microcontroller, which restarts the firmware.

## Display diagnostic

Hold **MID** while you power the board on. The screen shows red, green and blue bands with a
white border, then solid colours. Use this to check the wiring and the panel settings in
`src/config.h`.

## Building

You need:

- CMake 3.13 or newer
- The Arm GCC toolchain (`arm-none-eabi-gcc`)
- The Pico SDK, version 2.x (tested with 2.3.1)

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The output is `build/src/tinker_pad.uf2` (and `tinker_pad.elf`). The CMake default board is
`rp2040_usb_stick`. Pass `-DPICO_BOARD=pico2` for an RP2350 board, or `-DPICO_BOARD=pico` for a
standard Pico.

## Flashing

Either of these works:

- **Button:** hold **BOOT** while you plug the stick in. It appears as a drive named `RPI-RP2`
  (or `RP2350`). Copy `tinker_pad.uf2` onto it.
- **Command line:** `picotool load -x build/src/tinker_pad.uf2`

## Project layout

```
src/game.c        your game: edit this
src/game.h        the three functions the firmware calls
src/main.c        the main loop, display update and diagnostic screen: leave alone
src/config.h      pins, panel offsets and the SPI clock
src/input.c/h     buttons, debounced
src/led.c/h       the RGB LED on the RP2350 board
src/ws2812.pio    PIO program for the RGB LED
lib/ili9488/      display driver (3.5" ILI9488)
lib/font5x7/      5x7 text font
boards/           board definition for the RP2040 USB stick
docs/hardware.md  wiring and panel notes
```

## Wiring

Display (SPI0 on the GP0–GP8 edge):

| Signal | Pin |
|---|---|
| SCK | GP2 |
| SDI (MOSI) | GP3 |
| SDO (MISO) | GP4 |
| CS | GP5 |
| DC | GP6 |
| RST | GP7 |
| Backlight | Tied to 3V3 |

Joystick and buttons (each switches to GND, internal pull-ups are used):

| Button | Pin |
|---|---|
| UP | GP26 |
| DOWN | GP27 |
| LEFT | GP28 |
| RIGHT | GP15 |
| MID | GP14 |
| SET | GP0 |
| RST (game restart) | GP8 |

The RP2350 USB board also has an RGB LED (WS2812B) on GP22. It runs a short red, green, blue,
white test at boot. The RP2040 stick has no LED.

See [docs/hardware.md](docs/hardware.md) for the full pinout and board photos.

## Writing your game

`game.h` has three functions, and `main.c` calls them in this order every tick:

1. `game_init()` once at power-up, and again when RST is pressed.
2. `game_update(held, pressed)` once per tick. `held` lists the buttons down now.
   `pressed` lists the buttons that went down since the last tick.
3. `game_draw(buf, y0, rows)` several times per tick, once for each strip of rows. Draw everything
   into `buf`, which holds `rows` rows of the frame starting at row `y0`. Anything outside those rows
   is clipped away.

Coordinates are in pixels: x from 0 to 479, y from 0 to 319. `buf` is a flat array of RGB565
pixels, row by row. Use `RGB565(r, g, b)` for colours.

Start by changing the colours, the speed and the text. Then add your own variables next to the
box's, and draw them in `game_draw()`.

## License

MIT. See [LICENSE](LICENSE).
