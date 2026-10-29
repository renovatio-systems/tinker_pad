#ifndef CONFIG_H
#define CONFIG_H

// Wiring for the "RP2040 USB" stick board, driving a 2.25" ST7789 76 x 284 panel
// over SPI0 on the GP0-GP8 edge. See docs/hardware.md for the full table.
// SCK/MOSI must be a valid SPI0 group (SCK 2/6/18/22, TX 3/7/19/23). The panel
// has no MISO line in this wiring, so none is configured.
#define TFT_SPI  spi0
#define PIN_SCK  2  // display SCK  (pin SCL)
#define PIN_MOSI 3  // display MOSI (pin SDA)
#define PIN_CS   5  // display CS
#define PIN_DC   6  // display DC
#define PIN_RST  7  // display RST
#define PIN_BL   -1 // BL goes to GND on this module (backlight on); set to 8 to drive it from GP8

// Panel RAM offsets and orientation. 82 / 18 is the portrait offset reported for
// this 76 x 284 ST7789 module (TFT_eSPI pull request #3769); the offset test on the
// panel put the visible window inside those ranges.
#define ST7789_X_OFFSET 82
#define ST7789_Y_OFFSET 18
#define ST7789_MADCTL   0x00 // 0x00 portrait RGB, 0x08 BGR, 0xC0 / 0x80 for flips
#define ST7789_INVERT   0    // 1 inverts all colours (red shows as cyan): off for this module

// Joystick buttons: each switches its GPIO to GND (internal pull-ups are used).
#define PIN_JOY_UP    26
#define PIN_JOY_DOWN  27
#define PIN_JOY_LEFT  28
#define PIN_JOY_RIGHT 15
#define PIN_JOY_MID   14 // centre button
#define PIN_JOY_SET   0  // SET button
#define PIN_JOY_RESET 1  // software restart of the game (the stick's own RST button resets the MCU)

#define SPI_BAUD (10 * 1000 * 1000) // 10 MHz for jumper wiring; raise once the picture is clean

#endif
