#include "st7789.h"

#include "hardware/dma.h"
#include "pico/stdlib.h"

#define READ_BAUD (1000 * 1000)

static st7789_config_t cfg;
static int dma_chan = -1;
static bool dma_busy;
static uint8_t row_buf[2 * ST7789_WIDTH]; // big-endian RGB565, one row

// Command, data length, data... Terminated by a zero command.
// Values are the common ST7789 IPS defaults; the porch and gate settings may need
// tuning for this module (see README, "unverified").
static const uint8_t init_cmds[] = {
    0x3A, 1, 0x55,                         // 16 bits per pixel
    0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33, // porch control
    0xB7, 1, 0x35,                         // gate control
    0xBB, 1, 0x19,                         // VCOM
    0xC0, 1, 0x2C,                         // LCM control
    0xC2, 1, 0x01,                         // VDV and VRH from registers
    0xC3, 1, 0x12,                         // VRH
    0xC4, 1, 0x20,                         // VDV
    0xC6, 1, 0x0F,                         // 60 Hz frame rate
    0xD0, 2, 0xA4, 0xA1,                   // power control 1
    0xE0, 14, 0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
              0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23, // positive gamma
    0xE1, 14, 0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
              0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23, // negative gamma
    0x00,
};

static inline void cs(bool level) { gpio_put(cfg.pin_cs, level); }
static inline void dc(bool level) { gpio_put(cfg.pin_dc, level); }

static void write_cmd(uint8_t c) {
    st7789_wait();
    dc(0);
    cs(0);
    spi_write_blocking(cfg.spi, &c, 1);
    cs(1);
}

static void write_data(const uint8_t *d, size_t n) {
    dc(1);
    cs(0);
    spi_write_blocking(cfg.spi, d, n);
    cs(1);
}

static void gpio_out(uint pin, bool level) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, level);
}

// Column and row address windows are offset by the panel's RAM position.
// `raw` skips the offset, so the whole controller RAM can be addressed.
static void set_window_at(int x0, int y0, int x1, int y1, bool raw) {
    if (!raw) {
        x0 += cfg.x_off;
        x1 += cfg.x_off;
        y0 += cfg.y_off;
        y1 += cfg.y_off;
    }
    uint8_t d[4] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF};
    write_cmd(0x2A); // column address set
    write_data(d, 4);
    d[0] = y0 >> 8; d[1] = y0 & 0xFF; d[2] = y1 >> 8; d[3] = y1 & 0xFF;
    write_cmd(0x2B); // row address set
    write_data(d, 4);
    write_cmd(0x2C); // memory write
}

uint st7789_init(const st7789_config_t *c) {
    cfg = *c;
    dma_chan = dma_claim_unused_channel(true);

    uint actual = spi_init(cfg.spi, cfg.baud); // mode 0, 8 bit, MSB first
    gpio_set_function(cfg.pin_sck, GPIO_FUNC_SPI);
    gpio_set_function(cfg.pin_mosi, GPIO_FUNC_SPI);

    gpio_out(cfg.pin_cs, 1);
    gpio_out(cfg.pin_dc, 1);
    gpio_out(cfg.pin_rst, 1);
    if (cfg.pin_bl >= 0) gpio_out(cfg.pin_bl, 1);

    sleep_ms(5);
    gpio_put(cfg.pin_rst, 0);
    sleep_ms(20);
    gpio_put(cfg.pin_rst, 1);
    sleep_ms(120);
    write_cmd(0x01); // software reset
    sleep_ms(150);
    write_cmd(0x11); // sleep out
    sleep_ms(120);

    for (const uint8_t *p = init_cmds; *p; p += 2 + p[1]) {
        write_cmd(p[0]);
        write_data(p + 2, p[1]);
    }

    write_cmd(0x36); // memory access control
    write_data(&cfg.madctl, 1);
    write_cmd(cfg.invert ? 0x21 : 0x20); // display inversion on / off
    write_cmd(0x13); // normal display mode
    sleep_ms(10);
    write_cmd(0x29); // display on
    sleep_ms(20);

    return actual;
}

static void set_window(int x0, int y0, int x1, int y1) { set_window_at(x0, y0, x1, y1, false); }

void st7789_send_frame(const uint16_t *pixels) {
    set_window(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1); // also waits for the previous transfer
    dc(1);
    cs(0);
    // 16-bit frames go out MSB first, so native RGB565 needs no byte swap.
    spi_set_format(cfg.spi, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    dma_channel_config dc_cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dc_cfg, DMA_SIZE_16);
    channel_config_set_dreq(&dc_cfg, spi_get_dreq(cfg.spi, true));
    dma_channel_configure(dma_chan, &dc_cfg, &spi_get_hw(cfg.spi)->dr, pixels,
                          ST7789_WIDTH * ST7789_HEIGHT, true);
    dma_busy = true;
}

void st7789_wait(void) {
    if (!dma_busy) return;
    dma_busy = false;
    dma_channel_wait_for_finish_blocking(dma_chan);
    while (spi_is_busy(cfg.spi)) tight_loop_contents();
    cs(1);
    spi_set_format(cfg.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    // Transmit-only DMA still fills the receive FIFO; empty it.
    while (spi_is_readable(cfg.spi)) (void)spi_get_hw(cfg.spi)->dr;
    spi_get_hw(cfg.spi)->icr = SPI_SSPICR_RORIC_BITS;
}

void st7789_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ST7789_WIDTH) w = ST7789_WIDTH - x;
    if (y + h > ST7789_HEIGHT) h = ST7789_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    for (int i = 0; i < w; i++) {
        row_buf[2 * i] = color >> 8;
        row_buf[2 * i + 1] = color & 0xFF;
    }
    set_window(x, y, x + w - 1, y + h - 1);
    dc(1);
    cs(0);
    for (int row = 0; row < h; row++) spi_write_blocking(cfg.spi, row_buf, 2 * w);
    cs(1);
}

void st7789_fill_ram(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    for (int i = 0; i < w; i++) {
        row_buf[2 * i] = color >> 8;
        row_buf[2 * i + 1] = color & 0xFF;
    }
    set_window_at(x, y, x + w - 1, y + h - 1, true);
    dc(1);
    cs(0);
    for (int row = 0; row < h; row++) spi_write_blocking(cfg.spi, row_buf, 2 * w);
    cs(1);
}
