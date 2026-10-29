// The blank canvas. Everything on screen comes from game_draw(), and everything
// that changes comes from game_update(). Start by changing the text, the colours
// or the speed, then add your own variables and drawing.

#include "game.h"

#include <string.h>

#include "font5x7.h"

// Screen areas. The title sits at the top, the legend at the bottom, and the
// playfield in between.
#define TITLE_Y   6
#define PLAY_TOP  30
#define PLAY_BOTTOM 244
#define LEGEND_Y1 256
#define LEGEND_Y2 268

#define BOX_SIZE  8
#define BOX_SPEED 2 // pixels per tick

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

static void put(uint16_t *fb, int x, int y, uint16_t c) {
    if (x >= 0 && x < GAME_W && y >= 0 && y < GAME_H) fb[y * GAME_W + x] = c;
}

static void fill(uint16_t *fb, int x, int y, int w, int h, uint16_t c) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) put(fb, xx, yy, c);
}

// Draws text in the 5x7 font, `scale` times bigger. Each character is 6 * scale wide.
static void text(uint16_t *fb, int x, int y, const char *s, uint16_t c, int scale) {
    for (; *s; s++, x += 6 * scale) {
        const uint8_t *cols = font5x7_glyph(*s);
        for (int gx = 0; gx < 5; gx++)
            for (int gy = 0; gy < 7; gy++)
                if ((cols[gx] >> gy) & 1) fill(fb, x + gx * scale, y + gy * scale, scale, scale, c);
    }
}

static int text_width(const char *s, int scale) {
    int n = (int)strlen(s);
    return n ? n * 6 * scale - scale : 0;
}

static void text_centred(uint16_t *fb, int y, const char *s, uint16_t c, int scale) {
    text(fb, (GAME_W - text_width(s, scale)) / 2, y, s, c, scale);
}

// Draws one legend label, lit up while its button is held.
static void label(uint16_t *fb, int x, int y, const char *s, uint8_t bit) {
    text(fb, x, y, s, (buttons & bit) ? COLOUR_TEXT : COLOUR_LABEL, 1);
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

void game_draw(uint16_t *fb) {
    memset(fb, 0, GAME_W * GAME_H * sizeof *fb);

    text_centred(fb, TITLE_Y, "HELLO", COLOUR_TEXT, 2);

    fill(fb, 0, PLAY_TOP - 2, GAME_W, 1, COLOUR_RULE);
    fill(fb, 0, PLAY_BOTTOM + 2, GAME_W, 1, COLOUR_RULE);

    fill(fb, box_x, box_y, BOX_SIZE, BOX_SIZE, colours[colour]);

    if (paused) text_centred(fb, 130, "PAUSED", COLOUR_TEXT, 1);

    // Button legend: each label lights up while its button is held.
    label(fb, 5, LEGEND_Y1, "UP", JOY_UP);
    label(fb, 23, LEGEND_Y1, "DN", JOY_DOWN);
    label(fb, 41, LEGEND_Y1, "LT", JOY_LEFT);
    label(fb, 59, LEGEND_Y1, "RT", JOY_RIGHT);
    label(fb, 5, LEGEND_Y2, "MID", JOY_MID);
    label(fb, 29, LEGEND_Y2, "SET", JOY_SET);
    label(fb, 53, LEGEND_Y2, "RST", JOY_RESET);
}
