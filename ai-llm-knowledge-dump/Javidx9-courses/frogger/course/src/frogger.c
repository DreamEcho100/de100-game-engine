/* =============================================================================
   frogger.c — Game Logic
   =============================================================================

   This file contains ONLY game logic — no X11, no Raylib, no OS calls.
   It reads from InputState, writes to GameState, and calls platform_* from
   platform.h.

   DOD NOTE: Notice how the lane data below is split into two separate arrays:
     lane_speeds[]      — just the floats, 10 × 4 = 40 bytes (fits in one cache line)
     lane_patterns[][]  — just the char grids, touched separately

   When frogger_tick() updates frog position from platform riding, it loops
   over lane_speeds[] only — it never touches lane_patterns[]. The CPU fetches
   exactly the data it needs.
   ============================================================================= */

/* Required for clock_gettime / CLOCK_MONOTONIC on POSIX systems.
   Must come BEFORE any #include.                                           */
#define _POSIX_C_SOURCE 200809L

#include "frogger.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* -------------------------------------------------------------------------
   Lane data  (read-only, lives in .rodata segment — not on stack or heap)

   DOD layout: speeds and patterns are SEPARATE arrays.
   If we used struct Lane { float speed; char pattern[65]; }, each struct
   would be 69 bytes. Iterating speeds alone would waste 65 bytes per step.
   With separate arrays, iterating speeds touches only 40 bytes total.

   Lane map (top to bottom):
     0  — home row  (wall + homes, stationary)
     1  — river     (logs + water, moves LEFT  at 3)
     2  — river     (logs + water, moves RIGHT at 3)
     3  — river     (logs + water, moves RIGHT at 2)
     4  — pavement  (safe middle strip, stationary)
     5  — road      (bus,  moves LEFT  at 3)
     6  — road      (car2, moves RIGHT at 3)
     7  — road      (car1, moves LEFT  at 4)
     8  — road      (car2, moves RIGHT at 2)
     9  — pavement  (safe start row, stationary)
   ------------------------------------------------------------------------- */

static const float lane_speeds[NUM_LANES] = {
     0.0f,  /* 0 home row    */
    -3.0f,  /* 1 river left  */
     3.0f,  /* 2 river right */
     2.0f,  /* 3 river right */
     0.0f,  /* 4 pavement    */
    -3.0f,  /* 5 road left   */
     3.0f,  /* 6 road right  */
    -4.0f,  /* 7 road left   */
     2.0f,  /* 8 road right  */
     0.0f,  /* 9 pavement    */
};

/* Each row is exactly LANE_PATTERN_LEN (64) characters.
   Tile characters:
     w = wall (dangerous)        h = home (safe)
     , = water (dangerous)       j/l/k = log start/mid/end (safe)
     p = pavement (safe)
     . = road    (safe)          a/s/d/f = bus segments (dangerous)
     z/x = car1 back/front       t/y = car2 back/front  (all dangerous)  */
static const char lane_patterns[NUM_LANES][LANE_PATTERN_LEN + 1] = {
    "wwwhhwwwhhwwwhhwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww",
    ",,,jllk,,jllllk,,,,,,,jllk,,,,,jk,,,jlllk,,,,jllllk,,,,jlllk,,,,",
    ",,,,jllk,,,,,jllk,,,,jllk,,,,,,,,,jllk,,,,,jk,,,,,,jllllk,,,,,,,",
    ",,jlk,,,,,jlk,,,,,jk,,,,,jlk,,,jlk,,,,jk,,,,jllk,,,,jk,,,,,,jk,,",
    "pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp",
    "....asdf.......asdf....asdf..........asdf........asdf....asdf....",
    ".....ty..ty....ty....ty.....ty........ty..ty.ty......ty.......ty.",
    "..zx.....zx.........zx..zx........zx...zx...zx....zx...zx...zx..",
    "..ty.....ty.......ty.....ty......ty..ty.ty.......ty....ty........",
    "pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp",
};

/* Returns 1 if the tile character is safe to stand on, 0 if deadly. */
static int tile_is_safe(char c) {
    return (c == '.' || c == 'j' || c == 'l' || c == 'k' ||
            c == 'p' || c == 'h');
}

/* -------------------------------------------------------------------------
   Sprite loader
   .spr binary layout:
     int32 width; int32 height;
     int16 colors[w*h];   (FG in low nibble, BG in high nibble)
     int16 glyphs[w*h];   (0x2588 = solid block, 0x0020 = transparent)
   ------------------------------------------------------------------------- */
static int sprite_load(SpriteBank *bank, int spr_id, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "FATAL: cannot open sprite '%s'\n", path);
        return 0;
    }

    int32_t w, h;
    fread(&w, sizeof(int32_t), 1, f);
    fread(&h, sizeof(int32_t), 1, f);

    int offset = bank->offsets[spr_id];   /* where in the pool to write  */
    int count  = w * h;

    /* Safety check — pool must be large enough */
    if (offset + count > SPR_POOL_CELLS) {
        fprintf(stderr, "FATAL: sprite pool overflow for '%s'\n", path);
        fclose(f);
        return 0;
    }

    fread(bank->colors + offset, sizeof(int16_t), count, f);
    fread(bank->glyphs + offset, sizeof(int16_t), count, f);

    bank->widths [spr_id] = (int)w;
    bank->heights[spr_id] = (int)h;

    fclose(f);
    return 1;
}

/* -------------------------------------------------------------------------
   frogger_init — load sprites, place frog at starting position
   ------------------------------------------------------------------------- */
void frogger_init(GameState *state, const char *assets_dir) {
    /* Zero the whole struct — sets frog pos to 0, time to 0, etc. */
    memset(state, 0, sizeof(GameState));

    /* Frog starts at tile column 8, row 9 (bottom safe pavement) */
    state->frog_x = 8.0f;
    state->frog_y = 9.0f;

    /* Compute pool offsets for each sprite (pack them sequentially).
       offsets[i] = sum of all cells for sprites 0..i-1               */
    static const int spr_cells[NUM_SPRITES] = {
        /* frog */ 8*8,  /* water */ 8*8,  /* pavement */ 8*8,
        /* wall */ 8*8,  /* home  */ 8*8,  /* log  */ 24*8,
        /* car1 */ 16*8, /* car2  */ 16*8, /* bus  */ 32*8,
    };
    int offset = 0;
    for (int i = 0; i < NUM_SPRITES; i++) {
        state->sprites.offsets[i] = offset;
        offset += spr_cells[i];
    }

    /* Build paths and load each sprite */
    char path[256];
    static const struct { int id; const char *filename; } spr_files[NUM_SPRITES] = {
        {SPR_FROG,     "frog.spr"},
        {SPR_WATER,    "water.spr"},
        {SPR_PAVEMENT, "pavement.spr"},
        {SPR_WALL,     "wall.spr"},
        {SPR_HOME,     "home.spr"},
        {SPR_LOG,      "log.spr"},
        {SPR_CAR1,     "car1.spr"},
        {SPR_CAR2,     "car2.spr"},
        {SPR_BUS,      "bus.spr"},
    };

    for (int i = 0; i < NUM_SPRITES; i++) {
        snprintf(path, sizeof(path), "%s/%s", assets_dir, spr_files[i].filename);
        if (!sprite_load(&state->sprites, spr_files[i].id, path)) {
            fprintf(stderr, "Failed to load: %s\n", path);
            exit(1);
        }
    }
}

/* -------------------------------------------------------------------------
   frogger_tick — pure game logic, called once per frame
   No drawing here. No platform calls.

   Steps:
     1. Accumulate time
     2. Handle input (move frog one tile per key release)
     3. River: push frog sideways if on a log row
     4. Rebuild danger[] buffer based on scrolling lanes
     5. Collision-check frog's four inner corners
     6. Reset frog if dead
   ------------------------------------------------------------------------- */
void frogger_tick(GameState *state, const InputState *input, float dt) {

    /* Cap dt so a debugger pause or lag spike doesn't teleport everything */
    if (dt > 0.1f) dt = 0.1f;

    state->time += dt;

    /* --- Death flash timer --- */
    if (state->dead) {
        state->dead_timer -= dt;
        if (state->dead_timer <= 0.0f) {
            /* Reset frog position, clear dead flag */
            state->frog_x  = 8.0f;
            state->frog_y  = 9.0f;
            state->dead    = 0;
        }
        return; /* skip input + collision while flashing */
    }

    /* --- Input: move one tile per key RELEASE (not press) ---
       This matches the original: tap a key, frog hops exactly one tile.    */
    if (input->up_released)    state->frog_y -= 1.0f;
    if (input->down_released)  state->frog_y += 1.0f;
    if (input->left_released)  state->frog_x -= 1.0f;
    if (input->right_released) state->frog_x += 1.0f;

    /* --- River: logs carry frog ---
       Rows 0-3 are in the "river zone". The frog is pushed horizontally
       at the lane's scroll speed.
       Row 0 has speed 0 (home row), so no movement there.                 */
    int fy_int = (int)state->frog_y;
    if (fy_int >= 0 && fy_int <= 3) {
        /* Subtract because negative speed moves left, we follow the tile */
        state->frog_x -= lane_speeds[fy_int] * dt;
    }

    /* --- Rebuild danger buffer ---
       Uses lane_scroll() — same math as platform_render — so the collision
       grid always matches exactly what is drawn on screen.                 */

    /* Default everything to deadly; safe tiles will clear their cells. */
    memset(state->danger, 1, SCREEN_CELLS_W * SCREEN_CELLS_H);

    for (int y = 0; y < NUM_LANES; y++) {
        int tile_start, px_offset;
        lane_scroll(state->time, lane_speeds[y], &tile_start, &px_offset);
        /* Convert pixel offset to cell offset for the cell-resolution buffer */
        int cell_offset = px_offset / CELL_PX;   /* 0..TILE_CELLS-1 */

        /* Draw starts at tile x = -1 so a partial tile is visible at left */
        for (int i = 0; i < LANE_WIDTH; i++) {
            char c    = lane_patterns[y][(tile_start + i) % LANE_PATTERN_LEN];
            int  safe = tile_is_safe(c);

            /* Cell X range for this tile on screen */
            int cx_start = (-1 + i) * TILE_CELLS - cell_offset;

            /* Each lane is TILE_CELLS (8) cell-rows tall.
               Lane y=9 → cell rows 72..79, NOT row 9.
               Without the dy loop the frog always lands on cells that were
               never cleared by the lane pass → danger=1 → instant death.   */
            for (int dy = 0; dy < TILE_CELLS; dy++) {
                int cy = y * TILE_CELLS + dy;
                for (int cx = cx_start; cx < cx_start + TILE_CELLS; cx++) {
                    if (cx >= 0 && cx < SCREEN_CELLS_W)
                        state->danger[cy * SCREEN_CELLS_W + cx] = (uint8_t)(!safe);
                }
            }
        }
    }

    /* --- Collision: center-cell check ---

       WHY NOT 4-CORNER CHECK:
       The danger buffer is cell-granular (8px steps). Logs scroll at pixel
       granularity. At the exact frame a log edge crosses a cell boundary the
       border cells flip dangerous. If the frog's corner cells land there →
       false death flash. Using the CENTER cell of the frog sprite (TILE_PX/2
       pixels in from any edge = 4 cells from any boundary) is always well
       inside the log and can never be the flickering border cell.

       WHY PIXEL BOUNDS (not tile bounds):
       Integer tile check only triggers when frog is a full tile off-screen.
       River drift can push frog_x to 15.9 before it dies — it would appear
       to run off the right edge. Pixel check kills immediately.            */
    int frog_px_x = (int)(state->frog_x * (float)TILE_PX);
    int frog_px_y = (int)(state->frog_y * (float)TILE_PX);

    if (frog_px_x < 0 || frog_px_y < 0 ||
        frog_px_x + TILE_PX > SCREEN_PX_W ||
        frog_px_y + TILE_PX > SCREEN_PX_H) {
        state->dead       = 1;
        state->dead_timer = 0.4f;
        return;
    }

    /* Center cell is TILE_PX/2 px into the sprite = TILE_CELLS/2 cells in.
       4 cells from any tile boundary → immune to log-edge cell artifacts.  */
    int cx = (frog_px_x + TILE_PX / 2) / CELL_PX;
    int cy = (frog_px_y + TILE_PX / 2) / CELL_PX;

    if (state->danger[cy * SCREEN_CELLS_W + cx]) {
        state->dead       = 1;
        state->dead_timer = 0.4f;
        return;
    }

    /* --- Win detection ---
       If frog is on row 0 AND standing on a home ('h') tile, count it.   */
    if (fy_int == 0) {
        /* Tile at frog's center column */
        int pattern_pos = (int)(state->frog_x + 0.5f) % LANE_PATTERN_LEN;
        if (pattern_pos < 0)
            pattern_pos += LANE_PATTERN_LEN;
        char c = lane_patterns[0][pattern_pos % LANE_PATTERN_LEN];
        if (c == 'h') {
            state->homes_reached++;
            /* Send frog back to start */
            state->frog_x = 8.0f;
            state->frog_y = 9.0f;
        }
    }
}

/* -------------------------------------------------------------------------
   frogger_run — main entry point
   Sets up timing, runs the game loop, calls platform functions.
   The loop lives here (not in the platform files) so both X11 and Raylib
   share the same loop structure.
   ------------------------------------------------------------------------- */
void frogger_run(const char *assets_dir) {
    GameState state;
    frogger_init(&state, assets_dir);

    platform_init(SCREEN_PX_W, SCREEN_PX_H, "Frogger");

    InputState input = {0};

    /* Timing: clock_gettime gives nanosecond precision.
       JS analogy: like performance.now() but in C.                         */
    struct timespec prev, now;
    clock_gettime(CLOCK_MONOTONIC, &prev);

    while (!platform_should_quit()) {
        /* Measure dt */
        clock_gettime(CLOCK_MONOTONIC, &now);
        float dt = (float)(now.tv_sec  - prev.tv_sec) +
                   (float)(now.tv_nsec - prev.tv_nsec) * 1e-9f;
        prev = now;

        /* Poll input, update logic, draw */
        platform_get_input(&input);
        frogger_tick(&state, &input, dt);
        platform_render(&state);

        /* Cap to ~60 FPS */
        platform_sleep_ms(16);
    }

    platform_shutdown();
}
