// The blank canvas. Everything on screen comes from game_draw(), and everything
// that changes comes from game_update(). Start by changing the text, the colours
// or the speed, then add your own variables and drawing.

#include "game.h"

#include <string.h>

#include "font5x7.h"

// Screen areas. The title sits at the top, the legend at the bottom, and the
// playfield in between.
#define TITLE_Y     20
#define PLAY_TOP    82
#define PLAY_BOTTOM 258
#define LEGEND_Y1   272
#define LEGEND_Y2   296

#define BOX_SIZE  24
#define BOX_SPEED 6 // pixels per tick

static const uint16_t colours[] = {
    RGB565(255, 80, 80),  // red
    RGB565(80, 220, 80),  // green
    RGB565(80, 160, 255), // blue
    RGB565(255, 220, 40), // yellow
    RGB565(255, 255, 255),
};
#define N_COLOURS (sizeof colours / sizeof colours[0])

#define COLOUR_TEXT  RGB565(255, 255, 255)
#define COLOUR_LABEL RGB565(90, 90, 100)
#define COLOUR_RULE  RGB565(50, 50, 60)

// Game state. Add your own variables here.
static int box_x, box_y;
static unsigned colour;
static int paused;
static uint8_t buttons; // buttons held down, kept for drawing the legend

// The strip being drawn right now. put() ignores pixels outside it.
static uint16_t *strip;
static int strip_y0, strip_rows;

static void put(int x, int y, uint16_t c) {
    if (x >= 0 && x < GAME_W && y >= strip_y0 && y < strip_y0 + strip_rows)
        strip[(y - strip_y0) * GAME_W + x] = c;
}

static void fill(int x, int y, int w, int h, uint16_t c) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) put(xx, yy, c);
}

// Draws text in the 5x7 font, `scale` times bigger. Each character is 6 * scale wide.
static void text(int x, int y, const char *s, uint16_t c, int scale) {
    for (; *s; s++, x += 6 * scale) {
        const uint8_t *cols = font5x7_glyph(*s);
        for (int gx = 0; gx < 5; gx++)
            for (int gy = 0; gy < 7; gy++)
                if ((cols[gx] >> gy) & 1) fill(x + gx * scale, y + gy * scale, scale, scale, c);
    }
}

static int text_width(const char *s, int scale) {
    int n = (int)strlen(s);
    return n ? n * 6 * scale - scale : 0;
}

static void text_centred(int y, const char *s, uint16_t c, int scale) {
    text((GAME_W - text_width(s, scale)) / 2, y, s, c, scale);
}

// Draws one legend label at x, lit up while its button is held.
static void label(int x, int y, const char *s, uint8_t bit) {
    text(x, y, s, (buttons & bit) ? COLOUR_TEXT : COLOUR_LABEL, 2);
}

void game_init(void) {
    box_x = (GAME_W - BOX_SIZE) / 2;
    box_y = (PLAY_TOP + PLAY_BOTTOM - BOX_SIZE) / 2;
    colour = 0;
    paused = 0;
}

void game_update(uint8_t held, uint8_t pressed) {
    buttons = held;

    if (pressed & JOY_RESET) {
        game_init();
        return;
    }
    if (pressed & JOY_SET) paused = !paused;
    if (pressed & JOY_MID) colour = (colour + 1) % N_COLOURS;

    if (paused) return;

    if (held & JOY_LEFT) box_x -= BOX_SPEED;
    if (held & JOY_RIGHT) box_x += BOX_SPEED;
    if (held & JOY_UP) box_y -= BOX_SPEED;
    if (held & JOY_DOWN) box_y += BOX_SPEED;

    if (box_x < 0) box_x = 0;
    if (box_x > GAME_W - BOX_SIZE) box_x = GAME_W - BOX_SIZE;
    if (box_y < PLAY_TOP) box_y = PLAY_TOP;
    if (box_y > PLAY_BOTTOM - BOX_SIZE) box_y = PLAY_BOTTOM - BOX_SIZE;
}

void game_draw(uint16_t *buf, int y0, int rows) {
    strip = buf;
    strip_y0 = y0;
    strip_rows = rows;
    memset(buf, 0, (size_t)rows * GAME_W * sizeof *buf);

    text_centred(TITLE_Y, "HELLO", COLOUR_TEXT, 4);

    fill(0, PLAY_TOP - 4, GAME_W, 2, COLOUR_RULE);
    fill(0, PLAY_BOTTOM + 2, GAME_W, 2, COLOUR_RULE);

    fill(box_x, box_y, BOX_SIZE, BOX_SIZE, colours[colour]);

    if (paused) text_centred(150, "PAUSED", COLOUR_TEXT, 3);

    // Button legend: each label lights up while its button is held.
    label(40, LEGEND_Y1, "UP", JOY_UP);
    label(160, LEGEND_Y1, "DN", JOY_DOWN);
    label(280, LEGEND_Y1, "LT", JOY_LEFT);
    label(400, LEGEND_Y1, "RT", JOY_RIGHT);
    label(60, LEGEND_Y2, "MID", JOY_MID);
    label(200, LEGEND_Y2, "SET", JOY_SET);
    label(340, LEGEND_Y2, "RST", JOY_RESET);
}
