#ifndef CONFIG_H
#define CONFIG_H

// Wiring for the "RP2040 USB" stick board, driving a 3.5" ILI9488 panel (480 x 320,
// landscape) over SPI0 on the GP0-GP8 edge. See docs/hardware.md for the full table.
// SCK/MOSI/MISO must be a valid SPI0 group (SCK 2/6/18/22, TX 3/7/19/23, RX 0/4/16/20).
#define TFT_SPI  spi0
#define PIN_SCK  2  // display SCK
#define PIN_MOSI 3  // display SDI/MOSI
#define PIN_MISO 4  // display SDO/MISO (status reads only)
#define PIN_CS   5  // display CS
#define PIN_DC   6  // display DC/RS
#define PIN_RST  7  // display RESET
#define PIN_BL   -1 // display LED tied to 3V3; set to 8 to switch it from GP8

// RGB LED (WS2812B-0807) on the RP2350 USB board. The RP2040 stick has none, so -1.
#if PICO_RP2350
#define PIN_LED 22
#else
#define PIN_LED -1
#endif

// Joystick buttons: each switches its GPIO to GND (internal pull-ups are used).
#define PIN_JOY_UP    26
#define PIN_JOY_DOWN  27
#define PIN_JOY_LEFT  28
#define PIN_JOY_RIGHT 15
#define PIN_JOY_MID   14 // centre button
#define PIN_JOY_SET   0  // SET button
#define PIN_JOY_RESET 8  // software restart of the game (the stick's own RST button resets the MCU)

// The ILI9488 takes 18-bit pixels over SPI, so the panel is slow. 20 MHz is the fastest we
// run it; if the picture breaks up, drop it back to 15 MHz.
#define SPI_BAUD (20 * 1000 * 1000)

#endif
