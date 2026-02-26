# Lesson 2 — Cell Grid Math

**By the end of this lesson you will have:**
The window fills with a dark teal background. You understand how the virtual
128×80 cell grid maps to 1024×640 pixels — and why this design lets you think
in tiles instead of pixels.

---

## Why a cell grid?

The original Frogger ran in a Windows console: 128 columns × 80 rows of text
characters. Each character was 12×12 pixels. Our port keeps the same 128×80 grid
but maps each cell to 8×8 pixels (giving a 1024×640 window).

Think of it like CSS grid: the browser thinks in pixels, but you design in `fr`
units and the browser handles the translation. Here, game logic thinks in **cells**,
and the renderer multiplies by `CELL_PX = 8` to get actual pixels.

---

## Step 1 — Read the screen constants in `frogger.h`

Open `src/frogger.h`:

```c
#define SCREEN_CELLS_W   128   /* virtual screen width  in cells */
#define SCREEN_CELLS_H    80   /* virtual screen height in cells */
#define CELL_PX            8   /* pixels per cell */
#define SCREEN_PX_W  (SCREEN_CELLS_W * CELL_PX)   /* = 1024 */
#define SCREEN_PX_H  (SCREEN_CELLS_H * CELL_PX)   /* =  640 */

#define TILE_CELLS        8    /* one game tile = 8 cells wide */
#define LANE_WIDTH       18    /* tiles drawn per lane per frame */
#define LANE_PATTERN_LEN 64    /* circular pattern length (in tiles) */
#define NUM_LANES        10
```

**New C concept — `#define`:**
Like `const` in JS, but simpler: it's text substitution before compile.
`SCREEN_PX_W` gets replaced with `(128 * 8)` everywhere in your code.
No type, no memory — just a find-and-replace the compiler does automatically.

**The coordinate system:**

```
Cell grid:                   Pixel grid (what the screen sees):
 0   1   2  ... 127           0   8  16  ... 1016 1024
 ┌───┬───┬───┬───┐            ┌───────┬───────┬───────┐
 │   │   │   │   │  row 0     │       │       │       │  y=0
 ├───┼───┼───┼───┤            ├───────┼───────┼───────┤
 │   │   │   │   │  row 1     │       │       │       │  y=8
 └───┴───┴───┴───┘            └───────┴───────┴───────┘

 cell (col, row) → pixel (col * CELL_PX, row * CELL_PX)
 Example: cell (3, 2) → pixel (24, 16)
```

---

## Step 2 — The tile level above cells

One "game tile" = TILE_CELLS × TILE_CELLS = 8 × 8 = 64 cells wide.
In pixels: 8 cells × 8 px/cell = 64 px per tile.

```
Tile grid (what the game logic uses):
 0          1          2         ...  15
 ┌──────────┬──────────┬──────────┐
 │  64 px   │  64 px   │  64 px   │  row 0  (lane 0)
 ├──────────┼──────────┼──────────┤
 │          │          │          │  row 1  (lane 1)
 └──────────┴──────────┴──────────┘

 lane_y × TILE_CELLS × CELL_PX = lane pixel Y
 Example: lane 3 → 3 × 8 × 8 = 192 pixels from top
```

Why three levels (tiles → cells → pixels)?
- Sprites are stored at cell resolution (1 cell = 1 sprite pixel)
- Game logic (frog position, collision) works in tile units
- The screen renders in actual pixels

---

## Step 3 — How `platform_render` clears the screen

Look at the render functions. In Raylib (`main_raylib.c`):

```c
void platform_render(const GameState *state) {
    BeginDrawing();
    ClearBackground((Color){0, 0, 0, 255});  /* black */
    /* ... draw tiles and frog ... */
    EndDrawing();
}
```

**New C concept — compound literal `(Color){r, g, b, a}`:**
This creates a temporary `Color` struct inline.
JS analogy: like an object literal `{ r: 0, g: 0, b: 0, a: 255 }`.
The struct is defined by Raylib as `typedef struct { uint8_t r,g,b,a; } Color;`.

In X11 (`main_x11.c`):
```c
void platform_render(const GameState *state) {
    /* Clear the BACKBUFFER to black first (not the window directly)  */
    XSetForeground(g_display, g_gc, 0x000000);
    XFillRectangle(g_display, g_backbuffer, g_gc,
                   0, 0, SCREEN_PX_W, SCREEN_PX_H);
    /* ... draw tiles and frog to g_backbuffer ... */

    /* Blit the finished frame to the window in one operation          */
    XCopyArea(g_display, g_backbuffer, g_window, g_gc,
              0, 0, SCREEN_PX_W, SCREEN_PX_H, 0, 0);
    XFlush(g_display);  /* send all commands to X server */
}
```

**Why draw to `g_backbuffer` and not `g_window`?**
If you draw hundreds of rectangles directly to `g_window`, the user sees each
one appear one-at-a-time — the window flickers on every frame.
`g_backbuffer` is an offscreen Pixmap: invisible until `XCopyArea` copies the
entire completed frame to the window in a single operation. Zero flicker.
Raylib's `BeginDrawing()`/`EndDrawing()` does the same thing internally.

---

## Step 4 — Draw a single tile manually (experiment)

As a sanity check, let's draw one colored rectangle at a known tile position.
Add this temporarily inside `platform_render()`, BEFORE `EndDrawing()` in Raylib:

```c
/* Lesson 2 experiment: draw a green tile at tile position (2, 3) */
int tile_x = 2, tile_y = 3;
int px = tile_x * TILE_CELLS * CELL_PX;   /* 2 × 8 × 8 = 128 */
int py = tile_y * TILE_CELLS * CELL_PX;   /* 3 × 8 × 8 = 192 */
int size = TILE_CELLS * CELL_PX;          /* 8 × 8 = 64 */
DrawRectangle(px, py, size, size, (Color){0, 255, 0, 255});
```

For X11, add before `XCopyArea`:
```c
int tile_x = 2, tile_y = 3;
int px = tile_x * TILE_CELLS * CELL_PX;
int py = tile_y * TILE_CELLS * CELL_PX;
int size = TILE_CELLS * CELL_PX;
XSetForeground(g_display, g_gc, 0x00FF00);  /* green */
XFillRectangle(g_display, g_backbuffer, g_gc, px, py, size, size);
```

> Note: all X11 drawing goes to `g_backbuffer` — never to `g_window` directly.
> `XCopyArea` copies the finished frame to the window at the end of `platform_render`.

Rebuild and run. You should see a 64×64 green square at column 2, row 3.

Verify with math:
- pixel X = 2 × 8 × 8 = **128** from left
- pixel Y = 3 × 8 × 8 = **192** from top
- size = 8 × 8 = **64** pixels square

**Remove the experiment code** after verifying. It's done its job.

---

## Build & Run

```sh
cd course/
./build_x11.sh    # or ./build_raylib.sh
./frogger_x11
```

**What you should see:**
A completely black window (once you remove the experiment rectangle).
No sprites yet — those come in Lessons 3 and 4.

---

## Mental Model

Three coordinate spaces, each a fixed multiplier apart:

```
Tile coords   ×  (TILE_CELLS × CELL_PX)  →  Pixel coords
              =  (8 × 8)
              =  64 px per tile

Cell coords   ×  CELL_PX                 →  Pixel coords
              =  8 px per cell

Example: frog at tile (8, 9):
  pixel X = 8 × 64 = 512
  pixel Y = 9 × 64 = 576
```

Keep this conversion in your head. You'll use it every lesson.

---

## Exercise

Change `CELL_PX` from 8 to 12. What happens to the window size?
Calculate it first: `SCREEN_PX_W = 128 × 12 = ?`. Then rebuild and check.

Restore to 8 before continuing — later lessons assume 8.
