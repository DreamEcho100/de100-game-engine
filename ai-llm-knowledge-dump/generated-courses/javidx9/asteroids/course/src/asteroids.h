/* =============================================================================
   asteroids.h — Game Types, Constants, and Public API

   All types and function declarations for the Asteroids course.
   No platform headers included here — game code is platform-independent.
   ============================================================================= */

#pragma once

#include <stdint.h>
#include <math.h>

/* ─── Screen ──────────────────────────────────────────────────────────────── */
#define SCREEN_W 800
#define SCREEN_H 600

/* ─── Backbuffer ──────────────────────────────────────────────────────────── */
/* CPU-side pixel array: game writes here, platform uploads to GPU once/frame. */
typedef struct {
    uint32_t *pixels; /* flat array: pixels[y * width + x] */
    int       width;
    int       height;
} AsteroidsBackbuffer;

/* ─── Color System ────────────────────────────────────────────────────────── */
/*
 * ASTEROIDS_RGBA packs bytes in memory as [RR, GG, BB, AA] on little-endian x86.
 * The uint32_t value is 0xAABBGGRR.
 *
 * Why this layout?
 *   GL_RGBA (X11/OpenGL) reads bytes left-to-right as R, G, B, A.
 *   PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 (Raylib) reads the same order.
 *   Both backends get correct colours with no extra swapping.
 */
#define ASTEROIDS_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
     ((uint32_t)(g) <<  8) |  (uint32_t)(r))
#define ASTEROIDS_RGB(r, g, b) ASTEROIDS_RGBA(r, g, b, 0xFF)

/* Named color constants — avoids magic numbers in game code */
#define COLOR_BLACK   ASTEROIDS_RGB(  0,   0,   0)
#define COLOR_WHITE   ASTEROIDS_RGB(255, 255, 255)
#define COLOR_YELLOW  ASTEROIDS_RGB(255, 255,   0)
#define COLOR_RED     ASTEROIDS_RGB(255,   0,   0)
#define COLOR_GREEN   ASTEROIDS_RGB(  0, 255,   0)
#define COLOR_CYAN    ASTEROIDS_RGB(  0, 255, 255)
#define COLOR_GRAY    ASTEROIDS_RGB(128, 128, 128)
#define COLOR_ORANGE  ASTEROIDS_RGB(255, 165,   0)

/* ─── Input System ────────────────────────────────────────────────────────── */
/*
 * GameButtonState tracks more than just "is it held?".
 * half_transition_count: how many state changes happened this frame.
 *   - 0 = no change (held or released the whole frame)
 *   - 1 = changed once (pressed or released exactly once)
 *   - 2 = pressed and released within the same frame (very fast tap)
 *
 * "Just pressed" = ended_down == 1 && half_transition_count > 0
 * "Just released" = ended_down == 0 && half_transition_count > 0
 * "Held" = ended_down == 1 (regardless of htc)
 *
 * This design is from Casey Muratori's Handmade Hero — it captures sub-frame
 * events that a simple bool would miss.
 */
typedef struct {
    int half_transition_count; /* state changes this frame */
    int ended_down;            /* 1 = currently held, 0 = released */
} GameButtonState;

/* UPDATE_BUTTON: call once per event (key press or release).
 * Only records a transition when the state actually changes. */
#define UPDATE_BUTTON(btn, is_down)                     \
    do {                                                 \
        if ((btn).ended_down != (is_down)) {             \
            (btn).half_transition_count++;               \
            (btn).ended_down = (is_down);                \
        }                                                \
    } while (0)

typedef struct {
    GameButtonState left;  /* rotate counter-clockwise */
    GameButtonState right; /* rotate clockwise         */
    GameButtonState up;    /* thrust forward            */
    GameButtonState fire;  /* shoot bullet              */
    int             quit;  /* 1 = exit game loop        */
} GameInput;

/* ─── Geometry ────────────────────────────────────────────────────────────── */
/* 2-component float vector — used for model vertices and transformed points.
 * In JS this would be: { x: number, y: number }                             */
typedef struct { float x, y; } Vec2;

/* ─── Space Objects ───────────────────────────────────────────────────────── */
/*
 * One struct covers both asteroids and bullets.
 * For asteroids: size = radius in pixels; timer is unused.
 * For bullets:   size = 0 (just a dot); timer = remaining lifetime (seconds).
 *
 * In the original C++ code, a std::vector held these and used remove_if to
 * delete them. We use fixed-size C arrays + a count — no heap allocation
 * in the game loop hot path.
 */
typedef struct {
    float x, y;    /* world position (pixels) */
    float dx, dy;  /* velocity (pixels/sec)   */
    float angle;   /* heading / rotation (radians) */
    float timer;   /* bullet: lifetime remaining; asteroid: unused */
    int   size;    /* asteroid: radius; bullet: 0                  */
    int   active;  /* 1 = alive, 0 = dead (triggers compact_pool)  */
} SpaceObject;

/* ─── Game Phase ──────────────────────────────────────────────────────────── */
/* Typed enum — prevents passing a raw int where a phase is expected.
 * In C++ you'd use enum class; in C, typedef enum achieves the same clarity. */
typedef enum {
    PHASE_PLAYING,
    PHASE_DEAD
} GAME_PHASE;

/* ─── Pool Limits ─────────────────────────────────────────────────────────── */
#define MAX_ASTEROIDS  64 /* large(1) → medium(2) → small(4): 7 per original → 14 total; 64 is safe */
#define MAX_BULLETS    32 /* fire rate is "just pressed", so 32 is plenty */

/* ─── Model Sizes ─────────────────────────────────────────────────────────── */
#define SHIP_VERTS      3  /* isoceles triangle */
#define ASTEROID_VERTS 20  /* jagged circle, 20-sided polygon */

/* ─── Game Constants ──────────────────────────────────────────────────────── */
#define LARGE_SIZE     64  /* asteroid radius (pixels) — largest generation  */
#define MEDIUM_SIZE    32  /* asteroid radius — second generation             */
#define SMALL_SIZE     16  /* asteroid radius — smallest before disappearing  */

#define SHIP_SCALE     10.0f   /* model coords × 10 = pixel size (triangle ~50px tall) */
#define BULLET_SPEED  300.0f   /* pixels/second                                        */
#define BULLET_LIFE     2.5f   /* seconds before bullet expires                        */
#define THRUST_ACCEL  150.0f   /* pixels/second² of acceleration when thrust held      */
#define ROTATE_SPEED    4.0f   /* radians/second rotation rate                         */
#define DEAD_ANIM_TIME  1.5f   /* seconds the death flash plays before reset           */

/* ─── Game State ──────────────────────────────────────────────────────────── */
typedef struct {
    SpaceObject player;
    SpaceObject asteroids[MAX_ASTEROIDS];
    int         asteroid_count;
    SpaceObject bullets[MAX_BULLETS];
    int         bullet_count;
    int         score;
    GAME_PHASE  phase;
    float       dead_timer;
    /* Wireframe models — built once in asteroids_init, constant thereafter */
    Vec2        ship_model[SHIP_VERTS];
    Vec2        asteroid_model[ASTEROID_VERTS];
} GameState;

/* ─── Public API ──────────────────────────────────────────────────────────── */
/* Call once from main before the game loop */
void asteroids_init(GameState *state);

/* Call every frame: processes input, updates physics, collision, state */
void asteroids_update(GameState *state, const GameInput *input, float dt);

/* Call every frame after update: writes pixels to backbuffer */
void asteroids_render(const GameState *state, AsteroidsBackbuffer *bb);

/* Call at the start of each frame to reset half_transition_count */
void prepare_input_frame(GameInput *input);
