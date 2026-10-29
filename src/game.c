// Agent Jatayu, Level 1: The Rakshasa Assault.
//
// Title screen -> intro -> battle against three Rakshasas -> victory or game over.
// Controls: the joystick flies Jatayu in 8 directions (two directions at once),
//   MID   magical claw: left and right swings alternate, a third quick swing is the
//         double-claw X slash
//   SET   divine claw burst (costs 40 divine energy)
//   double-tap a direction to dash (brief invulnerability)
//   RST   back to the title
//
// game_draw() is called once per 8-row strip, so everything is drawn clipped to the strip.

#include "game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "font5x7.h"
#include "jatayu_img.h"

// ---- Layout ----------------------------------------------------------------------------

#define HUD_H    28
#define PLAY_L   12
#define PLAY_R   468
#define PLAY_T   32
#define PLAY_B   310
#define PLAYER_W SPR_JATAYU_F0_L1_W
#define PLAYER_H SPR_JATAYU_F0_L1_H
#define PLAYER_BOUND 30 // how close Jatayu's centre may get to the playfield edge
#define START_X  240
#define START_Y  255

// ---- Colours ---------------------------------------------------------------------------

#define COL_TEXT   RGB565(255, 255, 255)
#define COL_SHADOW RGB565(0, 0, 0)
#define COL_GOLD   RGB565(255, 200, 60)
#define COL_GOLD2  RGB565(255, 244, 170)
#define COL_EMBER  RGB565(255, 110, 30)
#define COL_SMOKE  RGB565(54, 26, 64)
#define COL_SMOKE2 RGB565(96, 44, 76)
#define COL_RED    RGB565(220, 40, 50)
#define COL_HUD    RGB565(14, 3, 12)
#define COL_HUDLN  RGB565(150, 110, 40)
#define COL_ORB    RGB565(150, 70, 230)
#define COL_ORB2   RGB565(240, 200, 255)
#define COL_WHITE  RGB565(255, 255, 255)
#define COL_SILH   RGB565(26, 4, 30)
#define COL_GLOW   RGB565(255, 130, 40)

// ---- Timing (ticks run at GAME_TICK_HZ = 30) -------------------------------------------

#define INTRO_TICKS      90
#define CLAW_COOLDOWN    8   // about 250 ms
#define DOUBLE_COOLDOWN  18  // about 600 ms
#define BURST_COOLDOWN   60  // 2 s
#define DASH_COOLDOWN    30  // 1 s
#define DASH_TICKS       8
#define DASH_SPEED       11
#define COMBO_WINDOW     20
#define INVULN_TICKS     30
#define MOVE_SPEED       4
#define BURST_COST       40
#define MAX_HP           200
#define MAX_ENERGY       100

// Level layout: three Rakshasas, then four more.
#define N_EN             7
#define WAVE1            3
#define WAVE_DELAY       40  // ticks between wave 1's last defeat and wave 2 appearing
#define BANNER_TICKS     75

// Final score = kills + a bonus for speed, a bonus for few attacks and a bonus for health left.
#define TIME_PAR_S       100 // seconds; each second under it is worth TIME_PTS
#define TIME_PTS         15
#define ATTACK_PAR       80  // attacks (claw swings and bursts); each one under it is worth ATTACK_PTS
#define ATTACK_PTS       15
#define HEALTH_PTS       10  // per health point left

// Tiles the game reports as changed, per strip (see game_dirty_tiles() below).
#define STRIPS (GAME_H / GAME_STRIP_ROWS)
#define TILES (GAME_W / GAME_TILE_W)
#define ALL_TILES ((1u << TILES) - 1)

static uint32_t tiles_now[STRIPS], tiles_prev[STRIPS], tiles_out[STRIPS];

enum { ST_TITLE, ST_INTRO, ST_BATTLE, ST_VICTORY, ST_GAMEOVER };
enum { SW_NONE, SW_LEFT, SW_RIGHT, SW_DOUBLE };

// ---- Small helpers ---------------------------------------------------------------------

static uint32_t rng_state = 2463534242u;
static uint32_t rnd(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
static float frand(float a, float b) { return a + (float)(rnd() & 0xFFFF) / 65535.0f * (b - a); }

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static int overlap(float ax0, float ay0, float ax1, float ay1, float bx0, float by0, float bx1, float by1) {
    return ax0 < bx1 && ax1 > bx0 && ay0 < by1 && ay1 > by0;
}

// ---- Strip drawing ---------------------------------------------------------------------

// The strip being drawn right now. Every drawing helper clips to it.
static uint16_t *strip;
static int strip_y0, strip_rows;

static void fill(int x, int y, int w, int h, uint16_t c) {
    int y1 = y + h, x1 = x + w;
    if (y < strip_y0) y = strip_y0;
    if (y1 > strip_y0 + strip_rows) y1 = strip_y0 + strip_rows;
    if (x < 0) x = 0;
    if (x1 > GAME_W) x1 = GAME_W;
    for (int yy = y; yy < y1; yy++) {
        uint16_t *row = strip + (yy - strip_y0) * GAME_W;
        for (int xx = x; xx < x1; xx++) row[xx] = c;
    }
}

// Draws text in the 5x7 font, `scale` times bigger. Each character is 6 * scale wide.
static void text(int x, int y, const char *s, uint16_t c, int scale) {
    if (y >= strip_y0 + strip_rows || y + 7 * scale <= strip_y0) return;
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

// Text with a black shadow so it reads over the artwork.
static void text_centred_shadow(int y, const char *s, uint16_t c, int scale) {
    text_centred(y + scale, s, COL_SHADOW, scale);
    text_centred(y, s, c, scale);
}

// Draws a colour-keyed sprite with its top-left at (x0, y0). `solid` >= 0 paints every
// opaque pixel that colour (hit flash, silhouette); `dissolve` (0..16) drops that many
// sixteenths of the pixels, in a fixed scatter, for the defeat animation.
static void blit_sprite(const uint16_t *spr, int w, int h, int x0, int y0, int solid, int dissolve) {
    int ya = y0 > strip_y0 ? y0 : strip_y0;
    int yb = y0 + h < strip_y0 + strip_rows ? y0 + h : strip_y0 + strip_rows;
    for (int y = ya; y < yb; y++) {
        const uint16_t *src = spr + (y - y0) * w;
        uint16_t *row = strip + (y - strip_y0) * GAME_W;
        for (int i = 0; i < w; i++) {
            int x = x0 + i;
            if (x < 0 || x >= GAME_W) continue;
            uint16_t c = src[i];
            if (c == SPRITE_KEY) continue;
            if (dissolve && (int)((x * 7 + y * 13 + ((x ^ y) * 3)) & 15) < dissolve) continue;
            row[x] = solid >= 0 ? (uint16_t)solid : c;
        }
    }
}

static uint16_t avg565(uint16_t a, uint16_t b) {
    return (uint16_t)(((a >> 1) & 0x7BEF) + ((b >> 1) & 0x7BEF));
}

// Gold wash over the whole strip: level 1 = 25%, 2 = 50%.
static void tint_gold(int level) {
    uint16_t g = RGB565(255, 214, 110);
    for (int i = 0; i < strip_rows * GAME_W; i++) {
        uint16_t p = avg565(strip[i], g);
        strip[i] = level >= 2 ? p : avg565(strip[i], p);
    }
}

// Darkens the strip: level 1 = half brightness, 2 = quarter.
static void tint_dark(int level) {
    for (int i = 0; i < strip_rows * GAME_W; i++) {
        uint16_t p = (strip[i] >> 1) & 0x7BEF;
        strip[i] = level >= 2 ? (p >> 1) & 0x7BEF : p;
    }
}

static void num_str(char *out, int v, int digits) {
    for (int i = digits - 1; i >= 0; i--, v /= 10) out[i] = (char)('0' + v % 10);
    out[digits] = 0;
}

// ---- Particles -------------------------------------------------------------------------

#define MAX_PARTICLES 80

typedef struct {
    float x, y, vx, vy, g;
    int16_t life;
    uint16_t col;
    uint8_t size;
} Particle;

static Particle parts[MAX_PARTICLES];

static void spark(float x, float y, float vx, float vy, float g, int life, uint16_t col, int size) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (parts[i].life <= 0) {
            parts[i] = (Particle){x, y, vx, vy, g, (int16_t)life, col, (uint8_t)size};
            return;
        }
    }
}

static void burst(float x, float y, int n, float speed, int life, uint16_t c1, uint16_t c2, int size, float g) {
    for (int i = 0; i < n; i++) {
        float a = frand(0, 6.2832f), sp = frand(0.3f, 1.0f) * speed;
        spark(x, y, cosf(a) * sp, sinf(a) * sp, g, life - (int)(rnd() % 6), (rnd() & 1) ? c1 : c2, size);
    }
}

static void update_particles(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &parts[i];
        if (p->life <= 0) continue;
        p->x += p->vx;
        p->y += p->vy;
        p->vy += p->g;
        p->life--;
    }
}

// ---- Game state ------------------------------------------------------------------------

typedef struct {
    float x, y, kx, ky; // position (sprite centre) and knockback velocity
    int hp, dying, flash, stun, t, last_hit;
    int atk_cd, dodge_cd, shoot_cd, phase, ptimer;
    float tx, ty;
    uint8_t type, alive;
} Enemy;

typedef struct {
    float x, y, vx, vy;
    uint8_t alive;
} Orb;

static const struct { int w, h, hp, score; const uint16_t *spr; } etab[3] = {
    {SPR_BLOOD_WARRIOR_W, SPR_BLOOD_WARRIOR_H, 2, 100, spr_blood_warrior},
    {SPR_SHADOW_SPEARMAN_W, SPR_SHADOW_SPEARMAN_H, 3, 200, spr_shadow_spearman},
    {SPR_DEMON_COMMANDER_W, SPR_DEMON_COMMANDER_H, 5, 500, spr_demon_commander},
};
// Where each Rakshasa enters, from the top. 0-2 are wave 1; 3-6 are wave 2, and have one more
// hit point each.
static const struct { uint8_t type; float x, y; } wave_tab[N_EN] = {
    {0, 110, -30}, {1, 240, -50}, {2, 370, -30},
    {0, 70, -30}, {1, 190, -50}, {0, 310, -30}, {2, 430, -50},
};

static Enemy en[N_EN];
#define MAX_ORBS 6
static Orb orbs[MAX_ORBS];

static struct {
    float x, y, kx, ky;
    int hp, energy, score, attacks;
    int invuln, hitflash;
    int swing, swing_t, swing_id, cooldown;
    int combo, combo_t, next_side;
    int burst_t, burst_cd, burst_id;
    int dash_t, dash_cd;
    float dash_dx, dash_dy;
    uint8_t tap[4]; // ticks since each direction was last pressed
    int lean;       // -1 left, 0 up, +1 right
    float last_dx, last_dy;
} pl;

static int state;
static int force_all; // redraw every strip on the next frame
static unsigned ticks;
static int state_t; // ticks spent in the current state
static int defeated;
static int wave, wave_t;       // wave 1 or 2, and ticks since wave 2 was called
static int battle_ticks;       // ticks spent fighting, for the time bonus
static int bonus_time, bonus_attacks, bonus_health, final_score;
#define N_EMBERS 6
static float ember_x[N_EMBERS], ember_y[N_EMBERS];

static void spawn_enemy(int i) {
    Enemy *e = &en[i];
    memset(e, 0, sizeof *e);
    e->type = wave_tab[i].type;
    e->alive = 1;
    e->hp = etab[e->type].hp + (i >= WAVE1 ? 1 : 0);
    e->x = wave_tab[i].x;
    e->y = wave_tab[i].y;
    e->tx = e->x; // the Shadow Spearman zigzags around this
    e->last_hit = -1;
    e->shoot_cd = 40;
}

static void reset_level(void) {
    memset(&pl, 0, sizeof pl);
    pl.x = START_X;
    pl.y = 340; // flies in from below during the intro
    pl.hp = MAX_HP;
    pl.energy = 50;
    pl.next_side = SW_LEFT;
    pl.last_dy = -1;
    memset(en, 0, sizeof en);
    for (int i = 0; i < WAVE1; i++) spawn_enemy(i);
    wave = 1;
    wave_t = 0;
    battle_ticks = 0;
    memset(orbs, 0, sizeof orbs);
    memset(parts, 0, sizeof parts);
    defeated = 0;
    for (int i = 0; i < N_EMBERS; i++) {
        ember_x[i] = frand(20, 460);
        ember_y[i] = frand(PLAY_T, 300);
    }
    state = ST_INTRO;
    state_t = 0;
    force_all = 1;
}

void game_init(void) {
    state = ST_TITLE;
    state_t = 0;
    ticks = 0;
    force_all = 1;
}

// ---- Combat ----------------------------------------------------------------------------

static void hurt_player(int dmg, float fx, float fy) {
    if (state != ST_BATTLE || pl.invuln > 0 || pl.dash_t > 0) return;
    pl.hp -= dmg;
    pl.invuln = INVULN_TICKS;
    pl.hitflash = 6;
    float dx = pl.x - fx, dy = pl.y - fy, d = sqrtf(dx * dx + dy * dy);
    if (d < 1) { dx = 0; dy = 1; d = 1; }
    pl.kx = dx / d * 9;
    pl.ky = dy / d * 9;
    burst(pl.x, pl.y, 10, 3.0f, 12, COL_RED, COL_GOLD, 2, 0.1f);
    if (pl.hp <= 0) {
        pl.hp = 0;
        state = ST_GAMEOVER;
        state_t = 0;
    }
}

static void damage_enemy(Enemy *e, int dmg, int id, float kb) {
    e->last_hit = id;
    e->hp -= dmg;
    e->flash = 3;
    e->stun = 4;
    float dx = e->x - pl.x, dy = e->y - pl.y, d = sqrtf(dx * dx + dy * dy);
    if (d < 1) { dx = 0; dy = -1; d = 1; }
    e->kx = dx / d * kb;
    e->ky = dy / d * kb;
    pl.energy += 10;
    if (pl.energy > MAX_ENERGY) pl.energy = MAX_ENERGY;
    burst(e->x, e->y, 8, 3.2f, 10, COL_GOLD, COL_GOLD2, 2, 0.15f);
    if (e->hp <= 0) {
        e->hp = 0;
        e->dying = 14;
    }
}

// The area a swing can hit, relative to Jatayu's centre.
static void swing_box(int type, float *x0, float *y0, float *x1, float *y1) {
    switch (type) {
    case SW_LEFT:  *x0 = pl.x - 54; *x1 = pl.x + 8;  *y0 = pl.y - 60; *y1 = pl.y + 4; break;
    case SW_RIGHT: *x0 = pl.x - 8;  *x1 = pl.x + 54; *y0 = pl.y - 60; *y1 = pl.y + 4; break;
    default:       *x0 = pl.x - 66; *x1 = pl.x + 66; *y0 = pl.y - 72; *y1 = pl.y + 8; break;
    }
}

static void check_swing_hits(void) {
    float bx0, by0, bx1, by1;
    swing_box(pl.swing, &bx0, &by0, &bx1, &by1);
    int dmg = pl.swing == SW_DOUBLE ? 2 : 1;
    for (int i = 0; i < N_EN; i++) {
        Enemy *e = &en[i];
        if (!e->alive || e->dying || e->last_hit == pl.swing_id) continue;
        float hw = etab[e->type].w / 2 - 4, hh = etab[e->type].h / 2 - 4;
        if (overlap(bx0, by0, bx1, by1, e->x - hw, e->y - hh, e->x + hw, e->y + hh))
            damage_enemy(e, dmg, pl.swing_id, e->type == 2 ? 5.0f : 8.0f);
    }
}

static void check_burst_hits(void) {
    for (int i = 0; i < N_EN; i++) {
        Enemy *e = &en[i];
        if (!e->alive || e->dying || e->last_hit == pl.burst_id) continue;
        float dx = e->x - pl.x, dy = e->y - pl.y;
        if (dx * dx + dy * dy < 100.0f * 100.0f) damage_enemy(e, 2, pl.burst_id, e->type == 2 ? 7.0f : 12.0f);
    }
}

static void start_dash(float dx, float dy) {
    float d = sqrtf(dx * dx + dy * dy);
    if (d < 0.5f) { dx = pl.last_dx; dy = pl.last_dy; d = sqrtf(dx * dx + dy * dy); }
    if (d < 0.5f) { dx = 0; dy = -1; d = 1; }
    pl.dash_dx = dx / d;
    pl.dash_dy = dy / d;
    pl.dash_t = DASH_TICKS;
    pl.dash_cd = DASH_COOLDOWN;
}

static void update_player(uint8_t held, uint8_t pressed) {
    // cooldown timers
    if (pl.cooldown > 0) pl.cooldown--;
    if (pl.burst_cd > 0) pl.burst_cd--;
    if (pl.dash_cd > 0) pl.dash_cd--;
    if (pl.invuln > 0) pl.invuln--;
    if (pl.hitflash > 0) pl.hitflash--;
    if (pl.combo_t > 0 && --pl.combo_t == 0) pl.combo = 0;
    if (ticks % 12 == 0 && pl.energy < MAX_ENERGY) pl.energy++;
    for (int d = 0; d < 4; d++)
        if (pl.tap[d] < 255) pl.tap[d]++;

    // movement direction from the held buttons
    float dx = (held & JOY_RIGHT ? 1 : 0) - (held & JOY_LEFT ? 1 : 0);
    float dy = (held & JOY_DOWN ? 1 : 0) - (held & JOY_UP ? 1 : 0);
    if (dx != 0 && dy != 0) { dx *= 0.7071f; dy *= 0.7071f; }
    if (dx != 0 || dy != 0) { pl.last_dx = dx; pl.last_dy = dy; }
    pl.lean = dx > 0 ? 1 : (dx < 0 ? -1 : 0);

    // double-tap a direction to dash
    static const uint8_t dirs[4] = {JOY_UP, JOY_DOWN, JOY_LEFT, JOY_RIGHT};
    for (int d = 0; d < 4; d++) {
        if (!(pressed & dirs[d])) continue;
        if (pl.tap[d] <= 9 && pl.dash_cd == 0 && pl.dash_t == 0) start_dash(dx, dy);
        pl.tap[d] = 0;
    }

    if (pl.dash_t > 0) {
        pl.dash_t--;
        pl.x += pl.dash_dx * DASH_SPEED;
        pl.y += pl.dash_dy * DASH_SPEED;
        for (int i = 0; i < 2; i++)
            spark(pl.x + frand(-10, 10), pl.y + frand(-6, 14), frand(-0.4f, 0.4f), 0.8f, 0.02f, 12,
                  (rnd() & 1) ? COL_GOLD : COL_GOLD2, 2);
    } else {
        pl.x += dx * MOVE_SPEED;
        pl.y += dy * MOVE_SPEED;
    }
    pl.x += pl.kx;
    pl.y += pl.ky;
    pl.kx *= 0.7f;
    pl.ky *= 0.7f;
    pl.x = clampf(pl.x, PLAY_L + PLAYER_BOUND, PLAY_R - PLAYER_BOUND);
    pl.y = clampf(pl.y, PLAY_T + PLAYER_BOUND, PLAY_B - PLAYER_BOUND);

    // claw swing
    if (pl.swing != SW_NONE) {
        if (pl.swing_t < 3) check_swing_hits();
        if (++pl.swing_t >= (pl.swing == SW_DOUBLE ? 9 : 6)) pl.swing = SW_NONE;
    }
    if ((pressed & JOY_MID) && pl.cooldown == 0) {
        pl.attacks++;
        pl.swing_id++;
        pl.swing_t = 0;
        if (pl.combo >= 2) {
            pl.swing = SW_DOUBLE;
            pl.cooldown = DOUBLE_COOLDOWN;
            pl.combo = 0;
            pl.combo_t = 0;
        } else {
            pl.swing = pl.next_side;
            pl.next_side = pl.next_side == SW_LEFT ? SW_RIGHT : SW_LEFT;
            pl.cooldown = CLAW_COOLDOWN;
            pl.combo++;
            pl.combo_t = COMBO_WINDOW;
        }
        check_swing_hits();
    }

    // divine claw burst
    if (pl.burst_t > 0 && ++pl.burst_t > 12) pl.burst_t = 0;
    if ((pressed & JOY_SET) && pl.burst_cd == 0 && pl.energy >= BURST_COST) {
        pl.energy -= BURST_COST;
        pl.attacks++;
        pl.burst_cd = BURST_COOLDOWN;
        pl.burst_t = 1;
        pl.burst_id = ++pl.swing_id;
        burst(pl.x, pl.y, 24, 6.0f, 14, COL_GOLD, COL_GOLD2, 3, 0);
        check_burst_hits();
    }

    // golden glow around the claws
    if ((ticks & 1) == 0)
        spark(pl.x + frand(6, 24), pl.y + frand(16, 28), 0, -0.4f, 0, 9, (rnd() & 1) ? COL_GOLD : COL_GOLD2, 1);
}

// ---- Rakshasa AI -----------------------------------------------------------------------

static void spawn_orb(float x, float y) {
    float dx = pl.x - x, dy = pl.y - y, d = sqrtf(dx * dx + dy * dy);
    if (d < 1) d = 1;
    for (int i = 0; i < MAX_ORBS; i++) {
        if (!orbs[i].alive) {
            orbs[i] = (Orb){x, y, dx / d * 2.6f, dy / d * 2.6f, 1};
            return;
        }
    }
}

static void move_toward(Enemy *e, float tx, float ty, float speed) {
    float dx = tx - e->x, dy = ty - e->y, d = sqrtf(dx * dx + dy * dy);
    if (d < 1) return;
    e->x += dx / d * speed;
    e->y += dy / d * speed;
}

// The Blood Warrior drops diagonally onto Jatayu, weaving as it comes, and backs off after
// each strike.
static void ai_blood_warrior(Enemy *e) {
    if (e->atk_cd > 0) {
        e->atk_cd--;
        move_toward(e, e->x + (e->x < pl.x ? -60 : 60), e->y - 80, 2.0f);
        return;
    }
    float dx = pl.x - e->x, dy = pl.y - e->y, d = sqrtf(dx * dx + dy * dy);
    float speed = d < 120 ? 2.6f : 1.8f;
    move_toward(e, pl.x + sinf((float)e->t * 0.25f) * 24, pl.y, speed);
    if (dy < 20 && e->y < 40) e->y += 0.8f;
}

// The Shadow Spearman zigzags down, hovers, throws slow dark orbs and slips away when
// Jatayu gets close.
static void ai_shadow_spearman(Enemy *e) {
    if (e->dodge_cd > 0) e->dodge_cd--;
    if (e->y < 96) {
        e->y += 1.4f;
        e->x = clampf(e->tx + sinf((float)e->t * 0.11f) * 110, 40, 440);
    } else {
        float hover_y = 96 + sinf((float)e->t * 0.07f) * 18;
        e->y += (hover_y - e->y) * 0.1f;
        e->x += sinf((float)e->t * 0.09f) * 2.2f;
    }
    float dx = pl.x - e->x, dy = pl.y - e->y;
    if (e->dodge_cd == 0 && dx * dx + dy * dy < 80.0f * 80.0f) {
        e->kx = dx > 0 ? -9 : 9;
        e->dodge_cd = 45;
    }
    if (e->y > 20 && --e->shoot_cd <= 0) {
        spawn_orb(e->x, e->y + 14);
        e->shoot_cd = 55;
    }
}

// The Demon Commander lumbers down, then hunts Jatayu with short charges after a glowing
// wind-up.
static void ai_demon_commander(Enemy *e) {
    switch (e->phase) {
    case 0: // approach
        if (e->y < 70) {
            e->y += 0.9f;
            e->x += (pl.x - e->x) * 0.01f;
        } else {
            move_toward(e, pl.x, pl.y, 1.2f);
            if (++e->ptimer >= 70) { e->phase = 1; e->ptimer = 0; }
        }
        break;
    case 1: // wind-up
        if (++e->ptimer >= 14) {
            e->tx = pl.x;
            e->ty = pl.y;
            e->phase = 2;
            e->ptimer = 0;
        }
        break;
    case 2: // charge
        move_toward(e, e->tx, e->ty, 6.5f);
        if (++e->ptimer >= 14) { e->phase = 3; e->ptimer = 0; }
        break;
    default: // recover
        if (++e->ptimer >= 18) { e->phase = 0; e->ptimer = 0; }
        break;
    }
}

static void finish_enemy(Enemy *e) {
    burst(e->x, e->y, 22, 3.5f, 22, COL_SMOKE, COL_SMOKE2, 3, -0.05f);
    burst(e->x, e->y, 16, 4.0f, 18, COL_EMBER, COL_GOLD, 2, 0.08f);
    e->alive = 0;
    defeated++;
    pl.score += etab[e->type].score;
}

static void update_enemies(void) {
    for (int i = 0; i < N_EN; i++) {
        Enemy *e = &en[i];
        if (!e->alive) continue;
        e->t++;
        if (e->flash > 0) e->flash--;

        if (e->dying) {
            if (--e->dying == 0) finish_enemy(e);
            continue;
        }

        if (e->stun > 0) e->stun--;
        else if (state == ST_BATTLE) {
            if (e->type == 0) ai_blood_warrior(e);
            else if (e->type == 1) ai_shadow_spearman(e);
            else ai_demon_commander(e);
        } else if (state == ST_INTRO && state_t > INTRO_TICKS / 2) {
            e->y += 1.0f; // silhouettes drift into view during the intro
        }

        e->x += e->kx;
        e->y += e->ky;
        e->kx *= 0.78f;
        e->ky *= 0.78f;
        e->x = clampf(e->x, 20, 460);
        if (e->y > 292) e->y = 292;

        // contact with Jatayu
        if (state == ST_BATTLE && e->y > 8) {
            float hw = etab[e->type].w / 2 - 6 + 10, hh = etab[e->type].h / 2 - 6 + 14;
            if (overlap(e->x - hw, e->y - hh, e->x + hw, e->y + hh, pl.x - 16, pl.y - 20, pl.x + 16, pl.y + 20)) {
                if (e->type == 0 && e->atk_cd == 0) {
                    hurt_player(12, e->x, e->y);
                    e->atk_cd = 40;
                } else if (e->type == 1) {
                    hurt_player(8, e->x, e->y);
                } else if (e->type == 2 && e->atk_cd == 0) {
                    hurt_player(e->phase == 2 ? 20 : 15, e->x, e->y);
                    e->atk_cd = 40;
                }
            }
        }
        if (e->atk_cd > 0 && e->type == 2) e->atk_cd--;
    }
}

static void update_orbs(void) {
    for (int i = 0; i < MAX_ORBS; i++) {
        Orb *o = &orbs[i];
        if (!o->alive) continue;
        o->x += o->vx;
        o->y += o->vy;
        if (o->x < -10 || o->x > GAME_W + 10 || o->y < -10 || o->y > GAME_H + 10) { o->alive = 0; continue; }
        if (state == ST_BATTLE && overlap(o->x - 5, o->y - 5, o->x + 5, o->y + 5, pl.x - 16, pl.y - 20, pl.x + 16, pl.y + 20)) {
            if (pl.invuln == 0 && pl.dash_t == 0) {
                hurt_player(10, o->x, o->y);
                o->alive = 0;
            }
        }
        if ((ticks & 1) == 0) spark(o->x, o->y, 0, 0, 0, 6, COL_ORB, 2);
    }
}

static void update_embers(void) {
    for (int i = 0; i < N_EMBERS; i++) {
        ember_y[i] -= 0.5f + (float)(i % 3) * 0.2f;
        ember_x[i] += sinf((float)(ticks + (unsigned)i * 17) * 0.05f) * 0.4f;
        if (ember_y[i] < PLAY_T) {
            ember_y[i] = 306;
            ember_x[i] = frand(20, 460);
        }
    }
}

// ---- State machine ---------------------------------------------------------------------

static void step(uint8_t held, uint8_t pressed) {
    ticks++;
    state_t++;

    if (pressed & JOY_RESET) {
        game_init();
        return;
    }

    if (state == ST_TITLE) {
        if (pressed & JOY_SET) reset_level();
        return;
    }

    update_particles();
    update_embers();
    update_orbs();
    update_enemies();

    switch (state) {
    case ST_INTRO: {
        // Jatayu flies in from the bottom and settles at (240, 255).
        float t = (float)state_t / 45.0f;
        if (t > 1) t = 1;
        t = 1 - (1 - t) * (1 - t);
        pl.y = 340 + (START_Y - 340) * t;
        if (ticks % 12 == 0 && pl.energy < MAX_ENERGY) pl.energy++;
        spark(pl.x + frand(6, 24), pl.y + frand(16, 28), 0, -0.4f, 0, 9, COL_GOLD, 1);
        if (state_t >= INTRO_TICKS) { state = ST_BATTLE; state_t = 0; }
        break;
    }
    case ST_BATTLE:
        update_player(held, pressed);
        battle_ticks++;
        if (wave == 1 && defeated >= WAVE1) wave = 2;
        if (wave == 2 && wave_t < 90 && ++wave_t == WAVE_DELAY)
            for (int i = WAVE1; i < N_EN; i++) spawn_enemy(i);
        if (defeated >= N_EN) {
            int secs = battle_ticks / GAME_TICK_HZ;
            bonus_time = secs < TIME_PAR_S ? (TIME_PAR_S - secs) * TIME_PTS : 0;
            bonus_attacks = pl.attacks < ATTACK_PAR ? (ATTACK_PAR - pl.attacks) * ATTACK_PTS : 0;
            bonus_health = pl.hp * HEALTH_PTS;
            final_score = pl.score + bonus_time + bonus_attacks + bonus_health;
            state = ST_VICTORY;
            state_t = 0;
            for (int i = 0; i < MAX_ORBS; i++) {
                if (orbs[i].alive) burst(orbs[i].x, orbs[i].y, 6, 2.0f, 12, COL_ORB2, COL_GOLD2, 2, 0);
                orbs[i].alive = 0;
            }
            pl.swing = SW_NONE;
            pl.burst_t = 0;
        }
        break;
    case ST_VICTORY: {
        // Jatayu glides to the centre with his claws blazing.
        float t = (float)state_t / 40.0f;
        if (t > 1) t = 1;
        // then steps aside, to the left, while the score is shown
        pl.x += ((state_t >= 100 ? 62 : 240) - pl.x) * 0.15f;
        pl.y += ((state_t >= 100 ? 200 : 170) - pl.y) * 0.15f;
        pl.lean = 0;
        if (state_t < 120 && (ticks & 1) == 0) {
            float a = frand(0, 6.2832f);
            spark(pl.x + cosf(a) * 60, pl.y + sinf(a) * 60, -cosf(a) * 1.4f, -sinf(a) * 1.4f, 0, 26,
                  (rnd() & 1) ? COL_GOLD : COL_GOLD2, 2);
        }
        spark(pl.x + frand(6, 24), pl.y + frand(16, 28), 0, -0.6f, 0, 12, COL_GOLD2, 2);
        if (state_t >= 100 && (pressed & JOY_SET)) reset_level();
        break;
    }
    case ST_GAMEOVER:
        if (pl.hitflash > 0) pl.hitflash--;
        if (state_t >= 36 && (pressed & JOY_SET)) reset_level();
        break;
    }
}

// ---- Drawing ---------------------------------------------------------------------------

static void draw_title(void) {
    memcpy(strip, jatayu_img + (size_t)strip_y0 * JATAYU_IMG_W,
           (size_t)strip_rows * JATAYU_IMG_W * sizeof *strip);
    text_centred_shadow(14, "AGENT JATAYU", COL_TEXT, 4);
    if ((ticks / (GAME_TICK_HZ / 2)) % 2 == 0) text_centred_shadow(284, "PRESS SET", COL_GOLD, 3);
}

static void draw_ring(float cx, float cy, float r, int thick, uint16_t col) {
    if (cy + r + thick < strip_y0 || cy - r - thick >= strip_y0 + strip_rows) return;
    int steps = (int)(r * 0.9f) + 12;
    for (int i = 0; i < steps; i++) {
        float a = 6.2832f * (float)i / (float)steps;
        fill((int)(cx + cosf(a) * r), (int)(cy + sinf(a) * r), thick, thick, col);
    }
}

// A golden arc from angle a0 to a1 (radians, screen coordinates), drawn up to `frac` of its length.
static void draw_arc(float cx, float cy, float r, float a0, float a1, float frac, int thick) {
    if (cy - r - thick >= strip_y0 + strip_rows || cy + r + thick < strip_y0) return;
    const int steps = 18;
    int n = (int)((float)steps * frac);
    for (int i = 0; i <= n; i++) {
        float a = a0 + (a1 - a0) * (float)i / (float)steps;
        int x = (int)(cx + cosf(a) * r), y = (int)(cy + sinf(a) * r);
        fill(x - 1, y - 1, thick + 2, thick + 2, COL_GOLD);
        fill(x, y, thick, thick, COL_GOLD2);
    }
}

static void draw_swing(void) {
    if (pl.swing == SW_NONE) return;
    int len = pl.swing == SW_DOUBLE ? 9 : 6;
    float frac = (float)(pl.swing_t + 1) * 2.0f / (float)len;
    if (frac > 1) frac = 1;
    float cx = pl.x, cy = pl.y - 6;
    if (pl.swing == SW_LEFT || pl.swing == SW_DOUBLE)
        draw_arc(cx, cy, 48, 3.1416f, 4.7124f, frac, 3);
    if (pl.swing == SW_RIGHT || pl.swing == SW_DOUBLE)
        draw_arc(cx, cy, 48, 0, -1.5708f, frac, 3);
    if (pl.swing == SW_DOUBLE) { // the X slash
        int n = (int)(14.0f * frac);
        for (int i = 0; i <= n; i++) {
            int o = i * 3 - 21;
            fill((int)cx + o, (int)cy - 26 + o, 3, 3, COL_GOLD2);
            fill((int)cx - o, (int)cy - 26 + o, 3, 3, COL_GOLD2);
        }
    }
}

static void draw_enemy(int i) {
    const Enemy *e = &en[i];
    if (!e->alive) return;
    int w = etab[e->type].w, h = etab[e->type].h;
    int bob = (int)(sinf((float)e->t * 0.15f + (float)i) * 2.0f);
    int x0 = (int)e->x - w / 2, y0 = (int)e->y - h / 2 + bob;
    if (y0 + h <= strip_y0 || y0 >= strip_y0 + strip_rows) return;

    int solid = -1, dissolve = 0;
    if (state == ST_INTRO) solid = COL_SILH;
    else if (e->dying) {
        dissolve = (14 - e->dying) * 16 / 14;
        if (e->dying & 1) solid = COL_WHITE;
    } else if (e->flash > 0) solid = COL_WHITE;
    else if (e->type == 2 && e->phase == 1 && (e->ptimer & 1)) solid = COL_GLOW;
    blit_sprite(etab[e->type].spr, w, h, x0, y0, solid, dissolve);
}

static void draw_player(void) {
    if (state == ST_BATTLE && pl.invuln > 0 && !pl.hitflash && (pl.invuln & 2) && pl.dash_t == 0) return; // blink after a hit
    static const uint16_t *const poses[2][3] = {
        {spr_jatayu_f0_l0, spr_jatayu_f0_l1, spr_jatayu_f0_l2},
        {spr_jatayu_f1_l0, spr_jatayu_f1_l1, spr_jatayu_f1_l2},
    };
    int flap = (int)((ticks >> 2) & 1);
    const uint16_t *spr = poses[flap][pl.lean + 1];
    int bob = (int)(sinf((float)ticks * 0.3f) * 2.0f);
    int lunge = pl.swing != SW_NONE && pl.swing_t < 3 ? -4 : 0;
    int solid = pl.hitflash > 0 && (pl.hitflash & 1) ? COL_RED : -1;
    blit_sprite(spr, PLAYER_W, PLAYER_H, (int)pl.x - PLAYER_W / 2, (int)pl.y - PLAYER_H / 2 + bob + lunge, solid, 0);
    // a faint divine aura
    if (pl.burst_t == 0 && state != ST_GAMEOVER)
        draw_ring(pl.x, pl.y, 40 + (float)((ticks >> 2) & 1), 1, RGB565(120, 90, 30));
}

static void draw_bar(int x, int y, int w, int h, int value, int max, uint16_t col) {
    fill(x - 1, y - 1, w + 2, h + 2, COL_HUDLN);
    fill(x, y, w, h, RGB565(30, 8, 18));
    int fw = value * w / max;
    if (fw > 0) fill(x, y, fw, h, col);
}

static void draw_hud(void) {
    fill(0, 0, GAME_W, HUD_H, COL_HUD);
    fill(0, HUD_H - 1, GAME_W, 1, COL_HUDLN);

    // Jatayu icon and health
    blit_sprite(spr_jatayu_icon, SPR_JATAYU_ICON_W, SPR_JATAYU_ICON_H, 6, 6, -1, 0);
    draw_bar(28, 10, 84, 8, pl.hp, MAX_HP, pl.hp > MAX_HP * 3 / 10 ? COL_RED : RGB565(255, 90, 40));
    text(28, 20, "HEALTH", RGB565(150, 110, 100), 1);

    // level and score
    char buf[24];
    text_centred(4, "LEVEL 1", COL_GOLD, 1);
    memcpy(buf, "SCORE ", 6);
    num_str(buf + 6, pl.score, 5);
    text_centred(15, buf, COL_TEXT, 1);

    // Rakshasa counter and divine energy
    memcpy(buf, "RAKSHASA ", 9);
    buf[9] = (char)('0' + defeated);
    memcpy(buf + 10, "/7", 3);
    text(GAME_W - 100, 4, buf, COL_TEXT, 1);
    draw_bar(GAME_W - 100, 15, 90, 6, pl.energy, MAX_ENERGY, pl.energy >= BURST_COST ? COL_GOLD : RGB565(120, 90, 30));
    fill(GAME_W - 100 + BURST_COST * 90 / MAX_ENERGY, 14, 1, 8, COL_HUDLN);
}

static void draw_battle_scene(void) {
    // Copy only the columns that can have changed: the background is read from slow flash.
    uint32_t m = tiles_out[strip_y0 / GAME_STRIP_ROWS];
    int lo = m ? __builtin_ctz(m) : 0, hi = m ? 31 - __builtin_clz(m) : TILES - 1;
    for (int r = 0; r < strip_rows; r++)
        memcpy(strip + r * GAME_W + lo * GAME_TILE_W, bg_forest + (size_t)(strip_y0 + r) * BG_W + lo * GAME_TILE_W,
               (size_t)(hi - lo + 1) * GAME_TILE_W * sizeof *strip);

    for (int i = 0; i < N_EMBERS; i++) {
        int x = (int)ember_x[i], y = (int)ember_y[i];
        fill(x, y, 2, 2, (i & 1) ? COL_EMBER : COL_GOLD);
    }

    for (int i = 0; i < N_EN; i++) draw_enemy(i);

    for (int i = 0; i < MAX_ORBS; i++) {
        if (!orbs[i].alive) continue;
        int x = (int)orbs[i].x, y = (int)orbs[i].y;
        fill(x - 5, y - 3, 10, 6, COL_ORB);
        fill(x - 3, y - 5, 6, 10, COL_ORB);
        fill(x - 2, y - 2, 4, 4, COL_ORB2);
    }

    draw_player();
    draw_swing();
    if (pl.burst_t > 0) {
        float r = (float)pl.burst_t * 8.0f;
        draw_ring(pl.x, pl.y, r, 4, COL_GOLD);
        draw_ring(pl.x, pl.y, r - 6, 2, COL_GOLD2);
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &parts[i];
        if (p->life > 0) fill((int)p->x, (int)p->y, p->size, p->size, p->col);
    }
}

// One line of the end-of-level score: "LABEL        +00600".
static void result_line(int y, const char *label, int value) {
    char buf[32];
    int n = (int)strlen(label);
    memcpy(buf, label, (size_t)n);
    while (n < 15) buf[n++] = ' ';
    buf[n++] = '+';
    num_str(buf + n, value, 5);
    text_centred_shadow(y, buf, COL_WHITE, 2);
}

void game_draw(uint16_t *buf, int y0, int rows) {
    strip = buf;
    strip_y0 = y0;
    strip_rows = rows;

    if (state == ST_TITLE) {
        draw_title();
        return;
    }

    draw_battle_scene();

    if (state == ST_VICTORY) {
        int lvl = state_t >= 44 ? 2 : (state_t >= 30 ? 1 : 0);
        if (lvl) tint_gold(lvl);
        // Jatayu and his claws stay bright over the golden light
        if (lvl) {
            draw_player();
            draw_ring(pl.x, pl.y, 34 + (float)((state_t * 2) % 40), 2, COL_WHITE);
            for (int i = 0; i < MAX_PARTICLES; i++) {
                const Particle *p = &parts[i];
                if (p->life > 0) fill((int)p->x, (int)p->y, p->size, p->size, p->col);
            }
        }
    } else if (state == ST_GAMEOVER) {
        int lvl = state_t >= 24 ? 2 : (state_t >= 10 ? 1 : 0);
        if (lvl) tint_dark(lvl);
    }

    if (y0 < HUD_H) draw_hud();

    if (state == ST_INTRO && state_t < INTRO_TICKS - 8) {
        text_centred_shadow(70, "LEVEL 1", COL_GOLD, 4);
        text_centred_shadow(112, "THE RAKSHASA ASSAULT", COL_TEXT, 2);
    } else if (state == ST_VICTORY) {
        if (state_t >= 40 && state_t < 100) text_centred_shadow(120, "DHARMA PREVAILS!", COL_WHITE, 4);
        if (state_t >= 100) {
            char label[20], buf[24];
            text_centred_shadow(36, "LEVEL 1 COMPLETE", COL_WHITE, 3);
            result_line(72, "KILLS", pl.score);
            snprintf(label, sizeof label, "TIME %dS", battle_ticks / GAME_TICK_HZ);
            result_line(96, label, bonus_time);
            snprintf(label, sizeof label, "ATTACKS %d", pl.attacks);
            result_line(120, label, bonus_attacks);
            snprintf(label, sizeof label, "HEALTH %d", pl.hp);
            result_line(144, label, bonus_health);
            memcpy(buf, "TOTAL ", 6);
            num_str(buf + 6, final_score, 5);
            text_centred_shadow(178, buf, COL_GOLD, 3);
            text_centred_shadow(226, "LEVEL 2 COMING SOON", COL_GOLD, 2);
            if ((ticks / (GAME_TICK_HZ / 2)) % 2 == 0) text_centred_shadow(264, "SET: PLAY AGAIN", COL_WHITE, 2);
        }
    } else if (state == ST_BATTLE && wave == 2 && wave_t < BANNER_TICKS) {
        text_centred_shadow(76, "WAVE 2", COL_GOLD, 4);
    } else if (state == ST_GAMEOVER && state_t >= 36) {
        text_centred_shadow(100, "GAME OVER", COL_RED, 5);
        if ((ticks / (GAME_TICK_HZ / 2)) % 2 == 0) text_centred_shadow(190, "SET: TRY AGAIN", COL_WHITE, 2);
    }
}

// ---- Dirty tiles -----------------------------------------------------------------------
// Redrawing the whole 480x320 frame (the background is read from flash) is too slow for
// 30 ticks a second, so the game says which tiles could have changed: everything that
// moved this tick or the last one, and the parts of the HUD whose numbers changed.

static void mark(float x0, float y0, float x1, float y1) {
    if (x1 < 0 || x0 >= GAME_W || y1 < 0 || y0 >= GAME_H) return;
    int s0 = (int)floorf(y0 / GAME_STRIP_ROWS), s1 = (int)floorf(y1 / GAME_STRIP_ROWS);
    int t0 = (int)floorf(x0 / GAME_TILE_W), t1 = (int)floorf(x1 / GAME_TILE_W);
    if (s0 < 0) s0 = 0;
    if (s1 >= STRIPS) s1 = STRIPS - 1;
    if (t0 < 0) t0 = 0;
    if (t1 >= TILES) t1 = TILES - 1;
    uint32_t m = ((1u << (t1 - t0 + 1)) - 1) << t0;
    for (int s = s0; s <= s1; s++) tiles_now[s] |= m;
}

static void mark_all(void) {
    for (int s = 0; s < STRIPS; s++) tiles_now[s] = ALL_TILES;
}

static void battle_dirty(void) {
    static int hud_hp = -1, hud_score, hud_energy, hud_defeated;
    if (pl.hp != hud_hp) mark(0, 0, 130, HUD_H);
    if (pl.score != hud_score) mark(150, 0, 330, HUD_H);
    if (pl.energy != hud_energy || defeated != hud_defeated) mark(370, 0, GAME_W - 1, HUD_H);
    hud_hp = pl.hp;
    hud_score = pl.score;
    hud_energy = pl.energy;
    hud_defeated = defeated;

    for (int i = 0; i < N_EMBERS; i++) mark(ember_x[i] - 1, ember_y[i] - 1, ember_x[i] + 3, ember_y[i] + 3);
    for (int i = 0; i < N_EN; i++)
        if (en[i].alive)
            mark(en[i].x - etab[en[i].type].w / 2 - 4, en[i].y - etab[en[i].type].h / 2 - 6,
                 en[i].x + etab[en[i].type].w / 2 + 4, en[i].y + etab[en[i].type].h / 2 + 6);
    if (wave == 2 && wave_t < BANNER_TICKS) mark(110, 70, 370, 112); // the wave banner
    for (int i = 0; i < MAX_ORBS; i++)
        if (orbs[i].alive) mark(orbs[i].x - 7, orbs[i].y - 7, orbs[i].x + 7, orbs[i].y + 7);
    for (int i = 0; i < MAX_PARTICLES; i++)
        if (parts[i].life > 0) mark(parts[i].x - 2, parts[i].y - 2, parts[i].x + 6, parts[i].y + 6);
    mark(pl.x - 50, pl.y - 50, pl.x + 50, pl.y + 50);
    if (pl.swing != SW_NONE) mark(pl.x - 72, pl.y - 76, pl.x + 72, pl.y + 14);
    if (pl.burst_t > 0) mark(pl.x - 112, pl.y - 112, pl.x + 112, pl.y + 112);
}

void game_update(uint8_t held, uint8_t pressed) {
    step(held, pressed);

    memset(tiles_now, 0, sizeof tiles_now);
    if (force_all || state == ST_VICTORY || state == ST_GAMEOVER) {
        mark_all();
        force_all = 0;
    } else if (state == ST_TITLE) {
        if (ticks % (GAME_TICK_HZ / 2) == 0) mark(150, 284, 330, 312); // the blinking prompt
    } else {
        battle_dirty();
        if (state == ST_INTRO) mark(110, 66, 370, 132); // the level title
    }
    for (int s = 0; s < STRIPS; s++) {
        tiles_out[s] = tiles_now[s] | tiles_prev[s];
        tiles_prev[s] = tiles_now[s];
    }
}

const uint32_t *game_dirty_tiles(void) { return tiles_out; }
