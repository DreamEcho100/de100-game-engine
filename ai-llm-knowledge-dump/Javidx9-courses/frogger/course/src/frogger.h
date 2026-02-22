#ifndef FROGGER_H
#define FROGGER_H

/* =============================================================================
   frogger.h — Shared types, constants, and declarations
   =============================================================================

   DESIGN PHILOSOPHY — Data-Oriented Design (DOD)
   ------------------------------------------------
   Separate arrays for each DATA FIELD. One flat GameState. No heap.
   Update (frogger_tick) and Render (platform_render) are completely isolated.
   ============================================================================= */

#include <stdint.h>

/* -------------------------------------------------------------------------
   Screen constants
   ------------------------------------------------------------------------- */
#define SCREEN_CELLS_W   128
#define SCREEN_CELLS_H    80
#define CELL_PX            8
#define SCREEN_PX_W  (SCREEN_CELLS_W * CELL_PX)   /* 1024 */
#define SCREEN_PX_H  (SCREEN_CELLS_H * CELL_PX)   /* 640  */

/* -------------------------------------------------------------------------
   Tile / lane constants
   ------------------------------------------------------------------------- */
#define TILE_CELLS        8
#define TILE_PX          (TILE_CELLS * CELL_PX)    /* 64 pixels per tile */
#define LANE_WIDTH       18
#define LANE_PATTERN_LEN 64
#define NUM_LANES        10

/* -------------------------------------------------------------------------
   Sprite constants
   ------------------------------------------------------------------------- */
#define NUM_SPRITES      9
#define SPR_POOL_CELLS   (9 * 32 * 8)

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

/* -------------------------------------------------------------------------
   SpriteBank — all sprite data in one flat pool (no heap)
   ------------------------------------------------------------------------- */
typedef struct {
    int16_t colors[SPR_POOL_CELLS];
    int16_t glyphs[SPR_POOL_CELLS];
    int     widths [NUM_SPRITES];
    int     heights[NUM_SPRITES];
    int     offsets[NUM_SPRITES];
} SpriteBank;

/* -------------------------------------------------------------------------
   InputState — one-frame keyboard snapshot
   ------------------------------------------------------------------------- */
typedef struct {
    int up_released;
    int down_released;
    int left_released;
    int right_released;
} InputState;

/* -------------------------------------------------------------------------
   GameState — ALL mutable game data, flat struct, no pointers, no heap.
   ------------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------------
   lane_scroll — pixel-accurate scroll position for a lane.

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

   Used in BOTH frogger_tick (danger buffer) and platform_render (drawing)
   so the collision grid always matches what is on screen.
   ------------------------------------------------------------------------- */
static inline void lane_scroll(float time, float speed,
                                int *out_tile_start, int *out_px_offset) {
    const int PATTERN_PX = LANE_PATTERN_LEN * TILE_PX;  /* 64 * 64 = 4096 */

    int sc = (int)(time * speed * (float)TILE_PX) % PATTERN_PX;
    if (sc < 0) sc += PATTERN_PX;

    *out_tile_start = sc / TILE_PX;
    *out_px_offset  = sc % TILE_PX;
}

/* -------------------------------------------------------------------------
   Declarations (implemented in frogger.c)
   ------------------------------------------------------------------------- */
void frogger_init(GameState *state, const char *assets_dir);
void frogger_tick(GameState *state, const InputState *input, float dt);
void frogger_run(const char *assets_dir);

#endif /* FROGGER_H */
