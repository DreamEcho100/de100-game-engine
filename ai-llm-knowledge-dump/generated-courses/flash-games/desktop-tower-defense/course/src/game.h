/* src/game.h  —  Desktop Tower Defense | Shared Types and API
 *
 * DTD coordinate system: Y-DOWN, pixel units.
 *   pixel_x = col * CELL_SIZE,  pixel_y = row * CELL_SIZE
 *   Origin (0,0) = top-left corner of grid.
 *   NO world-to-screen flip — screen IS the world.
 * This is an intentional exception from the course-builder.md Y-up default
 * because DTD is a grid game; a Y-flip would add noise with zero benefit.
 */
#ifndef DTD_GAME_H
#define DTD_GAME_H

#include <stdint.h>   /* uint8_t, uint32_t, int16_t */
#include <math.h>     /* atan2f, cosf, sinf, powf, sqrtf */
#include <string.h>   /* memset */

#include "utils/backbuffer.h"
#include "utils/math.h"
#include "utils/audio.h"

/* =========================================================================
 * DEBUG MACROS
 * ========================================================================= */
#ifdef DEBUG
#  include <stdio.h>
#  ifdef _WIN32
#    include <intrin.h>
#    define DEBUG_TRAP() __debugbreak()
#  else
#    include <signal.h>
#    define DEBUG_TRAP() raise(SIGTRAP)
#  endif
#  define ASSERT(expr) \
       do { if (!(expr)) { \
           fprintf(stderr, "ASSERT failed: %s  at %s:%d\n", #expr, __FILE__, __LINE__); \
           DEBUG_TRAP(); \
       } } while (0)
#else
#  define DEBUG_TRAP()   do {} while (0)
#  define ASSERT(expr)   do {} while (0)
#endif

/* =========================================================================
 * CANVAS + GRID CONSTANTS
 * ========================================================================= */
#define CANVAS_W       760   /* total canvas width  (600 grid + 160 panel) */
#define CANVAS_H       520   /* total canvas height (grid = 400, extra = 120 for HUD) */

#define GRID_COLS      30
#define GRID_ROWS      20
#define CELL_SIZE      20    /* pixels per cell (Y-down; pixel_y = row * CELL_SIZE) */
#define GRID_PIXEL_W   (GRID_COLS * CELL_SIZE)   /* 600 */
#define GRID_PIXEL_H   (GRID_ROWS * CELL_SIZE)   /* 400 */
#define PANEL_X        GRID_PIXEL_W              /* side panel starts at x=600 */
#define PANEL_W        (CANVAS_W - GRID_PIXEL_W) /* 160 */

/* Entry / exit cell positions (left-to-right Easy mode) */
#define ENTRY_COL      0
#define ENTRY_ROW      9
#define EXIT_COL       (GRID_COLS - 1)
#define EXIT_ROW       9

/* =========================================================================
 * ENTITY POOL LIMITS
 * ========================================================================= */
#define MAX_TOWERS       256
#define MAX_CREEPS       256
#define MAX_PROJECTILES  512
#define MAX_PARTICLES    256

/* =========================================================================
 * ECONOMY CONSTANTS
 * ========================================================================= */
#define STARTING_GOLD    50
#define STARTING_LIVES   20
#define SELL_RATIO       0.70f
#define INTEREST_RATE    0.05f
#define HP_SCALE_PER_WAVE 1.10f
#define GOLD_PER_SECOND_EARLY 10.0f  /* gold bonus per second for early send */

/* =========================================================================
 * COLOR CONSTANTS  (0xAARRGGBB)
 * ========================================================================= */
/* Grid */
#define COLOR_GRID_EVEN     GAME_RGB(0xDD, 0xDD, 0xDD)
#define COLOR_GRID_ODD      GAME_RGB(0xC8, 0xC8, 0xC8)
#define COLOR_ENTRY_CELL    GAME_RGB(0x88, 0xFF, 0x88)
#define COLOR_EXIT_CELL     GAME_RGB(0xFF, 0x88, 0x88)
/* Towers */
#define COLOR_PELLET        GAME_RGB(0x88, 0x88, 0x88)
#define COLOR_SQUIRT        GAME_RGB(0x44, 0x88, 0xCC)
#define COLOR_DART          GAME_RGB(0x22, 0x44, 0x88)
#define COLOR_SNAP          GAME_RGB(0xCC, 0x22, 0x22)
#define COLOR_SWARM         GAME_RGB(0xFF, 0x88, 0x00)
#define COLOR_FROST         GAME_RGB(0x88, 0xCC, 0xFF)
#define COLOR_BASH          GAME_RGB(0x88, 0x66, 0x44)
/* Creeps */
#define COLOR_CREEP_NORMAL  GAME_RGB(0x88, 0x88, 0x88)
#define COLOR_CREEP_FAST    GAME_RGB(0xFF, 0xCC, 0x00)
#define COLOR_CREEP_FLYING  GAME_RGB(0xFF, 0xFF, 0xFF)
#define COLOR_CREEP_ARMOURED GAME_RGB(0x44, 0x44, 0x44)
#define COLOR_CREEP_SPAWN   GAME_RGB(0x44, 0xCC, 0x44)
#define COLOR_CREEP_BOSS    GAME_RGB(0xCC, 0x00, 0x00)
/* UI */
#define COLOR_PANEL_BG      GAME_RGB(0x30, 0x30, 0x30)
#define COLOR_PANEL_BORDER  GAME_RGB(0x55, 0x55, 0x55)
#define COLOR_WHITE         GAME_RGB(0xFF, 0xFF, 0xFF)
#define COLOR_BLACK         GAME_RGB(0x00, 0x00, 0x00)
#define COLOR_GOLD_TEXT     GAME_RGB(0xFF, 0xCC, 0x00)
#define COLOR_LIVES_TEXT    GAME_RGB(0xFF, 0x44, 0x44)
#define COLOR_HP_FULL       GAME_RGB(0x00, 0xCC, 0x00)
#define COLOR_HP_LOW        GAME_RGB(0xCC, 0x00, 0x00)
#define COLOR_BTN_NORMAL    GAME_RGB(0x44, 0x44, 0x44)
#define COLOR_BTN_HOVER     GAME_RGB(0x66, 0x66, 0x66)
#define COLOR_BTN_SELECTED  GAME_RGB(0x22, 0x66, 0x22)
#define COLOR_BTN_ERROR     GAME_RGB(0x88, 0x22, 0x22)
#define COLOR_PREVIEW_OK    GAME_RGBA(0x00, 0xFF, 0x00, 0x60)
#define COLOR_PREVIEW_BAD   GAME_RGBA(0xFF, 0x00, 0x00, 0x60)
#define COLOR_BARREL        GAME_RGB(0xFF, 0xFF, 0xFF)
#define COLOR_PROJECTILE    GAME_RGB(0xFF, 0xFF, 0x00)

/* =========================================================================
 * ENUMERATIONS
 * ========================================================================= */

/* Grid cell state — each cell is exactly one of these */
typedef enum {
    CELL_EMPTY = 0,
    CELL_TOWER,
    CELL_ENTRY,
    CELL_EXIT,
    /* Terrain types stored in GameState::terrain[] (MOD_TERRAIN) */
    CELL_WATER,     /* creeps slowed 50%; towers cannot be placed here         */
    CELL_MOUNTAIN,  /* impassable to creeps; towers get +20% range             */
    CELL_SWAMP,     /* creeps slowed to 25% speed; towers cannot be placed     */
} CellState;

/* Tower types — TOWER_NONE = slot is inactive */
typedef enum {
    TOWER_NONE = 0,
    TOWER_PELLET,
    TOWER_SQUIRT,
    TOWER_DART,
    TOWER_SNAP,
    TOWER_SWARM,
    TOWER_FROST,
    TOWER_BASH,
    TOWER_COUNT,
} TowerType;

/* Creep types */
typedef enum {
    CREEP_NONE = 0,
    CREEP_NORMAL,
    CREEP_FAST,
    CREEP_FLYING,
    CREEP_ARMOURED,
    CREEP_SPAWN,
    CREEP_SPAWN_CHILD,
    CREEP_BOSS,
    CREEP_COUNT,
} CreepType;

/* Targeting mode for towers */
typedef enum {
    TARGET_FIRST = 0,     /* lowest dist[] — closest to exit */
    TARGET_LAST,          /* highest dist[] — farthest from exit */
    TARGET_STRONGEST,     /* most current HP */
    TARGET_WEAKEST,       /* least current HP */
    TARGET_CLOSEST,       /* Euclidean distance to tower */
    TARGET_COUNT,
} TargetMode;

/* Game mod — selected on the mod-select screen before play begins */
typedef enum {
    MOD_DEFAULT = 0,  /* standard BFS pathfinding */
    MOD_TERRAIN,      /* water/mountain/swamp terrain affect movement & placement */
    MOD_WEATHER,      /* rain/wind/storm events affect pathfinding & speed */
    MOD_NIGHT,        /* reduced tower range; night-vision bonus for certain towers */
    MOD_BOSS,         /* boss-heavy waves, bosses have unique abilities */
    MOD_COUNT,
} GameMod;

/* Game phase (state machine) */
typedef enum {
    GAME_PHASE_TITLE = 0,
    GAME_PHASE_MOD_SELECT,    /* player chooses a game mod */
    GAME_PHASE_PLACING,       /* between waves: player builds */
    GAME_PHASE_WAVE,          /* wave in progress */
    GAME_PHASE_WAVE_CLEAR,    /* wave ended; brief pause */
    GAME_PHASE_GAME_OVER,
    GAME_PHASE_VICTORY,
    GAME_PHASE_COUNT,
} GamePhase;

/* Sound effect IDs */
typedef enum {
    SFX_TOWER_FIRE_PELLET = 0,
    SFX_TOWER_FIRE_SQUIRT,
    SFX_TOWER_FIRE_DART,
    SFX_TOWER_FIRE_SNAP,
    SFX_TOWER_FIRE_SWARM,
    SFX_FROST_PULSE,
    SFX_BASH_HIT,
    SFX_CREEP_DEATH,
    SFX_BOSS_DEATH,
    SFX_LIFE_LOST,
    SFX_TOWER_PLACE,
    SFX_TOWER_SELL,
    SFX_WAVE_COMPLETE,
    SFX_GAME_OVER,
    SFX_VICTORY,
    /* -- additional feedback sounds -- */
    SFX_WAVE_START,    /* plays when a new wave begins */
    SFX_INTEREST_EARN, /* plays when interest gold is added between waves */
    SFX_EARLY_SEND,    /* plays when the player sends a wave early for a bonus */
    SFX_TOWER_UPGRADE, /* plays when a tower is upgraded */
    SFX_COUNT,
} SfxId;

/* =========================================================================
 * DATA TABLE STRUCTS (read-only, static)
 * ========================================================================= */

/* One entry in the static tower definition table */
typedef struct {
    const char *name;
    int         cost;
    int         damage;
    float       range_cells;   /* range in cells; stored in pixels in Tower */
    float       fire_rate;     /* shots per second */
    uint32_t    color;
    int         is_aoe;        /* 1 = Swarm/Frost/Bash type */
    float       splash_radius; /* pixels; 0 = single target */
    int         upgrade_cost[2];         /* [0]=lv0→1, [1]=lv1→2 */
    float       upgrade_damage_mult[2];  /* damage multiplier at lv1, lv2 */
} TowerDef;

/* One entry in the static creep definition table */
typedef struct {
    float    speed;        /* pixels per second */
    int      base_hp;
    int      lives_cost;   /* lives lost if this creep reaches exit */
    uint32_t color;
    int      is_flying;    /* ignores BFS distance field */
    int      is_armoured;  /* 50% DR from most towers */
    int      kill_gold;    /* gold earned on kill */
    int      size;         /* draw size in pixels (relative to CELL_SIZE) */
} CreepDef;

/* =========================================================================
 * ENTITY STRUCTS
 * ========================================================================= */

/* Mouse input state (double-buffered edge detection) */
typedef struct {
    int x, y;              /* current canvas pixel position */
    int prev_x, prev_y;    /* last frame position */
    int left_down;         /* button held */
    int left_pressed;      /* edge: 0→1 this frame */
    int left_released;     /* edge: 1→0 this frame */
    int right_pressed;     /* right-click this frame */
} MouseState;

/* A placed tower on the grid */
typedef struct {
    int        col, row;        /* grid cell (integer) */
    int        cx, cy;          /* centre in pixels */
    TowerType  type;
    float      range;           /* pixels (= range_cells * CELL_SIZE) */
    int        damage;
    float      fire_rate;       /* shots/s */
    float      cooldown_timer;  /* seconds until next shot */
    float      angle;           /* current barrel angle (radians) */
    int        target_id;       /* id of current target creep (-1 = none) */
    TargetMode target_mode;
    int        active;          /* 0 = slot empty */
    int        sell_value;      /* gold returned on sell */
    float      place_flash;     /* brief flash timer on placement */
    int        upgrade_level;   /* 0 = base, 1 = upgraded, 2 = max */
} Tower;

/* A creep moving toward the exit */
typedef struct {
    float     x, y;            /* float pixel position */
    int       col, row;        /* current integer grid cell */
    CreepType type;
    float     hp;
    float     max_hp;
    float     speed;           /* base speed px/s */
    int       active;
    int       id;              /* unique ID for target tracking */
    int       is_flying;
    float     slow_timer;      /* seconds of slow remaining */
    float     slow_factor;     /* speed multiplier when slowed (0.5) */
    float     stun_timer;      /* seconds of stun remaining (speed=0) */
    float     death_timer;     /* counting-down death animation */
    int       lives_cost;      /* lives lost if this creep exits */
    /* Flying creep direction (unit vector) */
    float     fly_dx, fly_dy;
    /* MOD_BOSS fields */
    float     shield_timer;    /* if > 0, immune to all damage (boss spawn shield) */
    int       half_hp_spawned; /* 1 once the 50%-HP child-spawn has triggered */
} Creep;

/* A projectile in flight */
typedef struct {
    float     x, y;
    float     vx, vy;
    int       damage;
    float     splash_radius;   /* 0 = single target */
    int       target_id;       /* -1 = no tracking (splash on proximity) */
    int       active;
    TowerType tower_type;      /* which tower fired this (for damage calc) */
} Projectile;

/* A visual-only cosmetic particle */
typedef struct {
    float    x, y;
    float    vx, vy;
    float    lifetime;
    float    max_lifetime;
    uint32_t color;
    int      size;
    char     text[12];         /* non-empty = floating text particle */
    int      active;
} Particle;

/* Wave definition (one per wave, stored in levels.c) */
typedef struct {
    CreepType creep_type;
    int       count;
    float     spawn_interval;  /* seconds between each creep */
    float     base_hp_override; /* 0 = use CREEP_DEFS[type].base_hp */
} WaveDef;

/* =========================================================================
 * GAME STATE
 * ========================================================================= */

typedef struct {
    /* Phase */
    GamePhase  phase;
    float      phase_timer;        /* general-purpose phase countdown */

    /* Active mod — set before game_init(), read throughout simulation */
    GameMod    active_mod;
    int        mod_hover_idx;    /* which mod button mouse is over (-1 = none) */

    /* Grid */
    uint8_t    grid[GRID_ROWS * GRID_COLS];
    int        dist[GRID_ROWS * GRID_COLS];  /* BFS distance from exit */

    /* MOD_TERRAIN: per-cell terrain type and pre-computed slow multiplier */
    uint8_t    terrain[GRID_ROWS * GRID_COLS];      /* CellState: CELL_WATER/MOUNTAIN/SWAMP */
    float      terrain_slow[GRID_ROWS * GRID_COLS]; /* 0.5 water, 0.25 swamp, 1.0 otherwise */

    /* MOD_WEATHER: 4-phase weather cycle (0=clear 1=rain 2=wind 3=storm).
     * Cycle length: 20 s clear / 15 s rain / 15 s wind / 10 s storm = 60 s total.
     * During RAIN or STORM, weather_flood[] cells become temporary BFS walls. */
    int        weather_phase;                       /* 0-3 */
    float      weather_timer;                       /* accumulates 0–60 s */
    uint8_t    weather_flood[GRID_ROWS * GRID_COLS];/* 1 = flooded (rain/storm) */
    int        prev_weather_phase;                  /* for detecting phase transitions */
    float      lightning_timer;                     /* seconds until next lightning flash */
    float      lightning_flash;                     /* 0-1 brightness (decays per frame) */

    /* Towers */
    Tower      towers[MAX_TOWERS];
    int        tower_count;
    TowerType  selected_tower_type; /* which tower type is in the shop selection */
    int        selected_tower_idx;  /* index into towers[] if player clicked a placed tower */
    float      shop_error_timer;    /* brief red flash when can't afford */

    /* Creeps */
    Creep      creeps[MAX_CREEPS];
    int        creep_count;
    int        next_creep_id;       /* incremented to give each creep a unique ID */

    /* Projectiles */
    Projectile projectiles[MAX_PROJECTILES];
    int        projectile_count;

    /* Particles */
    Particle   particles[MAX_PARTICLES];
    int        particle_count;

    /* Wave system */
    int        current_wave;       /* 1-based; 0 = before first wave */
    int        wave_spawn_index;   /* how many creeps have been spawned this wave */
    float      wave_spawn_timer;   /* time until next creep spawn */
    float      wave_clear_timer;   /* countdown in WAVE_CLEAR phase */
    float      remaining_wave_time;/* used for early-send gold bonus */

    /* Economy */
    int        player_gold;
    int        player_lives;

    /* Input */
    MouseState mouse;
    int        hover_col, hover_row;  /* grid cell under mouse (-1 = off grid) */

    /* Audio */
    GameAudioState audio;

    /* Misc */
    int        should_quit;
} GameState;

/* =========================================================================
 * EXTERNAL DATA TABLES (defined in game.c)
 * ========================================================================= */
extern const TowerDef TOWER_DEFS[TOWER_COUNT];
extern const CreepDef CREEP_DEFS[CREEP_COUNT];

/* Wave table (defined in levels.c) */
#define WAVE_COUNT 40
extern const WaveDef g_waves[WAVE_COUNT];

/* =========================================================================
 * GAME API (implemented in game.c)
 * ========================================================================= */

/* Initialise all game state (zero fill + set starting values). */
void game_init(GameState *state);

/* Advance simulation by dt seconds. */
void game_update(GameState *state, float dt);

/* Draw everything to the backbuffer (pure read of state — no mutations). */
void game_render(const GameState *state, Backbuffer *bb);

/* =========================================================================
 * AUDIO API (implemented in audio.c)
 * ========================================================================= */

/* Play a one-shot sound effect. */
void game_play_sound(GameAudioState *audio, SfxId id);
void game_play_sound_at(GameAudioState *audio, SfxId id, float pan);

/* Called every frame to advance music sequencer. */
void game_audio_update(GameAudioState *audio, float dt);

/* Fill AudioOutputBuffer with PCM samples (called by platform audio update). */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *out);

/* Initialise audio state (sine tables, sound defs). */
void game_audio_init(GameAudioState *audio);

#endif /* DTD_GAME_H */
