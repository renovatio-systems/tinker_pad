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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "font5x7.h"
#include "jatayu_img.h"
#include "l2_sprites.h"
#include "l3_assets.h"

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

// ---- Level 2: The Boxing Ring ----------------------------------------------------------
// Jatayu fights three Rakshasas one after the other, on the ground, Street Fighter style.
//   LEFT / RIGHT  walk            UP  jump
//   DOWN (hold)   shield, takes a quarter of the damage
//   MID           chakra from his beak, knocks back what it hits
//   SET           trident thrown from his chest (costs energy)
//   RST           back to the title

#define L2_GROUND       310
#define L2_WALK         4.0f
#define L2_JUMP         (-11.5f)
#define L2_GRAVITY      0.75f
#define L2_HP           200
#define L2_ENERGY       100
#define L2_CANNON_COST  10
#define L2_BANNER       90 // ticks the next enemy and its power are shown
#define L2_PROJS        16
#define L2_BOX_HALF     28 // Jatayu's hit box: 56 wide, 100 tall, standing on his feet
#define L2_BOX_H        100

enum { PJ_BEAM, PJ_ORB, PJ_DROP, PJ_WAVE, PJ_PCHAKRA }; // PJ_BEAM is Jatayu's trident, PJ_PCHAKRA his punch chakra
enum { FOE_SHADOW, FOE_BLOOD, FOE_COMMANDER };
enum { FOE_WALK, FOE_HIDE, FOE_CHARGE, FOE_WIND };
enum { L2_BANNER_ST, L2_FIGHT, L2_VICTORY, L2_LOST };

// w and h are the sprite. bw is the body the punches and hits land on, narrower than the
// sprite where the sprite has a spear or lightning.
static const struct { const char *name, *power; int w, h, bw, hp; const uint16_t *spr; } l2_foe[3] = {
    {"SHADOW SPEARMAN", "POWER: SHADOW STEP", L2_SHADOW_SPEARMAN_W, L2_SHADOW_SPEARMAN_H, 56, 60, l2_shadow_spearman},
    {"BLOOD WARRIOR", "POWER: BLOOD RAGE", L2_BLOOD_WARRIOR_W, L2_BLOOD_WARRIOR_H, 62, 80, l2_blood_warrior},
    {"DEMON COMMANDER", "POWER: THUNDER ROAR", L2_DEMON_COMMANDER_W, L2_DEMON_COMMANDER_H, 84, 120, l2_demon_commander},
};

typedef struct {
    float x, y, vx, vy;
    int kind, life, alive;
    int hit; // it has connected: it stops and stays on screen for a few ticks
} L2Proj;

static struct {
    float x, y, vy; // x is the centre, y the feet
    int hp, energy, facing, on_ground, shielding;
    int invuln, cd_punch, cd_cannon, energy_t;
} l2p;

static struct {
    float x, y, vx; // x is the centre, y the feet
    float recoil;   // pushed back by a punch, eases to zero
    int type, state, t, cd, hp, dying, visible, hit_done, cd_touch;
} l2e;

static L2Proj l2_proj[L2_PROJS];
static int l2_on, l2_stage, l2_state, l2_t, l2_score;
static int l3_on; // Level 3 (Stage 3A) is running
static int l4_on, l4_t; // the Level 4 placeholder screen
static int l2_prev_hp = -1, l2_prev_energy = -1, l2_prev_score = -1, l2_prev_foe = -1;

static void level2_start(void);
static void l2_step(uint8_t held, uint8_t pressed);
static void l2_draw(void);
static void l2_dirty(void);
static void level3_start(void);
static void level4_start(void);
static void l3_step(uint8_t held, uint8_t pressed);
static void l3_draw(void);
static void l3_dirty(void);

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
static void blit_sprite_flip(const uint16_t *spr, int w, int h, int x0, int y0, int solid, int dissolve, int flip);

static void blit_sprite(const uint16_t *spr, int w, int h, int x0, int y0, int solid, int dissolve) {
    blit_sprite_flip(spr, w, h, x0, y0, solid, dissolve, 0);
}

// The same, mirrored left to right when `flip` is set, so a sprite drawn facing right can face left.
static void blit_sprite_flip(const uint16_t *spr, int w, int h, int x0, int y0, int solid, int dissolve, int flip) {
    int ya = y0 > strip_y0 ? y0 : strip_y0;
    int yb = y0 + h < strip_y0 + strip_rows ? y0 + h : strip_y0 + strip_rows;
    for (int y = ya; y < yb; y++) {
        const uint16_t *src = spr + (y - y0) * w;
        uint16_t *row = strip + (y - strip_y0) * GAME_W;
        for (int i = 0; i < w; i++) {
            int x = x0 + i;
            if (x < 0 || x >= GAME_W) continue;
            uint16_t c = src[flip ? w - 1 - i : i];
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

#define TITLE_SET_WINDOW (GAME_TICK_HZ * 12 / 10) // ticks to wait after a SET press on the title (1.2 s)
static int title_set_t; // ticks left before the title's SET presses are acted on
static int title_set_n; // SET presses on the title so far
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
    l2_on = 0;
    l3_on = 0;
    l4_on = 0;
    title_set_t = 0;
    title_set_n = 0;
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
        // SET presses are counted. Once 0.6 s pass without another press: 1 starts level 1,
        // 2 starts level 2, 3 starts level 3 and 4 starts the Level 4 placeholder.
        if (pressed & JOY_SET) {
            title_set_n++;
            title_set_t = TITLE_SET_WINDOW;
        }
        if (title_set_t && --title_set_t == 0) {
            int n = title_set_n;
            title_set_n = 0;
            if (n == 4) level4_start();
            else if (n == 3) level3_start();
            else if (n == 2) level2_start();
            else if (n == 1) reset_level();
        }
        return;
    }

    if (l4_on) {
        l4_t++;
        if (l4_t >= 30 && (pressed & JOY_SET)) game_init();
        return;
    }
    if (l3_on) {
        l3_step(held, pressed);
        return;
    }
    if (l2_on) {
        l2_step(held, pressed);
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
        if (state_t >= 100 && (pressed & JOY_SET)) level2_start();
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
    if (l4_on) {
        memset(strip, 0, (size_t)strip_rows * GAME_W * sizeof *strip);
        text_centred_shadow(110, "LEVEL 4", COL_GOLD, 4);
        text_centred_shadow(170, "COMING SOON", COL_WHITE, 3);
        if (((ticks / (GAME_TICK_HZ / 2)) % 2) == 0) text_centred_shadow(250, "SET: BACK TO TITLE", COL_TEXT, 2);
        return;
    }
    if (l3_on) {
        l3_draw();
        return;
    }
    if (l2_on) {
        l2_draw();
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
            text_centred_shadow(226, "NEXT: THE BOXING RING", COL_GOLD, 2);
            if ((ticks / (GAME_TICK_HZ / 2)) % 2 == 0) text_centred_shadow(264, "SET: LEVEL 2", COL_WHITE, 2);
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


// ---- Level 2: helpers and state --------------------------------------------------------

static void l2_foe_box(float *x0, float *y0, float *x1, float *y1) {
    int bw = l2_foe[l2e.type].bw, h = l2_foe[l2e.type].h;
    *x0 = l2e.x - bw / 2;
    *x1 = l2e.x + bw / 2;
    *y0 = l2e.y - h;
    *y1 = l2e.y;
}

static int l2_hits_jatayu(float x0, float y0, float x1, float y1) {
    return overlap(x0, y0, x1, y1, l2p.x - L2_BOX_HALF, l2p.y - L2_BOX_H, l2p.x + L2_BOX_HALF, l2p.y);
}

static void l2_spawn_proj(int kind, float x, float y, float vx, float vy, int life) {
    for (int i = 0; i < L2_PROJS; i++) {
        if (l2_proj[i].alive) continue;
        l2_proj[i] = (L2Proj){x, y, vx, vy, kind, life, 1};
        return;
    }
}

static void l2_hurt_jatayu(int dmg) {
    if (l2_state != L2_FIGHT || l2p.invuln) return;
    if (l2p.shielding) dmg /= 4;
    l2p.hp -= dmg;
    l2p.invuln = 24;
    if (l2p.hp <= 0) {
        l2p.hp = 0;
        l2_state = L2_LOST;
        l2_t = 0;
    }
}

static void l2_hurt_foe(int dmg, float x, float y) {
    if (l2e.dying) return;
    l2e.hp -= dmg;
    if (l2e.hp <= 0) {
        l2e.hp = 0;
        l2e.dying = 40;
        l2_score += 200;
        burst(l2e.x, l2e.y - 30, 14, 3.5f, 22, COL_GOLD, COL_EMBER, 3, 0.1f);
    }
}

static void l2_spawn_foe(int type) {
    memset(&l2e, 0, sizeof l2e);
    l2e.type = type;
    l2e.hp = l2_foe[type].hp;
    l2e.x = 360;
    l2e.y = L2_GROUND;
    l2e.visible = 1;
    l2e.cd = 60;
}

static void level2_start(void) {
    state = ST_BATTLE; // leave the title, so the title code stops running
    memset(&l2p, 0, sizeof l2p);
    l2p.x = 240;
    l2p.y = L2_GROUND;
    l2p.hp = L2_HP;
    l2p.energy = 50;
    l2p.facing = 1;
    l2p.on_ground = 1;
    memset(l2_proj, 0, sizeof l2_proj);
    l2_on = 1;
    l2_stage = 0;
    l2_score = 0;
    l2_t = 0;
    l2_state = L2_BANNER_ST;
    l2_spawn_foe(FOE_SHADOW);
    l2_prev_hp = l2_prev_energy = l2_prev_score = l2_prev_foe = -1;
    force_all = 1;
}

// ---- Jatayu ----------------------------------------------------------------------------

static void l2_player(uint8_t held, uint8_t pressed, int can_fight) {
    if (l2p.invuln) l2p.invuln--;
    if (l2p.cd_punch) l2p.cd_punch--;
    if (l2p.cd_cannon) l2p.cd_cannon--;
    if (++l2p.energy_t >= 6) {
        l2p.energy_t = 0;
        if (l2p.energy < L2_ENERGY) l2p.energy++;
    }

    // Walking and jumping. Holding DOWN on the ground raises the shield and stops him moving.
    l2p.shielding = can_fight && l2p.on_ground && (held & JOY_DOWN) ? 1 : 0;
    if (!l2p.shielding) {
        if (held & JOY_LEFT) l2p.x -= L2_WALK;
        if (held & JOY_RIGHT) l2p.x += L2_WALK;
        if ((pressed & JOY_UP) && l2p.on_ground) {
            l2p.vy = L2_JUMP;
            l2p.on_ground = 0;
        }
    }
    l2p.x = clampf(l2p.x, 70, GAME_W - 70);
    l2p.facing = l2e.x >= l2p.x ? 1 : -1; // always face the Rakshasa
    l2p.vy += L2_GRAVITY;
    l2p.y += l2p.vy;
    if (l2p.y >= L2_GROUND) {
        l2p.y = L2_GROUND;
        l2p.vy = 0;
        l2p.on_ground = 1;
    }

    if (!can_fight) return;

    // MID: punch. A chakra flies from his beak and knocks back whatever it hits.
    if ((pressed & JOY_MID) && !l2p.shielding && l2p.cd_punch == 0) {
        l2p.cd_punch = 18;
        l2_spawn_proj(PJ_PCHAKRA, l2p.x + l2p.facing * 31, l2p.y - L2_JATAYU_H + 24, l2p.facing * 10.0f, 0, 36);
    }

    // SET: the trident, thrown from his chest.
    if ((pressed & JOY_SET) && !l2p.shielding && l2p.cd_cannon == 0 && l2p.energy >= L2_CANNON_COST) {
        l2p.energy -= L2_CANNON_COST;
        l2p.cd_cannon = 12;
        l2_spawn_proj(PJ_BEAM, l2p.x + l2p.facing * 21, l2p.y - L2_JATAYU_H * 55 / 100, l2p.facing * 9.0f, 0, 45);
    }
}

// ---- The Rakshasas ---------------------------------------------------------------------

static void l2_foe_ai(void) {
    int t = l2e.type;
    float dir = l2p.x < l2e.x ? -1.0f : 1.0f; // towards Jatayu
    float dist = fabsf(l2p.x - l2e.x);
    if (l2e.cd_touch) l2e.cd_touch--;
    if (l2e.recoil != 0) {
        l2e.x = clampf(l2e.x + l2e.recoil, 30, GAME_W - 30);
        l2e.recoil *= 0.7f;
        if (fabsf(l2e.recoil) < 0.3f) l2e.recoil = 0;
    }

    switch (l2e.state) {
    case FOE_WALK: {
        static const float speed[3] = {1.6f, 1.2f, 0.8f};
        if (dist > 60) l2e.x += dir * speed[t];
        if (--l2e.cd <= 0) {
            l2e.t = 0;
            if (t == FOE_SHADOW) {
                l2e.state = FOE_HIDE;
                l2e.visible = 0;
            } else if (t == FOE_BLOOD) {
                l2e.state = FOE_CHARGE;
                l2e.vx = dir * 7.0f;
                l2e.hit_done = 0;
            } else {
                l2e.state = FOE_WIND;
            }
        }
        break;
    }
    case FOE_HIDE:
        // Shadow Step: reappear beside Jatayu and throw a dark orb back at him.
        if (++l2e.t >= 14) {
            l2e.x = clampf(l2p.x - dir * 50.0f, 40, GAME_W - 40);
            l2e.visible = 1;
            l2e.state = FOE_WALK;
            l2e.cd = 100;
            // The chakra flies from the Spearman's eyes, near the top of his head.
            l2_spawn_proj(PJ_ORB, l2e.x + dir * 14, l2e.y - l2_foe[FOE_SHADOW].h + 22, dir * 5.5f, 0, 120);
        }
        break;
    case FOE_CHARGE: {
        // Blood Rage: a charge across the ring, then blood rains down.
        l2e.x = clampf(l2e.x + l2e.vx, 30, GAME_W - 30);
        l2e.t++;
        float x0, y0, x1, y1;
        l2_foe_box(&x0, &y0, &x1, &y1);
        if (!l2e.hit_done && l2_hits_jatayu(x0, y0, x1, y1)) {
            l2_hurt_jatayu(14);
            l2e.hit_done = 1;
        }
        if (l2e.t >= 24 || l2e.hit_done) {
            for (int i = 0; i < 4; i++) l2_spawn_proj(PJ_DROP, 30 + rnd() % 420, 30, 0, 3.6f, 150);
            l2e.state = FOE_WALK;
            l2e.cd = 90;
        }
        break;
    }
    case FOE_WIND:
        // Thunder Roar: winds up, then a shockwave runs along the floor. Jump over it.
        if (++l2e.t >= 30) {
            l2_spawn_proj(PJ_WAVE, l2e.x + dir * 20, L2_GROUND - 16, dir * 6.5f, 0, 150);
            l2e.state = FOE_WALK;
            l2e.cd = 120;
        }
        break;
    }
}

// Touching a Rakshasa hurts, except while Blood Warrior is mid-charge (that has its own hit).
static void l2_foe_touch(void) {
    if (!l2e.visible || l2e.dying || l2e.cd_touch || l2e.state == FOE_HIDE || l2e.state == FOE_CHARGE) return;
    float x0, y0, x1, y1;
    l2_foe_box(&x0, &y0, &x1, &y1);
    if (!l2_hits_jatayu(x0, y0, x1, y1)) return;
    static const int touch[3] = {8, 10, 12};
    l2_hurt_jatayu(touch[l2e.type]);
    l2e.cd_touch = 30;
}

// ---- Projectiles -----------------------------------------------------------------------

// A projectile that connects stops where it hit and stays on screen for a few ticks, so the hit
// is always seen, even when the Rakshasa is right next to Jatayu.
static void l2_proj_hit(L2Proj *p) {
    p->hit = 1;
    p->vx = p->vy = 0;
    p->life = 3;
}

static void l2_projectiles(void) {
    for (int i = 0; i < L2_PROJS; i++) {
        L2Proj *p = &l2_proj[i];
        if (!p->alive) continue;
        if (p->hit) { // stopped where it connected, so it can't hit twice
            if (--p->life <= 0) p->alive = 0;
            continue;
        }
        p->x += p->vx;
        p->y += p->vy;
        if (--p->life <= 0 || p->x < -20 || p->x > GAME_W + 20 || p->y > GAME_H) {
            p->alive = 0;
            continue;
        }
        switch (p->kind) {
        case PJ_BEAM: // the trident: a hit ends it
        case PJ_PCHAKRA: { // the punch chakra: a hit knocks the Rakshasa back
            float x0, y0, x1, y1;
            float r = p->kind == PJ_BEAM ? 14 : 8;
            if (l2e.visible && !l2e.dying) {
                l2_foe_box(&x0, &y0, &x1, &y1);
                if (overlap(p->x - r, p->y - r, p->x + r, p->y + r, x0, y0, x1, y1)) {
                    l2_hurt_foe(p->kind == PJ_BEAM ? 6 : 4, p->x, p->y);
                    l2_score += p->kind == PJ_BEAM ? 10 : 20;
                    if (p->kind == PJ_PCHAKRA) l2e.recoil = (p->vx > 0 ? 1.0f : -1.0f) * 6.0f;
                    l2_proj_hit(p);
                }
            }
            break;
        }
        case PJ_ORB:
            if (l2_hits_jatayu(p->x - 7, p->y - 7, p->x + 7, p->y + 7)) {
                l2_hurt_jatayu(10);
                l2_proj_hit(p);
            }
            break;
        case PJ_DROP:
            if (p->y >= L2_GROUND) {
                burst(p->x, L2_GROUND, 2, 1.0f, 8, RGB565(200, 20, 30), COL_EMBER, 1, 0);
                l2_proj_hit(p);
            } else if (l2_hits_jatayu(p->x - 1, p->y, p->x + 1, p->y + 4)) {
                l2_hurt_jatayu(6);
                l2_proj_hit(p);
            }
            break;
        case PJ_WAVE:
            if (l2_hits_jatayu(p->x, L2_GROUND - 22, p->x + 22, L2_GROUND)) {
                l2_hurt_jatayu(16);
                l2_proj_hit(p);
            }
            break;
        }
    }
}

// ---- Turn by turn ----------------------------------------------------------------------

static void l2_step(uint8_t held, uint8_t pressed) {
    update_particles();
    l2_t++;

    switch (l2_state) {
    case L2_BANNER_ST:
        l2_player(held, pressed, 0);
        if (l2_t >= L2_BANNER) {
            l2_state = L2_FIGHT;
            l2_t = 0;
        }
        break;
    case L2_FIGHT:
        l2_player(held, pressed, 1);
        if (l2e.dying) {
            l2e.dying--;
            if (l2e.dying % 6 == 0) burst(l2e.x + frand(-20, 20), l2e.y - 30 - frand(0, 30), 4, 2.0f, 14, COL_GOLD, COL_WHITE, 2, 0);
            if (l2e.dying == 0) {
                // Next Rakshasa, or the end of the level. Stray projectiles are cleared.
                memset(l2_proj, 0, sizeof l2_proj);
                if (++l2_stage < 3) {
                    l2_spawn_foe(l2_stage);
                    l2_state = L2_BANNER_ST;
                    l2_t = 0;
                } else {
                    l2_score += l2p.hp * 5;
                    l2_state = L2_VICTORY;
                    l2_t = 0;
                }
            }
        } else {
            l2_foe_ai();
            l2_foe_touch();
        }
        l2_projectiles();
        break;
    case L2_VICTORY:
        if (l2_t >= 60 && (pressed & JOY_SET)) level3_start();
        break;
    case L2_LOST:
        if (l2_t >= 36 && (pressed & JOY_SET)) level2_start();
        break;
    }
}


// ---- Level 2: drawing ------------------------------------------------------------------

// A plain dark night: a sky that fades from near black to deep blue, a few stars and a dark
// floor with a gold edge. It is drawn in code each time, so there is no picture stored in RAM.
static const struct { int x, y; } l2_stars[] = {
    {30, 50}, {95, 120}, {160, 35}, {230, 85}, {300, 60}, {372, 30}, {440, 110}, {70, 200}, {410, 190}, {260, 150},
};

static void l2_arena(void) {
    for (int y = strip_y0; y < strip_y0 + strip_rows; y++) {
        if (y < L2_GROUND) {
            int t = y * 40 / L2_GROUND; // 0 at the top, 39 at the floor
            fill(0, y, GAME_W, 1, RGB565(4 + t / 4, 6 + t / 3, 20 + t));
        } else {
            fill(0, y, GAME_W, 1, RGB565(18, 16, 28));
        }
    }
    for (int i = 0; i < (int)(sizeof l2_stars / sizeof l2_stars[0]); i++)
        fill(l2_stars[i].x, l2_stars[i].y, 2, 2, RGB565(200, 200, 230));
    fill(0, L2_GROUND, GAME_W, 2, RGB565(90, 70, 40));
}

static void l2_hud(void) {
    fill(0, 0, GAME_W, HUD_H, COL_HUD);
    fill(0, HUD_H - 1, GAME_W, 1, COL_HUDLN);

    blit_sprite(spr_jatayu_icon, SPR_JATAYU_ICON_W, SPR_JATAYU_ICON_H, 6, 4, -1, 0);
    draw_bar(28, 6, 84, 8, l2p.hp, L2_HP, l2p.hp > L2_HP * 3 / 10 ? COL_RED : RGB565(255, 90, 40));
    draw_bar(28, 20, 84, 4, l2p.energy, L2_ENERGY, COL_GOLD);

    text_centred(4, "LEVEL 2", COL_GOLD, 1);
    char buf[24];
    memcpy(buf, "SCORE ", 6);
    num_str(buf + 6, l2_score, 5);
    text_centred(15, buf, COL_TEXT, 1);

    // The Rakshasa being fought, and how many are left.
    char foe[24];
    snprintf(foe, sizeof foe, "%d/3 %s", l2_stage + 1, l2_foe[l2e.type].name);
    text(GAME_W - 6 * (int)strlen(foe) - 6, 4, foe, COL_TEXT, 1);
    draw_bar(GAME_W - 104, 16, 98, 6, l2e.hp, l2_foe[l2e.type].hp, RGB565(255, 90, 40));
}

// A blue beak, about the size of Jatayu's: thick at the base, hooked at the tip, pointing the
// way it flies (dir is +1 or -1). It is drawn in bright blue so it stands out from the gold scene.
static void l2_draw_beak(int x, int y, int dir) {
    static const int half[14] = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1, 1};
    const uint16_t blue = RGB565(40, 120, 255), light = RGB565(160, 215, 255), dark = RGB565(15, 40, 130);
    for (int i = 0; i < 14; i++) {
        int px = dir > 0 ? x - 7 + i : x + 6 - i;
        fill(px, y - half[i], 1, 2 * half[i] + 1, blue);
    }
    for (int i = 0; i < 8; i++) fill(dir > 0 ? x - 7 + i : x + 6 - i, y - 2, 1, 1, light); // shine on the upper edge
    fill(dir > 0 ? x + 6 : x - 7, y, 1, 1, dark); // the hooked tip
}

// Thunder Roar: a wall of fire along the floor. Its flames shift every tick, so it looks alive.
static void l2_draw_fire(int x) {
    const uint16_t red = RGB565(220, 40, 10), orange = RGB565(255, 130, 20), yellow = RGB565(255, 230, 90);
    for (int i = 0; i < 22; i++) {
        int h = 8 + (int)((i * 7 + ticks * 3 + i * i) % 14); // 8 to 21 px tall, ragged
        fill(x + i, L2_GROUND - h, 1, h, red);
        if (i >= 3 && i <= 18) fill(x + i, L2_GROUND - h * 2 / 3, 1, h * 2 / 3, orange);
        if (i >= 6 && i <= 15) fill(x + i, L2_GROUND - h / 3, 1, h / 3, yellow);
    }
}

static void l2_draw_foe(void) {
    if (!l2e.visible && !l2e.dying) return;
    const uint16_t *spr = l2_foe[l2e.type].spr;
    int w = l2_foe[l2e.type].w, h = l2_foe[l2e.type].h;
    int solid = -1, dissolve = 0;
    if (l2e.dying) dissolve = (40 - l2e.dying) * 16 / 40;
    if (l2e.state == FOE_WIND) solid = COL_GLOW; // steady glow while winding up
    // The art faces right, so a Rakshasa to the right of Jatayu is mirrored to face him.
    blit_sprite_flip(spr, w, h, (int)(l2e.x - w / 2), (int)(l2e.y - h), solid, dissolve, l2p.x < l2e.x);
}

static void l2_draw_player(void) {
    blit_sprite_flip(l2_jatayu, L2_JATAYU_W, L2_JATAYU_H, (int)l2p.x - L2_JATAYU_W / 2, (int)l2p.y - L2_JATAYU_H,
                     -1, 0, l2p.facing < 0);
    if (l2p.shielding) draw_ring(l2p.x, l2p.y - 62, 66, 3, COL_GOLD);
}

// A trident, about 60 px long: a shaft, a binding and three prongs pointing the way the shot
// travels (dir is +1 or -1).
static void hspan(int x0, int x1, int y, int h, uint16_t c) {
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    fill(x0, y, x1 - x0, h, c);
}

static void l2_draw_trident(int x, int y, int dir) {
    const uint16_t gold = RGB565(255, 200, 60), white = RGB565(255, 244, 200), dark = RGB565(150, 90, 30);
    int tip = x + 10 * dir;
    hspan(x - 24 * dir, tip, y - 1, 3, dark);
    hspan(x - 24 * dir, tip, y, 1, gold);
    fill(tip - 2, y - 11, 4, 22, gold);
    hspan(tip, tip + 18 * dir, y - 1, 3, gold);
    hspan(tip, tip + 14 * dir, y - 9, 3, gold);
    hspan(tip, tip + 14 * dir, y + 7, 3, gold);
    hspan(tip + 18 * dir, tip + 16 * dir, y - 2, 5, white);
    hspan(tip + 14 * dir, tip + 12 * dir, y - 10, 5, white);
    hspan(tip + 14 * dir, tip + 12 * dir, y + 6, 5, white);
}

static void l2_draw_projectiles(void) {
    for (int i = 0; i < L2_PROJS; i++) {
        const L2Proj *p = &l2_proj[i];
        if (!p->alive) continue;
        int x = (int)p->x, y = (int)p->y;
        switch (p->kind) {
        case PJ_BEAM:
            l2_draw_trident(x, y, p->vx >= 0 ? 1 : -1);
            break;
        case PJ_PCHAKRA:
            l2_draw_beak(x, y, p->vx >= 0 ? 1 : -1);
            break;
        case PJ_ORB:
            l2_draw_beak(x, y, p->vx >= 0 ? 1 : -1);
            break;
        case PJ_DROP:
            fill(x, y, 2, 4, RGB565(200, 20, 30));
            break;
        case PJ_WAVE:
            l2_draw_fire(x);
            break;
        }
    }
}

static void l2_draw(void) {
    l2_arena();
    l2_draw_foe();
    l2_draw_player();
    l2_draw_projectiles();
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &parts[i];
        if (p->life > 0) fill((int)p->x, (int)p->y, p->size, p->size, p->col);
    }
    if (strip_y0 < HUD_H) l2_hud();

    if (l2_state == L2_BANNER_ST) {
        char num[24];
        snprintf(num, sizeof num, "ENEMY %d OF 3", l2_stage + 1);
        text_centred_shadow(40, num, COL_TEXT, 2);
        text_centred_shadow(70, l2_foe[l2e.type].name, COL_GOLD, 3);
        text_centred_shadow(118, l2_foe[l2e.type].power, COL_TEXT, 2);
    } else if (l2_state == L2_VICTORY) {
        tint_gold(l2_t >= 30 ? 2 : 1);
        text_centred_shadow(60, "LEVEL 2 COMPLETE", COL_WHITE, 3);
        char buf[24];
        memcpy(buf, "SCORE ", 6);
        num_str(buf + 6, l2_score, 6);
        text_centred_shadow(120, buf, COL_GOLD, 4);
        if ((ticks / (GAME_TICK_HZ / 2)) % 2 == 0) text_centred_shadow(240, "SET: LEVEL 3", COL_WHITE, 2);
    } else if (l2_state == L2_LOST) {
        tint_dark(l2_t >= 24 ? 2 : 1);
        if (l2_t >= 36) {
            text_centred_shadow(100, "GAME OVER", COL_RED, 5);
            if ((ticks / (GAME_TICK_HZ / 2)) % 2 == 0) text_centred_shadow(190, "SET: TRY AGAIN", COL_WHITE, 2);
        }
    }
}

// Only the parts of the screen that changed are sent. Everything that moves is marked here;
// the previous tick's marks are added by the caller, which clears the old positions.
static void l2_dirty(void) {
    if (l2_state == L2_VICTORY || l2_state == L2_LOST) {
        mark_all();
        return;
    }
    if (l2p.hp != l2_prev_hp || l2p.energy != l2_prev_energy || l2_score != l2_prev_score || l2e.hp != l2_prev_foe) {
        mark(0, 0, GAME_W, HUD_H);
        l2_prev_hp = l2p.hp;
        l2_prev_energy = l2p.energy;
        l2_prev_score = l2_score;
        l2_prev_foe = l2e.hp;
    }
    if (l2_state == L2_BANNER_ST) mark(0, 30, GAME_W, 150);
    mark(l2p.x - 84, l2p.y - 150, l2p.x + 84, l2p.y + 20);
    if (l2e.visible || l2e.dying) {
        // The whole sprite, not just the body: spears and lightning reach beyond it.
        float w = l2_foe[l2e.type].w, h = l2_foe[l2e.type].h;
        mark(l2e.x - w / 2 - 6, l2e.y - h - 6, l2e.x + w / 2 + 6, l2e.y + 6);
    }
    for (int i = 0; i < L2_PROJS; i++)
        if (l2_proj[i].alive) mark(l2_proj[i].x - 36, l2_proj[i].y - 18, l2_proj[i].x + 36, l2_proj[i].y + 18);
    for (int i = 0; i < MAX_PARTICLES; i++)
        if (parts[i].life > 0) mark(parts[i].x - 2, parts[i].y - 2, parts[i].x + 6, parts[i].y + 6);
}


// ---- Level 3, Stage 3A: Temple Ascent ----------------------------------------------------
// A vertical platformer. Jatayu climbs the temple to the Pushpaka Vimana.
//   LEFT / RIGHT  walk          UP  jump          MID  light the nearest diya (a checkpoint)
//   SET           start, and on the last screen, back to the title
// Physics is fixed point: y and vy are in 1/16 pixel units ("Q4").

#define L3_WORLD_H    960
#define L3_CAM_MAX    (L3_WORLD_H - GAME_H)
#define L3_GRAV       6       // Q4 per tick, so 0.375 px per tick^2
#define L3_JUMP       (-117)  // Q4 per tick, about 7.3 px per tick: a jump of about 72 px
#define L3_SPEED      3
#define L3_HALF       20      // Jatayu's hit box, half its width either side of his centre
#define L3_HEIGHT     42      // and its height above his feet
#define L3_REACH_X    50      // how close he must be, sideways, to light a diya
#define L3_REACH_Y    60      // and above or below its platform
#define L3_PLATS      23
#define L3_GOAL       22      // the top platform, where the chariot waits
#define L3_CP_MAX     3

enum { L3_STORY, L3_INTRO, L3_PLAY, L3_ESCAPE, L3_DONE };

typedef struct {
    int x, y, w, moving, dir, min_x, max_x, dx; // y is the top of the platform
} L3Plat;

// Stage 3A layout, from the courtyard (bottom) to the chariot (top).
static const L3Plat l3_layout[L3_PLATS] = {
    {0, 930, 480, 0, 1, 0, 0, 0},
    {60, 892, 144, 0, 1, 0, 0, 0},
    {300, 856, 96, 1, 1, 230, 380, 0},
    {180, 818, 96, 0, 1, 0, 0, 0},
    {330, 782, 96, 0, 1, 0, 0, 0},
    {150, 744, 96, 1, 1, 60, 260, 0},
    {230, 706, 96, 0, 1, 0, 0, 0},
    {60, 668, 96, 0, 1, 0, 0, 0},
    {250, 630, 144, 0, 1, 0, 0, 0},
    {30, 592, 96, 1, 1, 20, 220, 0},
    {300, 554, 96, 0, 1, 0, 0, 0},
    {120, 516, 96, 0, 1, 0, 0, 0},
    {320, 478, 96, 1, 1, 260, 384, 0},
    {140, 440, 96, 0, 1, 0, 0, 0},
    {30, 402, 96, 0, 1, 0, 0, 0},
    {250, 364, 96, 0, 1, 0, 0, 0},
    {120, 326, 96, 1, 1, 60, 300, 0},
    {320, 288, 96, 0, 1, 0, 0, 0},
    {150, 250, 96, 0, 1, 0, 0, 0},
    {30, 212, 96, 0, 1, 0, 0, 0},
    {250, 174, 96, 1, 1, 200, 380, 0},
    {130, 136, 96, 0, 1, 0, 0, 0},
    {260, 110, 144, 0, 1, 0, 0, 0}, // goal
};
// Platforms that carry a diya, in order. Each diya can be lit once.
static const int l3_diya_plat[L3_CP_MAX] = {8, 11, 15};

static L3Plat l3_plat[L3_PLATS];
static int l3_state, l3_t;
static struct {
    int x;          // centre, px
    int y_q;        // feet, Q4
    int vy_q;       // vertical speed, Q4 per tick
    int on;         // platform he stands on, or -1 in the air
    int facing;     // 1 right, -1 left
} l3p;
static int l3_lit[L3_CP_MAX];
static int l3_cp;   // the platform he respawns on
static int l3_cam;  // world y of the top of the screen
static int l3_prev_cam;
static int l3_chariot_y;
static int l3_prev_lit = -1;

static int l3_clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static int l3_feet(void) { return l3p.y_q >> 4; }

static int l3_over(const L3Plat *p) { return l3p.x - L3_HALF < p->x + p->w && l3p.x + L3_HALF > p->x; }

static void l3_respawn(void) {
    const L3Plat *p = &l3_plat[l3_cp];
    l3p.x = p->x + p->w / 2;
    l3p.y_q = p->y << 4;
    l3p.vy_q = 0;
    l3p.on = l3_cp;
    l3p.facing = 1;
}

static void l3_start(void) {
    memcpy(l3_plat, l3_layout, sizeof l3_plat);
    memset(l3_lit, 0, sizeof l3_lit);
    l3_cp = 0;
    l3_chariot_y = 0;
    l3_respawn();
    l3_cam = l3_clamp(l3_feet() - 230, 0, L3_CAM_MAX);
    l3_prev_cam = l3_cam;
    l3_prev_lit = -1;
}

static void level3_start(void) {
    state = ST_BATTLE; // leave the title, so the title code stops running
    l2_on = 0;
    l3_on = 1;
    l3_state = L3_STORY;
    l3_t = 0;
    force_all = 1;
    l3_start();
}

static void l3_step(uint8_t held, uint8_t pressed) {
    l3_t++;
    if (l3_state == L3_STORY) {
        if (pressed & JOY_SET) {
            l3_state = L3_INTRO;
            l3_t = 0;
        }
        return;
    }
    if (l3_state == L3_INTRO) {
        if (l3_t >= 90) {
            l3_state = L3_PLAY;
            l3_t = 0;
        }
        return;
    }
    if (l3_state == L3_ESCAPE) {
        // The chariot climbs out of the top of the stage.
        l3_chariot_y -= 3;
        if (l3_t >= 120) {
            l3_state = L3_DONE;
            l3_t = 0;
        }
        return;
    }
    if (l3_state == L3_DONE) {
        if (l3_t >= 30 && (pressed & JOY_SET)) game_init();
        return;
    }

    // 1. Platforms move between their two ends.
    for (int i = 0; i < L3_PLATS; i++) {
        L3Plat *p = &l3_plat[i];
        p->dx = 0;
        if (!p->moving) continue;
        int old = p->x;
        p->x += p->dir;
        if (p->x <= p->min_x) {
            p->x = p->min_x;
            p->dir = 1;
        } else if (p->x >= p->max_x) {
            p->x = p->max_x;
            p->dir = -1;
        }
        p->dx = p->x - old;
    }

    // 2. Standing on a moving platform carries Jatayu with it, or he steps off the end.
    if (l3p.on >= 0) {
        const L3Plat *p = &l3_plat[l3p.on];
        if (!l3_over(p)) {
            l3p.on = -1;
        } else {
            l3p.x += p->dx;
            l3p.y_q = p->y << 4;
        }
    }

    // 3. Walking and jumping.
    if (held & JOY_LEFT) {
        l3p.x -= L3_SPEED;
        l3p.facing = -1;
    }
    if (held & JOY_RIGHT) {
        l3p.x += L3_SPEED;
        l3p.facing = 1;
    }
    l3p.x = l3_clamp(l3p.x, L3_HALF, GAME_W - L3_HALF);
    if ((pressed & JOY_UP) && l3p.on >= 0) {
        l3p.vy_q = L3_JUMP;
        l3p.on = -1;
    }

    // 4. Divine light: MID lights the nearest unlit diya within reach, and it becomes the checkpoint.
    if (pressed & JOY_MID) {
        int best = -1, best_d = 1 << 30;
        for (int i = 0; i < L3_CP_MAX; i++) {
            if (l3_lit[i]) continue;
            const L3Plat *p = &l3_plat[l3_diya_plat[i]];
            int dx = abs(l3p.x - (p->x + p->w / 2)), dy = abs(l3_feet() - p->y);
            if (dx > L3_REACH_X || dy > L3_REACH_Y) continue; // too far from this diya
            if (dx + dy < best_d) {
                best_d = dx + dy;
                best = i;
            }
        }
        if (best >= 0) {
            l3_lit[best] = 1;
            l3_cp = l3_diya_plat[best];
        }
    }

    // 5. Gravity, and landing on a platform from above only.
    if (l3p.on < 0) {
        int prev = l3_feet();
        l3p.vy_q += L3_GRAV;
        l3p.y_q += l3p.vy_q;
        int feet = l3_feet();
        if (l3p.vy_q >= 0) {
            for (int i = 0; i < L3_PLATS; i++) {
                const L3Plat *p = &l3_plat[i];
                // Any crossing of the platform top from above counts, however fast he falls.
                if (prev <= p->y && feet >= p->y && l3_over(p)) {
                    l3p.on = i;
                    l3p.y_q = p->y << 4;
                    l3p.vy_q = 0;
                    break;
                }
            }
        }
    }

    // 6. Reaching the goal starts the escape. Falling off the bottom sends him back to the checkpoint.
    if (l3p.on == L3_GOAL) {
        l3_state = L3_ESCAPE;
        l3_t = 0;
        return;
    }
    if (l3_feet() > l3_cam + GAME_H + 20) l3_respawn();

    // 7. The camera follows him up and down, and stays inside the stage.
    int sy = l3_feet() - l3_cam;
    if (sy < 110) l3_cam = l3_feet() - 110;
    else if (sy > 230) l3_cam = l3_feet() - 230;
    l3_cam = l3_clamp(l3_cam, 0, L3_CAM_MAX);
}


// Stage 3A drawing. Every strip is drawn from scratch (sky, temple, platforms, diyas, chariot,
// Jatayu, then the HUD and any message), so it works with the dirty-strip system.
static void l3_sky(void) {
    // Sunset: saffron at the top, orange and deep red towards the temple.
    for (int y = strip_y0; y < strip_y0 + strip_rows; y++) {
        int t = y * 5 / GAME_H;
        static const uint16_t band[5] = {
            RGB565(250, 190, 90), RGB565(250, 150, 60), RGB565(240, 110, 50),
            RGB565(200, 70, 50), RGB565(120, 40, 50),
        };
        fill(0, y, GAME_W, 1, band[t]);
    }
}

static void l3_draw_temple(void) {
    // The temple skyline across the bottom of the screen: a fixed picture, copied from flash.
    const int y0 = GAME_H - L3_TEMPLE_STRIP_H;
    for (int y = y0; y < GAME_H; y++) {
        if (y < strip_y0 || y >= strip_y0 + strip_rows) continue;
        memcpy(strip + (y - strip_y0) * GAME_W, l3_temple_strip + (y - y0) * GAME_W, GAME_W * sizeof *strip);
    }
}

static void l3_draw_platforms(void) {
    for (int i = 0; i < L3_PLATS; i++) {
        const L3Plat *p = &l3_plat[i];
        int sy = p->y - l3_cam;
        if (sy + 14 < strip_y0 || sy > strip_y0 + strip_rows) continue;
        const uint16_t *spr;
        int tw, th;
        if (i == L3_GOAL) {
            spr = l3_cloud_platform, tw = L3_CLOUD_PLATFORM_W, th = L3_CLOUD_PLATFORM_H;
        } else {
            spr = l3_temple_platform, tw = L3_TEMPLE_PLATFORM_W, th = L3_TEMPLE_PLATFORM_H;
        }
        for (int x = 0; x < p->w; x += tw) blit_sprite(spr, tw, th, p->x + x, sy, -1, 0);
    }
}

static void l3_draw_diyas(void) {
    for (int i = 0; i < L3_CP_MAX; i++) {
        const L3Plat *p = &l3_plat[l3_diya_plat[i]];
        int x = p->x + p->w / 2 - L3_DIYA_LAMP_W / 2;
        int y = p->y - L3_DIYA_LAMP_H - l3_cam;
        // A lit diya is bright; an unlit one is dimmed.
        blit_sprite(l3_diya_lamp, L3_DIYA_LAMP_W, L3_DIYA_LAMP_H, x, y, -1, l3_lit[i] ? 0 : 8);
    }
}

static void l3_draw(void) {
    if (l3_state == L3_STORY) {
        memcpy(strip, l3_story + (size_t)strip_y0 * GAME_W, (size_t)strip_rows * GAME_W * sizeof *strip);
        if (strip_y0 + strip_rows > 260 && strip_y0 < 300) {
            fill(0, 262, GAME_W, 40, COL_SHADOW);
            text_centred(272, "RAVANA KIDNAPS SITA", COL_GOLD, 2);
        }
        if (strip_y0 + strip_rows > 300 && strip_y0 < 320 && ((ticks / 15) & 1) == 0)
            text_centred(302, "SET: START", COL_WHITE, 1);
        return;
    }

    l3_sky();
    l3_draw_temple();
    l3_draw_platforms();
    l3_draw_diyas();

    // The Pushpaka Vimana, with Ravana and Sita, above the goal.
    blit_sprite(l3_pushpaka_vimana, L3_PUSHPAKA_VIMANA_W, L3_PUSHPAKA_VIMANA_H, 160, l3_chariot_y - l3_cam, -1, 0);

    // Jatayu, reusing the Level 2 sprite, drawn facing the way he last moved.
    int jx = l3p.x - L3_JATAYU_W / 2, jy = l3_feet() - L3_JATAYU_H - l3_cam;
    blit_sprite_flip(l3_jatayu, L3_JATAYU_W, L3_JATAYU_H, jx, jy, -1, 0, l3p.facing < 0);

    if (strip_y0 < HUD_H) {
        fill(0, 0, GAME_W, HUD_H, COL_HUD);
        fill(0, HUD_H - 1, GAME_W, 1, COL_HUDLN);
        text(6, 9, "JATAYU", COL_TEXT, 1);
        char buf[24];
        int lit = 0;
        for (int i = 0; i < L3_CP_MAX; i++) lit += l3_lit[i];
        snprintf(buf, sizeof buf, "STAGE 3A  DIYAS %d/%d", lit, L3_CP_MAX);
        text(GAME_W - 6 * (int)strlen(buf) - 6, 9, buf, COL_GOLD, 1);
    }

    if (l3_state == L3_INTRO) {
        text_centred_shadow(120, "STAGE 3A", COL_GOLD, 3);
        text_centred_shadow(160, "TEMPLE ASCENT", COL_TEXT, 2);
    } else if (l3_state == L3_ESCAPE || l3_state == L3_DONE) {
        text_centred_shadow(120, "RAVANA ESCAPES!", COL_GOLD, 3);
        if (l3_state == L3_DONE && ((ticks / (GAME_TICK_HZ / 2)) % 2 == 0))
            text_centred_shadow(200, "SET: TITLE", COL_WHITE, 2);
    }
}

// Only the parts that changed are sent. When the camera moves, everything has moved, so the
// whole frame is redrawn; otherwise only the moving platforms, Jatayu and the HUD are marked.
static void l3_dirty(void) {
    if (l3_cam != l3_prev_cam || l3_state != L3_PLAY) {
        mark_all();
        l3_prev_cam = l3_cam;
        return;
    }
    for (int i = 0; i < L3_PLATS; i++) {
        const L3Plat *p = &l3_plat[i];
        if (p->moving) mark(p->x, p->y - l3_cam - 2, p->x + p->w, p->y - l3_cam + 14);
    }
    // Wide enough for the whole sprite, so nothing is left behind when he moves.
    mark(l3p.x - L3_JATAYU_W / 2 - 4, l3_feet() - L3_JATAYU_H - l3_cam - 4, l3p.x + L3_JATAYU_W / 2 + 4, l3_feet() - l3_cam + 4);
    int lit = 0;
    for (int i = 0; i < L3_CP_MAX; i++) lit += l3_lit[i];
    if (lit != l3_prev_lit) {
        mark(0, 0, GAME_W, HUD_H);
        l3_prev_lit = lit;
    }
}

// Level 4 is not written yet: this placeholder waits until SET takes you back to the title.
static void level4_start(void) {
    state = ST_BATTLE; // leave the title, so the title code stops running
    l2_on = 0;
    l3_on = 0;
    l4_on = 1;
    l4_t = 0;
    force_all = 1;
}

void game_update(uint8_t held, uint8_t pressed) {
    step(held, pressed);

    memset(tiles_now, 0, sizeof tiles_now);
    if (l4_on) {
        if (force_all) mark_all();
        else mark(0, 240, GAME_W, 262); // only the blinking prompt changes
        force_all = 0;
    } else if (l3_on) {
        if (force_all) mark_all();
        else l3_dirty();
        force_all = 0;
    } else if (l2_on) {
        if (force_all) mark_all();
        else l2_dirty();
        force_all = 0;
    } else if (force_all || state == ST_VICTORY || state == ST_GAMEOVER) {
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
