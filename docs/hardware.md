# Hardware notes

## Parts

| Part | Details |
|---|---|
| Display | 2.25" SPI TFT, **ST7789**, **76 × 284** portrait, RGB565, 3.3 V logic |
| MCU board | Generic **"RP2040 USB" stick**: RP2040-B2, 12 MHz crystal, W25Q32JV 4 MB QSPI flash, USB-A plug, BOOT and RST buttons |

![RP2040 USB stick](images/rp2040_board.jpg)

### RP2040 USB stick pinout

![Pinout](images/rp2040_pinout.jpg)

| Location | Pins |
|---|---|
| Edge, power side | 5V, GND, 3V3, GP29, GP28, GP27, GP26, GP15, GP14 |
| Edge, GPIO side | GP0 – GP8 |
| Inner pads | GP9 – GP13, GP20 – GP25 |
| Not broken out | GP16 – GP19 |

GP16–GP19 (the Pico's usual SPI0 pins) are missing, so the display uses SPI0's
alternate pins GP2 (SCK) and GP3 (TX/MOSI), both on the GP0–GP8 edge.

## Display module

The module has 8 signal pins: BL, CS, DC, RST, SDA, SCL, VCC and GND. There is
no MISO, so the firmware only writes to the panel and never reads from it.

| Module pin | Signal | RP2040 USB stick |
|---|---|---|
| VCC | Supply | `3V3` |
| GND | Ground | `GND` |
| CS | Chip select, active low | GP5 |
| RST | Reset, active low | GP7 |
| DC | Data (high) / command (low) | GP6 |
| SDA | SPI MOSI | GP3 (SPI0 TX) |
| SCL | SPI clock | GP2 (SPI0 SCK) |
| BL | Backlight (active low on this module) | GND, backlight always on |

On this module BL must go to GND to light the backlight. Tying it to 3V3 leaves the panel dark. The firmware doesn't drive BL, so `PIN_BL` is `-1` in `src/config.h`.

Confirm VCC is 3.3 V before powering the module. The module's logic levels are
3.3 V, which matches the RP2040.

## Joystick

Seven buttons, each switching its GPIO to GND (see [`src/config.h`](../src/config.h)):

| Button | RP2040 USB stick |
|---|---|
| UP | GP26 |
| DOWN | GP27 |
| LEFT | GP28 |
| RIGHT | GP15 |
| MID (centre) | GP14 |
| SET | GP0 |
| RESET | GP1 |

## Unverified on the real module

These values come from the brief and typical ST7789 modules, not from this panel:

- **Panel offsets** (`ST7789_X_OFFSET`, `ST7789_Y_OFFSET`). A 76 × 284 panel usually
  sits at an offset inside the controller's 240 × 320 RAM, but the exact values need
  checking on the panel. The diagnostic screen shows whether the image is shifted or cut off.
- **Orientation and colour order** (`ST7789_MADCTL`). Mirrored or upside-down output
  means changing the MADCTL value; swapped red and blue means toggling the BGR bit (0x08).
- **Inversion** (`ST7789_INVERT`). IPS panels usually need it on.
- **Porch, gate, VCOM and gamma values** in `lib/st7789/src/st7789.c`. These are the
  common defaults and may need tuning for this module.
- **SPI clock**: the firmware uses 31.25 MHz. Lower `SPI_BAUD` if the picture is noisy.

Run the diagnostic screen first (hold MID while powering on) to check these.

## Power

The RP2040 stick runs from USB. The display takes its supply from the 3V3 pin, so
the total current is set by the RP2040 regulator and the backlight. Check the
module's current draw before adding anything else to the 3V3 rail.
