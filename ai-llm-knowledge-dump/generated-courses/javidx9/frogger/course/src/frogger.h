#ifndef FROGGER_H
#define FROGGER_H

/* =============================================================================
   frogger.h — Shared types, constants, and public API

   DESIGN PHILOSOPHY — Data-Oriented Design (DOD)
   ------------------------------------------------
   Separate arrays for each DATA FIELD. One flat GameState. No heap.
   frogger_tick (update) and frogger_render (draw) are completely isolated.

   Port of OneLoneCoder Frogger (javidx9) from C++/Windows to C/Linux.
   ============================================================================= */

#include <stdint.h>  /* uint32_t, int16_t, int32_t */
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* memset */

/* ─── Screen constants ──────────────────────────────────────────────────────── */
#define SCREEN_CELLS_W   128
#define SCREEN_CELLS_H    80
#define CELL_PX            8
#define SCREEN_PX_W  (SCREEN_CELLS_W * CELL_PX)   /* 1024 */
#define SCREEN_PX_H  (SCREEN_CELLS_H * CELL_PX)   /* 640  */

/* ─── Tile / lane constants ─────────────────────────────────────────────────── */
#define TILE_CELLS        8
#define TILE_PX          (TILE_CELLS * CELL_PX)    /* 64 pixels per tile */
#define LANE_WIDTH       18
#define LANE_PATTERN_LEN 64
#define NUM_LANES        10

/* ─── Sprite constants ──────────────────────────────────────────────────────── */
#define NUM_SPRITES      9
#define SPR_POOL_CELLS   (9 * 32 * 8)

/* Sprite IDs */
#define SPR_FROG       0
#define SPR_WATER      1
#define SPR_PAVEMENT   2
#define SPR_WALL       3
#define SPR_HOME       4
#define SPR_LOG        5
#define SPR_CAR1       6
#define SPR_CAR2       7
#define SPR_BUS        8

/* ─── Backbuffer ────────────────────────────────────────────────────────────── */

/* The game renders into this CPU pixel buffer.
 * Platforms upload it to the GPU each frame — no platform drawing API
 * is ever called from game code.
 *
 * Pixel format: 0xAARRGGBB  (alpha = high byte, blue = low byte) */
typedef struct {
    uint32_t *pixels;
    int       width;
    int       height;
} FroggerBackbuffer;

/* ─── Color System ──────────────────────────────────────────────────────────── */

/* Pack (r,g,b,a) so that bytes in memory are [RR, GG, BB, AA] on
 * little-endian x86 — i.e. the uint32_t value is 0xAABBGGRR.
 * This matches PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 (Raylib) which
 * reads bytes in memory order as R, G, B, A, and GL_RGBA (X11/OpenGL)
 * which does the same.  Both backends see correct colours.          */
#define FROGGER_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
     ((uint32_t)(g) <<  8) |  (uint32_t)(r))

#define FROGGER_RGB(r, g, b) FROGGER_RGBA(r, g, b, 0xFF)

/* Windows console color palette — maps 4-bit FG index to RGB.
   color_attr & 0x0F = FG index.
   Glyph 0x2588 (solid block) → fill cell with FG color.
   Glyph 0x0020 (space)       → transparent, skip.              */
static const uint8_t CONSOLE_PALETTE[16][3] = {
    {  0,   0,   0},  /* 0  Black        */
    {  0,   0, 128},  /* 1  Dark Blue    */
    {  0, 128,   0},  /* 2  Dark Green   */
    {  0, 128, 128},  /* 3  Dark Cyan    */
    {128,   0,   0},  /* 4  Dark Red     */
    {128,   0, 128},  /* 5  Dark Magenta */
    {128, 128,   0},  /* 6  Dark Yellow  */
    {192, 192, 192},  /* 7  Gray         */
    {128, 128, 128},  /* 8  Dark Gray    */
    {  0,   0, 255},  /* 9  Blue         */
    {  0, 255,   0},  /* 10 Bright Green */
    {  0, 255, 255},  /* 11 Cyan         */
    {255,   0,   0},  /* 12 Red          */
    {255,   0, 255},  /* 13 Magenta      */
    {255, 255,   0},  /* 14 Yellow       */
    {255, 255, 255},  /* 15 White        */
};

/* ─── SpriteBank — all sprite data in one flat pool (no heap) ───────────────── */
typedef struct {
    int16_t colors[SPR_POOL_CELLS];
    int16_t glyphs[SPR_POOL_CELLS];
    int     widths [NUM_SPRITES];
    int     heights[NUM_SPRITES];
    int     offsets[NUM_SPRITES];
} SpriteBank;

/* ─── Input System ──────────────────────────────────────────────────────────── */

/* GameButtonState tracks both the current state AND transitions within a frame.
 * Origin: Casey Muratori's Handmade Hero.
 *
 * ended_down:            1 = key is currently held down; 0 = released.
 * half_transition_count: number of state changes this frame.
 *   0 = no change (held or idle)
 *   1 = normal press or release
 *   2 = full tap (pressed + released within one frame)
 *
 * "Just pressed this frame" check:
 *   button.ended_down && button.half_transition_count > 0 */
typedef struct {
    int half_transition_count;
    int ended_down;
} GameButtonState;

/* Call on KeyPress:   UPDATE_BUTTON(btn, 1)
 * Call on KeyRelease: UPDATE_BUTTON(btn, 0)
 * Only increments half_transition_count when state actually changes. */
#define UPDATE_BUTTON(button, is_down)                    \
    do {                                                   \
        if ((button).ended_down != (is_down)) {            \
            (button).half_transition_count++;              \
            (button).ended_down = (is_down);               \
        }                                                  \
    } while (0)

typedef struct {
    GameButtonState move_up;
    GameButtonState move_down;
    GameButtonState move_left;
    GameButtonState move_right;
    int quit;
} GameInput;

/* ─── GameState ─────────────────────────────────────────────────────────────── */

typedef struct {
    float      frog_x;
    float      frog_y;
    float      time;
    int        homes_reached;
    int        dead;
    float      dead_timer;
    uint8_t    danger[SCREEN_CELLS_W * SCREEN_CELLS_H];
    SpriteBank sprites;
} GameState;

/* ─── lane_scroll ───────────────────────────────────────────────────────────── */
/*
   Pixel-accurate scroll position for a lane.

   ROOT CAUSE of the "jumping sprites" bug (now fixed here):
   The naive code used  (int)(time * speed) % PATTERN_LEN  to get tile_start
   and  (int)(TILE_CELLS * time * speed) % TILE_CELLS  for the cell offset.

   Two problems:
   1. C truncates (int) toward ZERO, not toward -infinity.
      floor(-4.5) = -5, but (int)(-4.5) = -4.
      For negative speeds this makes tile_start jump by an extra tile.

   2. The sub-tile offset was in CELL units (8-pixel steps), so sprites
      visually jumped 8 pixels at a time instead of scrolling smoothly.

   The fix: work entirely in PIXELS, use positive modulo.
     sc = (time * speed * TILE_PX) % PATTERN_PX   -- may be negative
     if (sc < 0) sc += PATTERN_PX                 -- always [0, PATTERN_PX)
     tile_start = sc / TILE_PX
     px_offset  = sc % TILE_PX                    -- 0..63 pixels

   Then the tile's screen X = (-1+i)*TILE_PX - px_offset   (in pixels).
   This is sub-pixel smooth: each frame the offset changes by
   |speed| * TILE_PX * dt pixels (e.g. 3 * 64 * 0.016 = 3.07 px/frame).

   Used in BOTH frogger_tick (danger buffer) and frogger_render (drawing)
   so the collision grid always matches what is on screen.
*/
static inline void lane_scroll(float time, float speed,
                                int *out_tile_start, int *out_px_offset) {
    const int PATTERN_PX = LANE_PATTERN_LEN * TILE_PX;  /* 64 * 64 = 4096 */

    int sc = (int)(time * speed * (float)TILE_PX) % PATTERN_PX;
    if (sc < 0) sc += PATTERN_PX;

    *out_tile_start = sc / TILE_PX;
    *out_px_offset  = sc % TILE_PX;
}

/* ─── Public Game API ───────────────────────────────────────────────────────── */

/* Reset per-frame transition counts — call BEFORE platform_get_input. */
void prepare_input_frame(GameInput *input);

/* Load sprites and place frog at starting position. */
void frogger_init(GameState *state, const char *assets_dir);

/* Advance the game by dt seconds — pure logic, no drawing. */
void frogger_tick(GameState *state, const GameInput *input, float dt);

/* Render the full game frame into backbuffer — platform-independent. */
void frogger_render(const GameState *state, FroggerBackbuffer *bb);

#endif /* FROGGER_H */
