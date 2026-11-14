// Reckalambo. Title screen, then Level 1: wreck the Urus with the hammer, sell it, dance.
// Design notes are in docs/reckalambo_level1.md. The title picture comes from title_img.c
// (made by tools/make_title_img.py). Everything else is drawn here with placeholder shapes.

#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font5x7.h"
#include "reck_hammer.h"
#include "reck_jaguar.h"
#include "reck_sprites.h"
#include "title_img.h"

// ---- Tuning ----------------------------------------------------------------------------

#define BLINK_TICKS (GAME_TICK_HZ / 2)
#define END_WAIT    30  // ticks before SET is accepted on the wreck and end screens

#define GROUND_Y   280  // the feet line of the jaguar and the bottom of the car
#define MAN_SPEED  3
#define MAN_MIN_Y  200
#define MAN_MAX_Y  280
#define MAN_START_X 60
#define MAN_START_Y 260

#define CAR_X  296
#define CAR_W  RECK_CAR_W
#define CAR_H  RECK_CAR_H
#define CAR_Y  (GROUND_Y - CAR_H) // the car sprite is bottom-aligned on the ground
#define CAR_CX (CAR_X + CAR_W / 2)
#define CAR_CY (CAR_Y + CAR_H / 2)
#define MAN_MAX_X (CAR_X - 20) // he can stand at most this far right, so the head reaches the car
#define CAR_HP_MAX 1500

#define SWING_TICKS  12
#define IMPACT_TICK  6  // the hammer is at the car on this tick of the swing
#define HIT_PAD      6  // the car's hit zone is this much bigger than the car on each side
#define SALE_BONUS   500

#define GORILLA_MAX   4
#define GORILLA_FIRST 60 // ticks before the first gorilla
#define GORILLA_EVERY 60 // ticks between gorillas
#define GORILLA_LIFE  80
#define GORILLA_HALF  12 // half the size of the gorilla hit box
#define GORILLA_ANIM  4  // ticks per frame of the tumble
#define GORILLA_SPEED 3.5f

#define HITS_LIMIT   11 // the 11th hit restarts the level
#define IMMUNE_TICKS 20 // no damage for this long after a gorilla hit
#define DANCE_TICKS  120
#define POPUP_TICKS  30

// ---- Colours ---------------------------------------------------------------------------

#define COL_TEXT    RGB565(255, 255, 255)
#define COL_GOLD    RGB565(255, 210, 40)
#define COL_RED     RGB565(255, 60, 40)
#define COL_GREEN   RGB565(60, 220, 90)
#define COL_SKY     RGB565(40, 16, 56)
#define COL_ROAD    RGB565(58, 58, 66)
#define COL_LINE    RGB565(200, 190, 60)
#define COL_BUILD   RGB565(22, 22, 52)
#define COL_CARBLK  RGB565(12, 12, 12)
#define COL_CARYEL  RGB565(255, 210, 30)
#define COL_GLASS   RGB565(70, 100, 140)
#define COL_WHEEL   RGB565(40, 40, 40)
#define COL_WRECK   RGB565(96, 96, 96)
#define COL_SMOKE   RGB565(120, 120, 120)
#define COL_BAR_BG  RGB565(40, 10, 10)
#define COL_JAG     RGB565(235, 140, 40)
#define COL_JAG_DK  RGB565(150, 80, 20)
#define COL_SPOT    RGB565(20, 12, 8)
#define COL_HEAD    RGB565(25, 18, 12)
#define COL_EYE     RGB565(255, 40, 20)
#define COL_HANDLE  RGB565(140, 90, 40)
#define COL_HAM_G   RGB565(40, 200, 80)
#define COL_HAM_R   RGB565(230, 40, 40)
#define COL_GOR     RGB565(120, 70, 30)

// ---- State -----------------------------------------------------------------------------

enum { S_TITLE, S_PLAY, S_WRECKED, S_DANCE, S_DONE, S_LOST };

typedef struct {
    float x, y, vx, vy;
    int life; // ticks left, 0 when the slot is free
} Gorilla;

static int state, ticks, state_t;
static int man_x, man_y;
static int swing_t;          // 0 when not swinging, else 1..SWING_TICKS
static int car_hp;
static int points, money, hits, sold_for;
static int immune;           // ticks of no damage left
static int gorilla_t;        // ticks since the level started, for firing
static int popup_t, popup_val;
static Gorilla gor[GORILLA_MAX];

// The strip being drawn right now. Everything is clipped to it.
static uint16_t *strip;
static int strip_y0, strip_rows;

// ---- Drawing helpers -------------------------------------------------------------------

static void fill(int x, int y, int w, int h, uint16_t c) {
    int x0 = x < 0 ? 0 : x, x1 = x + w > GAME_W ? GAME_W : x + w;
    int y0 = y < strip_y0 ? strip_y0 : y;
    int y1 = y + h > strip_y0 + strip_rows ? strip_y0 + strip_rows : y + h;
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) strip[(yy - strip_y0) * GAME_W + xx] = c;
}

// A thick line made of small squares. Good enough for arms and handles.
static void line(int x0, int y0, int x1, int y1, int thick, uint16_t c) {
    int dx = x1 - x0, dy = y1 - y0;
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    if (steps == 0) steps = 1;
    for (int i = 0; i <= steps; i++) {
        int x = x0 + dx * i / steps, y = y0 + dy * i / steps;
        fill(x - thick / 2, y - thick / 2, thick, thick, c);
    }
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

// ---- Hammer ----------------------------------------------------------------------------

// Where the hammer head is on swing tick f (1..12), from the frame table. The grip is at
// (man_x + GRIP_DX, man_y + GRIP_DY); the head offset is from the grip.
#define GRIP_DX -7 // the closed fist in the jaguar sprite, relative to his feet
#define GRIP_DY -52
static void head_at(int f, int *dx, int *dy) {
    *dx = GRIP_DX + reck_hammer_head[f - 1][0];
    *dy = GRIP_DY + reck_hammer_head[f - 1][1];
}

// Called on the impact tick. Scores the hit if the head is inside the car.
static void impact(void) {
    int dx, dy;
    head_at(IMPACT_TICK, &dx, &dy);
    int hx = man_x + dx, hy = man_y + dy;
    int inside = hx >= CAR_X - HIT_PAD && hx <= CAR_X + CAR_W + HIT_PAD &&
                 hy >= CAR_Y - HIT_PAD && hy <= CAR_Y + CAR_H + HIT_PAD;
    if (!inside || car_hp <= 0) return; // a miss still counts as an attack, but scores nothing

    // Quality: 100 when the head is at the centre of the car, 0 at the edge of the hit zone.
    int dxn = abs(hx - CAR_CX) * 100 / (CAR_W / 2 + HIT_PAD);
    int dyn = abs(hy - CAR_CY) * 100 / (CAR_H / 2 + HIT_PAD);
    int d = dxn > dyn ? dxn : dyn;
    int q = 100 - d;
    if (q < 0) q = 0;
    int pts = 100 + 2 * q; // 100 to 300

    points += pts;
    car_hp -= pts;
    popup_val = pts;
    popup_t = POPUP_TICKS;
    if (car_hp <= 0) {
        car_hp = 0;
        state = S_WRECKED;
        state_t = 0;
    }
}

// ---- Level ------------------------------------------------------------------------------

static void start_level(void) {
    state = S_PLAY;
    state_t = 0;
    ticks = 0;
    man_x = MAN_START_X;
    man_y = MAN_START_Y;
    swing_t = 0;
    car_hp = CAR_HP_MAX;
    points = 0;
    money = 0;
    hits = 0;
    sold_for = 0;
    immune = 0;
    gorilla_t = 0;
    popup_t = 0;
    memset(gor, 0, sizeof gor);
}

static void fire_gorilla(void) {
    for (int i = 0; i < GORILLA_MAX; i++) {
        if (gor[i].life) continue;
        float sx = CAR_X + 20, sy = CAR_Y + 10;
        float tx = man_x, ty = man_y - 20;
        float dx = tx - sx, dy = ty - sy;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1) len = 1;
        gor[i].x = sx;
        gor[i].y = sy;
        gor[i].vx = dx / len * GORILLA_SPEED;
        gor[i].vy = dy / len * GORILLA_SPEED;
        gor[i].life = GORILLA_LIFE;
        return;
    }
}

// ---- Firmware entry points -------------------------------------------------------------

void game_init(void) {
    state = S_TITLE;
    state_t = 0;
    ticks = 0;
}

void game_update(uint8_t held, uint8_t pressed) {
    if (pressed & JOY_RESET) {
        game_init();
        return;
    }
    ticks++;
    state_t++;

    switch (state) {
    case S_TITLE:
        if (pressed & JOY_SET) start_level();
        return;

    case S_PLAY: {
        // Walk in 8 directions, kept on the ground strip and left of the car.
        int dx = 0, dy = 0;
        if (held & JOY_LEFT) dx -= MAN_SPEED;
        if (held & JOY_RIGHT) dx += MAN_SPEED;
        if (held & JOY_UP) dy -= MAN_SPEED;
        if (held & JOY_DOWN) dy += MAN_SPEED;
        man_x += dx;
        man_y += dy;
        if (man_x < 0) man_x = 0;
        if (man_x > MAN_MAX_X) man_x = MAN_MAX_X;
        if (man_y < MAN_MIN_Y) man_y = MAN_MIN_Y;
        if (man_y > MAN_MAX_Y) man_y = MAN_MAX_Y;

        // The swing. The impact is checked on its own tick, then the swing moves on.
        if (swing_t) {
            if (swing_t == IMPACT_TICK) impact();
            swing_t++;
            if (swing_t > SWING_TICKS) swing_t = 0;
        }
        if ((pressed & JOY_MID) && swing_t == 0 && state == S_PLAY) swing_t = 1;

        // The car fires gorillas until it is wrecked.
        gorilla_t++;
        if (gorilla_t >= GORILLA_FIRST && (gorilla_t - GORILLA_FIRST) % GORILLA_EVERY == 0)
            fire_gorilla();

        for (int i = 0; i < GORILLA_MAX; i++) {
            if (!gor[i].life) continue;
            gor[i].x += gor[i].vx;
            gor[i].y += gor[i].vy;
            gor[i].life--;
            // The jaguar's body is about 28 wide and 80 tall, feet at man_y.
            int gx = (int)gor[i].x, gy = (int)gor[i].y;
            if (immune == 0 && gx > man_x - 14 - GORILLA_HALF && gx < man_x + 14 + GORILLA_HALF &&
                gy > man_y - 80 - GORILLA_HALF && gy < man_y + GORILLA_HALF) {
                hits++;
                immune = IMMUNE_TICKS;
                gor[i].life = 0;
                if (hits >= HITS_LIMIT) {
                    state = S_LOST;
                    state_t = 0;
                    return;
                }
            }
        }
        if (immune) immune--;
        if (popup_t) popup_t--;
        return;
    }

    case S_WRECKED:
        // The car is still on fire for a moment. Then SET sells it.
        if (state_t >= END_WAIT && (pressed & JOY_SET)) {
            sold_for = points + SALE_BONUS;
            money = sold_for;
            state = S_DANCE;
            state_t = 0;
        }
        return;

    case S_DANCE:
        if (state_t >= DANCE_TICKS) {
            state = S_DONE;
            state_t = 0;
        }
        return;

    case S_DONE:
        if (state_t >= END_WAIT && (pressed & JOY_SET)) game_init();
        return;

    case S_LOST:
        if (state_t >= END_WAIT && (pressed & JOY_SET)) start_level();
        return;
    }
}

// ---- Drawing ---------------------------------------------------------------------------

static void draw_background(void) {
    fill(0, 0, GAME_W, GROUND_Y, COL_SKY);
    // A row of simple buildings. Heights and positions are fixed.
    static const struct { int x, w, h; } b[] = {
        {0, 60, 130}, {70, 44, 96}, {124, 70, 150}, {210, 40, 86},
        {260, 56, 120}, {330, 64, 160}, {404, 76, 110},
    };
    for (unsigned i = 0; i < sizeof b / sizeof b[0]; i++) fill(b[i].x, GROUND_Y - b[i].h, b[i].w, b[i].h, COL_BUILD);
    fill(0, GROUND_Y, GAME_W, GAME_H - GROUND_Y, COL_ROAD);
    fill(0, GROUND_Y + 10, GAME_W, 2, COL_LINE);
}

// Draws a sprite whose transparent pixels are RECK_KEY, clipped to the strip.
static void blit(const uint16_t *spr, int w, int h, int x, int y) {
    for (int yy = 0; yy < h; yy++) {
        int py = y + yy;
        if (py < strip_y0 || py >= strip_y0 + strip_rows) continue;
        for (int xx = 0; xx < w; xx++) {
            uint16_t c = spr[yy * w + xx];
            int px = x + xx;
            if (c != RECK_KEY && px >= 0 && px < GAME_W) strip[(py - strip_y0) * GAME_W + px] = c;
        }
    }
}

static void draw_car(void) {
    int wrecked = state != S_PLAY;
    blit(wrecked ? reck_car_wreck : reck_car_intact, RECK_CAR_W, RECK_CAR_H, CAR_X, CAR_Y);

    if (!wrecked) {
        // Cracks as the car takes damage, and the health bar above it.
        int n = (CAR_HP_MAX - car_hp) * 8 / CAR_HP_MAX;
        for (int i = 0; i < n; i++) {
            int x = CAR_X + 40 + i * 12;
            line(x, CAR_Y + 44, x + 6, CAR_Y + 60, 2, COL_TEXT);
        }
        fill(CAR_X, CAR_Y - 12, CAR_W, 6, COL_BAR_BG);
        int w = car_hp * CAR_W / CAR_HP_MAX;
        fill(CAR_X, CAR_Y - 12, w, 6, car_hp > CAR_HP_MAX / 2 ? COL_GREEN : COL_RED);
    }
}

static void draw_gorillas(void) {
    // The tumble alternates between the two poses while the gorilla flies.
    const uint16_t *frame = ((ticks / GORILLA_ANIM) & 1) ? reck_gorilla_arms : reck_gorilla_tumble;
    for (int i = 0; i < GORILLA_MAX; i++) {
        if (!gor[i].life) continue;
        blit(frame, RECK_GOR_W, RECK_GOR_H, (int)gor[i].x - RECK_GOR_W / 2, (int)gor[i].y - RECK_GOR_H / 2);
    }
}

// The jaguar. The sprite already holds his hammer, so the strike is not drawn yet: the
// hammer's hit box still follows head_at(). `dance` sways him from side to side.
static void draw_man(int mx, int my, int dance) {
    int sway = dance ? (((state_t / 8) & 1) ? -8 : 8) : 0;
    int x = mx + sway;
    blit(reck_jaguar, RECK_JAG_W, RECK_JAG_H, x - RECK_JAG_W / 2, my - RECK_JAG_H);

    // The hammer: frame 1 when at rest (raised behind), else the frame for this swing tick.
    int f = swing_t ? swing_t : 1;
    int gx = x + GRIP_DX, gy = my + GRIP_DY;
    blit(reck_hammer[f - 1], RECK_HAMMER_W, RECK_HAMMER_W, gx - RECK_HAMMER_PIVOT, gy - RECK_HAMMER_PIVOT);
}

static void draw_hud(void) {
    fill(0, 0, GAME_W, 22, RGB565(12, 4, 14));
    fill(0, 22, GAME_W, 2, COL_GOLD);
    char buf[32];
    snprintf(buf, sizeof buf, "POINTS %d", points);
    text(8, 7, buf, COL_TEXT, 2);
    snprintf(buf, sizeof buf, "HITS %d/%d", hits, HITS_LIMIT - 1);
    text(GAME_W - text_width(buf, 2) - 8, 7, buf, hits >= HITS_LIMIT - 2 ? COL_RED : COL_TEXT, 2);
}

static void draw_play(void) {
    draw_background();
    draw_car();
    draw_gorillas();

    // The jaguar blinks while he can't be hurt.
    if (!(immune && ((ticks / 3) & 1))) draw_man(man_x, man_y, 0);

    if (popup_t) {
        char buf[16];
        snprintf(buf, sizeof buf, "+%d", popup_val);
        text(CAR_X + 20, CAR_Y - 34, buf, COL_GOLD, 3);
    }
    draw_hud();

    if (state == S_WRECKED) {
        text_centred(120, "WRECKED!", COL_GOLD, 4);
        if (state_t >= END_WAIT && (ticks / BLINK_TICKS) % 2 == 0)
            text_centred(170, "SET: SELL", COL_TEXT, 2);
    }
}

void game_draw(uint16_t *buf, int y0, int rows) {
    strip = buf;
    strip_y0 = y0;
    strip_rows = rows;

    if (state == S_TITLE) {
        memcpy(buf, title_img + (size_t)y0 * GAME_W, (size_t)rows * GAME_W * sizeof *buf);
        text_centred(16, "RECKALAMBO", COL_GOLD, 4);
        if ((ticks / BLINK_TICKS) % 2 == 0) text_centred(284, "PRESS SET", COL_TEXT, 3);
        return;
    }

    if (state == S_DANCE) {
        draw_background();
        draw_car();
        draw_man(man_x, man_y, 1);
        draw_hud();
        text_centred(90, "SOLD", COL_GOLD, 5);
        char buf2[32];
        snprintf(buf2, sizeof buf2, "FOR $%d", sold_for);
        text_centred(150, buf2, COL_TEXT, 3);
        return;
    }

    if (state == S_DONE) {
        draw_background();
        text_centred(40, "LEVEL 1", COL_GOLD, 4);
        text_centred(80, "COMPLETE", COL_GOLD, 4);
        char buf2[32];
        snprintf(buf2, sizeof buf2, "POINTS %d", points);
        text_centred(150, buf2, COL_TEXT, 2);
        snprintf(buf2, sizeof buf2, "MONEY $%d", money);
        text_centred(180, buf2, COL_TEXT, 2);
        if ((ticks / BLINK_TICKS) % 2 == 0) text_centred(240, "SET: TITLE", COL_GOLD, 2);
        return;
    }

    if (state == S_LOST) {
        draw_background();
        text_centred(90, "TOO MANY HITS", COL_RED, 3);
        text_centred(130, "LEVEL RESTARTS", COL_TEXT, 2);
        if ((ticks / BLINK_TICKS) % 2 == 0) text_centred(200, "SET: RETRY", COL_TEXT, 2);
        return;
    }

    draw_play();
}
