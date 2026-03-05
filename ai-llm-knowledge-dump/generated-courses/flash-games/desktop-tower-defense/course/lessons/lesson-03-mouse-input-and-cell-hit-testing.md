# Lesson 03: Mouse Input and Cell Hit Testing

## What we're building

Move the mouse over the grid and watch a yellow cell follow your cursor — a hover
highlight that snaps to the cell boundary.  This gives the player visual feedback
before they place a tower.  By the end of this lesson you'll have a fully-working
mouse input pipeline: raw OS coordinates → letterbox transform → canvas pixel →
grid cell.

## What you'll learn

- The `MouseState` struct and **double-buffered edge detection** for buttons
- The **letterbox scale + offset transform** that maps a resizable window's mouse
  position back to canvas pixel coordinates
- The **pixel-to-cell** formula and a `mouse_in_grid()` helper

## Prerequisites

- Lesson 02 complete and building on both backends

---

## Step 1: `MouseState` — additions to `src/platform.h`

The `MouseState` struct is already declared in `platform.h`.  Here it is in full,
with all fields explained:

```c
/* src/platform.h  —  MouseState and UPDATE_BUTTON (Lesson 03 additions)
 *
 * Double-buffered edge detection pattern:
 *   - .left_down    = button is HELD (true every frame while held)
 *   - .left_pressed = true ONLY on the frame the button went 0→1
 *   - .left_released= true ONLY on the frame the button went 1→0
 *
 * JS analogy:
 *   mousedown  event → left_pressed  = true for one frame
 *   mouseup    event → left_released = true for one frame
 *   mousemove  event → x, y updated every frame
 */
typedef struct {
    int x, y;              /* current canvas pixel position          */
    int prev_x, prev_y;    /* last frame's canvas position           */
    int left_down;         /* 1 = button is currently held           */
    int left_pressed;      /* 1 = button went down THIS frame only   */
    int left_released;     /* 1 = button went up   THIS frame only   */
    int right_pressed;     /* 1 = right button down this frame only  */
} MouseState;

/* UPDATE_BUTTON: compute edge flags from the current OS button state.
 *
 * Usage:
 *   UPDATE_BUTTON(mouse.left_pressed, mouse.left_released,
 *                 mouse.left_down,    is_down_now);
 *
 * was_down is read THEN written, so it acts as "previous frame state".
 * pressed  = was NOT down last frame AND is down now  (0 → 1 transition)
 * released = WAS down last frame     AND is up  now  (1 → 0 transition)
 */
#define UPDATE_BUTTON(pressed, released, was_down, is_down_now) \
    do {                                                          \
        (pressed)  = (!(was_down) &&  (is_down_now));            \
        (released) = ( (was_down) && !(is_down_now));            \
        (was_down) = (is_down_now);                              \
    } while (0)
```

---

## Step 2: Letterbox transform — theory

When the Raylib window is resized, `platform_display_backbuffer` scales the 760×520
canvas to fit while keeping the aspect ratio.  Black bars fill the unused space.
The scale factor and offsets are stored in `g_scale`, `g_offset_x`, `g_offset_y`.

To go the other direction — from the OS window mouse position back to canvas pixels —
we invert that transform:

```
canvas_x = (window_mouse_x - g_offset_x) / g_scale
canvas_y = (window_mouse_y - g_offset_y) / g_scale
```

Then from canvas pixels to grid cells:

```
col = canvas_x / CELL_SIZE
row = canvas_y / CELL_SIZE
```

Visual diagram:

```
┌────────────────────────────────┐  ← OS window (e.g. 1140×780 after resize)
│░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│  ← black bar (g_offset_y pixels tall)
│░░┌──────────────────────┐░░░░│
│░░│  Canvas 760×520       │░░░░│  ← canvas starts at (g_offset_x, g_offset_y)
│░░│  scaled by g_scale    │░░░░│     each pixel = g_scale OS pixels
│░░└──────────────────────┘░░░░│
│░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│  ← black bar
└────────────────────────────────┘

  mouse OS pos (mx, my)
  canvas pos = ((mx - g_offset_x) / g_scale,  (my - g_offset_y) / g_scale)
  grid cell  = (canvas_x / CELL_SIZE, canvas_y / CELL_SIZE)
```

For X11 the window is **fixed-size** (760×520) — no letterbox.  Canvas pixels equal
window pixels directly, so `g_scale = 1.0f` and `g_offset_x = g_offset_y = 0`.

---

## Step 3: `platform_get_input` — Raylib implementation

Add or replace `platform_get_input` in `src/main_raylib.c`:

```c
/* src/main_raylib.c  —  platform_get_input (Lesson 03 version)
 *
 * Reads Raylib mouse position, transforms it through the letterbox, then
 * updates the MouseState struct with the current position and button edges.
 *
 * g_scale, g_offset_x, g_offset_y are set each frame inside
 * platform_display_backbuffer (they are already globals in main_raylib.c). */
void platform_get_input(MouseState *mouse) {
    if (!mouse) return;

    /* Save previous position for delta computation by callers. */
    mouse->prev_x = mouse->x;
    mouse->prev_y = mouse->y;

    /* Raw window mouse position from Raylib. */
    Vector2 raw = GetMousePosition();

    /* Invert letterbox transform: window → canvas pixel coordinates.
     * g_scale is the ratio (canvas_size / window_size) computed last frame.
     * Clamp so out-of-canvas positions don't produce negative indices. */
    if (g_scale > 0.0f) {
        mouse->x = (int)((raw.x - (float)g_offset_x) / g_scale);
        mouse->y = (int)((raw.y - (float)g_offset_y) / g_scale);
    } else {
        mouse->x = (int)raw.x;
        mouse->y = (int)raw.y;
    }

    /* Clamp to canvas bounds — avoids negative cell indices. */
    mouse->x = CLAMP(mouse->x, 0, CANVAS_W - 1);
    mouse->y = CLAMP(mouse->y, 0, CANVAS_H - 1);

    /* Edge detection: left button */
    int left_now  = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    UPDATE_BUTTON(mouse->left_pressed, mouse->left_released,
                  mouse->left_down,    left_now);

    /* Right button — only need "pressed" edge for deselect */
    mouse->right_pressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
}
```

---

## Step 4: `platform_get_input` — X11 implementation

Add or replace `platform_get_input` in `src/main_x11.c`:

```c
/* src/main_x11.c  —  platform_get_input (Lesson 03 version)
 *
 * Drains the X11 event queue.  Mouse position comes from MotionNotify
 * and ButtonPress/ButtonRelease events.  X11 window is fixed-size
 * (760×520), so canvas pixels == window pixels — no letterbox math. */
void platform_get_input(MouseState *mouse) {
    Atom wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);

    if (mouse) {
        mouse->prev_x        = mouse->x;
        mouse->prev_y        = mouse->y;
        mouse->left_pressed  = 0;
        mouse->left_released = 0;
        mouse->right_pressed = 0;
    }

    while (XPending(g_display)) {
        XEvent ev;
        XNextEvent(g_display, &ev);

        switch (ev.type) {
        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == wm_delete)
                g_should_quit = 1;
            break;

        case MotionNotify:
            if (mouse) {
                /* X11 window is fixed-size: no letterbox transform needed. */
                mouse->x = CLAMP(ev.xmotion.x, 0, CANVAS_W - 1);
                mouse->y = CLAMP(ev.xmotion.y, 0, CANVAS_H - 1);
            }
            break;

        case ButtonPress:
            if (!mouse) break;
            if (ev.xbutton.button == Button1) {
                /* UPDATE_BUTTON needs the "was_down" to be false before press.
                 * Set left_down=0 first so the macro fires pressed=1. */
                int is_down = 1;
                UPDATE_BUTTON(mouse->left_pressed, mouse->left_released,
                              mouse->left_down,    is_down);
            }
            if (ev.xbutton.button == Button3) {
                mouse->right_pressed = 1;
            }
            break;

        case ButtonRelease:
            if (!mouse) break;
            if (ev.xbutton.button == Button1) {
                int is_down = 0;
                UPDATE_BUTTON(mouse->left_pressed, mouse->left_released,
                              mouse->left_down,    is_down);
            }
            break;

        default:
            break;
        }
    }
}
```

---

## Step 5: Additions to `src/game.h`

```c
/* ===== LESSON 03 additions to src/game.h ===== */

/* Add these fields to GameState: */
typedef struct {
    int should_quit;

    uint8_t grid[GRID_ROWS * GRID_COLS];
    int     dist[GRID_ROWS * GRID_COLS];

    /* Mouse input — updated by platform_get_input each frame. */
    MouseState mouse;

    /* Grid cell the mouse is currently over.
     * -1 when the mouse is outside the grid area. */
    int hover_col;
    int hover_row;
} GameState;
```

---

## Step 6: Additions to `src/game.c`

```c
/* src/game.c  —  Desktop Tower Defense | Game Logic (Lesson 03 additions) */
#include "game.h"

/* -----------------------------------------------------------------------
 * mouse_in_grid: return 1 if canvas position (x, y) is inside the grid area.
 *
 * The grid occupies pixels (0, 0) → (GRID_PIXEL_W-1, GRID_PIXEL_H-1).
 * Positions on or to the right of GRID_PIXEL_W are in the side panel.
 *
 * JS analogy:
 *   function mouseInGrid(x, y) {
 *     return x >= 0 && x < GRID_PIXEL_W && y >= 0 && y < GRID_PIXEL_H;
 *   }
 * ----------------------------------------------------------------------- */
static int mouse_in_grid(int canvas_x, int canvas_y) {
    return (canvas_x >= 0 && canvas_x < GRID_PIXEL_W &&
            canvas_y >= 0 && canvas_y < GRID_PIXEL_H);
}

void game_init(GameState *state) {
    memset(state, 0, sizeof(*state));
    state->hover_col = -1;
    state->hover_row = -1;
    state->grid[ENTRY_ROW * GRID_COLS + ENTRY_COL] = CELL_ENTRY;
    state->grid[EXIT_ROW  * GRID_COLS + EXIT_COL ] = CELL_EXIT;
}

void game_update(GameState *state, float dt) {
    (void)dt;

    /* ----------------------------------------------------------------
     * Hover cell computation.
     *
     * state->mouse.x/y are already in canvas pixel coordinates (the
     * platform backend applied the letterbox transform before calling
     * game_update).
     *
     * Pixel → cell:
     *   col = canvas_x / CELL_SIZE
     *   row = canvas_y / CELL_SIZE
     *
     * Integer division truncates toward zero, which gives the correct
     * cell index because CELL_SIZE divides the canvas evenly.
     *
     * JS analogy:
     *   const col = Math.floor(mouse.x / CELL_SIZE);
     *   const row = Math.floor(mouse.y / CELL_SIZE);
     * ---------------------------------------------------------------- */
    if (mouse_in_grid(state->mouse.x, state->mouse.y)) {
        state->hover_col = state->mouse.x / CELL_SIZE;
        state->hover_row = state->mouse.y / CELL_SIZE;
    } else {
        state->hover_col = -1;
        state->hover_row = -1;
    }
}

/* draw_grid: same as Lesson 02, but now also draws the hover highlight. */
static void draw_grid(const GameState *state, Backbuffer *bb) {
    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            int px = col * CELL_SIZE;
            int py = row * CELL_SIZE;

            int       idx  = row * GRID_COLS + col;
            CellState cell = (CellState)state->grid[idx];
            uint32_t  color;

            if (cell == CELL_ENTRY) {
                color = COLOR_ENTRY_CELL;
            } else if (cell == CELL_EXIT) {
                color = COLOR_EXIT_CELL;
            } else {
                color = ((col + row) % 2 == 0) ? COLOR_GRID_EVEN : COLOR_GRID_ODD;
            }

            draw_rect(bb, px, py, CELL_SIZE, CELL_SIZE, color);

            /* Hover highlight: yellow overlay on the hovered cell.
             * We skip CELL_ENTRY and CELL_EXIT so those always show their
             * distinctive colors.
             * draw_rect_blend blends the yellow at 50% opacity. */
            if (col == state->hover_col && row == state->hover_row &&
                cell != CELL_ENTRY && cell != CELL_EXIT) {
                draw_rect_blend(bb, px, py, CELL_SIZE, CELL_SIZE,
                                COLOR_HOVER, 128);
            }
        }
    }
}

void game_render(const GameState *state, Backbuffer *bb) {
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BG);
    draw_grid(state, bb);
    draw_rect(bb, GRID_PIXEL_W, 0, bb->width - GRID_PIXEL_W, bb->height, COLOR_PANEL_BG);
}
```

---

## Step 7: Wire it up in both `main()` functions

The main loops already call `platform_get_input` and `game_update`.  No changes
are needed to `main()` itself — the new fields are transparent to the loop:

```c
/* Inside the game loop (both backends — already present from Lesson 01) */
platform_get_input(&state.mouse);   /* fills state.mouse.x/y + button edges */
game_update(&state, dt);            /* computes hover_col / hover_row        */
game_render(&state, &bb);           /* draws grid + yellow hover highlight   */
platform_display_backbuffer(&bb);
```

---

## Build and run

```bash
# Raylib
./build-dev.sh --backend=raylib -r

# X11
./build-dev.sh --backend=x11 -r
```

**Expected output:** Move the mouse over the grid and a yellow semi-transparent cell
follows the cursor.  The entry cell (column 0, row 9) and exit cell (column 29, row 9)
keep their green/red colors even when hovered.  Mouse over the side panel area: no
highlight, hover_col/hover_row = -1.

---

## Exercises

1. **Beginner:** Change the hover color from yellow (`COLOR_HOVER`) to cyan
   (`GAME_RGB(0x00, 0xFF, 0xFF)`).  Change the blend alpha from 128 to 200.
   Observe how a higher alpha makes the overlay more opaque.

2. **Intermediate:** Add a `hover_prev_col` / `hover_prev_row` pair to `GameState`
   and detect when the mouse moves from one cell to another (i.e., the hover cell
   changed this frame).  Print `"entered cell (%d, %d)\n"` to stdout using `printf`
   when the transition happens.  This pattern will be needed for drag-placement later.

3. **Challenge:** Raylib's `GetMousePosition` returns sub-pixel floats.  Currently
   we cast to `int` with truncation.  Change the transform to use `(int)floorf(...)` 
   from `<math.h>` and verify it makes no visible difference at 1× scale.  At what
   scale factor would truncation vs floor start to matter for hit-testing accuracy?

---

## What's next

In Lesson 04 we implement the BFS distance field — the core algorithm that gives
every creep a path from entry to exit without any per-creep pathfinding.  You'll
also add a debug visualization that color-codes cells by their distance value:
green near the exit, red far away.
