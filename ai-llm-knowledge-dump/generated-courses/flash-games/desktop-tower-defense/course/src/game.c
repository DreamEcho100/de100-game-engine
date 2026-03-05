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
#include "sprites.h"

/* Set USE_SPRITES to 1 to replace procedural rect/circle drawing with
 * sprite calls (placeholder colored rects until a real atlas is loaded).
 * Set to 0 to keep the original procedural rendering. */
#define USE_SPRITES 0

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
#define UPGRADE_BTN_Y (SELL_BTN_Y + SELL_BTN_H + 4)
#define UPGRADE_BTN_H 20
#define START_BTN_Y  (CANVAS_H - 42)
#define START_BTN_H  36

/* Mod-select screen layout (grid area: 0..GRID_PIXEL_W x 0..GRID_PIXEL_H) */
#define MOD_BTN_X    30
#define MOD_BTN_W    540
#define MOD_BTN_H    42
#define MOD_BTN_GAP  8
#define MOD_BTN_Y0   80   /* y of first mod button */

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
    /*                  name      cost  dmg  range  rate        color        aoe  splash  up_cost        up_dmg_mult */
    [TOWER_NONE]   = { "",          0,   0,  0.0f,  0.0f, 0,               0,   0.0f, {  0,   0}, {1.00f, 1.00f} },
    [TOWER_PELLET] = { "Pellet",    5,   5,  3.0f,  2.5f, COLOR_PELLET,    0,   0.0f, { 25,  50}, {1.50f, 2.25f} },
    [TOWER_SQUIRT] = { "Squirt",   15,  10,  3.5f,  1.5f, COLOR_SQUIRT,    0,   0.0f, { 35,  70}, {1.50f, 2.25f} },
    [TOWER_DART]   = { "Dart",     40,  35,  4.0f,  1.0f, COLOR_DART,      0,   0.0f, { 45,  90}, {1.50f, 2.25f} },
    [TOWER_SNAP]   = { "Snap",     75, 125,  4.0f,  0.4f, COLOR_SNAP,      0,   0.0f, { 80, 160}, {1.50f, 2.25f} },
    [TOWER_SWARM]  = { "Swarm",   125,  15,  4.0f,  0.7f, COLOR_SWARM,     1,  30.0f, { 65, 130}, {1.50f, 2.25f} },
    [TOWER_FROST]  = { "Frost",    60,   0,  3.5f,  1.0f, COLOR_FROST,     1,   0.0f, { 50, 100}, {1.00f, 1.00f} },
    [TOWER_BASH]   = { "Bash",     90,  40,  3.0f,  0.5f, COLOR_BASH,      1,  40.0f, { 55, 110}, {1.50f, 2.25f} },
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
static void upgrade_tower(GameState *s, int idx);
static void apply_night_bonus(GameState *s, Tower *t);

/* =========================================================================
 * MOD HELPERS
 * ========================================================================= */

/* Returns the creep-speed multiplier for the current weather state.
 * Phase 0=clear (1.0×), 1=rain (0.8×), 2=wind (1.3×), 3=storm (0.65×) */
static float get_weather_speed_mult(const GameState *s)
{
    if (s->active_mod != MOD_WEATHER) return 1.0f;
    switch (s->weather_phase) {
        case 1: return 0.8f;   /* RAIN  — heavy rain slows movement */
        case 2: return 1.3f;   /* WIND  — tailwind speeds creeps     */
        case 3: return 0.65f;  /* STORM — worst conditions, slowest  */
        default: return 1.0f;  /* CLEAR */
    }
}

/* Apply night-mode bonuses/penalties to a freshly-placed tower. */
static void apply_night_bonus(GameState *s, Tower *t)
{
    if (s->active_mod != MOD_NIGHT) return;
    if (t->type == TOWER_DART) {
        t->range   *= 1.2f;  /* dart: sniper bonus at night */
    } else if (t->type == TOWER_PELLET) {
        t->fire_rate *= 1.5f; /* pellet: machine-gun bonus at night */
    } else {
        t->range   *= 0.75f; /* all other towers: reduced range at night */
    }
}

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
            /* MOD_TERRAIN: mountains are impassable to creeps */
            if (s->active_mod == MOD_TERRAIN &&
                s->terrain[ni] == CELL_MOUNTAIN) continue;
            /* MOD_WEATHER: flooded cells are temporarily impassable */
            if (s->active_mod == MOD_WEATHER && s->weather_flood[ni]) continue;
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
    /* MOD_TERRAIN: cannot place on water or swamp cells */
    if (s->active_mod == MOD_TERRAIN &&
        (s->terrain[idx] == CELL_WATER || s->terrain[idx] == CELL_SWAMP)) return 0;

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
            /* MOD_TERRAIN: mountains block the path like towers do */
            if (s->active_mod == MOD_TERRAIN &&
                s->terrain[ni] == CELL_MOUNTAIN) continue;
            /* MOD_WEATHER: flooded cells block path for legality check */
            if (s->active_mod == MOD_WEATHER && s->weather_flood[ni]) continue;
            visited[ni] = 1;
            queue[tail++] = ni;
        }
    }

    s->grid[idx] = CELL_EMPTY; /* restore */
    return reachable;
}

/* =========================================================================
 * MOD STATE INITIALIZER
 * Called once after the player picks a mod from the selection screen.
 * Scatters terrain cells, pre-computes terrain_slow[], and re-runs BFS
 * for mods that need it.  Must be called AFTER active_mod is set and
 * AFTER the basic grid (CELL_ENTRY / CELL_EXIT) has been established.
 * ========================================================================= */
static void init_mod_state(GameState *s)
{
    /* Baseline: all cells move at normal speed */
    for (int i = 0; i < GRID_ROWS * GRID_COLS; i++) s->terrain_slow[i] = 1.0f;
    /* Clear any leftover terrain from a previous game */
    memset(s->terrain, 0, sizeof(s->terrain));
    memset(s->weather_flood, 0, sizeof(s->weather_flood));

    if (s->active_mod == MOD_TERRAIN) {
        /* =================================================================
         * TERRAIN MAP  (30×20 grid, ENTRY row=10 col=0, EXIT row=10 col=29)
         *
         * The natural "straight right" route (row 10) is blocked by a
         * central mountain wall, forcing creeps north OR south:
         *
         *   Mountain Wall: cols 12-15, rows 0-6   (north half)
         *                  cols 12-15, rows 13-19  (south half)
         *   → creates a 6-cell-wide chokepoint in rows 7-12
         *
         *   River (WATER, 50% speed): cols 4-6, rows 0-7   (NW stream)
         *                             cols 23-25, rows 12-19 (SE stream)
         *   → north detour is slowed; south detour is also slowed
         *
         *   Swamp (CELL_SWAMP, 25% speed): cols 7-11, rows 14-18
         *   → south-center detour is extremely punishing
         *   → Frost/Squirt towers near swamp edge dominate slow creeps
         *
         * Strategic summary:
         *   North route: slightly slowed by NW river, but best speed overall
         *   South route: slowed by SE river AND swamp — very dangerous
         *   Players can choke both routes at the mountain wall gap (rows 7-12)
         * =================================================================*/

        /* Mountain wall: north section */
        for (int r = 7; r <= 9; r++) {
            for (int c = 12; c <= 19; c++) {
                int i = r * GRID_COLS + c;
                if (s->grid[i] == CELL_EMPTY) s->terrain[i] = CELL_MOUNTAIN;
            }
        }
        /* Mountain wall: south section */
        for (int r = 12; r <= 13; r++) {
            for (int c = 12; c <= 20; c++) {
                int i = r * GRID_COLS + c;
                if (s->grid[i] == CELL_EMPTY) s->terrain[i] = CELL_MOUNTAIN;
            }
        }

        /* River: northwest stream (rows 0-7, cols 4-6) */
        for (int r = 4; r <= 13; r++) {
            for (int c = 4; c <= 6; c++) {
                int i = r * GRID_COLS + c;
                if (s->grid[i] == CELL_EMPTY && s->terrain[i] == 0)
                    s->terrain[i] = CELL_WATER;
            }
        }
        /* River: southeast stream (rows 12-19, cols 23-25) */
        for (int r = 12; r <= 19; r++) {
            for (int c = 23; c <= 25; c++) {
                int i = r * GRID_COLS + c;
                if (s->grid[i] == CELL_EMPTY && s->terrain[i] == 0)
                    s->terrain[i] = CELL_WATER;
            }
        }

        /* Swamp: south-center bog (rows 14-18, cols 7-11) */
        for (int r = 14; r <= 18; r++) {
            for (int c = 7; c <= 11; c++) {
                int i = r * GRID_COLS + c;
                if (s->grid[i] == CELL_EMPTY && s->terrain[i] == 0)
                    s->terrain[i] = CELL_SWAMP;
            }
        }

        /* Pre-compute speed multipliers */
        for (int i = 0; i < GRID_ROWS * GRID_COLS; i++) {
            if      (s->terrain[i] == CELL_WATER)    s->terrain_slow[i] = 0.5f;
            else if (s->terrain[i] == CELL_SWAMP)    s->terrain_slow[i] = 0.25f;
            else                                      s->terrain_slow[i] = 1.0f;
        }
        /* Mountains act as BFS walls — re-run BFS now that terrain is set */
        bfs_fill_distance(s);

    } else if (s->active_mod == MOD_WEATHER) {
        /* Weather: init timing so the first phase is CLEAR */
        s->weather_phase      = 0;
        s->prev_weather_phase = 0;
        s->weather_timer      = 0.0f;
        s->lightning_timer    = 15.0f;  /* first lightning in 15 s */
        s->lightning_flash    = 0.0f;
    }
    /* Night/Boss mods need no extra setup — per-frame logic handles them */
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

    /* terrain_slow baseline: 1.0 for all cells.
     * init_mod_state() is called after the player picks a mod and will
     * overwrite this for MOD_TERRAIN (water = 0.5, re-runs BFS). */
    for (int i = 0; i < GRID_ROWS * GRID_COLS; i++) s->terrain_slow[i] = 1.0f;
    s->mod_hover_idx = -1;

    /* CRITICAL: initialise audio state (volumes, music drone, SoundDef indices).
     * Must be called AFTER memset so we overwrite the zero-fill with real values.
     * Without this call master_volume stays 0 and no audio is ever heard. */
    game_audio_init(&s->audio);

    /* Sprite system: NULL = placeholder mode (colored rects).
     * Change to sprites_init("assets/sprites.png") once you have an atlas. */
    sprites_init(NULL);
}

/* =========================================================================
 * PHASE CHANGE
 * ========================================================================= */

static void change_phase(GameState *s, GamePhase p)
{
    s->phase       = p;
    s->phase_timer = 0.0f;

    switch (p) {
        case GAME_PHASE_MOD_SELECT:
            /* Reset hover so no button appears pre-selected */
            s->mod_hover_idx = -1;
            break;
        case GAME_PHASE_WAVE_CLEAR:
            s->phase_timer = 3.0f;
            game_play_sound(&s->audio, SFX_WAVE_COMPLETE);
            break;
        case GAME_PHASE_WAVE:
            game_play_sound(&s->audio, SFX_WAVE_START);
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

    /* MOD_BOSS: boss creeps spawn with a 5-second damage shield, +30% speed
     * and +50% HP to make them more threatening. */
    if (type == CREEP_BOSS && s->active_mod == MOD_BOSS) {
        c->shield_timer = 5.0f;
        c->speed       *= 1.3f;
        c->hp          *= 1.5f;
        c->max_hp       = c->hp;
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
    /* MOD_BOSS: boss is immune while its spawn shield is active */
    if (s->active_mod == MOD_BOSS && c->shield_timer > 0.0f) return;
    c->hp -= (float)effective_damage(dmg, c->type, tt);
    /* MOD_BOSS: when boss HP first crosses 50%, spawn 3 child creeps */
    if (s->active_mod == MOD_BOSS && c->type == CREEP_BOSS &&
        c->hp > 0.0f && c->hp <= c->max_hp * 0.5f && c->half_hp_spawned == 0)
    {
        c->half_hp_spawned = 1;
        float bx   = c->x;   float by   = c->y;
        int   bcol = c->col;  int   brow = c->row;
        for (int k = 0; k < 3; k++) {
            int ci = spawn_creep(s, CREEP_SPAWN_CHILD, 0.0f);
            if (ci >= 0) {
                s->creeps[ci].x   = bx;
                s->creeps[ci].y   = by;
                s->creeps[ci].col = bcol;
                s->creeps[ci].row = brow;
            }
        }
    }
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
    /* MOD_TERRAIN: apply per-cell terrain slow factor (water=0.5, swamp=0.25) */
    if (s->active_mod == MOD_TERRAIN) {
        int ci_t = c->row * GRID_COLS + c->col;
        spd *= s->terrain_slow[ci_t];
    }
    /* MOD_WEATHER: global weather speed multiplier */
    spd *= get_weather_speed_mult(s);

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
    /* MOD_WEATHER: global weather speed multiplier (flying creeps feel the wind too) */
    spd *= get_weather_speed_mult(s);

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
    static const WaveDef boss_mod_wave = { CREEP_BOSS, 1, 0.5f, 0 };
    if (s->active_mod == MOD_BOSS && (s->current_wave % 5 == 0))
        wd = &boss_mod_wave;
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
        game_play_sound(&s->audio, SFX_INTEREST_EARN);
    }

    const WaveDef *wd = &g_waves[s->current_wave - 1];
    /* MOD_BOSS: force a boss wave every 5 waves */
    static const WaveDef boss_mod_wave = { CREEP_BOSS, 1, 0.5f, 0 };
    if (s->active_mod == MOD_BOSS && (s->current_wave % 5 == 0))
        wd = &boss_mod_wave;

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
    game_play_sound(&s->audio, SFX_EARLY_SEND);

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
    /* MOD_BOSS: use boss wave data for multiples of 5 */
    static const WaveDef boss_mod_wave = { CREEP_BOSS, 1, 0.5f, 0 };
    if (s->active_mod == MOD_BOSS && (s->current_wave % 5 == 0))
        wd = &boss_mod_wave;

    /* MOD_WEATHER: advance 4-phase weather cycle (60 s total):
     *   0-20 s  = CLEAR (phase 0) — normal speed
     *  20-35 s  = RAIN  (phase 1) — 0.80× speed; flood cells block BFS
     *  35-50 s  = WIND  (phase 2) — 1.30× speed; no flood
     *  50-60 s  = STORM (phase 3) — 0.65× speed; flood + lightning
     * On phase transitions into RAIN or STORM: scatter flood cells + re-run BFS.
     * On phase transitions OUT of RAIN/STORM:  clear flood cells  + re-run BFS. */
    if (s->active_mod == MOD_WEATHER) {
        /* Advance lightning flash decay */
        if (s->lightning_flash > 0.0f) {
            s->lightning_flash -= dt * 3.0f;
            if (s->lightning_flash < 0.0f) s->lightning_flash = 0.0f;
        }

        s->weather_timer += dt;
        if (s->weather_timer >= 60.0f) s->weather_timer -= 60.0f;

        /* Map timer → phase */
        int new_phase;
        if      (s->weather_timer < 20.0f) new_phase = 0; /* CLEAR  */
        else if (s->weather_timer < 35.0f) new_phase = 1; /* RAIN   */
        else if (s->weather_timer < 50.0f) new_phase = 2; /* WIND   */
        else                               new_phase = 3; /* STORM  */
        s->weather_phase = new_phase;

        /* Detect phase transition */
        int was_wet = (s->prev_weather_phase == 1 || s->prev_weather_phase == 3);
        int is_wet  = (new_phase == 1 || new_phase == 3);

        if (new_phase != s->prev_weather_phase) {
            if (is_wet) {
                /* Scatter flood cells: a fixed set of 10 low-lying cells
                 * that represent natural drainage points on the map. */
                static const int flood_indices[] = {
                    /* Row 14-16, cols 5-8: south-centre depression */
                     14*30+5,  14*30+6,  14*30+7,  14*30+8,
                     15*30+5,  15*30+6,
                    /* Row 3-4, cols 20-22: north-east dip */
                      3*30+20,  3*30+21,
                      4*30+20,  4*30+21,
                };
                for (int fi = 0; fi < (int)(sizeof(flood_indices)/sizeof(flood_indices[0])); fi++) {
                    int idx = flood_indices[fi];
                    if (s->grid[idx] == CELL_EMPTY)
                        s->weather_flood[idx] = 1;
                }
                /* Storm: even wider flood area (add 4 more cells) */
                if (new_phase == 3) {
                    static const int storm_extra[] = {
                         16*30+5,  16*30+6,
                          5*30+20,  5*30+21,
                    };
                    for (int fi = 0; fi < 4; fi++) {
                        int idx = storm_extra[fi];
                        if (s->grid[idx] == CELL_EMPTY)
                            s->weather_flood[idx] = 1;
                    }
                }
                bfs_fill_distance(s);
                /* Spawn "flooded path!" particle notification */
                spawn_particle(s, GRID_PIXEL_W * 0.5f, GRID_PIXEL_H * 0.35f,
                               0.0f, -20.0f, 2.5f,
                               GAME_RGB(0x44, 0xAA, 0xFF), 1,
                               (new_phase == 3) ? "STORM: path flooded!" : "Rain: path flooded!");
            } else if (was_wet) {
                /* Clear flood cells and restore BFS */
                memset(s->weather_flood, 0, sizeof(s->weather_flood));
                bfs_fill_distance(s);
                if (new_phase == 2) {
                    spawn_particle(s, GRID_PIXEL_W * 0.5f, GRID_PIXEL_H * 0.35f,
                                   0.0f, -20.0f, 2.0f,
                                   GAME_RGB(0xCC, 0xCC, 0xFF), 1, "Wind: flooding cleared");
                }
            }

            /* Lightning timer (storm only) */
            if (new_phase == 3) s->lightning_timer = 8.0f;

            s->prev_weather_phase = new_phase;
        }

        /* Lightning countdown (storm phase) */
        if (new_phase == 3) {
            s->lightning_timer -= dt;
            if (s->lightning_timer <= 0.0f) {
                s->lightning_flash = 1.0f;           /* full brightness flash */
                s->lightning_timer = 6.0f + (float)(((int)(s->weather_timer * 100)) % 5);
            }
        }
    }

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
        /* MOD_BOSS: tick down the spawn shield */
        if (s->active_mod == MOD_BOSS && c->shield_timer > 0.0f) {
            c->shield_timer -= dt;
            if (c->shield_timer < 0.0f) c->shield_timer = 0.0f;
        }
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
 * TOWER UPGRADE
 * ========================================================================= */

static void upgrade_tower(GameState *s, int idx)
{
    if (idx < 0 || idx >= s->tower_count) return;
    Tower *t = &s->towers[idx];
    if (!t->active) return;
    if (t->upgrade_level >= 2) return;

    int cost = TOWER_DEFS[t->type].upgrade_cost[t->upgrade_level];
    if (s->player_gold < cost) { s->shop_error_timer = 0.5f; return; }

    s->player_gold -= cost;
    t->upgrade_level++;

    float mult = TOWER_DEFS[t->type].upgrade_damage_mult[t->upgrade_level - 1];
    t->damage = (int)((float)TOWER_DEFS[t->type].damage * mult);

    /* Special: Frost at lv1/lv2 gets range boost instead of damage boost */
    if (t->type == TOWER_FROST) {
        t->range = TOWER_DEFS[t->type].range_cells * CELL_SIZE
                   * (1.0f + 0.3f * (float)t->upgrade_level);
    }

    /* Recalculate sell value (include cumulative upgrade cost at SELL_RATIO) */
    int total_upgrade_cost = 0;
    for (int i = 0; i < t->upgrade_level; i++)
        total_upgrade_cost += TOWER_DEFS[t->type].upgrade_cost[i];
    t->sell_value = (int)((float)(TOWER_DEFS[t->type].cost + total_upgrade_cost) * SELL_RATIO);

    game_play_sound(&s->audio, SFX_TOWER_UPGRADE);
    spawn_particle(s, (float)t->cx, (float)t->cy - CELL_SIZE,
                   0.0f, -30.0f, 1.5f, COLOR_WHITE, 1, "UP!");
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
            if (m->y >= UPGRADE_BTN_Y && m->y < UPGRADE_BTN_Y + UPGRADE_BTN_H) {
                upgrade_tower(s, s->selected_tower_idx);
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
    /* MOD_TERRAIN: mountain towers get +20% range */
    if (s->active_mod == MOD_TERRAIN &&
        s->terrain[row * GRID_COLS + col] == CELL_MOUNTAIN) {
        t->range *= 1.2f;
    }
    /* MOD_NIGHT: apply per-tower-type night bonuses/penalties */
    apply_night_bonus(s, t);
    game_play_sound(&s->audio, SFX_TOWER_PLACE);
}

/* =========================================================================
 * PUBLIC: GAME UPDATE
 * ========================================================================= */

void game_update(GameState *s, float dt)
{
    /* Cap dt so a debugger pause can't explode the simulation */
    if (dt > 0.1f) dt = 0.1f;

    /* Advance music sequencer and ramp music volume each frame.
     * Without this call the ambient drone never reaches its target_volume
     * and game_audio_update()'s static freq_timer never ticks. */
    game_audio_update(&s->audio, dt);

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
            if (s->mouse.left_pressed) {
                /* Only advance on PLAY button click */
                int play_x = (CANVAS_W - 120) / 2;
                int play_y = 222;
                if (s->mouse.x >= play_x && s->mouse.x < play_x + 120 &&
                    s->mouse.y >= play_y && s->mouse.y < play_y + 36)
                    change_phase(s, GAME_PHASE_MOD_SELECT);
            }
            break;

        case GAME_PHASE_MOD_SELECT:
            s->mod_hover_idx = -1;
            for (int i = 0; i < MOD_COUNT; i++) {
                int by = MOD_BTN_Y0 + i * (MOD_BTN_H + MOD_BTN_GAP);
                if (s->mouse.x >= MOD_BTN_X && s->mouse.x < MOD_BTN_X + MOD_BTN_W &&
                    s->mouse.y >= by          && s->mouse.y < by + MOD_BTN_H) {
                    s->mod_hover_idx = i;
                    if (s->mouse.left_pressed) {
                        s->active_mod = (GameMod)i;
                        init_mod_state(s);   /* scatter terrain, re-run BFS */
                        change_phase(s, GAME_PHASE_PLACING);
                    }
                }
            }
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

            uint32_t cell_col;
            switch (s->grid[idx]) {
                case CELL_ENTRY: cell_col = COLOR_ENTRY_CELL; break;
                case CELL_EXIT:  cell_col = COLOR_EXIT_CELL;  break;
                default:
                    /* MOD_TERRAIN: draw solid terrain base colors instead of overlay */
                    if (s->active_mod == MOD_TERRAIN) {
                        switch (s->terrain[idx]) {
                            case CELL_WATER:
                                /* Animated shimmer: alternate between two blues each ~1 s */
                                cell_col = ((r + c + (int)(s->weather_timer)) & 1)
                                           ? GAME_RGB(0x22, 0x88, 0xCC)
                                           : GAME_RGB(0x11, 0x66, 0xAA);
                                break;
                            case CELL_MOUNTAIN:
                                /* Rocky grey with slight texture via checkerboard */
                                cell_col = ((r + c) & 1)
                                           ? GAME_RGB(0x66, 0x60, 0x58)
                                           : GAME_RGB(0x50, 0x4A, 0x44);
                                break;
                            case CELL_SWAMP:
                                /* Dark greenish-brown bog */
                                cell_col = ((r + c) & 1)
                                           ? GAME_RGB(0x2A, 0x3A, 0x18)
                                           : GAME_RGB(0x20, 0x2E, 0x12);
                                break;
                            default:
                                cell_col = ((c + r) & 1) ? COLOR_GRID_ODD : COLOR_GRID_EVEN;
                        }
                    } else {
                        cell_col = ((c + r) & 1) ? COLOR_GRID_ODD : COLOR_GRID_EVEN;
                    }
                    break;
            }
            draw_rect(bb, px, py, CELL_SIZE, CELL_SIZE, cell_col);
        }
    }

    /* MOD_TERRAIN: mountain peak indicator (small dark triangle drawn at top of each cell) */
    if (s->active_mod == MOD_TERRAIN) {
        for (int r = 0; r < GRID_ROWS; r++) {
            for (int c = 0; c < GRID_COLS; c++) {
                int idx2 = r * GRID_COLS + c;
                if (s->terrain[idx2] == CELL_MOUNTAIN) {
                    /* White "peak" dot at center-top of mountain cell */
                    int px2 = c * CELL_SIZE + CELL_SIZE / 2;
                    int py2 = r * CELL_SIZE + 3;
                    draw_rect(bb, px2 - 1, py2, 3, 2, GAME_RGB(0xDD, 0xDD, 0xCC));
                } else if (s->terrain[idx2] == CELL_SWAMP) {
                    /* Small bubbles indicator for swamp */
                    int px2 = c * CELL_SIZE + 4;
                    int py2 = r * CELL_SIZE + CELL_SIZE - 5;
                    if ((r * 7 + c * 13) % 3 == 0)
                        draw_rect(bb, px2, py2, 2, 2, GAME_RGB(0x40, 0x58, 0x28));
                }
            }
        }
    }

    /* MOD_WEATHER: flood cells visible during rain/storm */
    if (s->active_mod == MOD_WEATHER) {
        for (int r = 0; r < GRID_ROWS; r++) {
            for (int c = 0; c < GRID_COLS; c++) {
                if (s->weather_flood[r * GRID_COLS + c]) {
                    /* Animated flood water (slightly different shade) */
                    uint32_t flood_col = ((r + c + (int)(s->weather_timer * 2)) & 1)
                                         ? GAME_RGB(0x22, 0x66, 0xBB)
                                         : GAME_RGB(0x18, 0x55, 0xAA);
                    draw_rect(bb, c * CELL_SIZE, r * CELL_SIZE,
                              CELL_SIZE, CELL_SIZE, flood_col);
                }
            }
        }
    }

    /* Night overlay (MOD_NIGHT): dark blue semi-transparent wash */
    if (s->active_mod == MOD_NIGHT) {
        draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                        GAME_RGBA(0x00, 0x00, 0x20, 0x80));
    }

    /* MOD_WEATHER: storm lightning flash (bright white overlay, decays per frame) */
    if (s->active_mod == MOD_WEATHER && s->lightning_flash > 0.0f) {
        uint8_t alpha = (uint8_t)(s->lightning_flash * 180.0f);
        draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                        GAME_RGBA(0xFF, 0xFF, 0xDD, alpha));
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

#if USE_SPRITES
        /* Map TowerType (1-based) to SpriteId */
        static const SpriteId s_tower_spr[TOWER_COUNT] = {
            SPR_TOWER_PELLET,   /* TOWER_NONE  — fallback */
            SPR_TOWER_PELLET,   /* TOWER_PELLET */
            SPR_TOWER_SQUIRT,   /* TOWER_SQUIRT */
            SPR_TOWER_DART,     /* TOWER_DART   */
            SPR_TOWER_SNAP,     /* TOWER_SNAP   */
            SPR_TOWER_SWARM,    /* TOWER_SWARM  */
            SPR_TOWER_FROST,    /* TOWER_FROST  */
            SPR_TOWER_BASH,     /* TOWER_BASH   */
        };
        if (t->place_flash > 0.0f)
            draw_rect(bb, px + 2, py + 2, CELL_SIZE - 4, CELL_SIZE - 4, COLOR_WHITE);
        else
            draw_sprite(bb, s_tower_spr[t->type], px + 2, py + 2, CELL_SIZE - 4, CELL_SIZE - 4);
#else
        /* Body: brief flash on placement */
        uint32_t body_col = (t->place_flash > 0.0f) ? COLOR_WHITE
                                                     : TOWER_DEFS[t->type].color;
        draw_rect(bb, px + 2, py + 2, CELL_SIZE - 4, CELL_SIZE - 4, body_col);
#endif

        /* Barrel line */
        int bex = t->cx + (int)(cosf(t->angle) * BARREL_LEN);
        int bey = t->cy + (int)(sinf(t->angle) * BARREL_LEN);
        draw_line(bb, t->cx, t->cy, bex, bey, COLOR_BARREL);

        /* MOD_NIGHT: yellow glow outline one pixel outside the tower radius */
        if (s->active_mod == MOD_NIGHT) {
            draw_circle_outline(bb, t->cx, t->cy,
                                CELL_SIZE / 2 + 1, GAME_RGB(0xFF, 0xFF, 0x00));
        }
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

#if USE_SPRITES
        /* Map CreepType (1-based) to SpriteId */
        static const SpriteId s_creep_spr[CREEP_COUNT] = {
            SPR_CREEP_NORMAL,    /* CREEP_NONE        — fallback */
            SPR_CREEP_NORMAL,    /* CREEP_NORMAL      */
            SPR_CREEP_FAST,      /* CREEP_FAST        */
            SPR_CREEP_FLYING,    /* CREEP_FLYING      */
            SPR_CREEP_ARMOURED,  /* CREEP_ARMOURED    */
            SPR_CREEP_SPAWN,     /* CREEP_SPAWN       */
            SPR_CREEP_SPAWN,     /* CREEP_SPAWN_CHILD */
            SPR_CREEP_BOSS,      /* CREEP_BOSS        */
        };
        draw_sprite(bb, s_creep_spr[c->type], cx - r, cy - r, r * 2, r * 2);
#else
        draw_circle(bb, cx, cy, r, CREEP_DEFS[c->type].color);
#endif

        /* Status tints (applied in both modes) */
        if (c->slow_timer  > 0.0f)
            draw_circle(bb, cx, cy, r, GAME_RGBA(0x88, 0xCC, 0xFF, 0x70));
        if (c->stun_timer > 0.0f)
            draw_circle(bb, cx, cy, r, GAME_RGBA(0xFF, 0xFF, 0x00, 0x70));

        /* MOD_BOSS: yellow shield outline when spawn immunity is active */
        if (s->active_mod == MOD_BOSS && c->type == CREEP_BOSS &&
            c->shield_timer > 0.0f) {
            draw_circle_outline(bb, cx, cy, r + 4, GAME_RGB(0xFF, 0xFF, 0x00));
        }

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
#if USE_SPRITES
        draw_sprite(bb, SPR_PROJECTILE, (int)p->x - 4, (int)p->y - 4, 8, 8);
#else
        draw_rect(bb, (int)p->x - 2, (int)p->y - 2, 4, 4, COLOR_PROJECTILE);
#endif
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
     * MOD_WEATHER: rich weather visuals over the play area
     * ================================================================== */
    if (s->active_mod == MOD_WEATHER && s->weather_phase > 0) {
        /* Deterministic LCG seeded from weather_timer (coarse resolution)
         * so drops appear stable but slowly drift as timer advances. */
        int rng = (int)(s->weather_timer * 12.0f) * 1664525 + 1013904223;
#define WLCG(r) ((r) = (r) * 1664525 + 1013904223)
#define WRAND(r, range) ((int)((unsigned)(r) >> 16) % (range))

        if (s->weather_phase == 1 || s->weather_phase == 3) {
            /* RAIN / STORM: 70 rain drops of varying size and slant */
            int drop_count = (s->weather_phase == 3) ? 100 : 70;
            for (int i = 0; i < drop_count; i++) {
                WLCG(rng); int rx = WRAND(rng, GRID_PIXEL_W);
                WLCG(rng); int ry = WRAND(rng, GRID_PIXEL_H);
                WLCG(rng); int len = 2 + WRAND(rng, 3); /* 2–4 px tall */
                int slant = (s->weather_phase == 3) ? 1 : 0; /* storm = diagonal */
                draw_rect(bb, rx,         ry,     1,       len,
                          GAME_RGB(0x55, 0x99, 0xFF));
                if (slant)
                    draw_rect(bb, rx - 1, ry + 1, 1, len - 1,
                              GAME_RGB(0x44, 0x88, 0xEE));
            }
        }

        if (s->weather_phase == 2 || s->weather_phase == 3) {
            /* WIND / STORM: 25 diagonal streaks of varying length */
            for (int i = 0; i < 25; i++) {
                WLCG(rng); int rx  = WRAND(rng, GRID_PIXEL_W);
                WLCG(rng); int ry  = WRAND(rng, GRID_PIXEL_H);
                WLCG(rng); int len = 12 + WRAND(rng, 22); /* 12–33 px */
                draw_line(bb, rx, ry, rx + len, ry + len / 4,
                          GAME_RGB(0xBB, 0xBB, 0xEE));
            }
        }

        if (s->weather_phase == 3) {
            /* STORM: fog overlay (light grey-blue semi-transparent wash) */
            draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                            GAME_RGBA(0xAA, 0xBB, 0xCC, 0x28));
        }

#undef WLCG
#undef WRAND
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

        /* Upgrade level indicator */
        {
            char lvbuf[16];
            if (t->upgrade_level == 0)
                lvbuf[0] = '\0';
            else if (t->upgrade_level == 1)
                snprintf(lvbuf, sizeof(lvbuf), " LV1");
            else
                snprintf(lvbuf, sizeof(lvbuf), " LV2");
            if (lvbuf[0] != '\0')
                draw_text(bb, PANEL_X + 4 + 40, SEL_INFO_Y, lvbuf, COLOR_GOLD_TEXT, 1);
        }

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

        /* Upgrade button */
        if (t->upgrade_level < 2) {
            int up_cost = TOWER_DEFS[t->type].upgrade_cost[t->upgrade_level];
            uint32_t up_col = (s->player_gold >= up_cost) ? GAME_RGB(0x22, 0x55, 0x22)
                                                          : GAME_RGB(0x30, 0x20, 0x20);
            draw_rect(bb, SHOP_BTN_X, UPGRADE_BTN_Y, SHOP_BTN_W, UPGRADE_BTN_H, up_col);
            char ubuf[24];
            snprintf(ubuf, sizeof(ubuf), "Upgrade $%d", up_cost);
            draw_text(bb, SHOP_BTN_X + 4, UPGRADE_BTN_Y + (UPGRADE_BTN_H - 8) / 2,
                      ubuf, COLOR_WHITE, 1);
        } else {
            draw_rect(bb, SHOP_BTN_X, UPGRADE_BTN_Y, SHOP_BTN_W, UPGRADE_BTN_H,
                      GAME_RGB(0x33, 0x33, 0x33));
            draw_text(bb, SHOP_BTN_X + 4, UPGRADE_BTN_Y + (UPGRADE_BTN_H - 8) / 2,
                      "MAX", GAME_RGB(0x88, 0x88, 0x88), 1);
        }
    }

    /* Mod mode indicator (shown just above the Start/Send button) */
    {
        int mi_y = START_BTN_Y - 14;
        if (s->active_mod == MOD_TERRAIN) {
            draw_text(bb, PANEL_X + 4, mi_y, "Terrain Mod",
                      GAME_RGB(0x44, 0xCC, 0x44), 1);
        } else if (s->active_mod == MOD_WEATHER) {
            const char *wn;
            uint32_t    wc;
            switch (s->weather_phase) {
                case 1: wn = "Rain";  wc = GAME_RGB(0x55, 0xAA, 0xFF); break;
                case 2: wn = "Wind";  wc = GAME_RGB(0xCC, 0xCC, 0xFF); break;
                case 3: wn = "Storm"; wc = GAME_RGB(0xFF, 0xCC, 0x44); break;
                default:wn = "Clear"; wc = GAME_RGB(0x88, 0xDD, 0x88); break;
            }
            char wbuf[24];
            snprintf(wbuf, sizeof(wbuf), "Wx:%s", wn);
            draw_text(bb, PANEL_X + 4, mi_y, wbuf, wc, 1);
        } else if (s->active_mod == MOD_NIGHT) {
            draw_text(bb, PANEL_X + 4, mi_y, "Night Mode",
                      GAME_RGB(0x88, 0x88, 0xFF), 1);
        } else if (s->active_mod == MOD_BOSS) {
            draw_text(bb, PANEL_X + 4, mi_y, "Boss Mode",
                      GAME_RGB(0xFF, 0x44, 0x44), 1);
        }
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
                        GAME_RGBA(0x00, 0x00, 0x00, 0xD0));

        /* Title */
        draw_text(bb, (CANVAS_W - 21*8*2) / 2, 150,
                  "DESKTOP TOWER DEFENSE", COLOR_WHITE, 2);
        /* Subtitle */
        draw_text(bb, (CANVAS_W - 37*8) / 2, 178,
                  "A BFS pathfinding tower defense game", COLOR_GOLD_TEXT, 1);

        /* PLAY button */
        {
            int play_x = (CANVAS_W - 120) / 2;
            int play_y = 222;
            int play_hov = (s->mouse.x >= play_x && s->mouse.x < play_x + 120 &&
                            s->mouse.y >= play_y && s->mouse.y < play_y + 36);
            uint32_t play_col = play_hov ? GAME_RGB(0x33, 0xAA, 0x33)
                                         : GAME_RGB(0x22, 0x77, 0x22);
            draw_rect(bb, play_x, play_y, 120, 36, play_col);
            draw_text(bb, play_x + (120 - 4*8) / 2, play_y + (36 - 8) / 2,
                      "PLAY", COLOR_WHITE, 1);
        }

        /* Brief instructions */
        draw_text(bb, (CANVAS_W - 55*8) / 2, 280,
                  "Place towers to stop creeps | BFS pathfinding | 40 waves",
                  GAME_RGB(0xAA, 0xAA, 0xAA), 1);

    } else if (s->phase == GAME_PHASE_MOD_SELECT) {
        /* Dark overlay over grid area only */
        draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                        GAME_RGBA(0x00, 0x00, 0x00, 0xD8));

        /* Header */
        draw_text(bb, (GRID_PIXEL_W - 16*8*2) / 2, 20,
                  "SELECT GAME MODE", COLOR_WHITE, 2);

        /* Mod button data */
        static const char *mod_names[MOD_COUNT] = {
            "DEFAULT", "TERRAIN", "WEATHER", "NIGHT", "BOSS"
        };
        static const char *mod_descs[MOD_COUNT] = {
            "Standard BFS pathfinding. Classic DTD.",
            "Rivers+Mountains+Swamp shape the path. Towers get terrain bonuses.",
            "Rain/Wind/Storm flood cells — BFS reroutes creeps dynamically.",
            "Reduced tower range. Night-adapted towers bonus.",
            "Boss-heavy waves with special abilities."
        };
        static const uint32_t mod_colors[MOD_COUNT] = {
            GAME_RGB(0x66, 0x66, 0x66),   /* DEFAULT  — grey   */
            GAME_RGB(0x22, 0x88, 0x44),   /* TERRAIN  — green  */
            GAME_RGB(0x44, 0x88, 0xCC),   /* WEATHER  — blue   */
            GAME_RGB(0x22, 0x22, 0x88),   /* NIGHT    — indigo */
            GAME_RGB(0x99, 0x11, 0x11),   /* BOSS     — red    */
        };

        for (int i = 0; i < MOD_COUNT; i++) {
            int by = MOD_BTN_Y0 + i * (MOD_BTN_H + MOD_BTN_GAP);

            uint32_t bg;
            if (i == s->mod_hover_idx)
                bg = COLOR_BTN_HOVER;
            else if ((GameMod)i == s->active_mod)
                bg = COLOR_BTN_SELECTED;
            else
                bg = COLOR_BTN_NORMAL;

            draw_rect(bb, MOD_BTN_X, by, MOD_BTN_W, MOD_BTN_H, bg);

            /* Colour swatch */
            draw_rect(bb, MOD_BTN_X + 4, by + 4,
                      MOD_BTN_H - 8, MOD_BTN_H - 8, mod_colors[i]);

            /* Mode name */
            draw_text(bb, MOD_BTN_X + MOD_BTN_H + 4,
                      by + (MOD_BTN_H - 8) / 2 - 5,
                      mod_names[i], COLOR_WHITE, 1);

            /* Description */
            draw_text(bb, MOD_BTN_X + MOD_BTN_H + 4,
                      by + (MOD_BTN_H - 8) / 2 + 6,
                      mod_descs[i], GAME_RGB(0xCC, 0xCC, 0xCC), 1);
        }

        /* Footer hint */
        draw_text(bb, (GRID_PIXEL_W - 24*8) / 2, 360,
                  "Click a mode to begin playing", GAME_RGB(0xAA, 0xAA, 0xAA), 1);

    } else if (s->phase == GAME_PHASE_WAVE_CLEAR) {
        draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                        GAME_RGBA(0x00, 0x44, 0x00, 0x80));
        draw_text(bb, GRID_PIXEL_W / 2 - 64, GRID_PIXEL_H / 2 - 8,
                  "Wave Clear!", COLOR_WHITE, 2);

    } else if (s->phase == GAME_PHASE_GAME_OVER) {
        draw_rect_blend(bb, 0, 0, CANVAS_W, CANVAS_H,
                        GAME_RGBA(0x44, 0x00, 0x00, 0xC0));
        draw_text(bb, (CANVAS_W - 9*8*2) / 2, CANVAS_H / 2 - 36,
                  "GAME OVER", COLOR_LIVES_TEXT, 2);
        {
            char wbuf[32];
            snprintf(wbuf, sizeof(wbuf), "Wave reached: %d", s->current_wave);
            draw_text(bb, (CANVAS_W - (int)(14*8)) / 2, CANVAS_H / 2 + 4,
                      wbuf, COLOR_WHITE, 1);
        }
        draw_text(bb, (CANVAS_W - 26*8) / 2, CANVAS_H / 2 + 20,
                  "Click anywhere to try again", COLOR_WHITE, 1);

    } else if (s->phase == GAME_PHASE_VICTORY) {
        draw_rect_blend(bb, 0, 0, CANVAS_W, CANVAS_H,
                        GAME_RGBA(0x00, 0x22, 0x44, 0xC0));
        draw_text(bb, (CANVAS_W - 8*8*2) / 2, CANVAS_H / 2 - 36,
                  "VICTORY!", COLOR_GOLD_TEXT, 2);
        draw_text(bb, (CANVAS_W - 22*8) / 2, CANVAS_H / 2 + 4,
                  "All 40 waves survived!", COLOR_WHITE, 1);
        draw_text(bb, (CANVAS_W - 25*8) / 2, CANVAS_H / 2 + 20,
                  "Click anywhere to play again", COLOR_WHITE, 1);
    }
}
