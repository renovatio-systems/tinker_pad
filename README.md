# Agent Jatayu

Agent Jatayu is a small arcade game for a microcontroller, based on the Ramayana. You play
Jatayu, the king of the vultures, who tries to stop Ravana from carrying off Sita.

It runs on a RP2040 or RP2350 board with a 3.5" ILI9488 display (480 × 320, landscape) and
a 5-button joystick. There is no sound hardware, and no operating system. The game is written in
C with the Raspberry Pi Pico SDK.

## The story in four levels

| Level | Name | What happens |
|---|---|---|
| 1 | The Rakshasa Assault | Fight waves of Rakshasas in the air with your magical golden claws. |
| 2 | The Boxing Ring | Fight three Rakshasas one at a time on the ground. |
| 3 | The Pursuit of Ravana | Climb three stages of platforms to chase Ravana's chariot. |
| 4 | The Story | Five story scenes with one line of narration each, then the ending. There is no gameplay. |

You don't play the levels in order from a menu. Pick the level you want on the title screen
(see below).

## Controls

| Button | Level 1 (air) | Level 2 (ground) | Level 3 (platforms) |
|---|---|---|---|
| Joystick | Fly in 8 directions | Left / right to walk | Left / right to walk |
| UP | Fly up | Jump | Jump |
| DOWN (hold) | Fly down | Shield, takes a quarter of the damage | |
| MID | Claw swing. A third quick swing is a double-claw slash | Chakra from his beak, knocks back what it hits | Light the nearest diya (a checkpoint) |
| SET | Divine claw burst, costs 40 divine energy | Trident thrown from his chest, costs energy | Start, and continue on the final screen |
| Double-tap a direction | Dash, with brief invulnerability | | |
| RST (GP8) | Back to the title | Back to the title | Back to the title |

On the title and end screens, SET is the main button.

### Choosing a level

On the title screen, press SET the number of times that matches the level, then wait about
1.2 seconds:

- 1 press: Level 1
- 2 presses: Level 2
- 3 presses: Level 3
- 4 presses: Level 4 (the story scenes)

## Level 1 details

Wave 1 sends three Rakshasas down from the top: the **Blood Warrior** (2 hits, dives at you),
the **Shadow Spearman** (3 hits, zigzags and throws dark orbs) and the **Demon Commander**
(5 hits, charges after a glowing wind-up). When they are all down, **wave 2** brings four more
(two Blood Warriors, a Shadow Spearman and a Demon Commander), each with one extra hit point.
Defeat all seven to win.

Jatayu has 200 health. At 0 it is game over, and SET tries again.

### Score

Each kill scores points: Blood Warrior 100, Shadow Spearman 200, Demon Commander 500. When the
level is won, three bonuses are added:

| Bonus | How it is worked out |
|---|---|
| Time | 15 points for every second under 100 seconds of fighting |
| Attacks | 15 points for every attack under 80 (claw swings and bursts both count) |
| Health | 10 points for every health point left |

The numbers are `TIME_PAR_S`, `TIME_PTS`, `ATTACK_PAR`, `ATTACK_PTS` and `HEALTH_PTS` at the top
of `src/game.c`.

## Level 2 details

Three Rakshasas, one after another, on the ground. Jatayu has 200 health and 100 energy. The
trident costs energy, so use it with care.

## Level 3 details

Three stages of vertical platforming, each taller than the last. Jatayu climbs through a temple,
a cloud field and a blood moon to reach Ravana's flying chariot, the Pushpaka Vimana. Each stage
ends when he gets close enough to Ravana, who then escapes upward to the next stage.

Points come from the diyas you light and from how quickly you climb.

## Level 4 details

There is no gameplay here. After Level 3 is won, you can continue to five story scenes, one
picture each with a line of narration, followed by a closing message. Then you return to the
title.

## Building

You need:

- CMake 3.13 or newer
- The Arm GCC toolchain (`arm-none-eabi-gcc`)
- The Pico SDK, version 2.x (tested with 2.3.1)

The easiest way is the helper script in the project root:

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
./tinker_pad.sh all        # clean, configure, build and load
```

Run `./tinker_pad.sh help` for the other steps (`configure`, `build`, `reset`, `flash`, `load`).
It reads these environment variables, all optional:

| Variable | Default |
|---|---|
| `PICO_SDK_PATH` | `/work/pico/pico-sdk-2.3.1` |
| `PICO_BOARD` | `pico2` |
| `SERIAL_PORT` | `/dev/ttyACM0` |
| `DRIVE` | `/run/media/$USER/RP2350` |
| `BUILD_DIR` | `b2350` |

To build with CMake directly:

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The output is `build/src/tinker_pad.uf2` (and `tinker_pad.elf`). The CMake default board is
`pico2` (RP2350). Pass `-DPICO_BOARD=rp2040_usb_stick` for the RP2040 USB stick, or
`-DPICO_BOARD=pico` for a standard Pico.

## Flashing

Either of these works:

- **Button:** hold **BOOT** while you plug the stick in. It appears as a drive named `RPI-RP2`
  (or `RP2350`). Copy `tinker_pad.uf2` onto it.
- **Software:** if the game is already running, open the USB serial port at 1200 baud. The
  board reboots into flash mode. You might need a few tries. `./tinker_pad.sh load` does this
  for you.

You can also use `picotool load -x build/src/tinker_pad.uf2`.

## Project layout

```
src/game.c        all the game logic and drawing
src/game.h        the functions the main loop calls
src/main.c        the main loop, display update and diagnostic screen
src/config.h      pin numbers, display settings and SPI speed
src/input.c/h     buttons, debounced
src/led.c/h       the RGB LED on the RP2350 board
src/assets.c      level 1 art (generated C arrays)
src/jatayu_img.c  title picture (generated C array)
src/l2_sprites.c  level 2 sprites (generated C arrays)
src/l3_assets.c   level 3 art (generated C arrays)
src/l4_cutscenes.c story scene pictures (generated C arrays)
src/l_labels.c    text labels as images (generated C arrays)
lib/ili9488/      display driver
lib/font5x7/      5x7 text font
boards/           board definition for the RP2040 USB stick
docs/hardware.md  wiring and panel notes
```

The art is stored as RGB565 arrays in the `.c` files. The tools that made them and the source
PNGs are no longer in this branch. Edit the arrays directly, or restore the tools from git
history (`git log -- tools/`) if you need to regenerate them.

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
| Backlight | 3V3 (or GP8, see `src/config.h`) |

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

See [docs/hardware.md](docs/hardware.md) for the full pinout and the photos of the board.

## Tips

- Set `FPS_REPORT` to `1` in `src/main.c` to print the frame rate and the number of tiles sent
  per frame over USB serial.
- The panel is slow. The game only sends the 16-pixel tiles that changed each frame, which is
  why the background doesn't scroll.
- If the picture tears or breaks up, lower `SPI_BAUD` in `src/config.h` from 20 MHz to 15 MHz.

## License

See [LICENSE](LICENSE).
