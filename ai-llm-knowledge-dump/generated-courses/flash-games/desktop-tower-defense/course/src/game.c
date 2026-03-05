/* src/game.c  —  Desktop Tower Defense | Core Game Logic
 *
 * Contains every game system that is NOT platform-specific:
 *   - BFS distance field            - Tower / creep / projectile pools
 *   - Wave spawning & management    - Economy (gold, interest, sell)
 *   - Targeting & damage            - Particle effects
 *   - Game-phase FSM                - Full renderer
 *
 * Zero X11.  Zero Raylib.  Compile with any platform backend.
 */

#include "game.h"
#include "utils/draw-shapes.h"
#include "utils/draw-text.h"

#include <math.h>    /* atan2f, cosf, sinf, sqrtf, powf */
#include <stdio.h>   /* snprintf                        */
#include <stdlib.h>  /* (nothing needed at runtime)     */
#include <string.h>  /* memset                          */

/* =========================================================================
 * LOCAL CONSTANTS
 * ========================================================================= */

#define BARREL_LEN       8    /* pixels from tower centre to barrel tip */
#define PROJECTILE_SPEED 200.0f

/* UI panel layout — all x values are relative to canvas origin */
#define SHOP_BTN_X   (PANEL_X + 2)
#define SHOP_BTN_W   (PANEL_W - 4)
#define SHOP_BTN_Y0  75           /* y of first shop button                */
#define SHOP_BTN_H   34           /* height of each shop button row        */
#define SEL_INFO_Y   (SHOP_BTN_Y0 + 7 * SHOP_BTN_H + 8)
#define MODE_BTN_Y   (SEL_INFO_Y + 18)
#define MODE_BTN_H   20
#define SELL_BTN_Y   (MODE_BTN_Y + 24)
#define SELL_BTN_H   20
#define START_BTN_Y  (CANVAS_H - 42)
#define START_BTN_H  36

/* =========================================================================
 * PSEUDO-RANDOM NUMBER GENERATOR
 * ========================================================================= */

static int s_rng = 12345;

static int rng_next(void)
{
    s_rng = s_rng * 1664525 + 1013904223;
    return s_rng;
}

/* Set by game_update each frame; read by game_render to draw hover overlay.
 * Using a file-static avoids a const-cast inside the render function. */
static int s_hover_valid = 0;

/* =========================================================================
 * DATA TABLES
 * ========================================================================= */

/* Declared extern in game.h — must be non-static globals. */
const TowerDef TOWER_DEFS[TOWER_COUNT] = {
    /*                  name      cost  dmg  range  rate        color        aoe  splash */
    [TOWER_NONE]   = { "",          0,   0,  0.0f,  0.0f, 0,               0,   0.0f  },
    [TOWER_PELLET] = { "Pellet",    5,   5,  3.0f,  2.5f, COLOR_PELLET,    0,   0.0f  },
    [TOWER_SQUIRT] = { "Squirt",   15,  10,  3.5f,  1.5f, COLOR_SQUIRT,    0,   0.0f  },
    [TOWER_DART]   = { "Dart",     40,  35,  4.0f,  1.0f, COLOR_DART,      0,   0.0f  },
    [TOWER_SNAP]   = { "Snap",     75, 125,  4.0f,  0.4f, COLOR_SNAP,      0,   0.0f  },
    [TOWER_SWARM]  = { "Swarm",   125,  15,  4.0f,  0.7f, COLOR_SWARM,     1,  30.0f  },
    [TOWER_FROST]  = { "Frost",    60,   0,  3.5f,  1.0f, COLOR_FROST,     1,   0.0f  },
    [TOWER_BASH]   = { "Bash",     90,  40,  3.0f,  0.5f, COLOR_BASH,      1,  40.0f  },
};

const CreepDef CREEP_DEFS[CREEP_COUNT] = {
    /*                      speed    hp  lives       color              fly arm gold size */
    [CREEP_NONE]        = {   0.0f,   0,   0, 0,                        0,  0,  0,  CELL_SIZE },
    [CREEP_NORMAL]      = {  60.0f, 100,   1, COLOR_CREEP_NORMAL,       0,  0,  2,  14 },
    [CREEP_FAST]        = { 120.0f,  70,   1, COLOR_CREEP_FAST,         0,  0,  2,  12 },
    [CREEP_FLYING]      = {  80.0f,  80,   1, COLOR_CREEP_FLYING,       1,  0,  3,  12 },
    [CREEP_ARMOURED]    = {  50.0f, 200,   1, COLOR_CREEP_ARMOURED,     0,  1,  4,  16 },
    [CREEP_SPAWN]       = {  60.0f, 150,   1, COLOR_CREEP_SPAWN,        0,  0,  3,  16 },
    [CREEP_SPAWN_CHILD] = {  70.0f,  30,   0, COLOR_CREEP_SPAWN,        0,  0,  1,   8 },
    [CREEP_BOSS]        = {  40.0f,1000,   5, COLOR_CREEP_BOSS,         0,  0, 25,  24 },
};

/* =========================================================================
 * FORWARD DECLARATIONS
 * ========================================================================= */

static void bfs_fill_distance(GameState *s);
static int  can_place_tower(GameState *s, int col, int row);
static void change_phase(GameState *s, GamePhase p);
static int  spawn_creep(GameState *s, CreepType type, float hp_override);
static void compact_creeps(GameState *s);
static void compact_projectiles(GameState *s);
static void compact_particles(GameState *s);
static int  in_range(Tower *t, Creep *c);
static int  find_best_target(GameState *s, Tower *t);
static int  effective_damage(int raw, CreepType ct, TowerType tt);
static void spawn_projectile(GameState *s, float sx, float sy, int target_id,
                             int dmg, float splash_r, TowerType tt);
static void spawn_particle(GameState *s, float x, float y, float vx, float vy,
                           float lifetime, uint32_t color, int sz, const char *text);
static void spawn_explosion(GameState *s, float x, float y, uint32_t color, int count);
static void kill_creep(GameState *s, int idx);
static void apply_damage(GameState *s, int creep_idx, int dmg, TowerType tt);
static void creep_move_toward_exit(GameState *s, Creep *c, float dt);
static void creep_flying_move(GameState *s, Creep *c, float dt);
static void tower_update(GameState *s, Tower *t, float dt);
static void projectile_update(GameState *s, Projectile *p, float dt);
static int  check_wave_complete(GameState *s);
static void start_wave(GameState *s);
static void send_wave_early(GameState *s);
static void update_wave(GameState *s, float dt);
static void handle_placement_input(GameState *s);

/* =========================================================================
 * BFS DISTANCE FIELD
 *
 * Backward BFS from the exit cell.  Every reachable CELL_EMPTY/ENTRY/EXIT
 * cell gets dist[] = steps to exit.  CELL_TOWER cells are walls (-1).
 * Creeps follow the gradient: move to the neighbour with the smallest dist.
 * ========================================================================= */

static void bfs_fill_distance(GameState *s)
{
    int total = GRID_ROWS * GRID_COLS;
    for (int i = 0; i < total; i++) s->dist[i] = -1;

    static int queue[GRID_ROWS * GRID_COLS];
    int head = 0, tail = 0;

    int exit_idx = EXIT_ROW * GRID_COLS + EXIT_COL;
    s->dist[exit_idx] = 0;
    queue[tail++] = exit_idx;

    static const int dr[4] = { -1,  1,  0,  0 };
    static const int dc[4] = {  0,  0, -1,  1 };

    while (head < tail) {
        int idx = queue[head++];
        int row = idx / GRID_COLS;
        int col = idx % GRID_COLS;

        for (int d = 0; d < 4; d++) {
            int nr = row + dr[d], nc = col + dc[d];
            if (nr < 0 || nr >= GRID_ROWS || nc < 0 || nc >= GRID_COLS) continue;
            int ni = nr * GRID_COLS + nc;
            if (s->dist[ni] >= 0)          continue; /* already visited */
            if (s->grid[ni] == CELL_TOWER) continue; /* wall            */
            s->dist[ni] = s->dist[idx] + 1;
            queue[tail++] = ni;
        }
    }
}

/* =========================================================================
 * PLACEMENT LEGALITY  (double-BFS)
 *
 * Temporarily places the tower, runs a forward BFS from entry to exit, then
 * restores the cell.  Returns 1 only if the exit is still reachable.
 * NOTE: temporarily mutates s->grid[] but always restores it.
 * ========================================================================= */

static int can_place_tower(GameState *s, int col, int row)
{
    if (col < 0 || col >= GRID_COLS || row < 0 || row >= GRID_ROWS) return 0;
    int idx = row * GRID_COLS + col;
    if (s->grid[idx] != CELL_EMPTY) return 0;

    s->grid[idx] = CELL_TOWER; /* temporary block */

    static int visited[GRID_ROWS * GRID_COLS];
    static int queue[GRID_ROWS * GRID_COLS];
    static const int dr[4] = { -1,  1,  0,  0 };
    static const int dc[4] = {  0,  0, -1,  1 };

    int total = GRID_ROWS * GRID_COLS;
    for (int i = 0; i < total; i++) visited[i] = 0;

    int head = 0, tail = 0;
    int entry_idx = ENTRY_ROW * GRID_COLS + ENTRY_COL;
    int exit_idx  = EXIT_ROW  * GRID_COLS + EXIT_COL;
    visited[entry_idx] = 1;
    queue[tail++] = entry_idx;

    int reachable = 0;
    while (head < tail) {
        int ci = queue[head++];
        if (ci == exit_idx) { reachable = 1; break; }
        int r = ci / GRID_COLS, c = ci % GRID_COLS;
        for (int d = 0; d < 4; d++) {
            int nr = r + dr[d], nc = c + dc[d];
            if (nr < 0 || nr >= GRID_ROWS || nc < 0 || nc >= GRID_COLS) continue;
            int ni = nr * GRID_COLS + nc;
            if (visited[ni] || s->grid[ni] == CELL_TOWER) continue;
            visited[ni] = 1;
            queue[tail++] = ni;
        }
    }

    s->grid[idx] = CELL_EMPTY; /* restore */
    return reachable;
}

/* =========================================================================
 * GAME INIT
 * ========================================================================= */

void game_init(GameState *s)
{
    memset(s, 0, sizeof(*s));

    s->grid[ENTRY_ROW * GRID_COLS + ENTRY_COL] = CELL_ENTRY;
    s->grid[EXIT_ROW  * GRID_COLS + EXIT_COL]  = CELL_EXIT;

    s->player_gold        = STARTING_GOLD;
    s->player_lives       = STARTING_LIVES;
    s->current_wave       = 1;
    s->selected_tower_type = TOWER_NONE;
    s->selected_tower_idx  = -1;
    s->hover_col           = -1;
    s->hover_row           = -1;

    bfs_fill_distance(s);
    s->phase = GAME_PHASE_TITLE;
}

/* =========================================================================
 * PHASE CHANGE
 * ========================================================================= */

static void change_phase(GameState *s, GamePhase p)
{
    s->phase       = p;
    s->phase_timer = 0.0f;

    switch (p) {
        case GAME_PHASE_WAVE_CLEAR:
            s->phase_timer = 3.0f;
            game_play_sound(&s->audio, SFX_WAVE_COMPLETE);
            break;
        case GAME_PHASE_GAME_OVER:
            game_play_sound(&s->audio, SFX_GAME_OVER);
            break;
        case GAME_PHASE_VICTORY:
            game_play_sound(&s->audio, SFX_VICTORY);
            break;
        default:
            break;
    }
}

/* =========================================================================
 * CREEP SPAWNING
 * ========================================================================= */

static int spawn_creep(GameState *s, CreepType type, float hp_override)
{
    /* Reuse the first inactive slot; otherwise extend the pool. */
    int idx = -1;
    for (int i = 0; i < s->creep_count; i++) {
        if (!s->creeps[i].active) { idx = i; break; }
    }
    if (idx < 0) {
        if (s->creep_count >= MAX_CREEPS) return -1;
        idx = s->creep_count++;
    }

    const CreepDef *def = &CREEP_DEFS[type];
    Creep *c = &s->creeps[idx];
    memset(c, 0, sizeof(*c));

    c->x          = ENTRY_COL * CELL_SIZE + CELL_SIZE * 0.5f;
    c->y          = ENTRY_ROW * CELL_SIZE + CELL_SIZE * 0.5f;
    c->col        = ENTRY_COL;
    c->row        = ENTRY_ROW;
    c->type       = type;
    c->hp         = hp_override > 0.0f ? hp_override : (float)def->base_hp;
    c->max_hp     = c->hp;
    c->speed      = def->speed;
    c->active     = 1;
    c->id         = s->next_creep_id++;
    c->is_flying  = def->is_flying;
    c->slow_factor = 1.0f;
    c->lives_cost = def->lives_cost;

    if (def->is_flying) {
        float ex = EXIT_COL * CELL_SIZE + CELL_SIZE * 0.5f;
        float ey = EXIT_ROW * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = ex - c->x, dy = ey - c->y;
        float d  = sqrtf(dx * dx + dy * dy);
        if (d > 0.001f) { c->fly_dx = dx / d; c->fly_dy = dy / d; }
        else             { c->fly_dx = 1.0f;   c->fly_dy = 0.0f;   }
    }

    return idx;
}

/* =========================================================================
 * COMPACT (swap-remove inactive entities)
 * ========================================================================= */

static void compact_creeps(GameState *s)
{
    int i = 0;
    while (i < s->creep_count) {
        if (!s->creeps[i].active) s->creeps[i] = s->creeps[--s->creep_count];
        else                      i++;
    }
}

static void compact_projectiles(GameState *s)
{
    int i = 0;
    while (i < s->projectile_count) {
        if (!s->projectiles[i].active) s->projectiles[i] = s->projectiles[--s->projectile_count];
        else                           i++;
    }
}

static void compact_particles(GameState *s)
{
    int i = 0;
    while (i < s->particle_count) {
        if (!s->particles[i].active) s->particles[i] = s->particles[--s->particle_count];
        else                         i++;
    }
}

/* =========================================================================
 * TARGETING
 * ========================================================================= */

static int in_range(Tower *t, Creep *c)
{
    float dx = (float)t->cx - c->x;
    float dy = (float)t->cy - c->y;
    float r  = t->range;
    return (dx * dx + dy * dy) <= (r * r);
}

static int find_best_target(GameState *s, Tower *t)
{
    int   best     = -1;
    float best_val = 0.0f;

    for (int i = 0; i < s->creep_count; i++) {
        Creep *c = &s->creeps[i];
        if (!c->active || !in_range(t, c)) continue;

        float val;
        switch (t->target_mode) {
            case TARGET_FIRST: {
                /* Smallest dist[] = closest to exit */
                float d;
                if (c->is_flying) {
                    float ex = EXIT_COL * CELL_SIZE + CELL_SIZE * 0.5f;
                    float ey = EXIT_ROW * CELL_SIZE + CELL_SIZE * 0.5f;
                    float dx2 = ex - c->x, dy2 = ey - c->y;
                    d = sqrtf(dx2 * dx2 + dy2 * dy2);
                } else {
                    int ci = c->row * GRID_COLS + c->col;
                    d = (float)(s->dist[ci] >= 0 ? s->dist[ci] : 99999);
                }
                val = -d;
                break;
            }
            case TARGET_LAST: {
                /* Largest dist[] = farthest from exit */
                float d;
                if (c->is_flying) {
                    float ex = EXIT_COL * CELL_SIZE + CELL_SIZE * 0.5f;
                    float ey = EXIT_ROW * CELL_SIZE + CELL_SIZE * 0.5f;
                    float dx2 = ex - c->x, dy2 = ey - c->y;
                    d = sqrtf(dx2 * dx2 + dy2 * dy2);
                } else {
                    int ci = c->row * GRID_COLS + c->col;
                    d = (float)(s->dist[ci] >= 0 ? s->dist[ci] : 0);
                }
                val = d;
                break;
            }
            case TARGET_STRONGEST: val =  c->hp;                          break;
            case TARGET_WEAKEST:   val = -c->hp;                          break;
            case TARGET_CLOSEST: {
                float dx2 = (float)t->cx - c->x, dy2 = (float)t->cy - c->y;
                val = -(dx2 * dx2 + dy2 * dy2);
                break;
            }
            default: val = 0.0f; break;
        }

        if (best < 0 || val > best_val) {
            best     = i;
            best_val = val;
        }
    }

    return best;
}

/* =========================================================================
 * DAMAGE CALCULATION
 * ========================================================================= */

static int effective_damage(int raw, CreepType ct, TowerType tt)
{
    /* Armoured creeps take 50% damage from everything except Swarm. */
    if (ct == CREEP_ARMOURED && tt != TOWER_SWARM) return raw / 2;
    return raw;
}

/* =========================================================================
 * SPAWN HELPERS
 * ========================================================================= */

static void spawn_particle(GameState *s, float x, float y, float vx, float vy,
                           float lifetime, uint32_t color, int sz, const char *text)
{
    int idx = -1;
    for (int i = 0; i < s->particle_count; i++) {
        if (!s->particles[i].active) { idx = i; break; }
    }
    if (idx < 0) {
        if (s->particle_count >= MAX_PARTICLES) return;
        idx = s->particle_count++;
    }

    Particle *p = &s->particles[idx];
    memset(p, 0, sizeof(*p));
    p->x = x; p->y = y; p->vx = vx; p->vy = vy;
    p->lifetime = p->max_lifetime = lifetime;
    p->color    = color;
    p->size     = sz;
    if (text) {
        int i = 0;
        while (text[i] && i < 11) { p->text[i] = text[i]; i++; }
        p->text[i] = '\0';
    }
    p->active = 1;
}

static void spawn_explosion(GameState *s, float x, float y, uint32_t color, int count)
{
    for (int i = 0; i < count; i++) {
        int r1    = rng_next();
        float ang = (float)((unsigned)r1 & 0xFFFF) / 65536.0f * 6.28318f;
        int r2    = rng_next();
        float spd = 30.0f + (float)((unsigned)r2 & 0xFF) * 0.5f;
        float lt  = 0.25f + (float)(((unsigned)r2 >> 8) & 0xFF) / 512.0f;
        spawn_particle(s, x, y, cosf(ang) * spd, sinf(ang) * spd, lt, color, 2, NULL);
    }
}

static void spawn_projectile(GameState *s, float sx, float sy, int target_id,
                             int dmg, float splash_r, TowerType tt)
{
    int idx = -1;
    for (int i = 0; i < s->projectile_count; i++) {
        if (!s->projectiles[i].active) { idx = i; break; }
    }
    if (idx < 0) {
        if (s->projectile_count >= MAX_PROJECTILES) return;
        idx = s->projectile_count++;
    }

    Projectile *p = &s->projectiles[idx];
    memset(p, 0, sizeof(*p));
    p->x            = sx;
    p->y            = sy;
    p->damage       = dmg;
    p->splash_radius = splash_r;
    p->target_id    = target_id;
    p->tower_type   = tt;
    p->active       = 1;

    /* Initial velocity toward target's current position */
    float tx = sx, ty = sy;
    for (int i = 0; i < s->creep_count; i++) {
        if (s->creeps[i].id == target_id && s->creeps[i].active) {
            tx = s->creeps[i].x;
            ty = s->creeps[i].y;
            break;
        }
    }
    float dx = tx - sx, dy = ty - sy;
    float d  = sqrtf(dx * dx + dy * dy);
    if (d > 0.001f) { p->vx = dx / d * PROJECTILE_SPEED; p->vy = dy / d * PROJECTILE_SPEED; }
    else             { p->vx = PROJECTILE_SPEED;           p->vy = 0.0f;                      }
}

/* =========================================================================
 * KILL / DAMAGE
 * ========================================================================= */

static void apply_damage(GameState *s, int creep_idx, int dmg, TowerType tt)
{
    if (creep_idx < 0 || creep_idx >= s->creep_count) return;
    Creep *c = &s->creeps[creep_idx];
    if (!c->active) return;
    c->hp -= (float)effective_damage(dmg, c->type, tt);
    if (c->hp <= 0.0f) kill_creep(s, creep_idx);
}

static void kill_creep(GameState *s, int idx)
{
    Creep *c = &s->creeps[idx];
    if (!c->active) return;

    int gold = CREEP_DEFS[c->type].kill_gold;
    s->player_gold += gold;

    char txt[8];
    snprintf(txt, sizeof(txt), "+%d", gold);
    spawn_particle(s, c->x, c->y - 10.0f, 0.0f, -28.0f, 1.2f, COLOR_GOLD_TEXT, 1, txt);
    spawn_explosion(s, c->x, c->y, CREEP_DEFS[c->type].color, 5);

    if (c->type == CREEP_SPAWN) {
        /* Snap parent info before zeroing the slot */
        int   pcol = c->col, prow = c->row;
        float px   = c->x,   py   = c->y;
        c->active = 0; /* mark before spawning children to free the slot */
        for (int i = 0; i < 4; i++) {
            int ci = spawn_creep(s, CREEP_SPAWN_CHILD, 0.0f);
            if (ci >= 0) {
                s->creeps[ci].x   = px + (float)((rng_next() & 7) - 3);
                s->creeps[ci].y   = py + (float)((rng_next() & 7) - 3);
                s->creeps[ci].col = pcol;
                s->creeps[ci].row = prow;
            }
        }
        return; /* already marked inactive */
    }

    game_play_sound(&s->audio, c->type == CREEP_BOSS ? SFX_BOSS_DEATH : SFX_CREEP_DEATH);
    c->active = 0;
}

/* =========================================================================
 * CREEP MOVEMENT
 * ========================================================================= */

static void creep_move_toward_exit(GameState *s, Creep *c, float dt)
{
    if (c->stun_timer > 0.0f) { c->stun_timer -= dt; return; }

    float spd = c->speed;
    if (c->slow_timer > 0.0f) {
        spd *= c->slow_factor;
        c->slow_timer -= dt;
        if (c->slow_timer < 0.0f) c->slow_timer = 0.0f;
    }

    /* Find the 4-neighbour with the smallest dist (= direction toward exit). */
    static const int dr[4] = { -1,  1,  0,  0 };
    static const int dc[4] = {  0,  0, -1,  1 };

    int best_col = c->col, best_row = c->row;
    int ci       = c->row * GRID_COLS + c->col;
    int best_d   = s->dist[ci] >= 0 ? s->dist[ci] : 99999;

    for (int d = 0; d < 4; d++) {
        int nr = c->row + dr[d], nc = c->col + dc[d];
        if (nr < 0 || nr >= GRID_ROWS || nc < 0 || nc >= GRID_COLS) continue;
        int ni = nr * GRID_COLS + nc;
        if (s->grid[ni] == CELL_TOWER) continue;
        int nd = s->dist[ni];
        if (nd >= 0 && nd < best_d) {
            best_d   = nd;
            best_col = nc;
            best_row = nr;
        }
    }

    /* Interpolate toward the centre of the best cell. */
    float tx = best_col * CELL_SIZE + CELL_SIZE * 0.5f;
    float ty = best_row * CELL_SIZE + CELL_SIZE * 0.5f;
    float dx = tx - c->x, dy = ty - c->y;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist < 1.5f) {
        /* Arrived: commit to cell */
        c->x   = tx;  c->y   = ty;
        c->col = best_col; c->row = best_row;

        if (c->col == EXIT_COL && c->row == EXIT_ROW) {
            s->player_lives -= c->lives_cost;
            if (s->player_lives < 0) s->player_lives = 0;
            game_play_sound(&s->audio, SFX_LIFE_LOST);
            spawn_particle(s, c->x, c->y - 8.0f, 0.0f, -30.0f, 1.0f,
                           COLOR_LIVES_TEXT, 1, "-1");
            c->active = 0;
        }
    } else {
        float move = spd * dt;
        if (move > dist) move = dist;
        c->x += dx / dist * move;
        c->y += dy / dist * move;
    }
}

static void creep_flying_move(GameState *s, Creep *c, float dt)
{
    if (c->stun_timer > 0.0f) { c->stun_timer -= dt; return; }

    float spd = c->speed;
    if (c->slow_timer > 0.0f) {
        spd *= c->slow_factor;
        c->slow_timer -= dt;
        if (c->slow_timer < 0.0f) c->slow_timer = 0.0f;
    }

    c->x += c->fly_dx * spd * dt;
    c->y += c->fly_dy * spd * dt;

    /* Update integer grid cell for targeting purposes */
    int gc = (int)(c->x / CELL_SIZE);
    int gr = (int)(c->y / CELL_SIZE);
    c->col = (gc >= 0 && gc < GRID_COLS) ? gc : c->col;
    c->row = (gr >= 0 && gr < GRID_ROWS) ? gr : c->row;

    /* Arrival: within half a cell of exit centre */
    float ex = EXIT_COL * CELL_SIZE + CELL_SIZE * 0.5f;
    float ey = EXIT_ROW * CELL_SIZE + CELL_SIZE * 0.5f;
    float ddx = ex - c->x, ddy = ey - c->y;
    if (ddx * ddx + ddy * ddy < (float)(CELL_SIZE * CELL_SIZE / 4)) {
        s->player_lives -= c->lives_cost;
        if (s->player_lives < 0) s->player_lives = 0;
        game_play_sound(&s->audio, SFX_LIFE_LOST);
        spawn_particle(s, c->x, c->y - 8.0f, 0.0f, -30.0f, 1.0f,
                       COLOR_LIVES_TEXT, 1, "-1");
        c->active = 0;
    }
}

/* =========================================================================
 * TOWER UPDATE
 * ========================================================================= */

static void tower_update(GameState *s, Tower *t, float dt)
{
    if (!t->active) return;
    if (t->place_flash > 0.0f) t->place_flash -= dt;
    t->cooldown_timer -= dt;

    if (t->type == TOWER_FROST) {
        /* AOE slow pulse — no projectile */
        if (t->cooldown_timer <= 0.0f) {
            t->cooldown_timer = 1.0f / t->fire_rate;
            for (int i = 0; i < s->creep_count; i++) {
                Creep *c = &s->creeps[i];
                if (c->active && in_range(t, c)) {
                    c->slow_timer  = 2.0f;
                    c->slow_factor = 0.5f;
                }
            }
            game_play_sound(&s->audio, SFX_FROST_PULSE);
        }
        return;
    }

    if (t->type == TOWER_BASH) {
        /* AOE stun + damage pulse — no projectile */
        if (t->cooldown_timer <= 0.0f) {
            t->cooldown_timer = 1.0f / t->fire_rate;
            for (int i = 0; i < s->creep_count; i++) {
                Creep *c = &s->creeps[i];
                if (c->active && in_range(t, c)) {
                    c->stun_timer = 0.5f;
                    apply_damage(s, i, t->damage, t->type);
                }
            }
            spawn_explosion(s, (float)t->cx, (float)t->cy,
                            TOWER_DEFS[TOWER_BASH].color, 8);
            game_play_sound(&s->audio, SFX_BASH_HIT);
        }
        return;
    }

    /* Standard: find target, rotate barrel, fire when cooldown expires */
    int ti = find_best_target(s, t);
    if (ti >= 0) {
        Creep *tc = &s->creeps[ti];
        float dx = tc->x - (float)t->cx;
        float dy = tc->y - (float)t->cy;
        t->angle     = atan2f(dy, dx);
        t->target_id = tc->id;

        if (t->cooldown_timer <= 0.0f) {
            t->cooldown_timer = 1.0f / t->fire_rate;
            spawn_projectile(s, (float)t->cx, (float)t->cy,
                             tc->id, t->damage,
                             TOWER_DEFS[t->type].splash_radius, t->type);
            SfxId sfx;
            switch (t->type) {
                case TOWER_SQUIRT: sfx = SFX_TOWER_FIRE_SQUIRT; break;
                case TOWER_DART:   sfx = SFX_TOWER_FIRE_DART;   break;
                case TOWER_SNAP:   sfx = SFX_TOWER_FIRE_SNAP;   break;
                case TOWER_SWARM:  sfx = SFX_TOWER_FIRE_SWARM;  break;
                default:           sfx = SFX_TOWER_FIRE_PELLET; break;
            }
            game_play_sound(&s->audio, sfx);
        }
    }
}

/* =========================================================================
 * PROJECTILE UPDATE
 * ========================================================================= */

static void projectile_update(GameState *s, Projectile *p, float dt)
{
    if (!p->active) return;

    /* Home toward live target, check hit */
    if (p->target_id >= 0) {
        int target_alive = 0;
        for (int i = 0; i < s->creep_count; i++) {
            Creep *c = &s->creeps[i];
            if (!c->active || c->id != p->target_id) continue;
            target_alive = 1;

            float dx = c->x - p->x, dy = c->y - p->y;
            float d2 = dx * dx + dy * dy;

            /* Re-aim */
            if (d2 > 0.001f) {
                float d = sqrtf(d2);
                p->vx = dx / d * PROJECTILE_SPEED;
                p->vy = dy / d * PROJECTILE_SPEED;
            }

            /* Hit threshold: 8 px radius */
            if (d2 <= 64.0f) {
                if (p->splash_radius > 0.0f) {
                    float sr2 = p->splash_radius * p->splash_radius;
                    int   nc  = s->creep_count; /* snapshot — avoid re-counting children */
                    for (int j = 0; j < nc; j++) {
                        if (!s->creeps[j].active) continue;
                        float ex = s->creeps[j].x - p->x;
                        float ey = s->creeps[j].y - p->y;
                        if (ex * ex + ey * ey <= sr2)
                            apply_damage(s, j, p->damage, p->tower_type);
                    }
                    spawn_explosion(s, p->x, p->y, COLOR_PROJECTILE, 6);
                } else {
                    apply_damage(s, i, p->damage, p->tower_type);
                }
                p->active = 0;
                return;
            }
            break; /* target found but not yet in range — keep flying */
        }
        if (!target_alive) { p->active = 0; return; }
    }

    p->x += p->vx * dt;
    p->y += p->vy * dt;

    /* Cull off-screen */
    if (p->x < -20.0f || p->x > CANVAS_W + 20.0f ||
        p->y < -20.0f || p->y > CANVAS_H + 20.0f)
        p->active = 0;
}

/* =========================================================================
 * WAVE MANAGEMENT
 * ========================================================================= */

static int check_wave_complete(GameState *s)
{
    if (s->current_wave < 1 || s->current_wave > WAVE_COUNT) return 0;
    const WaveDef *wd = &g_waves[s->current_wave - 1];
    if (s->wave_spawn_index < wd->count) return 0;
    for (int i = 0; i < s->creep_count; i++) {
        if (s->creeps[i].active) return 0;
    }
    return 1;
}

static void start_wave(GameState *s)
{
    if (s->current_wave < 1 || s->current_wave > WAVE_COUNT) return;

    /* Interest on saved gold */
    int interest = (int)((float)s->player_gold * INTEREST_RATE);
    if (interest > 0) {
        s->player_gold += interest;
        char txt[16];
        snprintf(txt, sizeof(txt), "+%d int", interest);
        spawn_particle(s, (float)(PANEL_X + PANEL_W / 2), 60.0f,
                       0.0f, -20.0f, 2.0f, COLOR_GOLD_TEXT, 1, txt);
    }

    const WaveDef *wd = &g_waves[s->current_wave - 1];
    s->wave_spawn_index    = 0;
    s->wave_spawn_timer    = 0.0f;
    s->remaining_wave_time = (float)wd->count * wd->spawn_interval + 10.0f;

    change_phase(s, GAME_PHASE_WAVE);
}

static void send_wave_early(GameState *s)
{
    if (s->current_wave >= WAVE_COUNT) return;

    int bonus = (int)(s->remaining_wave_time * GOLD_PER_SECOND_EARLY);
    s->player_gold += bonus;

    char txt[16];
    snprintf(txt, sizeof(txt), "+%d early", bonus);
    spawn_particle(s, GRID_PIXEL_W * 0.5f, GRID_PIXEL_H * 0.5f,
                   0.0f, -30.0f, 1.5f, COLOR_GOLD_TEXT, 1, txt);

    s->current_wave++;
    s->wave_spawn_index = 0;
    s->wave_spawn_timer = 0.0f;
    const WaveDef *wd = &g_waves[s->current_wave - 1];
    s->remaining_wave_time = (float)wd->count * wd->spawn_interval + 10.0f;
}

static void update_wave(GameState *s, float dt)
{
    if (s->current_wave < 1 || s->current_wave > WAVE_COUNT) return;
    const WaveDef *wd = &g_waves[s->current_wave - 1];

    /* ---- Spawn ---- */
    if (s->wave_spawn_index < wd->count) {
        s->wave_spawn_timer -= dt;
        if (s->wave_spawn_timer <= 0.0f) {
            float hp = wd->base_hp_override > 0.0f
                       ? wd->base_hp_override
                       : (float)CREEP_DEFS[wd->creep_type].base_hp
                         * powf(HP_SCALE_PER_WAVE, (float)(s->current_wave - 1));
            spawn_creep(s, wd->creep_type, hp);
            s->wave_spawn_index++;
            s->wave_spawn_timer = wd->spawn_interval;
        }
    }

    /* ---- Move creeps (snapshot count; newly spawned children skip this frame) ---- */
    int nc = s->creep_count;
    for (int i = 0; i < nc; i++) {
        Creep *c = &s->creeps[i];
        if (!c->active) continue;
        if (c->is_flying) creep_flying_move(s, c, dt);
        else              creep_move_toward_exit(s, c, dt);
    }

    /* ---- Tower fire ---- */
    for (int i = 0; i < s->tower_count; i++)
        tower_update(s, &s->towers[i], dt);

    /* ---- Projectiles (snapshot count) ---- */
    int np = s->projectile_count;
    for (int i = 0; i < np; i++)
        projectile_update(s, &s->projectiles[i], dt);

    /* ---- Particles ---- */
    for (int i = 0; i < s->particle_count; i++) {
        Particle *p = &s->particles[i];
        if (!p->active) continue;
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->lifetime -= dt;
        if (p->lifetime <= 0.0f) p->active = 0;
    }

    compact_creeps(s);
    compact_projectiles(s);
    compact_particles(s);

    s->remaining_wave_time -= dt;
    if (s->remaining_wave_time < 0.0f) s->remaining_wave_time = 0.0f;

    if (check_wave_complete(s))
        change_phase(s, GAME_PHASE_WAVE_CLEAR);
}

/* =========================================================================
 * PLACEMENT INPUT  (shared by PLACING and WAVE phases)
 * ========================================================================= */

static void handle_placement_input(GameState *s)
{
    MouseState *m = &s->mouse;

    /* Right-click deselects everything */
    if (m->right_pressed) {
        s->selected_tower_type = TOWER_NONE;
        s->selected_tower_idx  = -1;
        return;
    }

    if (!m->left_pressed) return;

    /* ---- Panel ---- */
    if (m->x >= PANEL_X) {
        /* Shop: one button per tower type (skip TOWER_NONE=0) */
        for (int ti = 1; ti < TOWER_COUNT; ti++) {
            int by = SHOP_BTN_Y0 + (ti - 1) * SHOP_BTN_H;
            if (m->y >= by && m->y < by + SHOP_BTN_H) {
                s->selected_tower_type = (TowerType)ti;
                s->selected_tower_idx  = -1;
                return;
            }
        }

        /* Tower info buttons — only when a placed tower is selected */
        if (s->selected_tower_idx >= 0 &&
            s->selected_tower_idx < s->tower_count &&
            s->towers[s->selected_tower_idx].active)
        {
            Tower *t = &s->towers[s->selected_tower_idx];

            if (m->y >= MODE_BTN_Y && m->y < MODE_BTN_Y + MODE_BTN_H) {
                t->target_mode = (TargetMode)((t->target_mode + 1) % TARGET_COUNT);
                return;
            }
            if (m->y >= SELL_BTN_Y && m->y < SELL_BTN_Y + SELL_BTN_H) {
                s->player_gold += t->sell_value;
                s->grid[t->row * GRID_COLS + t->col] = CELL_EMPTY;
                t->active = 0;
                s->selected_tower_idx = -1;
                bfs_fill_distance(s);
                game_play_sound(&s->audio, SFX_TOWER_SELL);
                return;
            }
        }

        /* Start Wave / Send Next */
        if (m->y >= START_BTN_Y && m->y < START_BTN_Y + START_BTN_H) {
            if (s->phase == GAME_PHASE_PLACING)
                start_wave(s);
            else if (s->phase == GAME_PHASE_WAVE)
                send_wave_early(s);
            return;
        }
        return; /* click in panel but not on any active button */
    }

    /* ---- Grid ---- */
    if (m->x < 0 || m->x >= GRID_PIXEL_W || m->y < 0 || m->y >= GRID_PIXEL_H) return;

    int col = m->x / CELL_SIZE;
    int row = m->y / CELL_SIZE;

    /* Click on an existing tower → select it */
    for (int i = 0; i < s->tower_count; i++) {
        Tower *t = &s->towers[i];
        if (t->active && t->col == col && t->row == row) {
            s->selected_tower_idx  = i;
            s->selected_tower_type = TOWER_NONE;
            return;
        }
    }

    /* Try to place the currently selected tower type */
    if (s->selected_tower_type == TOWER_NONE) return;

    const TowerDef *def = &TOWER_DEFS[s->selected_tower_type];
    if (s->player_gold < def->cost) { s->shop_error_timer = 0.5f; return; }
    if (!can_place_tower(s, col, row)) return;

    /* Allocate slot */
    int idx = -1;
    for (int i = 0; i < s->tower_count; i++) {
        if (!s->towers[i].active) { idx = i; break; }
    }
    if (idx < 0) {
        if (s->tower_count >= MAX_TOWERS) return;
        idx = s->tower_count++;
    }

    Tower *t = &s->towers[idx];
    memset(t, 0, sizeof(*t));
    t->col         = col;
    t->row         = row;
    t->cx          = col * CELL_SIZE + CELL_SIZE / 2;
    t->cy          = row * CELL_SIZE + CELL_SIZE / 2;
    t->type        = s->selected_tower_type;
    t->range       = def->range_cells * (float)CELL_SIZE;
    t->damage      = def->damage;
    t->fire_rate   = def->fire_rate;
    t->target_mode = TARGET_FIRST;
    t->target_id   = -1;
    t->active      = 1;
    t->sell_value  = (int)((float)def->cost * SELL_RATIO);
    t->place_flash = 0.3f;

    s->grid[row * GRID_COLS + col] = CELL_TOWER;
    s->player_gold -= def->cost;
    bfs_fill_distance(s);
    game_play_sound(&s->audio, SFX_TOWER_PLACE);
}

/* =========================================================================
 * PUBLIC: GAME UPDATE
 * ========================================================================= */

void game_update(GameState *s, float dt)
{
    /* Cap dt so a debugger pause can't explode the simulation */
    if (dt > 0.1f) dt = 0.1f;

    /* Hover cell under mouse cursor */
    if (s->mouse.x >= 0 && s->mouse.x < GRID_PIXEL_W &&
        s->mouse.y >= 0 && s->mouse.y < GRID_PIXEL_H)
    {
        s->hover_col = s->mouse.x / CELL_SIZE;
        s->hover_row = s->mouse.y / CELL_SIZE;
    } else {
        s->hover_col = -1;
        s->hover_row = -1;
    }

    /* Precompute hover placement validity for the renderer */
    s_hover_valid = 0;
    if (s->hover_col >= 0 && s->selected_tower_type != TOWER_NONE)
        s_hover_valid = can_place_tower(s, s->hover_col, s->hover_row);

    if (s->shop_error_timer > 0.0f) s->shop_error_timer -= dt;

    switch (s->phase) {

        case GAME_PHASE_TITLE:
            if (s->mouse.left_pressed)
                change_phase(s, GAME_PHASE_PLACING);
            break;

        case GAME_PHASE_PLACING:
            handle_placement_input(s);
            /* Keep particles alive between waves */
            for (int i = 0; i < s->particle_count; i++) {
                Particle *p = &s->particles[i];
                if (!p->active) continue;
                p->x += p->vx * dt; p->y += p->vy * dt;
                p->lifetime -= dt;
                if (p->lifetime <= 0.0f) p->active = 0;
            }
            /* Tick place_flash on towers */
            for (int i = 0; i < s->tower_count; i++) {
                if (s->towers[i].active && s->towers[i].place_flash > 0.0f)
                    s->towers[i].place_flash -= dt;
            }
            compact_particles(s);
            break;

        case GAME_PHASE_WAVE:
            handle_placement_input(s);
            update_wave(s, dt);
            if (s->player_lives <= 0 && s->phase == GAME_PHASE_WAVE)
                change_phase(s, GAME_PHASE_GAME_OVER);
            break;

        case GAME_PHASE_WAVE_CLEAR:
            s->phase_timer -= dt;
            for (int i = 0; i < s->particle_count; i++) {
                Particle *p = &s->particles[i];
                if (!p->active) continue;
                p->x += p->vx * dt; p->y += p->vy * dt;
                p->lifetime -= dt;
                if (p->lifetime <= 0.0f) p->active = 0;
            }
            compact_particles(s);
            if (s->phase_timer <= 0.0f) {
                s->current_wave++;
                if (s->current_wave > WAVE_COUNT)
                    change_phase(s, GAME_PHASE_VICTORY);
                else
                    change_phase(s, GAME_PHASE_PLACING);
            }
            break;

        case GAME_PHASE_GAME_OVER:
        case GAME_PHASE_VICTORY:
            if (s->mouse.left_pressed)
                game_init(s);
            break;

        default:
            break;
    }
}

/* =========================================================================
 * RENDER HELPERS
 * ========================================================================= */

static const char *target_mode_name(TargetMode m)
{
    switch (m) {
        case TARGET_FIRST:     return "First";
        case TARGET_LAST:      return "Last";
        case TARGET_STRONGEST: return "Strong";
        case TARGET_WEAKEST:   return "Weak";
        case TARGET_CLOSEST:   return "Close";
        default:               return "?";
    }
}

/* =========================================================================
 * PUBLIC: GAME RENDER
 * ========================================================================= */

void game_render(const GameState *s, Backbuffer *bb)
{
    /* ---- Clear canvas ---- */
    draw_rect(bb, 0, 0, CANVAS_W, CANVAS_H, GAME_RGB(0x22, 0x22, 0x22));

    /* ==================================================================
     * GRID
     * ================================================================== */
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            int px  = c * CELL_SIZE;
            int py  = r * CELL_SIZE;
            int idx = r * GRID_COLS + c;

            uint32_t col;
            switch (s->grid[idx]) {
                case CELL_ENTRY: col = COLOR_ENTRY_CELL; break;
                case CELL_EXIT:  col = COLOR_EXIT_CELL;  break;
                default:         col = ((c + r) & 1) ? COLOR_GRID_ODD : COLOR_GRID_EVEN; break;
            }
            draw_rect(bb, px, py, CELL_SIZE, CELL_SIZE, col);
        }
    }

    /* Hover placement preview */
    if (s->hover_col >= 0 && s->selected_tower_type != TOWER_NONE) {
        int px = s->hover_col * CELL_SIZE;
        int py = s->hover_row * CELL_SIZE;
        draw_rect_blend(bb, px, py, CELL_SIZE, CELL_SIZE,
                        s_hover_valid ? COLOR_PREVIEW_OK : COLOR_PREVIEW_BAD);
        if (s_hover_valid) {
            int   rcx = s->hover_col * CELL_SIZE + CELL_SIZE / 2;
            int   rcy = s->hover_row * CELL_SIZE + CELL_SIZE / 2;
            int   rng = (int)(TOWER_DEFS[s->selected_tower_type].range_cells
                              * (float)CELL_SIZE);
            draw_circle_outline(bb, rcx, rcy, rng, GAME_RGB(0xAA, 0xAA, 0xAA));
        }
    }

    /* ==================================================================
     * TOWERS
     * ================================================================== */
    for (int i = 0; i < s->tower_count; i++) {
        const Tower *t = &s->towers[i];
        if (!t->active) continue;

        int px = t->col * CELL_SIZE;
        int py = t->row * CELL_SIZE;

        /* Highlight selected tower with a white border */
        if (i == s->selected_tower_idx)
            draw_rect(bb, px - 1, py - 1, CELL_SIZE + 2, CELL_SIZE + 2, COLOR_WHITE);

        /* Body: brief flash on placement */
        uint32_t body_col = (t->place_flash > 0.0f) ? COLOR_WHITE
                                                     : TOWER_DEFS[t->type].color;
        draw_rect(bb, px + 2, py + 2, CELL_SIZE - 4, CELL_SIZE - 4, body_col);

        /* Barrel line */
        int bex = t->cx + (int)(cosf(t->angle) * BARREL_LEN);
        int bey = t->cy + (int)(sinf(t->angle) * BARREL_LEN);
        draw_line(bb, t->cx, t->cy, bex, bey, COLOR_BARREL);
    }

    /* ==================================================================
     * CREEPS
     * ================================================================== */
    for (int i = 0; i < s->creep_count; i++) {
        const Creep *c = &s->creeps[i];
        if (!c->active) continue;

        int cx = (int)c->x;
        int cy = (int)c->y;
        int r  = CREEP_DEFS[c->type].size / 2;
        if (r < 1) r = 1;

        draw_circle(bb, cx, cy, r, CREEP_DEFS[c->type].color);

        /* Status tints */
        if (c->slow_timer  > 0.0f)
            draw_circle(bb, cx, cy, r, GAME_RGBA(0x88, 0xCC, 0xFF, 0x70));
        if (c->stun_timer > 0.0f)
            draw_circle(bb, cx, cy, r, GAME_RGBA(0xFF, 0xFF, 0x00, 0x70));

        /* HP bar */
        int bar_w = CELL_SIZE;
        int bar_x = cx - bar_w / 2;
        int bar_y = cy - r - 5;
        draw_rect(bb, bar_x, bar_y, bar_w, 3, GAME_RGB(0x33, 0x33, 0x33));
        float hp_ratio = (c->max_hp > 0.0f) ? (c->hp / c->max_hp) : 0.0f;
        if (hp_ratio < 0.0f) hp_ratio = 0.0f;
        if (hp_ratio > 1.0f) hp_ratio = 1.0f;
        int filled = (int)((float)bar_w * hp_ratio);
        if (filled > 0)
            draw_rect(bb, bar_x, bar_y, filled, 3,
                      hp_ratio > 0.5f ? COLOR_HP_FULL : COLOR_HP_LOW);
    }

    /* ==================================================================
     * PROJECTILES  — small 4×4 dots
     * ================================================================== */
    for (int i = 0; i < s->projectile_count; i++) {
        const Projectile *p = &s->projectiles[i];
        if (!p->active) continue;
        draw_rect(bb, (int)p->x - 2, (int)p->y - 2, 4, 4, COLOR_PROJECTILE);
    }

    /* ==================================================================
     * PARTICLES
     * ================================================================== */
    for (int i = 0; i < s->particle_count; i++) {
        const Particle *p = &s->particles[i];
        if (!p->active) continue;
        if (p->text[0] != '\0') {
            draw_text(bb, (int)p->x, (int)p->y, p->text, p->color, 1);
        } else {
            int half = p->size / 2;
            if (half < 1) half = 1;
            draw_rect(bb, (int)p->x - half, (int)p->y - half,
                      p->size, p->size, p->color);
        }
    }

    /* ==================================================================
     * UI PANEL  — drawn last so it always sits on top of the play area
     * ================================================================== */

    /* Background + left border */
    draw_rect(bb, PANEL_X, 0, PANEL_W, CANVAS_H, COLOR_PANEL_BG);
    draw_line(bb, PANEL_X, 0, PANEL_X, CANVAS_H - 1, COLOR_PANEL_BORDER);

    /* Title */
    draw_text(bb, PANEL_X + 4, 5, "DTD", COLOR_WHITE, 2);

    /* Counters */
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "Wave:%d/%d", s->current_wave, WAVE_COUNT);
        draw_text(bb, PANEL_X + 4, 26, buf, COLOR_WHITE, 1);
        snprintf(buf, sizeof(buf), "Lives:%d", s->player_lives);
        draw_text(bb, PANEL_X + 4, 40, buf, COLOR_LIVES_TEXT, 1);
        snprintf(buf, sizeof(buf), "Gold:%d", s->player_gold);
        draw_text(bb, PANEL_X + 4, 54, buf, COLOR_GOLD_TEXT, 1);
    }

    /* Separator */
    draw_line(bb, PANEL_X + 2, 68, PANEL_X + PANEL_W - 2, 68, COLOR_PANEL_BORDER);

    /* Tower shop buttons */
    for (int ti = 1; ti < TOWER_COUNT; ti++) {
        const TowerDef *def = &TOWER_DEFS[ti];
        int      by  = SHOP_BTN_Y0 + (ti - 1) * SHOP_BTN_H;
        int      bh  = SHOP_BTN_H - 2;
        uint32_t bg;

        if ((TowerType)ti == s->selected_tower_type)
            bg = COLOR_BTN_SELECTED;
        else if (s->shop_error_timer > 0.0f && s->player_gold < def->cost)
            bg = COLOR_BTN_ERROR;
        else if (s->player_gold < def->cost)
            bg = GAME_RGB(0x30, 0x20, 0x20);
        else
            bg = COLOR_BTN_NORMAL;

        draw_rect(bb, SHOP_BTN_X, by, SHOP_BTN_W, bh, bg);

        /* Colour swatch */
        draw_rect(bb, SHOP_BTN_X + 2, by + 2, 8, bh - 4, def->color);

        /* Label */
        char lbl[20];
        snprintf(lbl, sizeof(lbl), "%s $%d", def->name, def->cost);
        draw_text(bb, SHOP_BTN_X + 13, by + (bh - 8) / 2, lbl, COLOR_WHITE, 1);
    }

    /* Separator below shop */
    int sep2y = SHOP_BTN_Y0 + 7 * SHOP_BTN_H + 2;
    draw_line(bb, PANEL_X + 2, sep2y, PANEL_X + PANEL_W - 2, sep2y, COLOR_PANEL_BORDER);

    /* Selected placed-tower info */
    if (s->selected_tower_idx >= 0 &&
        s->selected_tower_idx < s->tower_count &&
        s->towers[s->selected_tower_idx].active)
    {
        const Tower *t = &s->towers[s->selected_tower_idx];

        draw_text(bb, PANEL_X + 4, SEL_INFO_Y,
                  TOWER_DEFS[t->type].name, TOWER_DEFS[t->type].color, 1);

        /* Targeting mode button */
        draw_rect(bb, SHOP_BTN_X, MODE_BTN_Y, SHOP_BTN_W, MODE_BTN_H, COLOR_BTN_NORMAL);
        char mbuf[20];
        snprintf(mbuf, sizeof(mbuf), "Tgt:%s", target_mode_name(t->target_mode));
        draw_text(bb, SHOP_BTN_X + 4, MODE_BTN_Y + (MODE_BTN_H - 8) / 2, mbuf, COLOR_WHITE, 1);

        /* Sell button */
        draw_rect(bb, SHOP_BTN_X, SELL_BTN_Y, SHOP_BTN_W, SELL_BTN_H, COLOR_BTN_ERROR);
        char sbuf[20];
        snprintf(sbuf, sizeof(sbuf), "Sell $%d", t->sell_value);
        draw_text(bb, SHOP_BTN_X + 4, SELL_BTN_Y + (SELL_BTN_H - 8) / 2, sbuf, COLOR_GOLD_TEXT, 1);
    }

    /* Start Wave / Send Next button */
    {
        const char *btn_txt = "Start Wave";
        uint32_t    btn_col = COLOR_BTN_NORMAL;

        if (s->phase == GAME_PHASE_WAVE && s->current_wave < WAVE_COUNT) {
            btn_txt = "Send Next";
            btn_col = GAME_RGB(0x22, 0x66, 0x44);
        } else if (s->phase != GAME_PHASE_PLACING) {
            btn_col = GAME_RGB(0x33, 0x33, 0x33);
        }

        draw_rect(bb, SHOP_BTN_X, START_BTN_Y, SHOP_BTN_W, START_BTN_H, btn_col);
        draw_text(bb, SHOP_BTN_X + 4, START_BTN_Y + (START_BTN_H - 8) / 2,
                  btn_txt, COLOR_WHITE, 1);
    }

    /* ==================================================================
     * PHASE OVERLAYS  — drawn on top of everything, including the panel
     * ================================================================== */

    if (s->phase == GAME_PHASE_TITLE) {
        draw_rect_blend(bb, 0, 0, CANVAS_W, CANVAS_H,
                        GAME_RGBA(0x00, 0x00, 0x00, 0xC0));
        draw_text(bb, 212, CANVAS_H / 2 - 30, "DESKTOP TOWER", COLOR_WHITE, 2);
        draw_text(bb, 308, CANVAS_H / 2 - 8,  "DEFENSE",       COLOR_WHITE, 2);
        draw_text(bb, 288, CANVAS_H / 2 + 22,
                  "Click anywhere to start", COLOR_WHITE, 1);

    } else if (s->phase == GAME_PHASE_WAVE_CLEAR) {
        draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                        GAME_RGBA(0x00, 0x44, 0x00, 0x80));
        draw_text(bb, GRID_PIXEL_W / 2 - 64, GRID_PIXEL_H / 2 - 8,
                  "Wave Clear!", COLOR_WHITE, 2);

    } else if (s->phase == GAME_PHASE_GAME_OVER) {
        draw_rect_blend(bb, 0, 0, CANVAS_W, CANVAS_H,
                        GAME_RGBA(0x44, 0x00, 0x00, 0xC0));
        draw_text(bb, CANVAS_W / 2 - 72, CANVAS_H / 2 - 20,
                  "GAME OVER", COLOR_LIVES_TEXT, 2);
        draw_text(bb, CANVAS_W / 2 - 64, CANVAS_H / 2 + 14,
                  "Click to restart", COLOR_WHITE, 1);

    } else if (s->phase == GAME_PHASE_VICTORY) {
        draw_rect_blend(bb, 0, 0, CANVAS_W, CANVAS_H,
                        GAME_RGBA(0x00, 0x22, 0x44, 0xC0));
        draw_text(bb, CANVAS_W / 2 - 64, CANVAS_H / 2 - 22,
                  "VICTORY!", COLOR_GOLD_TEXT, 2);
        draw_text(bb, CANVAS_W / 2 - 88, CANVAS_H / 2 + 8,
                  "All 40 waves survived!", COLOR_WHITE, 1);
        draw_text(bb, CANVAS_W / 2 - 64, CANVAS_H / 2 + 24,
                  "Click to restart", COLOR_WHITE, 1);
    }
}
