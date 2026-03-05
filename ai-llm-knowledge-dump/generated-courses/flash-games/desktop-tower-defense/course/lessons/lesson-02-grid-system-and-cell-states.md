# Lesson 02: Grid System and Cell States

## What we're building

A 30×20 checkerboard grid that fills the left 600×400 pixels of the canvas.
Two special cells stand out: the **entry** cell (column 0, row 9) glows bright green,
and the **exit** cell (column 29, row 9) glows bright red.  No creeps yet — just the
playing field that every future lesson will build on top of.

## What you'll learn

- The `CellState` enum and how to store a grid as a **flat `uint8_t` array**
  (and why flat arrays beat 2-D pointer arrays in C)
- `draw_rect` with **clipping** — how to draw rectangles safely even when
  coordinates go out of bounds
- `MIN` / `MAX` / `CLAMP` / `ABS` macros and the full set of **color constants**

> ⚠️ **Y-down coordinate system:** DTD uses Y-down pixels throughout.
> `pixel_y = row * CELL_SIZE`.  Row 0 = top of screen.  This is an intentional
> exception from a Y-up default because DTD is a grid game — a Y-flip would add
> noise with zero benefit.

## Prerequisites

- Lesson 01 complete and building on both backends

---

## Step 1: `src/utils/math.h`

Small math macros used throughout the game.  No new concepts — just named shorthand.

```c
/* src/utils/math.h  —  Desktop Tower Defense | Math Utilities */
#ifndef DTD_MATH_H
#define DTD_MATH_H

/* JS analogy: Math.min / Math.max / Math.clamp (proposal) / Math.abs */
#define MIN(a, b)        ((a) < (b) ? (a) : (b))
#define MAX(a, b)        ((a) > (b) ? (a) : (b))
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
#define ABS(a)           ((a) < 0 ? -(a) : (a))

#endif /* DTD_MATH_H */
```

**What's happening:**

- Every macro wraps its arguments in parentheses to prevent operator-precedence
  surprises.  `MIN(a+1, b)` expands to `((a+1) < (b) ? (a+1) : (b))` — correct.
- These are `#define` macros, not functions: no type checking, no function-call
  overhead.  Use them freely in hot loops.

---

## Step 2: `src/utils/draw-shapes.h` — header

```c
/* src/utils/draw-shapes.h  —  Desktop Tower Defense | Shape Drawing */
#ifndef DTD_DRAW_SHAPES_H
#define DTD_DRAW_SHAPES_H

#include <stdint.h>
#include "backbuffer.h"

/* Fill an axis-aligned rectangle with a solid color.
 * Clips to the backbuffer edge — safe to call with out-of-bounds coords.
 *
 * JS analogy: ctx.fillRect(x, y, w, h) — same signature, same clip behavior. */
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);

/* Draw a 1-pixel-thick rectangle outline. */
void draw_rect_outline(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);

/* Alpha-blend a rectangle over existing pixels.
 * alpha 0 = fully transparent; alpha 255 = fully opaque. */
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h,
                     uint32_t color, int alpha);

#endif /* DTD_DRAW_SHAPES_H */
```

---

## Step 3: `src/utils/draw-shapes.c` — implementation

```c
/* src/utils/draw-shapes.c  —  Desktop Tower Defense | Shape Drawing */
#include "draw-shapes.h"
#include "math.h"

/* draw_rect: fill axis-aligned rectangle, clipped to canvas bounds.
 *
 * Why clip manually instead of using unsigned cast tricks?
 * Because w/h can also be zero or negative (empty/inverted rect), and we want
 * to handle those gracefully (draw nothing) rather than loop over garbage.
 *
 * Clip math:
 *   x0 = max(x,  0)            — left edge never < 0
 *   y0 = max(y,  0)            — top  edge never < 0
 *   x1 = min(x+w, bb->width)   — right  edge never > canvas width
 *   y1 = min(y+h, bb->height)  — bottom edge never > canvas height
 *
 * JS analogy: exactly what ctx.fillRect does internally — clips to canvas. */
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
    int x0 = MAX(x,     0);
    int y0 = MAX(y,     0);
    int x1 = MIN(x + w, bb->width);
    int y1 = MIN(y + h, bb->height);
    /* After clipping, if x1 <= x0 or y1 <= y0, the rect is fully off-screen. */
    for (int py = y0; py < y1; py++) {
        uint32_t *row = bb->pixels + py * bb->width;
        for (int px = x0; px < x1; px++)
            row[px] = color;
    }
}

/* draw_rect_outline: four 1-pixel edges, each delegated to draw_rect.
 * Drawing through draw_rect means all four edges are also clipped. */
void draw_rect_outline(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
    draw_rect(bb, x,         y,         w, 1, color); /* top    */
    draw_rect(bb, x,         y + h - 1, w, 1, color); /* bottom */
    draw_rect(bb, x,         y,         1, h, color); /* left   */
    draw_rect(bb, x + w - 1, y,         1, h, color); /* right  */
}

/* draw_rect_blend: alpha-blend color over existing pixels.
 *
 * out = (src * alpha + dst * (255 - alpha)) >> 8
 *
 * Using integer shift-by-8 instead of divide-by-255 for speed.
 * The error is negligible (< 0.4%) and invisible at game resolution. */
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h,
                     uint32_t color, int alpha) {
    uint32_t sr = (color >> 16) & 0xFF;
    uint32_t sg = (color >>  8) & 0xFF;
    uint32_t sb =  color        & 0xFF;
    int inv_alpha = 255 - alpha;

    int x0 = MAX(x,     0);
    int y0 = MAX(y,     0);
    int x1 = MIN(x + w, bb->width);
    int y1 = MIN(y + h, bb->height);

    for (int py = y0; py < y1; py++) {
        uint32_t *row = bb->pixels + py * bb->width;
        for (int px = x0; px < x1; px++) {
            uint32_t d  = row[px];
            uint32_t dr = (d >> 16) & 0xFF;
            uint32_t dg = (d >>  8) & 0xFF;
            uint32_t db =  d        & 0xFF;
            row[px] = GAME_RGB(
                (sr * alpha + dr * inv_alpha) >> 8,
                (sg * alpha + dg * inv_alpha) >> 8,
                (sb * alpha + db * inv_alpha) >> 8);
        }
    }
}
```

**What's happening:**

- `bb->pixels + py * bb->width` computes a pointer to the start of row `py`.
  Writing `row[px]` is then just an array subscript from that row base.
- The inner loop body is a single `uint32_t` store — the compiler will auto-vectorize
  this with SIMD when optimizations are enabled.
- The blend uses integer-only arithmetic.  No `float` means no FPU stalls in tight loops.

---

## Step 4: Additions to `src/game.h`

Add these declarations to `game.h`.  Everything below the `/* ===== LESSON 02 ===== */`
comment is new; everything above it is unchanged from Lesson 01.

```c
/* ===== LESSON 02 additions to src/game.h ===== */

#include "utils/math.h"
#include "utils/draw-shapes.h"

/* -----------------------------------------------------------------------
 * GRID CONSTANTS
 * -----------------------------------------------------------------------
 * The grid is GRID_COLS wide and GRID_ROWS tall.
 * Each cell is CELL_SIZE×CELL_SIZE pixels.
 * Y-DOWN: pixel_y = row * CELL_SIZE  (row 0 = top of screen).
 *
 * JS analogy: a 2-D CSS grid where each cell is a 20×20px square,
 * laid out left-to-right, top-to-bottom.
 * ----------------------------------------------------------------------- */
#define GRID_COLS     30
#define GRID_ROWS     20
#define CELL_SIZE     20
#define GRID_PIXEL_W  (GRID_COLS * CELL_SIZE)   /* 600 */
#define GRID_PIXEL_H  (GRID_ROWS * CELL_SIZE)   /* 400 */

/* Entry (spawn) and exit cell positions */
#define ENTRY_COL  0
#define ENTRY_ROW  9
#define EXIT_COL   (GRID_COLS - 1)
#define EXIT_ROW   9

/* -----------------------------------------------------------------------
 * CELL STATE  —  each grid cell is exactly one of these values.
 *
 * Stored as uint8_t to keep the grid array small (600 bytes vs 2400 for int).
 *
 * JS analogy:
 *   const CELL_EMPTY  = 0;
 *   const CELL_TOWER  = 1;
 *   const CELL_ENTRY  = 2;
 *   const CELL_EXIT   = 3;
 * ----------------------------------------------------------------------- */
typedef enum {
    CELL_EMPTY = 0,
    CELL_TOWER,
    CELL_ENTRY,
    CELL_EXIT,
} CellState;

/* -----------------------------------------------------------------------
 * COLOR CONSTANTS  (0xAARRGGBB)
 * ----------------------------------------------------------------------- */
#define COLOR_BG          GAME_RGB(0x22, 0x22, 0x22)
#define COLOR_GRID_EVEN   GAME_RGB(0xDD, 0xDD, 0xDD)
#define COLOR_GRID_ODD    GAME_RGB(0xC8, 0xC8, 0xC8)
#define COLOR_ENTRY_CELL  GAME_RGB(0x88, 0xFF, 0x88)
#define COLOR_EXIT_CELL   GAME_RGB(0xFF, 0x88, 0x88)
#define COLOR_PANEL_BG    GAME_RGB(0x30, 0x30, 0x30)
#define COLOR_WHITE       GAME_RGB(0xFF, 0xFF, 0xFF)
#define COLOR_BLACK       GAME_RGB(0x00, 0x00, 0x00)
#define COLOR_HOVER       GAME_RGB(0xFF, 0xFF, 0x00)

/* -----------------------------------------------------------------------
 * GAME STATE  (additions)
 * ----------------------------------------------------------------------- */
typedef struct {
    int should_quit;

    /* The grid: a flat array indexed by (row * GRID_COLS + col).
     * Each element is a CellState value (uint8_t).
     *
     * JS analogy: const grid = new Uint8Array(GRID_ROWS * GRID_COLS);
     *             grid[row * GRID_COLS + col] = CELL_TOWER;
     */
    uint8_t grid[GRID_ROWS * GRID_COLS];

    /* BFS distance field — filled in Lesson 04. */
    int dist[GRID_ROWS * GRID_COLS];
} GameState;
```

**What's happening — flat array vs 2-D array:**

| Approach | Declaration | Access |
|---|---|---|
| Flat array ✓ | `uint8_t grid[ROWS * COLS]` | `grid[row * COLS + col]` |
| 2-D array | `uint8_t grid[ROWS][COLS]` | `grid[row][col]` |

The flat array is preferred because:
1. It is one contiguous block of memory — cache-friendly.
2. It can be `memset` / `memcpy` in one call.
3. Pointer arithmetic to pass sub-regions is straightforward.

JS analogy: `const grid = new Uint8Array(ROWS * COLS)` — exactly the same layout.

---

## Step 5: Additions to `src/game.c`

```c
/* src/game.c  —  Desktop Tower Defense | Game Logic (Lesson 02 version) */
#include "game.h"

/* -----------------------------------------------------------------------
 * draw_grid  —  internal helper, not exposed in game.h
 *
 * Iterates every cell and draws a CELL_SIZE×CELL_SIZE filled rectangle.
 * Cell color is determined by:
 *   - CELL_ENTRY → bright green
 *   - CELL_EXIT  → bright red
 *   - other      → alternating light/dark grey based on (col + row) % 2
 *
 * The checkerboard pattern is a classic trick: if (col + row) is even the
 * cell is "light", if odd it is "dark".  Parity flips every step in both
 * directions, creating the alternating grid.
 * ----------------------------------------------------------------------- */
static void draw_grid(const GameState *state, Backbuffer *bb) {
    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            /* Y-down: pixel_y = row * CELL_SIZE */
            int px = col * CELL_SIZE;
            int py = row * CELL_SIZE;

            int       idx   = row * GRID_COLS + col;
            CellState cell  = (CellState)state->grid[idx];
            uint32_t  color;

            if (cell == CELL_ENTRY) {
                color = COLOR_ENTRY_CELL;
            } else if (cell == CELL_EXIT) {
                color = COLOR_EXIT_CELL;
            } else {
                /* Checkerboard: even parity = light, odd parity = dark */
                color = ((col + row) % 2 == 0) ? COLOR_GRID_EVEN : COLOR_GRID_ODD;
            }

            draw_rect(bb, px, py, CELL_SIZE, CELL_SIZE, color);
        }
    }
}

void game_init(GameState *state) {
    memset(state, 0, sizeof(*state));

    /* Mark entry and exit cells in the grid.
     * All other cells default to CELL_EMPTY (0 from memset). */
    state->grid[ENTRY_ROW * GRID_COLS + ENTRY_COL] = CELL_ENTRY;
    state->grid[EXIT_ROW  * GRID_COLS + EXIT_COL ] = CELL_EXIT;
}

void game_update(GameState *state, float dt) {
    (void)state;
    (void)dt;
}

void game_render(const GameState *state, Backbuffer *bb) {
    /* 1. Clear the whole canvas to the dark background. */
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BG);

    /* 2. Draw the grid over the left 600×400 pixels. */
    draw_grid(state, bb);

    /* 3. Draw the side panel background (right 160 pixels). */
    draw_rect(bb, GRID_PIXEL_W, 0, bb->width - GRID_PIXEL_W, bb->height, COLOR_PANEL_BG);
}
```

**Coordinate math walkthrough (row=9, col=0, CELL_SIZE=20):**

```
pixel_x = col  * CELL_SIZE = 0  * 20 = 0
pixel_y = row  * CELL_SIZE = 9  * 20 = 180
```

So the entry cell is drawn at `(0, 180)` and occupies pixels `(0..19, 180..199)`.  This
is Y-down: larger row numbers are lower on screen.

---

## Step 6: Update `build-dev.sh` source list

Add `src/utils/draw-shapes.c` to the `SHARED` variable in `build-dev.sh`:

```bash
SHARED="src/game.c src/utils/draw-shapes.c"
```

The rest of the build script is unchanged from Lesson 01.

---

## Build and run

```bash
# Raylib
./build-dev.sh --backend=raylib -r

# X11
./build-dev.sh --backend=x11 -r
```

**Expected output:** The left 600×400 area shows a 30×20 light/dark grey checkerboard.
Cell (0, 9) is bright green (entry). Cell (29, 9) is bright red (exit).
The right 160 pixels are solid dark charcoal (side panel).

---

## Exercises

1. **Beginner:** Change `ENTRY_ROW` from `9` to `4`.  Rebuild and confirm the green
   cell moves to the 5th row (row 4 = 80px from the top in Y-down).

2. **Intermediate:** Write a second static helper `draw_grid_outline` that calls
   `draw_rect_outline` for every cell in the grid.  Add it to `game_render` after
   `draw_grid`.  What color should the outline be?  Try `COLOR_BLACK` with 50%
   alpha using `draw_rect_blend`.

3. **Challenge:** The current grid is hardcoded to start at `(0, 0)`.  Add `GRID_OFFSET_X`
   and `GRID_OFFSET_Y` constants (both `0` for now) and update `draw_grid` to use them.
   This will make it easy to center the grid later without changing every `col * CELL_SIZE`
   expression throughout the codebase.

---

## What's next

In Lesson 03 we add mouse input: a `MouseState` struct, the letterbox coordinate
transform that maps raw window mouse positions back to canvas pixels, and the
pixel-to-cell math that tells us which grid cell the mouse is hovering over.
