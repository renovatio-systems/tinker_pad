// Ninja Invasion: Jimmy Bob defeats the skeletons and ghosts in three jungle levels,
// then the Mega Boss. His friends help him: a golden eagle (lasers), a yellow living
// banana (peels) and a Tasmanian devil (poo).
//
// Controls
//   Title:   SET starts.
//   Playing: LEFT / RIGHT walk, UP jumps, MID attacks with the weapon in hand,
//            SET switches between the crossbow and the samurai sword.
//            RST asks "reset the game?": RST again resets, SET goes back to the game.
//   Out of hearts: SET tries the level again (coins are kept).
//   Shop:    only the arrows and MID. UP/DOWN pick the character, LEFT/RIGHT the armour,
//            MID buys or wears it. The back arrow (UP from the top row) leaves with MID.
//   Wallet:  coins and bought armour are saved in flash and survive power-off and RST.
//            Only flashing new firmware erases them.
//   Win:     SET goes back to the title.
//
// Coins: skeleton 5, ghost 10, Mega Boss 200. Every 5th kill in a row without losing a
// heart is +10. Finishing a level pays 10 x the level number, +20 if no heart was lost, and
// an accuracy bonus (crossbow bolts and sword swings that hit): 90% +30, 75% +20, 50% +10
// (needs at least 5 shots). The level-clear screen shows all of it; SET carries on.
// Danger: skeletons swing their swords when close, ghosts touch you, and the
// Mega Boss shoots snake poison darts. Five hearts; you are safe for a moment
// after each hit.
//
// Art: src/ninja_art.c, made from the artwork by tools/make_ninja_art.py.

#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font5x7.h"
#include "ninja_art.h"
#include "shop_art.h"
#include "wallet.h"

// ---- Layout ----------------------------------------------------------------

#define GROUND_Y   296                  // top of the earth strip; feet stand here (24 px of earth below)
#define HUD_BAND   44                   // dark band behind the HUD
#define HERO_DRAW  60                   // Jimmy Bob is drawn 60 px tall
#define HERO_SPEED 5
#define GRAVITY    1
#define MAX_FALL   12
#define JUMP_V     (-14)                // jump height is about 98 px
#define HEARTS_MAX 5
#define HURT_TICKS 45

#define ENEMY_DRAW 54                   // skeletons
#define GHOST_DRAW 46                   // ghosts are a bit smaller
#define GHOST_VIS  80                   // a ghost is visible for this many of each 100 ticks
#define GHOST_PAD  6                    // extra reach on each side when hitting a ghost
#define BOSS_DRAW  90
#define BOSS_FALLEN_H 30                // the boss lies this tall once beaten
#define BOSS_HP    24
#define BOSS_LEVEL 10                   // level 10 is the big boss
#define BOSS_X     336
#define BOSS_Y     (GROUND_Y - BOSS_DRAW)
#define MAX_ENEMY  24
#define MAX_PROJ   24
#define BANNER_MIN_TICKS 30             // the level-clear screen ignores SET for this long
#define COIN_SKELETON 5
#define COIN_GHOST    10
#define COIN_BOSS     200
#define STREAK_EVERY  5                 // every 5th kill in a row without losing a heart...
#define STREAK_BONUS  10                // ...pays this much
#define LEVEL_BONUS_PER_LEVEL 10        // finishing level n pays 10 x n
#define PERFECT_BONUS 20                // no heart lost this level
#define ACC_MIN_SHOTS 5                 // fewer shots than this and there is no accuracy bonus
#define BOLT_SPEED 12                   // crossbow bolt, pixels per tick

// Friends are drawn 56 px. The eagle hovers at EAGLE_Y; the banana and the devil
// walk and jump onto the platforms to get in range of the enemies.
#define EAGLE_Y 52
#define FRIEND_DRAW 56                  // the small sidekicks keep their size
#define BUDDY_SPEED 2
#define RETARGET_TICKS 90
#define COL_HP_GOOD RGB565(80, 220, 80)
#define COL_RED     RGB565(230, 40, 40)

// ---- Colours ---------------------------------------------------------------

#define COL_TEXT     RGB565(255, 255, 255)
#define COL_DIM      RGB565(160, 200, 200)
#define COL_GOLD     RGB565(255, 220, 40)
#define COL_HEAD_OFF RGB565(40, 60, 60)
#define COL_BAND     RGB565(6, 24, 28)
#define COL_BLACK    RGB565(0, 0, 0)
#define COL_LASER    RGB565(80, 240, 255)
#define COL_PEEL     RGB565(255, 230, 60)
#define COL_POO      RGB565(150, 100, 30)
#define COL_DART     RGB565(60, 220, 120)
#define COL_DAGGER   RGB565(230, 40, 40)
#define COL_GUARD    RGB565(255, 220, 40)

// ---- Types -----------------------------------------------------------------

typedef enum { ST_TITLE, ST_PLAY, ST_BANNER, ST_DEAD, ST_BOSS_DOWN, ST_WIN, ST_SHOP } state_t;
typedef enum { K_SKELETON, K_GHOST, K_BOSS } kind_t;
typedef enum { W_CROSSBOW, W_SWORD } weapon_t;
typedef enum { PJ_BOLT, PJ_PEEL, PJ_POO, PJ_DART } proj_kind_t;

// A place to stand: the ground, or a ledge. `top` is the y of its surface.
typedef struct {
    int x0, x1, top;
} surface_t;

static const surface_t surfaces[] = {
    {0, 480, GROUND_Y},  // the ground
    {30, 118, 256},
    {150, 238, 208},
    {300, 388, 256},
    {380, 468, 184},
    {20, 108, 160},
    {250, 338, 152},
};
#define N_SURFACES (sizeof surfaces / sizeof surfaces[0])

typedef struct {
    kind_t kind;
    int x, y, sp, dir, hp, cool;
    int vx;    // horizontal pixels per tick, for leading shots
    int vis_t, phase;  // ghosts: visible for GHOST_VIS ticks of each 100, then hidden
    int alive;
} enemy_t;

// Positions and velocities are in 1/16 pixel.
typedef struct {
    int x, y, vx, vy;
    proj_kind_t kind;
    int on;
} proj_t;

// The banana and the devil: they walk and jump between surfaces.
typedef struct {
    int x, y, vy;
    int target;   // index into surfaces[] they are heading for
    int cool;     // ticks until they pick a new target
    int shot;     // ticks until they can throw again
    int hp;       // hit points from their armour; at 0 they are out for the level
} buddy_t;

// ---- Game state ------------------------------------------------------------

static state_t state;
static int level;                // 1 to 3, then 4 for the Mega Boss
static int coins;
static int hero_hp, hero_inv;
static int hx, hy, vy, facing;   // Jimmy Bob: top-left, vertical speed, facing (+1 right)
static weapon_t weapon;
static int bolt_cool;            // ticks until the crossbow can fire again
static int attack_pending;       // MID was pressed and the attack has not fired yet
static int swing_t, swing_cool;  // sword: ticks left in the swing, ticks until the next swing
static uint32_t swing_mask;      // enemies already hit by this swing

static enemy_t enemies[MAX_ENEMY];
static int n_enemies;
static proj_t projs[MAX_PROJ];
static int eagle_cool, laser_t;
static int eagle_hp;            // the eagle's hit points (ghosts can reach it, skeletons cannot)
// Which sidekicks are with Jimmy Bob this level: none in level 1, one from level 2 on.
static int side_eagle, side_banana, side_devil;
static int eagle_x, eagle_y;
static buddy_t banana, devil;
static int laser_x0, laser_y0, laser_x1, laser_y1;
static int banner_timer, boss_timer;

// This level's coin and accuracy tally (cleared by start_level), and the level-clear sums.
static int lv_skel, lv_ghost;       // kills this level
static int lv_streak, lv_streak_coins; // kills in a row without losing a heart, coins paid for them
static int lv_lost_heart;           // 1 once a heart is lost this level
static int lv_shots, lv_hits;       // bolts fired and swings made, and how many hit
static int swing_scored;            // this swing has already counted as a hit
static int sum_level, sum_perfect, sum_acc_pct, sum_acc_bonus, sum_total; // sum_acc_pct is -1 if too few shots
static int ticks;
static int confirm_reset;        // RST was pressed: waiting for RST (reset) or SET (back to the game)

static uint32_t rng = 4242;

// The armour shop: five tiers for each of four characters. Starter is free and worn from the start.
#define N_ARMOUR 5
static const int armour_price[N_ARMOUR] = {0, 100, 300, 800, 2000};
static const int armour_def[N_ARMOUR] = {1, 3, 8, 25, 100};   // blocks this % of hits, up to 50
static const int armour_lives[N_ARMOUR] = {5, 6, 10, 20, 50}; // hit points
static const char *const armour_name[N_ARMOUR] = {"STARTER", "NINJA", "ROYAL", "LEGENDARY", "ULTIMATE"};
// Characters with armour: 0 Jimmy Bob, 1 banana, 2 golden eagle, 3 Tasmanian devil.
#define N_SHOPS 4
static const char *const character_name[N_SHOPS] = {"JIMMY BOB", "BANANA", "GOLDEN EAGLE", "TASMANIAN DEVIL"};
static uint16_t owned_armour[N_SHOPS];  // bit i set: tier i is bought
static int equipped[N_SHOPS];           // the tier each one is wearing
static int shop_row, shop_col;          // the cursor: character row 0-3 and tier column 0-4
static int shop_on_back;                // the cursor is on the back arrow
static int shop_next, shop_from_title;

// The strip being drawn right now. Everything is clipped to it.
static uint16_t *strip;
static int strip_y0, strip_rows;

// ---- Helpers ---------------------------------------------------------------

static int imax(int a, int b) { return a > b ? a : b; }
static int imin(int a, int b) { return a < b ? a : b; }
static int iabs(int a) { return a < 0 ? -a : a; }
static int sgn(int a) { return a > 0 ? 1 : a < 0 ? -1 : 0; }

static int rnd(int n) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (int)(rng % (uint32_t)n);
}

static int overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

// Fills a rectangle, clipped to the strip and the screen.
static void fill(int x, int y, int w, int h, uint16_t c) {
    int x0 = imax(x, 0), x1 = imin(x + w, GAME_W);
    int y0 = imax(y, strip_y0), y1 = imin(y + h, strip_y0 + strip_rows);
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) strip[(yy - strip_y0) * GAME_W + xx] = c;
}

// A border of thickness t around a rectangle.
static void frame(int x, int y, int w, int h, int t, uint16_t c) {
    fill(x, y, w, t, c);
    fill(x, y + h - t, w, t, c);
    fill(x, y, t, h, c);
    fill(x + w - t, y, t, h, c);
}

// A filled circle, clipped to the strip.
static void disc(int cx, int cy, int r, uint16_t c) {
    int y0 = imax(cy - r, strip_y0), y1 = imin(cy + r + 1, strip_y0 + strip_rows);
    int x0 = imax(cx - r, 0), x1 = imin(cx + r + 1, GAME_W);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy <= r * r) strip[(y - strip_y0) * GAME_W + x] = c;
        }
}

// Draws a picture of sw x sh pixels into a dw x dh box at (x, y), nearest-neighbour.
// Magenta is transparent. Mirrored left to right if flip is set.
static void spr(const uint16_t *img, int sw, int sh, int x, int y, int dw, int dh, int flip) {
    int ya = imax(y, strip_y0), yb = imin(y + dh, strip_y0 + strip_rows);
    for (int py = ya; py < yb; py++) {
        int sy = (py - y) * sh / dh;
        for (int ox = 0; ox < dw; ox++) {
            int px = x + ox;
            if (px < 0 || px >= GAME_W) continue;
            int sx = ox * sw / dw;
            if (flip) sx = sw - 1 - sx;
            uint16_t c = img[sy * sw + sx];
            if (c != NJ_KEY) strip[(py - strip_y0) * GAME_W + px] = c;
        }
    }
}

// A thick line in pixels.
static void line(int x0, int y0, int x1, int y1, uint16_t c) {
    int n = imax(iabs(x1 - x0), iabs(y1 - y0));
    if (n == 0) {
        fill(x0, y0, 2, 2, c);
        return;
    }
    for (int i = 0; i <= n; i++) fill(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, 2, 2, c);
}

// Text in the 5x7 font, `scale` times bigger. Each character is 6 * scale wide.
static void text_adv(int x, int y, const char *s, uint16_t c, int scale, int advance) {
    for (; *s; s++, x += advance) {
        const uint8_t *cols = font5x7_glyph(*s);
        for (int gx = 0; gx < 5; gx++)
            for (int gy = 0; gy < 7; gy++)
                if ((cols[gx] >> gy) & 1) fill(x + gx * scale, y + gy * scale, scale, scale, c);
    }
}

static void text(int x, int y, const char *s, uint16_t c, int scale) {
    text_adv(x, y, s, c, scale, 6 * scale);
}

static int text_width(const char *s, int scale) {
    int n = (int)strlen(s);
    return n ? n * 6 * scale - scale : 0;
}

static void text_centred(int y, const char *s, uint16_t c, int scale) {
    text((GAME_W - text_width(s, scale)) / 2, y, s, c, scale);
}

static void text_right(int xr, int y, const char *s, uint16_t c, int scale) {
    text(xr - text_width(s, scale), y, s, c, scale);
}

// ---- Sky and background ----------------------------------------------------

// The full-screen jungle, dimmed so the characters stand out. Copied row by row.
// ---- Sky -------------------------------------------------------------------
// Black-sky levels: stars, a moon and two planets. Level 2: a sunny blue sky with a
// sun, clouds and birds. All drawn in code.

#define N_STARS 40
#define COL_STAR_ON  RGB565(255, 255, 235)
#define COL_STAR_OFF RGB565(70, 80, 110)
#define COL_MOON     RGB565(240, 236, 210)
#define COL_PLANET   RGB565(230, 140, 60)
#define COL_RING     RGB565(250, 220, 150)
#define COL_PLANET2  RGB565(80, 200, 190)
#define COL_SUN      RGB565(255, 214, 40)
#define COL_CLOUD    RGB565(255, 255, 255)
#define COL_CLOUD_SH RGB565(205, 222, 238)
#define COL_BIRD     RGB565(30, 30, 60)
#define COL_SKY_TOP  RGB565(70, 150, 235)
#define COL_SKY_LOW  RGB565(170, 220, 250)

typedef struct {
    int x, y, big, phase;
} star_t;

static star_t stars[N_STARS];

// Fixed positions, so the stars never move between games.
static void make_stars(void) {
    uint32_t s = 2024;
    for (int i = 0; i < N_STARS; i++) {
        s = s * 1664525u + 1013904223u;
        stars[i].x = (s >> 8) % GAME_W;
        s = s * 1664525u + 1013904223u;
        stars[i].y = HUD_BAND + 4 + (s >> 8) % (GROUND_Y - HUD_BAND - 80);
        stars[i].big = (s >> 20) & 1;
        stars[i].phase = (s >> 24) & 63;
    }
}

// A ring round a planet: an ellipse band, drawn behind the planet's body.
static void ring(int cx, int cy, int rx, int ry, int thick, uint16_t c) {
    int y0 = imax(cy - ry, strip_y0), y1 = imin(cy + ry + 1, strip_y0 + strip_rows);
    int x0 = imax(cx - rx, 0), x1 = imin(cx + rx + 1, GAME_W);
    // Point inside the ellipse with semi-axes (a, b): dx^2 * b^2 + dy^2 * a^2 <= a^2 * b^2.
    long a = rx, b = ry, ai = rx - thick, bi = ry - thick;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            long dx = x - cx, dy = y - cy;
            int in_outer = dx * dx * b * b + dy * dy * a * a <= a * a * b * b;
            int in_inner = dx * dx * bi * bi + dy * dy * ai * ai <= ai * ai * bi * bi;
            if (in_outer && !in_inner) strip[(y - strip_y0) * GAME_W + x] = c;
        }
}

static void night_sky(void) {
    fill(0, strip_y0, GAME_W, strip_rows, COL_BLACK); // clear first, or last frame's pixels show through
    for (int i = 0; i < N_STARS; i++) {
        int lit = ((ticks + stars[i].phase) >> 3) % 5 != 0; // each star twinkles on its own phase
        fill(stars[i].x, stars[i].y, stars[i].big ? 2 : 1, stars[i].big ? 2 : 1, lit ? COL_STAR_ON : COL_STAR_OFF);
    }
    // Moon: a cream disc with a black disc over part of it, so it reads as a crescent.
    disc(440, 80, 18, COL_MOON);
    disc(447, 74, 16, COL_BLACK);
    // Two planets: a ringed orange one, and a small teal one.
    ring(330, 118, 30, 8, 3, COL_RING);
    disc(330, 118, 12, COL_PLANET);
    disc(64, 150, 7, COL_PLANET2);
}

static void cloud(int cx, int cy) {
    disc(cx + 18, cy + 12, 14, COL_CLOUD_SH);
    disc(cx, cy, 14, COL_CLOUD);
    disc(cx + 18, cy - 8, 18, COL_CLOUD);
    disc(cx + 38, cy, 14, COL_CLOUD);
    disc(cx + 18, cy + 6, 12, COL_CLOUD);
}

// A bird: two wings in a V that flap up and down.
static void bird(int x, int y, int up) {
    int dy = up ? -3 : 3;
    line(x - 7, y + dy, x, y, COL_BIRD);
    line(x, y, x + 7, y + dy, COL_BIRD);
}

static void day_sky(void) {
    for (int y = strip_y0; y < strip_y0 + strip_rows; y++) {
        int t = y * 256 / GROUND_Y;
        uint16_t c = RGB565(70 + (170 - 70) * t / 256, 150 + (220 - 150) * t / 256, 235 + (250 - 235) * t / 256);
        for (int x = 0; x < GAME_W; x++) strip[(y - strip_y0) * GAME_W + x] = c;
    }
    // Sun with a ring of rays.
    disc(90, 100, 22, COL_SUN);
    static const int ray[8][2] = {{34, 0}, {-34, 0}, {0, 34}, {0, -34}, {24, 24}, {-24, 24}, {24, -24}, {-24, -24}};
    for (int i = 0; i < 8; i++) disc(90 + ray[i][0], 100 + ray[i][1], 3, COL_SUN);
    // Clouds drift slowly to the right and wrap round.
    static const int cloud_y[3] = {150, 118, 186};
    static const int cloud_x[3] = {30, 210, 330};
    for (int i = 0; i < 3; i++) cloud((cloud_x[i] + ticks / 3) % (GAME_W + 120) - 60, cloud_y[i]);
    // Birds fly in from the right, flapping.
    static const int bird_x[4] = {430, 470, 390, 300};
    static const int bird_y[4] = {66, 92, 58, 80};
    for (int i = 0; i < 4; i++) {
        int x = (bird_x[i] - ticks / 4 + 2 * GAME_W) % (GAME_W + 40);
        bird(x, bird_y[i], (ticks / 6 + i) & 1);
    }
}

// A black sky with a stepped yellow pyramid, drawn in code. Its base sits on the ground.
#define PYR_TOP  60
#define PYR_TIER 39
#define COL_PYR  RGB565(206, 150, 30)
#define COL_PYR_EDGE RGB565(250, 210, 80)
#define COL_PYR_SHADE RGB565(120, 80, 10)
#define COL_EARTH RGB565(74, 46, 14)
#define COL_EARTH_JOINT RGB565(40, 24, 6)

// Helpers for the level backgrounds. Every background is drawn in code, strip by strip.

static void gradient(int y_top, int y_bot, int r0, int g0, int b0, int r1, int g1, int b1) {
    for (int y = imax(strip_y0, y_top); y < imin(strip_y0 + strip_rows, y_bot); y++) {
        int t = (y - y_top) * 256 / (y_bot - y_top);
        uint16_t c = RGB565(r0 + (r1 - r0) * t / 256, g0 + (g1 - g0) * t / 256, b0 + (b1 - b0) * t / 256);
        fill(0, y, GAME_W, 1, c);
    }
}

// The stepped pyramid, from PYR_TOP down to the ground.
static void pyramid(void) {
    for (int y = imax(strip_y0, PYR_TOP); y < imin(strip_y0 + strip_rows, GROUND_Y); y++) {
        int tier = (y - PYR_TOP) / PYR_TIER;              // 0 at the top, 5 at the base
        int half = 10 + tier * 22;                         // each step is wider
        uint16_t c = (y - PYR_TOP) % PYR_TIER == 0 ? COL_PYR_EDGE : COL_PYR;
        fill(GAME_W / 2 - half, y, 2 * half, 1, c);
        fill(GAME_W / 2 + half - 3, y, 3, 1, COL_PYR_SHADE);
    }
}

// The ground: a band of blocks from the ground line to the bottom, with a line on top.
static void ground_band(uint16_t body, uint16_t joint, uint16_t line_c) {
    for (int y = imax(strip_y0, GROUND_Y + 2); y < strip_y0 + strip_rows; y++) {
        int row = (y - GROUND_Y - 2) / 16;
        fill(0, y, GAME_W, 1, body);
        if ((y - GROUND_Y - 2) % 16 == 0) fill(0, y, GAME_W, 1, joint);
        for (int x = (row & 1) * 24; x < GAME_W; x += 48) fill(x, y, 2, 1, joint);
    }
    if (GROUND_Y >= strip_y0 && GROUND_Y < strip_y0 + strip_rows) fill(0, GROUND_Y, GAME_W, 2, line_c);
}

// Level 3: a race track with a crowd in the stands and a kerb.
static void race_track(void) {
    fill(0, strip_y0, GAME_W, strip_rows, COL_BLACK);
    gradient(44, 120, 120, 150, 190, 190, 205, 225);
    static const uint16_t crowd[4] = {RGB565(230, 40, 40), RGB565(255, 220, 40), RGB565(255, 255, 255), RGB565(40, 200, 190)};
    for (int y = 48; y < 112; y += 8)
        for (int x = (y & 8) ? 0 : 4; x < GAME_W; x += 8)
            fill(x, y, 5, 5, crowd[(x * 5 + y * 3 + ticks / 30) % 4]);
    fill(0, 120, GAME_W, 176, RGB565(58, 58, 66));
    for (int x = 0; x < GAME_W; x += 16)
        fill(x, 118, 16, 4, ((x / 16 + ticks / 10) & 1) ? COL_RED : COL_TEXT);
    for (int y = 160; y <= 210; y += 50)
        for (int x = -((ticks * 4) % 24); x < GAME_W; x += 24) fill(x, y, 14, 3, COL_TEXT);
    ground_band(RGB565(40, 40, 46), RGB565(70, 70, 80), COL_TEXT);
}

// Level 4: a graveyard at night: a moon, stars, tombstones and dead trees.
static void graveyard(void) {
    fill(0, strip_y0, GAME_W, strip_rows, COL_BLACK);
    gradient(44, GROUND_Y, 12, 10, 30, 40, 30, 62);
    for (int i = 0; i < N_STARS; i++) {
        int lit = ((ticks + stars[i].phase) >> 3) % 5 != 0;
        fill(stars[i].x, stars[i].y, stars[i].big ? 2 : 1, stars[i].big ? 2 : 1, lit ? COL_STAR_ON : COL_STAR_OFF);
    }
    disc(400, 72, 14, COL_MOON);
    disc(405, 68, 12, RGB565(20, 16, 44));
    static const int tomb_x[6] = {30, 110, 190, 290, 370, 440};
    for (int i = 0; i < 6; i++) {
        uint16_t stone = RGB565(130, 134, 142);
        fill(tomb_x[i] - 12, GROUND_Y - 40, 24, 40, stone);
        disc(tomb_x[i], GROUND_Y - 40, 12, stone);
    }
    static const int tree_x[2] = {230, 420};
    for (int i = 0; i < 2; i++) {
        int x = tree_x[i];
        fill(x - 3, 150, 6, GROUND_Y - 150, RGB565(50, 40, 40));
        line(x, 180, x - 22, 150, RGB565(50, 40, 40));
        line(x, 200, x + 20, 166, RGB565(50, 40, 40));
    }
    ground_band(RGB565(52, 38, 30), RGB565(30, 20, 16), RGB565(120, 110, 100));
}

// Level 5: a haunted mansion with lit windows.
static void mansion(void) {
    fill(0, strip_y0, GAME_W, strip_rows, COL_BLACK);
    gradient(44, GROUND_Y, 20, 8, 30, 70, 24, 64);
    fill(120, 120, 240, GROUND_Y - 120, RGB565(64, 40, 72));
    for (int y = 80; y < 120; y++) {
        int hw = 120 * (y - 80) / 40;
        fill(240 - hw, y, 2 * hw, 1, RGB565(40, 24, 48));
    }
    for (int wy = 140; wy <= 190; wy += 50)
        for (int wx = 150; wx <= 300; wx += 50) {
            int lit = (wx + wy + ticks / 25) % 3 != 0;
            fill(wx, wy, 20, 26, lit ? RGB565(255, 210, 80) : RGB565(40, 30, 40));
        }
    fill(228, GROUND_Y - 46, 24, 46, COL_BLACK);
    ground_band(RGB565(110, 70, 40), RGB565(70, 44, 24), RGB565(160, 110, 60));
}

// Level 6: a forest of round trees.
static void forest(void) {
    fill(0, strip_y0, GAME_W, strip_rows, COL_BLACK);
    gradient(44, GROUND_Y, 110, 190, 130, 40, 110, 60);
    static const int tree_x[6] = {30, 110, 190, 270, 350, 430};
    for (int i = 0; i < 6; i++) {
        int x = tree_x[i];
        uint16_t leaves = (i & 1) ? RGB565(30, 110, 50) : RGB565(24, 96, 44);
        fill(x - 6, 170, 12, GROUND_Y - 170, RGB565(90, 60, 30));
        disc(x, 150, 40, leaves);
        disc(x - 26, 176, 28, leaves);
        disc(x + 26, 176, 28, leaves);
        disc(x - 8, 140, 14, RGB565(60, 160, 80));
    }
    ground_band(RGB565(60, 110, 40), RGB565(40, 80, 26), RGB565(120, 200, 90));
}

// Level 7: a lava bridge over a glowing lava river.
static void lava_bridge(void) {
    fill(0, strip_y0, GAME_W, strip_rows, COL_BLACK);
    gradient(44, GROUND_Y, 60, 12, 14, 170, 60, 20);
    fill(40, 100, 60, 180, RGB565(60, 40, 40));
    fill(380, 90, 70, 190, RGB565(60, 40, 40));
    fill(0, 200, GAME_W, 14, RGB565(130, 110, 100));          // the bridge deck
    for (int x = 0; x < GAME_W; x += 40) fill(x, 188, 4, 12, RGB565(130, 110, 100));
    fill(0, 214, GAME_W, GROUND_Y - 214, RGB565(230, 90, 30)); // lava under the bridge
    for (int x = -((ticks * 2) % 60); x < GAME_W; x += 60)
        fill(x, 240 + ((x / 60) & 1) * 20, 26, 3, RGB565(255, 220, 80));
    ground_band(RGB565(200, 60, 20), RGB565(255, 160, 60), RGB565(255, 200, 80));
}

// Level 8: a football pitch with a crowd in the stands.
static void football(void) {
    fill(0, strip_y0, GAME_W, strip_rows, COL_BLACK);
    gradient(44, 110, 150, 200, 250, 220, 240, 255);
    fill(0, 44, GAME_W, 66, RGB565(90, 90, 100));
    static const uint16_t crowd[4] = {RGB565(230, 40, 40), RGB565(255, 220, 40), RGB565(60, 120, 255), RGB565(255, 255, 255)};
    for (int y = 48; y < 106; y += 8)
        for (int x = (y & 8) ? 0 : 6; x < GAME_W; x += 12) fill(x, y, 6, 5, crowd[(x + y + ticks / 40) % 4]);
    for (int y = 110; y < GROUND_Y; y++)                      // grass stripes
        fill(0, y, GAME_W, 1, ((y - 110) / 24) & 1 ? RGB565(40, 140, 50) : RGB565(50, 160, 60));
    frame(20, 118, 440, 172, 2, COL_TEXT);
    fill(239, 118, 2, 172, COL_TEXT);
    ring(240, 204, 28, 26, 2, COL_TEXT);
    frame(20, 160, 56, 88, 2, COL_TEXT);
    frame(404, 160, 56, 88, 2, COL_TEXT);
    ground_band(RGB565(30, 110, 40), RGB565(20, 80, 30), COL_TEXT);
}

// Level 9: the desert, with dunes and the pyramid.
static void desert(void) {
    fill(0, strip_y0, GAME_W, strip_rows, COL_BLACK);
    gradient(44, 180, 255, 170, 80, 255, 230, 170);
    disc(410, 80, 22, RGB565(255, 240, 120));
    pyramid();
    disc(90, 250, 110, RGB565(210, 170, 100));
    disc(330, 262, 120, RGB565(220, 180, 110));
    disc(470, 270, 90, RGB565(210, 170, 100));
    ground_band(RGB565(200, 160, 90), RGB565(160, 120, 60), RGB565(240, 200, 120));
}

// Level 10: the big boss arena: a red glow, gold pillars and a stone floor.
static void boss_arena(void) {
    fill(0, strip_y0, GAME_W, strip_rows, COL_BLACK);
    disc(240, 140, 120, RGB565(50, 10, 14));
    fill(30, 60, 22, 236, RGB565(180, 130, 30));
    fill(428, 60, 22, 236, RGB565(180, 130, 30));
    fill(24, 52, 34, 10, RGB565(230, 180, 60));
    fill(422, 52, 34, 10, RGB565(230, 180, 60));
    ground_band(RGB565(70, 60, 70), RGB565(40, 36, 44), RGB565(200, 160, 120));
}

static void background(void) {
    switch (level) {
    case 1:  night_sky(); pyramid(); ground_band(COL_EARTH, COL_EARTH_JOINT, COL_PYR_EDGE); break;
    case 2:  day_sky(); pyramid(); ground_band(COL_EARTH, COL_EARTH_JOINT, COL_PYR_EDGE); break;
    case 3:  race_track(); break;
    case 4:  graveyard(); break;
    case 5:  mansion(); break;
    case 6:  forest(); break;
    case 7:  lava_bridge(); break;
    case 8:  football(); break;
    case 9:  desert(); break;
    default: boss_arena(); break;
    }
}

// Names of the levels, for the HUD and the banner.
static const char *level_name[BOSS_LEVEL + 1] = {
    "", "NIGHT SKY", "SUNNY SKY", "RACE TRACK", "GRAVEYARD", "MANSION", "FOREST",
    "LAVA BRIDGE", "FOOTBALL PITCH", "DESERT", "BIG BOSS",
};

// ---- Setup -----------------------------------------------------------------

static void add_enemy(kind_t kind, int x, int y, int sp) {
    if (n_enemies >= MAX_ENEMY) return;
    enemy_t *e = &enemies[n_enemies++];
    e->kind = kind;
    e->x = x;
    e->y = y;
    e->sp = sp;
    e->dir = -1;
    e->hp = kind == K_BOSS ? BOSS_HP : kind == K_SKELETON ? 3 : 2;
    e->vx = 0;
    e->cool = 30 + rnd(40);
    e->vis_t = rnd(100);
    e->phase = rnd(60);
    e->alive = 1;
}

static void clear_projs(void) {
    for (int i = 0; i < MAX_PROJ; i++) projs[i].on = 0;
}

typedef struct {
    int skeletons, ghosts, sk_speed, gh_speed, at_once;
} wave_t;

static const wave_t waves[BOSS_LEVEL - 1] = {
    {4, 2, 1, 1, 1},  // 1 night sky: one at a time
    {4, 3, 2, 1, 2},  // 2 sunny sky: two at a time
    {5, 4, 2, 2, 3},  // 3 race track
    {5, 5, 2, 2, 3},  // 4 graveyard
    {6, 5, 2, 2, 3},  // 5 mansion
    {6, 6, 2, 2, 3},  // 6 forest
    {7, 6, 3, 2, 4},  // 7 lava bridge
    {7, 7, 3, 3, 4},  // 8 football pitch
    {8, 8, 3, 3, 4},  // 9 desert
};

static kind_t spawn_queue[MAX_ENEMY];
static int spawn_n, spawn_next, spawn_timer, spawn_at_once;
static int spawn_sk_speed, spawn_gh_speed;

static void start_level(int n) {
    level = n;
    lv_skel = lv_ghost = 0;
    lv_streak = lv_streak_coins = 0;
    lv_lost_heart = 0;
    lv_shots = lv_hits = 0;
    swing_scored = 0;
    n_enemies = 0;
    spawn_next = 0;
    spawn_timer = 30;
    clear_projs();
    hero_hp = armour_lives[equipped[0]];
    hero_inv = 0;
    hx = 40;
    hy = GROUND_Y - HERO_DRAW;
    vy = 0;
    facing = 1;
    bolt_cool = 0;
    attack_pending = 0;   // a shot waiting from the last level does not carry over
    swing_t = 0;
    swing_cool = 0;
    swing_mask = 0;
    eagle_cool = 30;
    laser_t = 0;
    // One sidekick from level 2 on (the banana); the big boss gets all three.
    side_eagle = level == BOSS_LEVEL;
    side_banana = level >= 2;
    side_devil = level == BOSS_LEVEL;
    eagle_x = 4;
    eagle_y = GROUND_Y - FRIEND_DRAW; // starts at the bottom and flies up
    banana = (buddy_t){2, GROUND_Y - FRIEND_DRAW, 0, 0, 0, 40, armour_lives[equipped[1]]};
    devil = (buddy_t){62, GROUND_Y - FRIEND_DRAW, 0, 0, 0, 60, armour_lives[equipped[3]]};
    eagle_hp = armour_lives[equipped[2]];
    if (level == BOSS_LEVEL) {
        spawn_queue[0] = K_BOSS;
        spawn_n = 1;
        spawn_at_once = 1;
        spawn_sk_speed = spawn_gh_speed = 1;
    } else {
        const wave_t *w = &waves[level - 1];
        int sk = w->skeletons, gh = w->ghosts;
        spawn_n = 0;
        for (int i = 0; sk > 0 || gh > 0; i++) {   // skeletons and ghosts take turns
            if (sk > 0 && (i % 2 == 0 || gh == 0)) { spawn_queue[spawn_n++] = K_SKELETON; sk--; }
            else { spawn_queue[spawn_n++] = K_GHOST; gh--; }
        }
        spawn_at_once = w->at_once;
        spawn_sk_speed = w->sk_speed;
        spawn_gh_speed = w->gh_speed;
    }
    state = ST_PLAY;
}

// Brings the next enemy on from the floor, when there is room and the timer has run out.
static void update_spawns(void) {
    if (spawn_next >= spawn_n) return;
    if (spawn_timer > 0) { spawn_timer--; return; }
    int on_screen = 0;
    for (int i = 0; i < n_enemies; i++) on_screen += enemies[i].alive;
    if (on_screen >= spawn_at_once) return;
    kind_t k = spawn_queue[spawn_next++];
    if (k == K_SKELETON) add_enemy(K_SKELETON, GAME_W - 10, GROUND_Y - ENEMY_DRAW, spawn_sk_speed);
    else if (k == K_GHOST) add_enemy(K_GHOST, GAME_W - 40, GROUND_Y - GHOST_DRAW, spawn_gh_speed);
    else add_enemy(K_BOSS, BOSS_X, BOSS_Y, 1);
    spawn_timer = 40;
}

// ---- Wallet ----------------------------------------------------------------
// Coins and bought armour live in flash (wallet.c) and survive power-off and RST.
// Only flashing new firmware wipes them. Saving stalls the CPU for a few milliseconds, so
// during a fight it waits until the coins have been still for a moment; between fights
// (shops, level end, death, RST) it saves straight away.

#define WALLET_QUIET_TICKS 45          // 1.5 seconds without a change, while playing
_Static_assert(N_SHOPS == WALLET_ITEMS, "wallet holds one armour set per character");

static wallet_t saved_wallet;          // what is in flash now
static wallet_t seen_wallet;           // the last state we looked at
static int wallet_seen_tick;           // when it last changed

static void wallet_snapshot(wallet_t *w) {
    memset(w, 0, sizeof *w);
    w->coins = coins;
    for (int k = 0; k < N_SHOPS; k++) {
        w->owned[k] = owned_armour[k];
        w->equipped[k] = (uint8_t)equipped[k];
    }
}

// Takes a saved wallet, or the starting wallet if there is none or it looks wrong.
static void wallet_apply(const wallet_t *w, int valid) {
    coins = 0;
    for (int k = 0; k < N_SHOPS; k++) {
        owned_armour[k] = 1;   // the starter armour
        equipped[k] = 0;
    }
    if (!valid) return;
    coins = w->coins < 0 ? 0 : w->coins;
    for (int k = 0; k < N_SHOPS; k++) {
        uint16_t own = (uint16_t)((w->owned[k] & ((1u << N_ARMOUR) - 1)) | 1u);
        owned_armour[k] = own;
        if (w->equipped[k] < N_ARMOUR && (own & (1u << w->equipped[k]))) equipped[k] = w->equipped[k];
    }
}

static void wallet_commit(int force) {
    wallet_t cur;
    wallet_snapshot(&cur);
    if (memcmp(&cur, &seen_wallet, sizeof cur) != 0) {
        seen_wallet = cur;
        wallet_seen_tick = ticks;
    }
    if (memcmp(&cur, &saved_wallet, sizeof cur) == 0) return;
    if (!force && state == ST_PLAY && ticks - wallet_seen_tick < WALLET_QUIET_TICKS) return;
    printf("wallet: saving %d coins... ", (int)cur.coins);
    int ok = wallet_save(&cur);
    printf("%s\n", ok ? "ok" : "FAILED");
    saved_wallet = cur;   // on failure, try again only after the next change
}

void game_init(void) {
    confirm_reset = 0;
    wallet_commit(1);   // keep anything not saved yet
    wallet_t w;
    int have = wallet_load(&w);
    wallet_apply(&w, have);
    wallet_snapshot(&saved_wallet);
    seen_wallet = saved_wallet;
    printf("wallet: %s, %d coins\n", have ? "loaded" : "empty", coins);

    make_stars();
    state = ST_TITLE;
    level = 1;
    weapon = W_CROSSBOW;
    hero_hp = armour_lives[equipped[0]];
    hero_inv = 0;
    hx = 40;
    hy = GROUND_Y - HERO_DRAW;
    vy = 0;
    facing = 1;
    n_enemies = 0;
    clear_projs();
    banner_timer = 0;
    boss_timer = 0;
}

// ---- Update ----------------------------------------------------------------

static void hit_hero(void) {
    if (hero_inv > 0) return;
    int block = imin(50, armour_def[equipped[0]]);
    if (rnd(100) < block) return;   // the armour takes this one
    hero_hp--;
    lv_lost_heart = 1;
    lv_streak = 0;
    hero_inv = HURT_TICKS;
    if (hero_hp <= 0) {
        hero_hp = 0;
        state = ST_DEAD;
    }
}

static int alive_targets(void) {
    int n = 0;
    for (int i = 0; i < n_enemies; i++)
        if (enemies[i].alive && enemies[i].kind != K_BOSS) n++;
    return n;
}

static void enemy_size(const enemy_t *e, int *w, int *h);

// The box a shot or swing must touch: the sprite, with some extra reach for ghosts.
static void enemy_hit_box(const enemy_t *e, int *x, int *y, int *w, int *h) {
    int sw, sh;
    enemy_size(e, &sw, &sh);
    int pad = e->kind == K_GHOST ? GHOST_PAD : 0;
    *x = e->x - pad;
    *y = e->y - pad;
    *w = sw + 2 * pad;
    *h = sh + 2 * pad;
}

static void enemy_size(const enemy_t *e, int *w, int *h) {
    int s = e->kind == K_BOSS ? BOSS_DRAW : e->kind == K_GHOST ? GHOST_DRAW : ENEMY_DRAW;
    *w = s;
    *h = s;
}

// Hurts an enemy. Returns 1 if it took the hit (a hidden ghost cannot be hurt).
// Kills pay: skeleton 5, ghost 10, the Mega Boss 200. Every 5th kill in a row without
// losing a heart pays a streak bonus.
static int damage_enemy(int i) {
    enemy_t *e = &enemies[i];
    if (!e->alive) return 0;
    if (e->kind == K_GHOST && e->vis_t >= GHOST_VIS) return 0; // a hidden ghost cannot be hurt
    e->hp--;
    if (e->hp > 0) return 1;
    e->alive = 0;
    if (e->kind == K_BOSS) {
        coins += COIN_BOSS;
        state = ST_BOSS_DOWN;
        boss_timer = 0;
        clear_projs();
    } else {
        if (e->kind == K_GHOST) { coins += COIN_GHOST; lv_ghost++; }
        else { coins += COIN_SKELETON; lv_skel++; }
        if (++lv_streak % STREAK_EVERY == 0) {
            coins += STREAK_BONUS;
            lv_streak_coins += STREAK_BONUS;
        }
    }
    return 1;
}

// The nearest visible enemy to the hero, or -1.
static int find_target(void) {
    int best = -1, bestd = 1 << 30;
    for (int i = 0; i < n_enemies; i++) {
        enemy_t *e = &enemies[i];
        if (!e->alive) continue;
        if (e->kind == K_GHOST && e->vis_t >= GHOST_VIS) continue;
        int d = iabs(e->x - hx);
        if (d < bestd) { bestd = d; best = i; }
    }
    return best;
}

// Starts a flying shot from (sx, sy) toward (tx, ty) at `speed` pixels per tick.
static int shoot(proj_kind_t kind, int sx, int sy, int tx, int ty, int speed) {
    for (int i = 0; i < MAX_PROJ; i++) {
        if (projs[i].on) continue;
        int dx = tx - sx, dy = ty - sy;
        int len = imax(iabs(dx), iabs(dy)) + imin(iabs(dx), iabs(dy)) / 2;
        if (len == 0) len = 1;
        proj_t *p = &projs[i];
        p->on = 1;
        p->kind = kind;
        p->x = sx * 16;
        p->y = sy * 16;
        p->vx = dx * speed * 16 / len;
        p->vy = dy * speed * 16 / len;
        return 1;
    }
    return 0;
}

// A sidekick is hit: its armour may block it, otherwise it loses a hit point. At zero it is
// out for the rest of the level.
static void damage_side(int who) {
    int k = who == 0 ? 2 : who == 1 ? 1 : 3;   // shop index for the eagle, banana, devil
    if (rnd(100) < imin(50, armour_def[equipped[k]])) return;
    if (who == 0) {
        eagle_hp--;
        if (eagle_hp <= 0) side_eagle = 0;
    } else {
        buddy_t *b = who == 1 ? &banana : &devil;
        b->hp--;
        if (b->hp <= 0) {
            if (who == 1) side_banana = 0;
            else side_devil = 0;
        }
    }
}

// The sidekick nearest to a skeleton or ghost, if it is in reach and closer than the hero.
static int side_in_reach(int x, int y, int w, int h, int hero_dist) {
    int best = -1, bestd = hero_dist;
    if (side_eagle && eagle_hp > 0 && overlap(x, y, w, h, eagle_x, eagle_y, FRIEND_DRAW, FRIEND_DRAW))
        best = 0, bestd = 0;
    if (best < 0 && side_banana && banana.hp > 0) {
        int d = iabs(banana.x + FRIEND_DRAW / 2 - (x + w / 2));
        if (d < 44 && d < bestd) { bestd = d; best = 1; }
    }
    if (best < 0 && side_devil && devil.hp > 0) {
        int d = iabs(devil.x + FRIEND_DRAW / 2 - (x + w / 2));
        if (d < 44 && d < bestd) { bestd = d; best = 2; }
    }
    return best;
}

static void update_enemies(void) {
    int hcx = hx + HERO_DRAW / 2, hcy = hy + HERO_DRAW / 2;
    for (int i = 0; i < n_enemies; i++) {
        enemy_t *e = &enemies[i];
        if (!e->alive) continue;
        int w, h;
        enemy_size(e, &w, &h);
        if (e->cool > 0) e->cool--;

        if (e->kind == K_SKELETON) {
            int cx = e->x + w / 2;
            int dx = hcx - cx;
            e->vx = 0;
            if (iabs(dx) > 44) {
                e->x += sgn(dx) * e->sp;
                e->vx = sgn(dx) * e->sp;
                e->dir = sgn(dx);
            } else if (e->cool == 0) {
                int who = side_in_reach(e->x, e->y, w, h, iabs(dx));
                if (who > 0) damage_side(who);   // the banana or the devil is closer
                else hit_hero();                 // the big sword swings
                e->cool = 50;
            }
            e->x = imax(0, imin(GAME_W - w, e->x));
        } else if (e->kind == K_GHOST) {
            e->vis_t = (e->vis_t + 1) % 100;
            int dx = hcx - (e->x + w / 2);
            e->vx = 0;
            if (iabs(dx) > 20) { e->x += sgn(dx) * e->sp; e->vx = sgn(dx) * e->sp; }
            // Float up and down.
            int t = (ticks + e->phase) % 60;
            int fy = 60 + (t < 30 ? t : 60 - t) * 2;   // the height it floats at
            if (e->y < fy) e->y = imin(fy, e->y + 3);  // rises from the bottom first
            else if (e->y > fy) e->y = imax(fy, e->y - 3);
            int visible = e->vis_t < GHOST_VIS;
            if (visible && e->cool == 0 && overlap(hx, hy, HERO_DRAW, HERO_DRAW, e->x, e->y, w, h)) {
                hit_hero();          // a ghost's touch
                e->cool = 60;
            } else if (visible && e->cool == 0 && side_eagle && eagle_hp > 0 &&
                       overlap(eagle_x, eagle_y, FRIEND_DRAW, FRIEND_DRAW, e->x, e->y, w, h)) {
                damage_side(0);      // the ghost gets the eagle
                e->cool = 60;
            }
        } else {                     // the Mega Boss
            if (ticks % 4 == 0) {
                e->x += e->dir;
                if (e->x < 250) { e->x = 250; e->dir = 1; }
                if (e->x > BOSS_X) { e->x = BOSS_X; e->dir = -1; }
            }
            if (e->cool == 0) {
                // A snake poison dart from the mouth, aimed at the hero.
                shoot(PJ_DART, e->x + 4, e->y + 45, hcx, hcy, 4);
                e->cool = 56;
            }
        }
    }
}

static void update_projs(void) {
    for (int i = 0; i < MAX_PROJ; i++) {
        proj_t *p = &projs[i];
        if (!p->on) continue;
        p->x += p->vx;
        p->y += p->vy;
        int px = p->x >> 4, py = p->y >> 4;
        if (px < -20 || px > GAME_W + 20 || py < -20 || py > GAME_H) {
            p->on = 0;
            continue;
        }
        if (p->kind == PJ_DART) {
            // The dart hits the hero if it reaches his body.
            if (px >= hx + 8 && px < hx + HERO_DRAW - 8 && py >= hy + 8 && py < hy + HERO_DRAW) {
                p->on = 0;
                hit_hero();
            }
            continue;
        }
        // Friends' shots and the bolts hit the first enemy they reach.
        for (int k = 0; k < n_enemies; k++) {
            enemy_t *e = &enemies[k];
            if (!e->alive) continue;
            int w, h;
            int hx0, hy0;
            enemy_hit_box(e, &hx0, &hy0, &w, &h);
            if (px >= hx0 && px < hx0 + w && py >= hy0 && py < hy0 + h) {
                p->on = 0;
                if (damage_enemy(k) && p->kind == PJ_BOLT) lv_hits++;   // only Jimmy Bob's bolts count
                break;
            }
        }
        if (state != ST_PLAY) return;
    }
}

// Jimmy Bob's jumps and falls. He lands on the highest surface he reaches.
// Gravity and landing for anything that stands on the surfaces. `y` is its top, `size`
// its height. It lands on the highest surface its feet reach while falling.
static void land_body(int *y, int *vy, int x, int size) {
    int feet_before = *y + size;
    *vy = imin(*vy + GRAVITY, MAX_FALL);
    *y += *vy;
    int feet = *y + size;
    if (*vy < 0) return;
    int cx = x + size / 2;
    int land = -1, land_top = 1 << 30;
    for (unsigned s = 0; s < N_SURFACES; s++) {
        const surface_t *sf = &surfaces[s];
        if (cx < sf->x0 || cx >= sf->x1) continue;
        if (feet_before <= sf->top && feet >= sf->top && sf->top < land_top) {
            land = (int)s;
            land_top = sf->top;
        }
    }
    if (land >= 0) {
        *y = land_top - size;
        *vy = 0;
    }
}

// Nothing jumps above the top of the play area.
static void clamp_top(int *y, int *vy) {
    if (*y < HUD_BAND) {
        *y = HUD_BAND;
        *vy = 0;
    }
}

// The samurai sword hits every enemy in the swing's box in front of Jimmy Bob, once per swing.
static void sword_hits(void) {
    int x0 = facing > 0 ? hx + HERO_DRAW - 6 : hx - 40;
    for (int i = 0; i < n_enemies; i++) {
        enemy_t *e = &enemies[i];
        if (!e->alive || (swing_mask & (1u << i))) continue;
        int w, h;
        int bx, by;
        enemy_hit_box(e, &bx, &by, &w, &h);
        if (overlap(x0, hy + 8, 46, 48, bx, by, w, h)) {
            swing_mask |= 1u << i;
            if (damage_enemy(i) && !swing_scored) {   // a swing counts once, however many it hits
                swing_scored = 1;
                lv_hits++;
            }
            if (state != ST_PLAY) return;
        }
    }
}

static void update_hero_body(void) {
    land_body(&hy, &vy, hx, HERO_DRAW);
    clamp_top(&hy, &vy);
}

// A banana or devil: pick the surface nearest the enemies, walk there, jump up to it.
static void update_buddy(buddy_t *b, int ex) {
    if (b->shot > 0) b->shot--;
    if (b->cool > 0) b->cool--;
    if (b->cool == 0) {
        b->cool = RETARGET_TICKS;
        int best = 0, bestd = 1 << 30;
        for (unsigned s = 0; s < N_SURFACES; s++) {
            int cx = (surfaces[s].x0 + surfaces[s].x1) / 2;
            // The ground counts as 60 px further away, so they climb onto platforms when they can.
            int d = iabs(cx - ex) + (s == 0 ? 60 : 0);
            if (d < bestd || (d == bestd && surfaces[s].top < surfaces[best].top)) {
                bestd = d;
                best = (int)s;
            }
        }
        b->target = best;
    }
    const surface_t *t = &surfaces[b->target];
    int dx = (t->x0 + t->x1) / 2 - (b->x + FRIEND_DRAW / 2);
    if (iabs(dx) > 3) b->x += sgn(dx) * BUDDY_SPEED;
    b->x = imax(0, imin(GAME_W - FRIEND_DRAW, b->x));
    if (b->vy == 0 && t->top < b->y + FRIEND_DRAW - 8 && iabs(dx) < 40) b->vy = JUMP_V;
    land_body(&b->y, &b->vy, b->x, FRIEND_DRAW);
    clamp_top(&b->y, &b->vy);
}

// The eagle flies to a spot above the nearest enemy.
static void update_eagle(int ex) {
    if (eagle_y > EAGLE_Y) eagle_y = imax(EAGLE_Y, eagle_y - 2); // rises from the bottom
    int tx = imax(0, imin(GAME_W - FRIEND_DRAW, ex - FRIEND_DRAW / 2));
    if (eagle_x < tx) eagle_x = imin(tx, eagle_x + 3);
    else if (eagle_x > tx) eagle_x = imax(tx, eagle_x - 3);
}

// The friends attack the nearest enemy: the eagle with a laser, the banana with peels,
// the devil with poo.
static void update_friends(void) {
    if (eagle_cool > 0) eagle_cool--;
    if (laser_t > 0) laser_t--;
    int target = find_target();
    int ex = hx;
    if (target >= 0) {
        enemy_t *e = &enemies[target];
        int w, h;
        enemy_size(e, &w, &h);
        ex = e->x + w / 2;
    }
    if (side_eagle) update_eagle(ex);
    if (side_banana) update_buddy(&banana, ex);
    if (side_devil) update_buddy(&devil, ex);
    if (target < 0) return;

    enemy_t *e = &enemies[target];
    int w, h;
    enemy_size(e, &w, &h);
    int tx = e->x + w / 2, ty = e->y + h / 2;
    if (side_eagle && eagle_cool == 0) {
        laser_t = 6;             // the laser hits at once
        laser_x0 = eagle_x + FRIEND_DRAW / 2;
        laser_y0 = eagle_y + FRIEND_DRAW / 2;
        laser_x1 = tx;
        laser_y1 = ty;
        damage_enemy(target);
        eagle_cool = 40;
    }
    if (side_banana && banana.shot == 0) {
        shoot(PJ_PEEL, banana.x + FRIEND_DRAW / 2, banana.y + FRIEND_DRAW / 2, tx, ty, 5);
        banana.shot = 50;
    }
    if (side_devil && devil.shot == 0) {
        shoot(PJ_POO, devil.x + FRIEND_DRAW / 2, devil.y + FRIEND_DRAW / 2, tx, ty, 3);
        devil.shot = 70;
    }
}

// Every skeleton and ghost is down: pay the level bonuses and show the tally.
static void level_clear(void) {
    sum_level = LEVEL_BONUS_PER_LEVEL * level;
    sum_perfect = lv_lost_heart ? 0 : PERFECT_BONUS;
    sum_acc_pct = -1;
    sum_acc_bonus = 0;
    if (lv_shots >= ACC_MIN_SHOTS) {
        sum_acc_pct = lv_hits * 100 / lv_shots;
        sum_acc_bonus = sum_acc_pct >= 90 ? 30 : sum_acc_pct >= 75 ? 20 : sum_acc_pct >= 50 ? 10 : 0;
    }
    sum_total = lv_skel * COIN_SKELETON + lv_ghost * COIN_GHOST + lv_streak_coins
              + sum_level + sum_perfect + sum_acc_bonus;
    coins += sum_level + sum_perfect + sum_acc_bonus;   // the kills and streaks are already in
    banner_timer = 0;
    state = ST_BANNER;
}

static void update_play(uint8_t held, uint8_t pressed) {
    // Walk.
    if (held & JOY_LEFT) { hx -= HERO_SPEED; facing = -1; }
    if (held & JOY_RIGHT) { hx += HERO_SPEED; facing = 1; }
    hx = imax(0, imin(GAME_W - HERO_DRAW, hx));

    // Jump, only from a surface.
    if ((pressed & JOY_UP) && vy == 0) vy = JUMP_V;

    // SET switches between the crossbow and the samurai sword.
    if (pressed & JOY_SET) weapon = weapon == W_CROSSBOW ? W_SWORD : W_CROSSBOW;

    // MID attacks with the weapon in hand. The crossbow fires at the nearest enemy (leading
    // it); the sword swings in front of Jimmy Bob. A press during a cooldown waits.
    if (bolt_cool > 0) bolt_cool--;
    if (swing_cool > 0) swing_cool--;
    if (swing_t > 0) {
        swing_t--;
        sword_hits();
    }
    if (pressed & JOY_MID) attack_pending = 1;
    if (attack_pending && weapon == W_SWORD && swing_cool == 0) {
        swing_t = 12;
        swing_cool = 16;
        swing_mask = 0;
        swing_scored = 0;
        lv_shots++;
        attack_pending = 0;
        sword_hits();
    }
    if (attack_pending && weapon == W_CROSSBOW) {
        int sx = hx + HERO_DRAW - 8, sy = hy + 30;
        int tx = sx + facing * 400, ty = sy;
        int target = find_target();
        if (target >= 0) {
            enemy_t *e = &enemies[target];
            int w, h;
            enemy_size(e, &w, &h);
            // Lead the target: it keeps walking while the shot flies.
            int flight = imax(iabs(e->x + w / 2 - sx), iabs(e->y + h / 2 - sy)) / BOLT_SPEED;
            tx = e->x + w / 2 + e->vx * flight;
            ty = e->y + h / 2;
        }
        if (bolt_cool == 0 && shoot(PJ_BOLT, sx, sy, tx, ty, BOLT_SPEED)) {
            bolt_cool = 8;
            lv_shots++;
            attack_pending = 0;
        }
    }
    if (state != ST_PLAY) return;

    if (hero_inv > 0) hero_inv--;
    update_hero_body();

    update_enemies();
    if (state != ST_PLAY) return;
    update_spawns();

    update_friends();
    if (state != ST_PLAY) return;

    update_projs();
    if (state != ST_PLAY) return;

    // Level clear: every skeleton and ghost is down.
    if (level < BOSS_LEVEL && spawn_next >= spawn_n && alive_targets() == 0) {
        level_clear();
    }
}

// After the boss falls, Jimmy Bob walks over and stands on him.
static void update_boss_down(void) {
    int target_x = BOSS_X + BOSS_DRAW / 2 - HERO_DRAW / 2;
    int target_y = GROUND_Y - BOSS_FALLEN_H - HERO_DRAW;  // stand on the fallen boss
    if (hx < target_x) hx += HERO_SPEED;
    if (hx > target_x) hx -= HERO_SPEED;
    if (hy < target_y) hy += HERO_SPEED;
    if (hy > target_y) hy -= HERO_SPEED;
    if (iabs(hx - target_x) <= HERO_SPEED && iabs(hy - target_y) <= HERO_SPEED) {
        boss_timer++;
        if (boss_timer >= 60) state = ST_WIN;
    }
}

// ---- Shop ------------------------------------------------------------------
// One screen (shop_art.c, from shop.png): a row for each character, five armour tiers across.
// Only the arrows and MID work here. The cursor can sit on any armour or on the back arrow in
// the top-left corner; MID on an armour buys or wears it, MID on the back arrow leaves.

static void open_shop(int next, int from_title) {
    shop_next = next;
    shop_from_title = from_title;
    shop_row = 0;
    shop_col = equipped[0];
    shop_on_back = 0;
    state = ST_SHOP;
}

// Back arrow: to the title if the shop was opened from there, otherwise on to the next level.
static void close_shop(void) {
    if (shop_from_title) state = ST_TITLE;
    else start_level(shop_next);
}

static void shop_move(uint8_t pressed) {
    if (shop_on_back) {
        if (pressed & JOY_DOWN) shop_on_back = 0;
        return;
    }
    if ((pressed & JOY_LEFT) && shop_col > 0) shop_col--;
    if ((pressed & JOY_RIGHT) && shop_col < N_ARMOUR - 1) shop_col++;
    if (pressed & JOY_DOWN) { if (shop_row < N_SHOPS - 1) shop_row++; }
    if (pressed & JOY_UP) { if (shop_row > 0) shop_row--; else shop_on_back = 1; }
}

// Buys the armour under the cursor, or wears it if it is already bought. Never charges twice.
static void shop_buy(void) {
    int i = shop_col, k = shop_row;
    if (owned_armour[k] & (1u << i)) {
        equipped[k] = i;
    } else if (coins >= armour_price[i]) {
        coins -= armour_price[i];
        owned_armour[k] |= (uint16_t)(1u << i);
        equipped[k] = i;
    }   // otherwise the button already says NOT ENOUGH COINS: nothing happens
}

void game_update(uint8_t held, uint8_t pressed) {
    ticks++;
    if (confirm_reset) {
        // The game is paused. RST again resets as usual; SET goes back to the game.
        if (pressed & JOY_RESET) game_init();
        else if (pressed & JOY_SET) confirm_reset = 0;
        return;
    }
    if ((pressed & JOY_RESET) && state != ST_SHOP) {   // RST and SET do nothing in the shop
        confirm_reset = 1;
        return;
    }
    switch (state) {
    case ST_TITLE:
        if (pressed & JOY_SET) start_level(1);
        if (pressed & JOY_MID) open_shop(1, 1);
        break;
    case ST_PLAY:
        update_play(held, pressed);
        break;
    case ST_BANNER:
        banner_timer++;   // SET carries on to the shop, after a moment so nobody skips it by accident
        if (banner_timer > BANNER_MIN_TICKS && (pressed & JOY_SET)) open_shop(level + 1, 0);
        break;
    case ST_DEAD:
        if (pressed & JOY_SET) start_level(level); // try again, coins kept
        break;
    case ST_BOSS_DOWN:
        update_boss_down();
        break;
    case ST_WIN:
        if (pressed & JOY_SET) game_init();
        break;
    case ST_SHOP:
        shop_move(pressed);
        if (pressed & JOY_MID) {
            if (shop_on_back) close_shop();
            else shop_buy();
        }
        break;
    }
    wallet_commit(0);
}

// ---- Draw ------------------------------------------------------------------

static void draw_title(void) {
    fill(0, 0, GAME_W, GAME_H, COL_BLACK);
    spr(nj_title_logo, NJ_TITLE_LOGO_W, NJ_TITLE_LOGO_H, 0, 0, 480, NJ_TITLE_LOGO_H, 0);
    if ((ticks / 15) & 1) text_centred(240, "SET TO START", COL_GOLD, 3);
    text_centred(264, "MID ARMOUR SHOP", COL_GOLD, 2);
    text_centred(282, "LEFT RIGHT WALK  UP JUMP", COL_TEXT, 2);
    text_centred(302, "MID ATTACK  SET WEAPON", COL_TEXT, 2);
}

static void draw_win(void) {
    char buf[40];
    spr(nj_story_picture, NJ_STORY_PICTURE_W, NJ_STORY_PICTURE_H, 0, 0, 480, 320, 0);
    fill(60, 196, 360, 100, COL_BLACK);
    text_centred(206, "NINJA INVASION COMPLETE", COL_GOLD, 2);
    snprintf(buf, sizeof buf, "COINS %d", coins);
    text_centred(236, buf, COL_TEXT, 3);
    text_centred(272, "SET: PLAY AGAIN", COL_DIM, 2);
}

static void draw_hud(void) {
    char buf[32];
    fill(0, 0, GAME_W, HUD_BAND, COL_BAND);
    int max_hp = armour_lives[equipped[0]];
    if (max_hp <= 6) {   // more hearts would run into the title: show a number instead
        for (int i = 0; i < max_hp; i++) {
            if (i < hero_hp) spr(nj_heart, NJ_HEART_W, NJ_HEART_H, 8 + i * 26, 10, 22, 22, 0);
            else fill(8 + i * 26, 10, 22, 22, COL_HEAD_OFF);
        }
    } else {
        snprintf(buf, sizeof buf, "HP %d", hero_hp);
        text(8, 12, buf, COL_TEXT, 2);
    }
    spr(nj_coin, NJ_COIN_W, NJ_COIN_H, 436, 9, 24, 24, 0);
    snprintf(buf, sizeof buf, "%d", coins);
    text_right(428, 14, buf, COL_GOLD, 3);

    if (level < BOSS_LEVEL) snprintf(buf, sizeof buf, "LEVEL %d", level);
    else snprintf(buf, sizeof buf, "BIG BOSS");
    text_centred(6, buf, COL_TEXT, 2);
    text_centred(26, weapon == W_CROSSBOW ? "CROSSBOW" : "SWORD", COL_DIM, 2);
}

// The 140 x 140 picture of a character wearing a tier. Starter is the original art; the other
// tiers come from the shop picture.
static const uint16_t *armour_sprite(int kind, int tier) {
    if (tier > 0) return shop_sprite[kind][tier];
    switch (kind) {
    case 0: return nj_jimmy_bob;
    case 1: return nj_yellow_banana;
    case 2: return nj_golden_eagle;
    default: return nj_tasmanian_devil;
    }
}

// The eagle, the banana and the devil, standing by Jimmy Bob's side.
static void draw_friends(void) {
    if (side_eagle)
        spr(armour_sprite(2, equipped[2]), 140, 140, eagle_x, eagle_y, FRIEND_DRAW, FRIEND_DRAW, 0);
    if (side_banana)
        spr(armour_sprite(1, equipped[1]), 140, 140, banana.x, banana.y, FRIEND_DRAW, FRIEND_DRAW, 0);
    if (side_devil)
        spr(armour_sprite(3, equipped[3]), 140, 140, devil.x, devil.y, FRIEND_DRAW, FRIEND_DRAW, 0);
}

// A health bar above an enemy's head: red when low, green when healthy.
static void draw_health(int x, int y, int w, int hp, int max) {
    int bw = w * 3 / 5, bx = x + (w - bw) / 2, by = y - 10;
    fill(bx - 1, by - 1, bw + 2, 7, COL_BLACK);
    int fw = bw * hp / max;
    fill(bx, by, fw, 5, hp * 2 > max ? COL_HP_GOOD : COL_RED);
}

static void draw_enemies(void) {
    for (int i = 0; i < n_enemies; i++) {
        enemy_t *e = &enemies[i];
        if (!e->alive) continue;
        int w, h;
        enemy_size(e, &w, &h);
        if (e->kind == K_SKELETON) {
            spr(nj_skeleton, NJ_SKELETON_W, NJ_SKELETON_H, e->x, e->y, ENEMY_DRAW, ENEMY_DRAW, e->dir > 0);
        } else if (e->kind == K_GHOST) {
            if (e->vis_t >= GHOST_VIS) continue; // hidden
            spr(nj_ghost, NJ_GHOST_W, NJ_GHOST_H, e->x, e->y, GHOST_DRAW, GHOST_DRAW, 0);
        } else {
            spr(nj_mega_boss, NJ_MEGA_BOSS_W, NJ_MEGA_BOSS_H, e->x, e->y, BOSS_DRAW, BOSS_DRAW, 0);
        }
        draw_health(e->x, e->y, w, e->hp, e->kind == K_BOSS ? BOSS_HP : 2);
    }
}

static void draw_projs(void) {
    for (int i = 0; i < MAX_PROJ; i++) {
        proj_t *p = &projs[i];
        if (!p->on) continue;
        int px = p->x >> 4, py = p->y >> 4;
        switch (p->kind) {
        case PJ_BOLT:
            spr(nj_fire_bolt, NJ_FIRE_BOLT_W, NJ_FIRE_BOLT_H, px - 20, py - 20, 40, 40, p->vx < 0);
            break;
        case PJ_PEEL:
            fill(px - 5, py - 3, 10, 3, COL_PEEL);
            fill(px - 3, py, 6, 3, COL_PEEL);
            break;
        case PJ_POO:
            disc(px, py, 5, COL_POO);
            break;
        case PJ_DART:
            line(px - 6, py - 2, px + 2, py, COL_DART);
            break;
        }
    }
}

static void draw_laser(void) {
    if (laser_t > 0) line(laser_x0, laser_y0, laser_x1, laser_y1, COL_LASER);
}

static void draw_hero(void) {
    if (hero_inv && ((hero_inv >> 2) & 1)) return; // blink after a hit
    // Jimmy Bob with the weapon in hand; the other one is on his back in the same picture.
    if (equipped[0] == 0) {
        if (weapon == W_CROSSBOW)
            spr(nj_jimmy_crossbow, NJ_JIMMY_CROSSBOW_W, NJ_JIMMY_CROSSBOW_H, hx, hy, HERO_DRAW, HERO_DRAW, facing < 0);
        else
            spr(nj_jimmy_sword, NJ_JIMMY_SWORD_W, NJ_JIMMY_SWORD_H, hx, hy, HERO_DRAW, HERO_DRAW, facing < 0);
    } else {
        // Armoured: the armour picture has no weapon, so the weapon is drawn in his front hand.
        spr(armour_sprite(0, equipped[0]), 140, 140, hx, hy, HERO_DRAW, HERO_DRAW, facing < 0);
        const uint16_t *w = weapon == W_CROSSBOW ? nj_crossbow : nj_samurai_sword;
        spr(w, 48, 48, facing > 0 ? hx + HERO_DRAW - 30 : hx - 6, hy + 20, 36, 36, facing < 0);
    }
    // The crescent slash, in front of him while the sword swings.
    if (weapon == W_SWORD && swing_t > 0)
        spr(nj_slash, NJ_SLASH_W, NJ_SLASH_H, facing > 0 ? hx + HERO_DRAW - 6 : hx - 50, hy - 2, 56, 56, facing < 0);
}

// Writes 1,234 style numbers.
static void format_coins(char *out, size_t n, int v) {
    char d[16];
    int len = snprintf(d, sizeof d, "%d", v < 0 ? 0 : v), o = 0;
    for (int i = 0; i < len && (size_t)o + 2 < n; i++) {
        if (i > 0 && (len - i) % 3 == 0) out[o++] = ',';
        out[o++] = d[i];
    }
    out[o] = 0;
}

// Text at the biggest of scale 2 or 1 that fits in `w` pixels.
static void text_fit(int x, int y, int w, const char *s, uint16_t c) {
    text(x, y + (text_width(s, 2) <= w ? 0 : 4), s, c, text_width(s, 2) <= w ? 2 : 1);
}

// The Armour Shop. The picture (shop_art.c) has the grid; the changing parts are drawn on top.
static void draw_shop(void) {
    // Where the cells are on the 480 x 320 picture: five tier columns and four character rows.
    static const int col_cx[N_ARMOUR] = {164, 232, 300, 369, 444};
    static const int row_y[N_SHOPS] = {31, 87, 142, 197};
    static const int row_h[N_SHOPS] = {51, 50, 49, 50};
    const uint16_t field = RGB565(6, 18, 40), edge = RGB565(70, 180, 230);
    char buf[24];
    int k = shop_row, t = shop_col;
    int owned = (owned_armour[k] >> t) & 1, worn = equipped[k] == t;
    int can_buy = coins >= armour_price[t];

    spr(shop_bg, SHOP_BG_W, SHOP_BG_H, 0, 0, 480, 320, 0);

    // The back arrow, top left, and the wallet.
    if (shop_on_back) frame(4, 0, 41, 26, 2, COL_GOLD);
    format_coins(buf, sizeof buf, coins);
    text_fit(397, 6, 72, buf, COL_GOLD);

    // A green tick on the armour each character wears; a gold frame on the cursor.
    for (int r = 0; r < N_SHOPS; r++) {
        int x = col_cx[equipped[r]] - 31;
        disc(x + 54, row_y[r] + 9, 7, COL_HP_GOOD);
        line(x + 51, row_y[r] + 9, x + 53, row_y[r] + 12, COL_TEXT);
        line(x + 53, row_y[r] + 12, x + 58, row_y[r] + 6, COL_TEXT);
    }
    if (!shop_on_back) frame(col_cx[t] - 31, row_y[k], 62, row_h[k], 3, COL_GOLD);

    // Bottom bar: the picture of the selected armour, then its name, DEF, LIVES and cost.
    spr(armour_sprite(k, t), 140, 140, 16, 258, 52, 52, 0);

    fill(77, 275, 96, 25, field);
    frame(77, 275, 96, 25, 1, edge);
    text(81, 278, character_name[k], COL_TEXT, 1);
    text(81, 289, armour_name[t], COL_GOLD, 1);

    snprintf(buf, sizeof buf, "%d", armour_def[t]);
    text(231, 267, buf, COL_TEXT, 2);
    snprintf(buf, sizeof buf, "%d", armour_lives[t]);
    text(236, 287, buf, COL_TEXT, 2);

    if (worn) text(315, 285, "EQUIPPED", COL_HP_GOOD, 1);
    else if (owned) text(315, 281, "OWNED", COL_HP_GOOD, 2);
    else {
        format_coins(buf, sizeof buf, armour_price[t]);
        text_fit(315, 281, 58, buf, COL_GOLD);
    }

    // The button shows what MID will do.
    const char *label;
    if (shop_on_back) label = "BACK";
    else if (worn) label = "EQUIPPED";
    else if (owned) label = "EQUIP";
    else label = "BUY";
    if (!shop_on_back && !owned && !can_buy) {
        fill(385, 268, 83, 30, RGB565(170, 30, 30));
        frame(385, 268, 83, 30, 2, COL_TEXT);
        text(399, 274, "NOT ENOUGH", COL_TEXT, 1);
        text(417, 285, "COINS", COL_TEXT, 1);
    } else if (worn) {
        text_adv(383 + 1, 276, label, COL_TEXT, 2, 11);
    } else {
        int sc = text_width(label, 3) <= 80 ? 3 : 2;
        text(427 - text_width(label, sc) / 2, 283 - 7 * sc / 2, label, COL_TEXT, sc);
    }
}

// One line of the level-clear tally: a label on the left and the coins on the right.
static void tally_row(int y, const char *label, int coins_won, uint16_t colour) {
    char buf[16];
    text(36, y, label, colour, 2);
    snprintf(buf, sizeof buf, "+%d", coins_won);
    text_right(444, y, buf, colour, 2);
}

// The level-clear screen: what Jimmy Bob earned this level, and why.
static void draw_level_clear(void) {
    char buf[40];
    fill(20, 40, 440, 272, COL_BLACK);
    frame(20, 40, 440, 272, 3, COL_GOLD);
    snprintf(buf, sizeof buf, "LEVEL %d CLEAR", level);
    text_centred(48, buf, COL_GOLD, 3);

    snprintf(buf, sizeof buf, "SKELETONS %d X %d", lv_skel, COIN_SKELETON);
    tally_row(82, buf, lv_skel * COIN_SKELETON, COL_TEXT);
    snprintf(buf, sizeof buf, "GHOSTS %d X %d", lv_ghost, COIN_GHOST);
    tally_row(104, buf, lv_ghost * COIN_GHOST, COL_TEXT);
    tally_row(126, "STREAK BONUS", lv_streak_coins, lv_streak_coins ? COL_TEXT : COL_DIM);
    snprintf(buf, sizeof buf, "LEVEL %d X %d", level, LEVEL_BONUS_PER_LEVEL);
    tally_row(148, buf, sum_level, COL_TEXT);
    tally_row(170, "NO HEARTS LOST", sum_perfect, sum_perfect ? COL_TEXT : COL_DIM);
    if (sum_acc_pct < 0) {
        tally_row(192, "ACCURACY: NEED 5 SHOTS", 0, COL_DIM);
    } else {
        const char *name = sum_acc_bonus == 30 ? " SHARPSHOOTER" : sum_acc_bonus == 20 ? " GREAT AIM"
                         : sum_acc_bonus == 10 ? " GOOD AIM" : "";
        snprintf(buf, sizeof buf, "ACCURACY %d%%%s", sum_acc_pct, name);
        tally_row(192, buf, sum_acc_bonus, sum_acc_bonus ? COL_TEXT : COL_DIM);
    }

    fill(36, 216, 408, 2, COL_DIM);
    tally_row(224, "LEVEL TOTAL", sum_total, COL_GOLD);
    snprintf(buf, sizeof buf, "%d", coins);
    text(36, 246, "WALLET", COL_TEXT, 2);
    text_right(444, 246, buf, COL_TEXT, 2);

    if (banner_timer > BANNER_MIN_TICKS && ((ticks / 15) & 1)) text_centred(270, "SET: CONTINUE", COL_GOLD, 2);
    snprintf(buf, sizeof buf, "NEXT: %s", level + 1 < BOSS_LEVEL ? level_name[level + 1] : "THE BIG BOSS");
    text_centred(290, buf, COL_DIM, 2);
}

static void draw_scene(void) {
    if (state == ST_TITLE) {
        draw_title();
        return;
    }
    if (state == ST_WIN) {
        draw_win();
        return;
    }
    if (state == ST_SHOP) {
        draw_shop();
        return;
    }

    background();
    for (unsigned s = 1; s < N_SURFACES; s++)  // the platforms, tiled 44 px wide
        for (int x = surfaces[s].x0; x < surfaces[s].x1; x += 44)
            spr(nj_platform, NJ_PLATFORM_W, NJ_PLATFORM_H, x, surfaces[s].top, 44, 13, 0);

    draw_friends();
    if (state == ST_BOSS_DOWN) {
        // The Mega Boss lies squashed on the ground.
        spr(nj_mega_boss, NJ_MEGA_BOSS_W, NJ_MEGA_BOSS_H, BOSS_X, GROUND_Y - BOSS_FALLEN_H, BOSS_DRAW, BOSS_FALLEN_H, 0);
    } else {
        draw_enemies();
    }
    draw_projs();
    draw_laser();
    draw_hero();
    draw_hud();

    if (state == ST_DEAD) {
        fill(60, 120, 360, 80, COL_BLACK);
        text_centred(134, "OUT OF HEARTS", COL_TEXT, 3);
        text_centred(170, "SET: TRY AGAIN", COL_GOLD, 2);
    }
    if (state == ST_BANNER) draw_level_clear();
}

// The "reset?" box, drawn over whatever screen the game is on.
static void draw_confirm(void) {
    fill(50, 100, 380, 120, COL_BLACK);
    frame(50, 100, 380, 120, 3, COL_GOLD);
    text_centred(114, "RESET THE GAME?", COL_TEXT, 3);
    text_centred(158, "RST: YES, RESET", COL_RED, 2);
    text_centred(186, "SET: BACK TO GAME", COL_GOLD, 2);
}

void game_draw(uint16_t *buf, int y0, int rows) {
    strip = buf;
    strip_y0 = y0;
    strip_rows = rows;
    draw_scene();
    if (confirm_reset) draw_confirm();
}
