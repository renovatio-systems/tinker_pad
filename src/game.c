// Reckalambo. Title screen, then Level 1: wreck the Urus with the hammer, sell it, dance.
// Design notes are in docs/reckalambo_level1.md. The title picture comes from title_img.c
// (made by tools/make_title_img.py). The sprites come from the reck_*.c files.

#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font5x7.h"
#include "reck_hammer.h"
#include "reck_jaguar.h"
#include "reck_sprites.h"
#include "reck_shop.h"
#include "title_img.h"

// ---- Tuning ----------------------------------------------------------------------------

#define BLINK_TICKS (GAME_TICK_HZ / 2)
#define END_WAIT    30  // ticks before SET is accepted on the end screen

#define GROUND_Y    280 // the bottom of the car, and the line the jaguar stands on
#define CAR_W       RECK_CAR_W
#define CAR_H       RECK_CAR_H
#define CAR_Y       (GROUND_Y - CAR_H) // the car sprite is bottom-aligned on the ground
#define CAR_MIN_X   12
#define CAR_MAX_X   (GAME_W - CAR_W - 12)
#define CAR_SPEED   2   // pixels per tick, about 60 a second
#define CAR_FOOT_H  20  // the bottom part of the car that the jaguar cannot walk into
#define CAR_HP_MAX  1500

#define MAN_SPEED   3
#define MAN_MIN_X   12
#define MAN_MAX_X   (GAME_W - 12)
#define MAN_MIN_Y   120 // feet, furthest back
#define MAN_MAX_Y   300 // feet, furthest forward
#define MAN_FOOT_W  12  // half the width of the jaguar's footprint
#define MAN_FOOT_H  6   // the footprint is this tall, above his feet
#define MAN_START_X 60
#define MAN_START_Y 260

#define GRIP_DX -7 // the closed fist in the jaguar sprite, relative to his feet, facing right
#define GRIP_DY -52

#define SWING_TICKS  12
#define IMPACT_TICK  6  // the hammer is at the car on this tick of the swing
#define HIT_PAD      6  // the hit zone is this much bigger than the car on each side
#define DEFLECT_FIRST 2 // the head can knock a gorilla away on any swing tick from here...
#define DEFLECT_LAST  11 // ...to here, as it sweeps past
#define DEFLECT_RADIUS 22 // how close the head must be to the gorilla's centre
#define SALE_BONUS   500 // the base price of the wreck
#define TIME_PAR_S   90  // a wreck faster than this gets a time bonus...
#define TIME_PER_S   10  // ...of this much WRECK$ for each second under it
#define BOUNCE_BONUS 150 // for each gorilla bounced back at the car, by the hammer or the shield
#define SHIELD_DMG   100 // what a gorilla stopped by the shield does to the car

#define SHIELD_TICKS    12 // a block lasts 0.4 s
#define SHIELD_RECHARGE 90 // then the shield takes 3 s to recharge
#define SPEED_WINDOW    90 // hits within 3 s of each other count as quick

#define GORILLA_MAX   4
#define GORILLA_FIRST 60 // ticks before the first gorilla
#define GORILLA_EVERY 60 // ticks between gorillas
#define GORILLA_LIFE  80
#define GORILLA_HALF  12 // half the size of the gorilla hit box
#define GORILLA_ANIM  4  // ticks per frame of the tumble
#define GORILLA_SPEED 3.5f
#define GORILLA_BACK  5.0f // speed of a gorilla knocked back at the car

#define JAG_HITS     3  // the third gorilla hit restarts the level
#define IMMUNE_TICKS 20 // no damage for this long after a gorilla hit
#define POPUP_TICKS  30

// ---- Colours ---------------------------------------------------------------------------

#define COL_TEXT    RGB565(255, 255, 255)
#define COL_GOLD    RGB565(255, 210, 40)
#define COL_RED     RGB565(255, 60, 40)
#define COL_GREEN   RGB565(60, 220, 90)
#define COL_CYAN    RGB565(90, 230, 255)
#define COL_SKY     RGB565(40, 16, 56)
#define COL_ROAD    RGB565(58, 58, 66)
#define COL_LINE    RGB565(200, 190, 60)
#define COL_BUILD   RGB565(22, 22, 52)
#define COL_BAR_BG  RGB565(40, 10, 10)
#define COL_SHIELD_BG RGB565(10, 30, 40)
#define COL_GOR     RGB565(120, 70, 30)

// ---- State -----------------------------------------------------------------------------

enum { S_TITLE, S_PLAY, S_DONE, S_LOST, S_SHOP };
enum { G_FLYING, G_KNOCKED }; // a gorilla on its way to the jaguar, or knocked back at the car

typedef struct {
    float x, y, vx, vy;
    int life;   // ticks left, 0 when the slot is free
    int state;  // G_FLYING or G_KNOCKED
    int damage; // knocked back: what it does to the car when it lands
} Gorilla;

static int state, ticks, state_t;
static int car_x, car_dir;   // the car moves left and right
static int man_x, man_y;     // feet
static int facing = 1;       // +1 faces right, -1 faces left (towards the car)
static int swing_t;          // 0 when not swinging, else 1..SWING_TICKS
static int car_hp;
static int wreck;            // WRECK$: the one currency, from hits, knocks and the sale
static int hits, sold_for;
static int bounces;         // gorillas bounced back at the car
static int wreck_secs;      // how long the wreck took, in seconds
static int sale_time, sale_bounce; // the parts of the sale bonus, for the end screen
static int immune;           // ticks of no damage left
static int shield_t, shield_cd; // the shield is up for shield_t ticks, then recharges for shield_cd
static int gorilla_t;        // ticks since the level started, for firing
static int last_hit_t;       // the tick of the last hit, for the speed bonus
static int popup_t, popup_val;
static uint32_t rng = 1;
static Gorilla gor[GORILLA_MAX];

// The strip being drawn right now. Everything is clipped to it.
static uint16_t *strip;
static int strip_y0, strip_rows;

static int rnd_range(int lo, int hi) {
    rng = rng * 1664525u + 1013904223u;
    return lo + (int)((rng >> 8) % (uint32_t)(hi - lo + 1));
}

// ---- Drawing helpers -------------------------------------------------------------------

static void fill(int x, int y, int w, int h, uint16_t c) {
    int x0 = x < 0 ? 0 : x, x1 = x + w > GAME_W ? GAME_W : x + w;
    int y0 = y < strip_y0 ? strip_y0 : y;
    int y1 = y + h > strip_y0 + strip_rows ? strip_y0 + strip_rows : y + h;
    if (x0 >= x1) return;
    for (int yy = y0; yy < y1; yy++) {
        uint16_t *p = strip + (yy - strip_y0) * GAME_W + x0;
        for (int n = x1 - x0; n > 0; n--) *p++ = c;
    }
}

// A thick line made of small squares.
static void line(int x0, int y0, int x1, int y1, int thick, uint16_t c) {
    int dx = x1 - x0, dy = y1 - y0;
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    if (steps == 0) steps = 1;
    for (int i = 0; i <= steps; i++) {
        int x = x0 + dx * i / steps, y = y0 + dy * i / steps;
        fill(x - thick / 2, y - thick / 2, thick, thick, c);
    }
}

// The font has no dollar sign, so it is drawn here (columns, bit 0 at the top).
static const uint8_t dollar_cols[5] = {0x24, 0x2A, 0x7F, 0x0A, 0x12};

// Draws text in the 5x7 font, `scale` times bigger. Each character is 6 * scale wide.
static void text(int x, int y, const char *s, uint16_t c, int scale) {
    for (; *s; s++, x += 6 * scale) {
        const uint8_t *cols = *s == '$' ? dollar_cols : font5x7_glyph(*s);
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

// ---- Geometry --------------------------------------------------------------------------

// The jaguar's hammer, relative to his feet and his facing: the grip, then the head on swing
// tick f (1..12). The head offset in the frame table is for facing right, so it is mirrored
// when he faces left.
static void grip_at(int *gx, int *gy) {
    *gx = man_x + facing * GRIP_DX;
    *gy = man_y + GRIP_DY;
}

static void head_at(int f, int *hx, int *hy) {
    int gx, gy;
    grip_at(&gx, &gy);
    *hx = gx + facing * reck_hammer_head[f - 1][0];
    *hy = gy + reck_hammer_head[f - 1][1];
}

static int car_cx(void) { return car_x + CAR_W / 2; }
static int car_cy(void) { return CAR_Y + CAR_H / 2; }

// The car's bottom strip and the jaguar's footprint: he cannot walk into the car.
static int foot_hits_car(int mx, int my, int cx) {
    return mx + MAN_FOOT_W > cx && mx - MAN_FOOT_W < cx + CAR_W &&
           my > GROUND_Y - CAR_FOOT_H && my - MAN_FOOT_H < GROUND_Y;
}

// Hit quality: 100 at the centre of the box, 0 at its edge plus the padding.
static int accuracy(int px, int py, int cx, int cy, int hw, int hh) {
    int dxn = abs(px - cx) * 100 / (hw / 2 + HIT_PAD);
    int dyn = abs(py - cy) * 100 / (hh / 2 + HIT_PAD);
    int d = dxn > dyn ? dxn : dyn;
    int q = 100 - d;
    return q < 0 ? 0 : q;
}

// ---- Scoring ---------------------------------------------------------------------------

// A hit's worth, in WRECK$: 100 to 300. Accuracy is most of it. Speed adds the rest: a hit
// that comes quickly after the last one scores more. A little luck is added on top.
static int hit_score(int q) {
    int since = ticks - last_hit_t;
    if (since > SPEED_WINDOW) since = SPEED_WINDOW;
    int speed = 100 - since * 100 / SPEED_WINDOW;
    last_hit_t = ticks;
    int s = 100 + (q * 120 + speed * 80) / 100 + rnd_range(-20, 20);
    if (s < 100) s = 100;
    if (s > 300) s = 300;
    return s;
}

// ---- Level ------------------------------------------------------------------------------

static void start_level(void) {
    state = S_PLAY;
    state_t = 0;
    ticks = 0;
    rng ^= 0x9E3779B9u; // a different run each time
    car_x = 150;
    car_dir = 1;
    man_x = MAN_START_X;
    man_y = MAN_START_Y;
    facing = 1;
    swing_t = 0;
    car_hp = CAR_HP_MAX;
    wreck = 0;
    hits = 0;
    sold_for = 0;
    bounces = 0;
    wreck_secs = 0;
    sale_time = sale_bounce = 0;
    immune = 0;
    shield_t = 0;
    shield_cd = 0;
    gorilla_t = 0;
    last_hit_t = -SPEED_WINDOW;
    popup_t = 0;
    memset(gor, 0, sizeof gor);
}

static void show_popup(int v) {
    popup_val = v;
    popup_t = POPUP_TICKS;
}

// The wreck is sold as soon as the car is destroyed. The sale bonus is the base price, plus a
// time bonus for a quick wreck, plus a bounce bonus. Then the completion screen starts.
static void complete_level(void) {
    wreck_secs = ticks / GAME_TICK_HZ;
    int short_by = TIME_PAR_S - wreck_secs;
    sale_time = short_by > 0 ? short_by * TIME_PER_S : 0;
    sale_bounce = bounces * BOUNCE_BONUS;
    sold_for = SALE_BONUS + sale_time + sale_bounce;
    wreck += sold_for;
    state = S_DONE;
    state_t = 0;
}

// The car takes damage. At zero it is wrecked and sold, and the level is complete.
static void damage_car(int dmg) {
    if (car_hp <= 0) return;
    car_hp -= dmg;
    if (car_hp <= 0) {
        car_hp = 0;
        complete_level();
    }
}

static void fire_gorilla(void) {
    for (int i = 0; i < GORILLA_MAX; i++) {
        if (gor[i].life) continue;
        float sx = car_cx(), sy = CAR_Y + 10;
        float dx = man_x - sx, dy = (man_y - 20) - sy;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1) len = 1;
        gor[i].x = sx;
        gor[i].y = sy;
        gor[i].vx = dx / len * GORILLA_SPEED;
        gor[i].vy = dy / len * GORILLA_SPEED;
        gor[i].life = GORILLA_LIFE;
        gor[i].state = G_FLYING;
        gor[i].damage = 0;
        return;
    }
}

// The hammer's head comes down on the car: a hit scores WRECK$ and does that much damage.
static void impact_car(void) {
    int hx, hy;
    head_at(IMPACT_TICK, &hx, &hy);
    int inside = hx >= car_x - HIT_PAD && hx <= car_x + CAR_W + HIT_PAD &&
                 hy >= CAR_Y - HIT_PAD && hy <= CAR_Y + CAR_H + HIT_PAD;
    if (!inside || car_hp <= 0) return; // a miss is still an attack, but scores nothing
    int s = hit_score(accuracy(hx, hy, car_cx(), car_cy(), CAR_W, CAR_H));
    wreck += s;
    show_popup(s);
    damage_car(s);
}

// Swing ticks around the impact: a gorilla in the head's path is knocked back at the car.
static void check_deflect(void) {
    int hx, hy;
    head_at(swing_t, &hx, &hy);
    for (int i = 0; i < GORILLA_MAX; i++) {
        if (!gor[i].life || gor[i].state != G_FLYING) continue;
        int gx = (int)gor[i].x, gy = (int)gor[i].y;
        if (abs(hx - gx) > DEFLECT_RADIUS || abs(hy - gy) > DEFLECT_RADIUS) continue;
        int s = hit_score(accuracy(hx, hy, gx, gy, 2 * GORILLA_HALF, 2 * GORILLA_HALF));
        wreck += s;
        show_popup(s);
        // Knock it back at the middle of the car.
        float dx = car_cx() - gx, dy = car_cy() - gy;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1) len = 1;
        gor[i].vx = dx / len * GORILLA_BACK;
        gor[i].vy = dy / len * GORILLA_BACK;
        gor[i].state = G_KNOCKED;
        gor[i].damage = s;
        bounces++;
        gor[i].life = 120;
    }
}

// Walks the jaguar, one axis at a time, so he slides round the car instead of stopping dead.
static void walk(int dx, int dy) {
    int nx = man_x + dx;
    if (nx < MAN_MIN_X) nx = MAN_MIN_X;
    if (nx > MAN_MAX_X) nx = MAN_MAX_X;
    if (!foot_hits_car(nx, man_y, car_x)) man_x = nx;
    int ny = man_y + dy;
    if (ny < MAN_MIN_Y) ny = MAN_MIN_Y;
    if (ny > MAN_MAX_Y) ny = MAN_MAX_Y;
    if (!foot_hits_car(man_x, ny, car_x)) man_y = ny;
}

// ---- Shop ------------------------------------------------------------------------------
// After Level 1 the shop sells what the jaguar drives, wields and wears in Level 2. The bank
// takes the WRECK$ from each run. What is bought is kept: the shop only resets when the board
// is powered on, not between runs. Level 2 will read the choices from these variables.

enum { SH_CAR, SH_HAMMER, SH_GEM, SH_BEAR, SH_BG, SH_NEXT };
#define SHOP_ROWS 16 // the rows the cursor moves over, the last one is NEXT
static const struct { int kind, idx, price; const char *name; } shop_rows[SHOP_ROWS] = {
    {SH_CAR, 0, 0, "URUS"},          {SH_CAR, 1, 300, "PICKUP"},
    {SH_CAR, 2, 800, "PORSCHE"},     {SH_CAR, 3, 1500, "FERRARI"},
    {SH_HAMMER, 0, 0, "CLASSIC"},    {SH_HAMMER, 1, 400, "SHINY BLACK"},
    {SH_GEM, 0, 150, "DIAMOND"},     {SH_GEM, 1, 150, "SAPPHIRE"},
    {SH_GEM, 2, 150, "RUBY"},        {SH_GEM, 3, 150, "EMERALD"},
    {SH_BEAR, 0, 0, "JAGUAR"},       {SH_BEAR, 1, 500, "BEAR"},
    {SH_BG, 0, 0, "RACE TRACK"},     {SH_BG, 1, 0, "TOWN"},
    {SH_BG, 2, 0, "VILLAGE"},        {SH_NEXT, 0, 0, "NEXT"},
};

static int shop_cur;            // the row the cursor is on
static int bank;                // WRECK$ to spend
static int shop_changed;        // something on the shop screen changed this tick
static int car_sel, ham_sel, bear_sel, bg_sel;
static uint8_t car_owned[4] = {1, 0, 0, 0};
static uint8_t ham_owned[2] = {1, 0};
static uint8_t bear_owned[2] = {1, 0};
static uint8_t gem_owned[4], gem_on[4];

static void enter_shop(void) {
    bank += wreck;     // the run's WRECK$ goes in the bank
    wreck = 0;
    shop_cur = 0;
    state = S_SHOP;
    state_t = 0;
    shop_changed = 1;
}

// Buy or select the row under the cursor. Owned items are just selected; gems toggle on and off.
static void shop_action(void) {
    int kind = shop_rows[shop_cur].kind, i = shop_rows[shop_cur].idx, price = shop_rows[shop_cur].price;
    uint8_t *owned = NULL;
    switch (kind) {
    case SH_CAR:   owned = car_owned + i; break;
    case SH_HAMMER: owned = ham_owned + i; break;
    case SH_BEAR:  owned = bear_owned + i; break;
    case SH_GEM:
        if (gem_owned[i]) {
            gem_on[i] = !gem_on[i];
        } else if (bank >= price) {
            bank -= price;
            gem_owned[i] = gem_on[i] = 1;
        }
        shop_changed = 1;
        return;
    case SH_BG:
        bg_sel = i;
        shop_changed = 1;
        return;
    case SH_NEXT:
        game_init(); // on to the title; Level 2 will start from here once it exists
        return;
    }
    if (!*owned) {
        if (bank < price) return;
        bank -= price;
        *owned = 1;
    }
    if (kind == SH_CAR) car_sel = i;
    if (kind == SH_HAMMER) ham_sel = i;
    if (kind == SH_BEAR) bear_sel = i;
    shop_changed = 1;
}

static void shop_step(uint8_t pressed) {
    if ((pressed & JOY_UP) && shop_cur > 0) { shop_cur--; shop_changed = 1; }
    if ((pressed & JOY_DOWN) && shop_cur < SHOP_ROWS - 1) { shop_cur++; shop_changed = 1; }
    if (pressed & JOY_SET) shop_action();
}

// ---- Firmware entry points -------------------------------------------------------------

void game_init(void) {
    state = S_TITLE;
    state_t = 0;
    ticks = 0;
}

static void step(uint8_t held, uint8_t pressed) {
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
        // Walk in 8 directions, round the car.
        int dx = 0, dy = 0;
        if (held & JOY_LEFT) dx -= MAN_SPEED;
        if (held & JOY_RIGHT) dx += MAN_SPEED;
        if (held & JOY_UP) dy -= MAN_SPEED;
        if (held & JOY_DOWN) dy += MAN_SPEED;
        walk(dx, dy);
        facing = car_cx() >= man_x ? 1 : -1;

        // The car drives left and right, and turns round at the ends or at the jaguar.
        int nx = car_x + car_dir * CAR_SPEED;
        if (nx < CAR_MIN_X || nx > CAR_MAX_X || foot_hits_car(man_x, man_y, nx)) {
            car_dir = -car_dir;
        } else {
            car_x = nx;
        }

        // SET raises the shield if it is charged. It stays up for a short time, then recharges.
        if (shield_cd) shield_cd--;
        if (shield_t) shield_t--;
        if ((pressed & JOY_SET) && shield_t == 0 && shield_cd == 0) {
            shield_t = SHIELD_TICKS;
            shield_cd = SHIELD_RECHARGE;
        }

        // The swing. The impact is checked on its own tick, then the swing moves on.
        if (swing_t) {
            if (swing_t == IMPACT_TICK) impact_car();
            if (state != S_PLAY) return; // the car is gone: the level is complete
            if (swing_t >= DEFLECT_FIRST && swing_t <= DEFLECT_LAST) check_deflect();
            swing_t++;
            if (swing_t > SWING_TICKS) swing_t = 0;
        }
        if ((pressed & JOY_MID) && swing_t == 0) swing_t = 1;

        // The car fires gorillas until it is wrecked.
        gorilla_t++;
        if (gorilla_t >= GORILLA_FIRST && (gorilla_t - GORILLA_FIRST) % GORILLA_EVERY == 0)
            fire_gorilla();

        for (int i = 0; i < GORILLA_MAX; i++) {
            if (!gor[i].life) continue;
            gor[i].x += gor[i].vx;
            gor[i].y += gor[i].vy;
            gor[i].life--;
            int gx = (int)gor[i].x, gy = (int)gor[i].y;

            if (gor[i].state == G_KNOCKED) {
                // It lands on the car: the damage it was scored for.
                if (gx > car_x - GORILLA_HALF && gx < car_x + CAR_W + GORILLA_HALF &&
                    gy > CAR_Y - GORILLA_HALF && gy < CAR_Y + CAR_H + GORILLA_HALF) {
                    gor[i].life = 0;
                    show_popup(gor[i].damage);
                    damage_car(gor[i].damage);
                    if (state != S_PLAY) return;
                }
                continue;
            }

            // A flying gorilla that reaches the jaguar.
            if (immune == 0 && gx > man_x - 14 - GORILLA_HALF && gx < man_x + 14 + GORILLA_HALF &&
                gy > man_y - 80 - GORILLA_HALF && gy < man_y + GORILLA_HALF) {
                gor[i].life = 0;
                if (shield_t) {
                    // Blocked: it bounces back onto the car, but the jaguar takes nothing.
                    gor[i].life = 0;
                    bounces++;
                    damage_car(SHIELD_DMG);
                    if (state != S_PLAY) return;
                } else {
                    hits++;
                    immune = IMMUNE_TICKS;
                    if (hits >= JAG_HITS) {
                        state = S_LOST;
                        state_t = 0;
                        return;
                    }
                }
            }
        }
        if (immune) immune--;
        if (popup_t) popup_t--;
        return;
    }

    case S_DONE:
        if (state_t >= END_WAIT && (pressed & JOY_SET)) enter_shop();
        return;

    case S_SHOP:
        shop_step(pressed);
        return;

    case S_LOST:
        if (state_t >= END_WAIT && (pressed & JOY_SET)) start_level();
        return;
    }
}

// ---- Dirty tiles -----------------------------------------------------------------------
// The frame is cut into 16-pixel tiles, in strips 8 rows high. Each tick the game marks the
// areas that may have changed. main.c only redraws and checks the marked tiles. The areas
// the marks cover this tick and last tick are both reported, so nothing is left behind.

#define DT_STRIPS (GAME_H / GAME_STRIP_ROWS)
#define DT_TILES  (GAME_W / GAME_TILE_W)
#define DT_ALL    ((1u << DT_TILES) - 1)

static uint32_t dt_now[DT_STRIPS], dt_prev[DT_STRIPS], dt_out[DT_STRIPS];

static void mark(int x0, int y0, int x1, int y1) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > GAME_W - 1) x1 = GAME_W - 1;
    if (y1 > GAME_H - 1) y1 = GAME_H - 1;
    if (x0 > x1 || y0 > y1) return;
    uint32_t m = 0;
    for (int t = x0 / GAME_TILE_W; t <= x1 / GAME_TILE_W; t++) m |= 1u << t;
    for (int s = y0 / GAME_STRIP_ROWS; s <= y1 / GAME_STRIP_ROWS; s++) dt_now[s] |= m;
}

static void mark_all(void) {
    for (int s = 0; s < DT_STRIPS; s++) dt_now[s] = DT_ALL;
}

// Last values seen, so the HUD and the car are only marked when they change.
static int mk_state = -1, mk_wreck = -1, mk_hits = -1, mk_car = -1;

// The jaguar, with his hammer, shield and recharge bar.
static void mark_jaguar(void) { mark(man_x - 80, man_y - 120, man_x + 80, man_y + 14); }

// Gorillas in the air. Their tumble animation changes every few ticks, so they are always marked.
static void mark_gorillas(void) {
    for (int i = 0; i < GORILLA_MAX; i++)
        if (gor[i].life) mark((int)gor[i].x - 16, (int)gor[i].y - 16, (int)gor[i].x + 16, (int)gor[i].y + 16);
}

// Works out what to mark for this tick. Anything that is not the play screen is redrawn in
// full: the transitions and the overlay screens are rare, so the cost is fine.
static void mark_for_tick(void) {
    if (state != mk_state) {
        mark_all(); // a change of screen: everything is new
    } else if (state == S_TITLE) {
        // The picture is still; only the title and the blinking prompt move.
        mark(0, 10, GAME_W - 1, 50);
        mark(0, 280, GAME_W - 1, 310);
    } else if (state == S_SHOP) {
        if (shop_changed) mark_all(); // the shop only redraws when something changed
    } else if (state == S_DONE || state == S_LOST) {
        mark_all(); // these screens are mostly text and are not drawn often
    } else {
        mark_jaguar();
        mark_gorillas();
        if (popup_t) mark(car_x + 14, CAR_Y - 40, car_x + 140, CAR_Y - 8);   // the points popup
        mark(car_x - 2, CAR_Y - 14, car_x + CAR_W + 2, CAR_Y + CAR_H + 2);   // the car moves every tick
        if (wreck != mk_wreck || hits != mk_hits) mark(0, 0, GAME_W - 1, 23); // the HUD
    }
    mk_state = state;
    mk_wreck = wreck;
    mk_hits = hits;
    mk_car = car_hp;
}

// The shield bar and the outline are part of the jaguar's marked area.
void game_update(uint8_t held, uint8_t pressed) {
    step(held, pressed);
    mark_for_tick();
    shop_changed = 0;
}

const uint32_t *game_dirty_tiles(void) {
    for (int s = 0; s < DT_STRIPS; s++) {
        dt_out[s] = dt_now[s] | dt_prev[s];
        dt_prev[s] = dt_now[s];
        dt_now[s] = 0;
    }
    return dt_out;
}

// ---- Drawing ---------------------------------------------------------------------------

// The background is the same every tick, so it is drawn one row at a time: sky above the
// road, a row of buildings standing on the road line, and the road with its white line.
static void draw_background(void) {
    static const struct { int x, w, h; } b[] = {
        {0, 60, 130}, {70, 44, 96}, {124, 70, 150}, {210, 40, 86},
        {260, 56, 120}, {330, 64, 160}, {404, 76, 110},
    };
    for (int r = 0; r < strip_rows; r++) {
        int y = strip_y0 + r;
        uint16_t *row = strip + r * GAME_W;
        uint16_t c = y < GROUND_Y ? COL_SKY : COL_ROAD;
        if (y == GROUND_Y + 10 || y == GROUND_Y + 11) c = COL_LINE;
        for (int x = 0; x < GAME_W; x++) row[x] = c;
        if (y < GROUND_Y) {
            for (unsigned i = 0; i < sizeof b / sizeof b[0]; i++)
                if (y >= GROUND_Y - b[i].h)
                    for (int x = b[i].x; x < b[i].x + b[i].w && x < GAME_W; x++) row[x] = COL_BUILD;
        }
    }
}

// Draws a sprite whose transparent pixels are RECK_KEY, clipped to the strip. `flip` mirrors
// it left to right, for facing left.
static void blit_dir(const uint16_t *spr, int w, int h, int x, int y, int flip) {
    for (int yy = 0; yy < h; yy++) {
        int py = y + yy;
        if (py < strip_y0 || py >= strip_y0 + strip_rows) continue;
        for (int xx = 0; xx < w; xx++) {
            uint16_t c = spr[yy * w + (flip ? w - 1 - xx : xx)];
            int px = x + xx;
            if (c != RECK_KEY && px >= 0 && px < GAME_W) strip[(py - strip_y0) * GAME_W + px] = c;
        }
    }
}

static void blit(const uint16_t *spr, int w, int h, int x, int y) { blit_dir(spr, w, h, x, y, 0); }

static void draw_car(void) {
    blit(reck_car_intact, RECK_CAR_W, RECK_CAR_H, car_x, CAR_Y);

    // Cracks as the car takes damage, and the health bar above it.
    int n = (CAR_HP_MAX - car_hp) * 8 / CAR_HP_MAX;
    for (int i = 0; i < n; i++) {
        int x = car_x + 40 + i * 12;
        line(x, CAR_Y + 44, x + 6, CAR_Y + 60, 2, COL_TEXT);
    }
    fill(car_x, CAR_Y - 12, CAR_W, 6, COL_BAR_BG);
    int w = car_hp * CAR_W / CAR_HP_MAX;
    fill(car_x, CAR_Y - 12, w, 6, car_hp > CAR_HP_MAX / 2 ? COL_GREEN : COL_RED);
}

static void draw_gorillas(void) {
    // The tumble alternates between the two poses while the gorilla flies.
    const uint16_t *frame = ((ticks / GORILLA_ANIM) & 1) ? reck_gorilla_arms : reck_gorilla_tumble;
    for (int i = 0; i < GORILLA_MAX; i++) {
        if (!gor[i].life) continue;
        blit(frame, RECK_GOR_W, RECK_GOR_H, (int)gor[i].x - RECK_GOR_W / 2, (int)gor[i].y - RECK_GOR_H / 2);
    }
}

// The jaguar, his hammer, the shield and the shield's recharge bar. `dance` sways him.
static void draw_man(int mx, int my, int dance) {
    int sway = dance ? (((state_t / 8) & 1) ? -8 : 8) : 0;
    int x = mx + sway;
    int flip = facing < 0;
    blit_dir(reck_jaguar, RECK_JAG_W, RECK_JAG_H, x - RECK_JAG_W / 2, my - RECK_JAG_H, flip);

    // The hammer: frame 1 when at rest (raised behind), else the frame for this swing tick.
    int f = swing_t ? swing_t : 1;
    int gx = x + facing * GRIP_DX, gy = my + GRIP_DY;
    blit_dir(reck_hammer[f - 1], RECK_HAMMER_W, RECK_HAMMER_W, gx - RECK_HAMMER_PIVOT,
             gy - RECK_HAMMER_PIVOT, flip);

    if (!dance) {
        if (shield_t) {
            // The shield: a cyan outline round the jaguar while it is up.
            fill(x - 22, my - 96, 44, 2, COL_CYAN);
            fill(x - 22, my - 2, 44, 2, COL_CYAN);
            fill(x - 22, my - 96, 2, 94, COL_CYAN);
            fill(x + 20, my - 96, 2, 94, COL_CYAN);
        }
        // The recharge bar above his head: full when the shield is ready.
        int ready = SHIELD_RECHARGE - shield_cd;
        fill(x - 14, my - 104, 28, 4, COL_SHIELD_BG);
        fill(x - 14, my - 104, ready * 28 / SHIELD_RECHARGE, 4, shield_cd ? COL_GOLD : COL_CYAN);
    }
}

static void draw_hud(void) {
    fill(0, 0, GAME_W, 22, RGB565(12, 4, 14));
    fill(0, 22, GAME_W, 2, COL_GOLD);
    char buf[32];
    snprintf(buf, sizeof buf, "WRECK$ %d", wreck);
    text(8, 7, buf, COL_TEXT, 2);
    snprintf(buf, sizeof buf, "HITS %d/%d", hits, JAG_HITS);
    text(GAME_W - text_width(buf, 2) - 8, 7, buf, hits >= JAG_HITS - 1 ? COL_RED : COL_TEXT, 2);
}

static void draw_play(void) {
    draw_background();
    draw_gorillas();

    // The jaguar is drawn behind the car when he is further back in depth, else in front.
    int behind = man_y - MAN_FOOT_H < GROUND_Y - CAR_FOOT_H;
    if (behind && !(immune && ((ticks / 3) & 1))) draw_man(man_x, man_y, 0);
    draw_car();
    if (!behind && !(immune && ((ticks / 3) & 1))) draw_man(man_x, man_y, 0);

    if (popup_t) {
        char buf[16];
        snprintf(buf, sizeof buf, "+%d", popup_val);
        text(car_x + 20, CAR_Y - 34, buf, COL_GOLD, 3);
    }
    draw_hud();
}

// The shop: the chosen Level 2 backdrop behind a panel with the list on the left, and a preview
// of the cursor's choice on the right.
#define COL_PANEL RGB565(14, 10, 26)

static void shop_status(int r, char *out, size_t n, uint16_t *col) {
    int kind = shop_rows[r].kind, i = shop_rows[r].idx, price = shop_rows[r].price;
    uint8_t owned = 0;
    int sel = 0;
    switch (kind) {
    case SH_CAR:   owned = car_owned[i]; sel = car_sel == i; break;
    case SH_HAMMER: owned = ham_owned[i]; sel = ham_sel == i; break;
    case SH_BEAR:  owned = bear_owned[i]; sel = bear_sel == i; break;
    case SH_GEM:   owned = gem_owned[i]; sel = gem_on[i]; break;
    case SH_BG:    owned = 1; sel = bg_sel == i; break;
    default:       out[0] = 0; return;
    }
    *col = COL_TEXT;
    if (kind == SH_GEM && owned) { snprintf(out, n, sel ? "ON" : "OFF"); return; }
    if (owned) { snprintf(out, n, sel ? "ON" : "OWNED"); return; }
    snprintf(out, n, "$%d", price);
    *col = bank >= price ? COL_GOLD : COL_RED;
}

static void shop_draw(void) {
    // The backdrop is the chosen Level 2 picture, a strip at a time.
    memcpy(strip, shop_bg[bg_sel] + (size_t)strip_y0 * GAME_W, (size_t)strip_rows * GAME_W * sizeof *strip);
    fill(0, 46, 252, 274, COL_PANEL);

    text(8, 6, "SHOP", COL_GOLD, 3);
    char buf[32];
    snprintf(buf, sizeof buf, "WRECK$ %d", bank);
    text(8, 32, buf, COL_GOLD, 2);

    for (int r = 0; r < SHOP_ROWS; r++) {
        int y = 52 + r * 16;
        if (r == shop_cur) text(4, y, ">", COL_GOLD, 2);
        text(20, y, shop_rows[r].name, r == shop_cur ? COL_GOLD : COL_TEXT, 2);
        char st[16];
        uint16_t col;
        shop_status(r, st, sizeof st, &col);
        if (st[0]) text(156, y, st, col, 2);
    }

    // The preview of the cursor's choice.
    int kind = shop_rows[shop_cur].kind, i = shop_rows[shop_cur].idx;
    static const char *const cat[] = {"CAR", "HAMMER", "JEWELS", "BEAR", "BACKDROP", ""};
    text(262, 52, cat[kind], COL_GOLD, 2);
    if (kind == SH_CAR) blit(shop_car[i], SHOP_CAR_W, SHOP_CAR_H, 270, 120);
    if (kind == SH_HAMMER) {
        blit(shop_hammer[i], SHOP_HAMMER_W, SHOP_HAMMER_H, 300, 84);
        for (int g = 0, k = 0; g < 4; g++)
            if (gem_on[g]) blit(shop_gem[g], SHOP_GEM, SHOP_GEM, 300 + 18 * k++, 156);
    }
    if (kind == SH_GEM)
        for (int g = 0; g < 4; g++) blit(shop_gem[g], SHOP_GEM, SHOP_GEM, 268 + 48 * g, 140);
    if (kind == SH_BEAR) blit(shop_bear, SHOP_BEAR_W, SHOP_BEAR_H, 300, 96);
    if (kind == SH_BG) text(262, 84, shop_rows[shop_cur].name, COL_TEXT, 2);
    if (kind == SH_NEXT) text(262, 84, "ON TO THE TITLE", COL_TEXT, 2);
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

    if (state == S_SHOP) {
        shop_draw();
        return;
    }

    if (state == S_DONE) {
        draw_background();
        draw_man(70, 300, 1); // the jaguar, swaying with the hammer up, on the left of the text
        text_centred(40, "LEVEL 1", COL_GOLD, 4);
        text_centred(80, "COMPLETE", COL_GOLD, 4);
        char buf2[32];
        snprintf(buf2, sizeof buf2, "TIME %d S", wreck_secs);
        text_centred(128, buf2, COL_TEXT, 2);
        snprintf(buf2, sizeof buf2, "BOUNCED %d", bounces);
        text_centred(150, buf2, COL_TEXT, 2);
        snprintf(buf2, sizeof buf2, "SALE +%d", sold_for);
        text_centred(172, buf2, COL_GOLD, 2);
        snprintf(buf2, sizeof buf2, "TOTAL WRECK$ %d", wreck);
        text_centred(198, buf2, COL_GOLD, 2);
        if ((ticks / BLINK_TICKS) % 2 == 0) text_centred(250, "SET: SHOP", COL_GOLD, 2);
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
