# Lesson 08 — Drawing Primitives in tetris.c

## By the end of this lesson you will have:

`draw_rect`, `draw_cell`, and `draw_piece` implemented in `tetris.c` — the building blocks for all game rendering. You can fill rectangles in the backbuffer, draw individual grid cells, and draw complete tetrominoes.

---

## The Architecture Shift

In the original course, drawing was done by platform backends:

```c
/* X11 original: */
XSetForeground(display, gc, colors[cell]);
XFillRectangle(display, window, gc, x, y, CELL_SIZE-1, CELL_SIZE-1);

/* Raylib original: */
DrawRectangle(x, y, CELL_SIZE-1, CELL_SIZE-1, piece_colors[cell]);
```

Each backend had its own drawing code. To add an SDL backend, you'd write a third set.

**New approach:** drawing lives entirely in `tetris.c`. It writes pixels directly into the `TetrisBackbuffer`. Platform backends just display the finished buffer.

---

## Step 1 — `draw_rect`: fill a rectangle with clipping

Add to `tetris.c` (not `tetris.h` — this implementation is internal, public API declared in `.h`):

```c
void draw_rect(TetrisBackbuffer *bb, int x, int y, int w, int h,
               uint32_t color) {
    /* Clip to backbuffer bounds */
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > bb->width  ? bb->width  : (x + w);
    int y1 = (y + h) > bb->height ? bb->height : (y + h);

    for (int py = y0; py < y1; py++) {
        uint32_t *row = bb->pixels + py * (bb->pitch / 4);
        for (int px = x0; px < x1; px++) {
            row[px] = color;
        }
    }
}
```

**Why clipping?**  
If `x = -5` and `w = 10`, the actual drawable area starts at `x0 = 0`. Without clipping, the inner loop would write to `bb->pixels[-5]`, `bb->pixels[-4]`, etc. — memory before the pixel array. That's undefined behavior — likely a crash or memory corruption.

**`bb->pitch / 4`:**  
`pitch` is bytes per row. Dividing by 4 converts to `uint32_t` units (pixels per row). Then `bb->pixels + py * (pitch/4)` is a pointer to the start of row `py`.

**New C concept — ternary operator `a ? b : c`:**  
Returns `b` if `a` is true, `c` otherwise. `int x0 = x < 0 ? 0 : x` means "if x is negative, clamp to 0, otherwise keep x."

**Declare in `tetris.h`:**
```c
void draw_rect(TetrisBackbuffer *bb, int x, int y, int w, int h,
               uint32_t color);
```

---

## Step 2 — `draw_cell`: grid coordinates to pixels

```c
static void draw_cell(TetrisBackbuffer *bb, int col, int row,
                      uint32_t color) {
    int x = col * CELL_SIZE + 1;
    int y = row * CELL_SIZE + 1;
    int w = CELL_SIZE - 2;
    int h = CELL_SIZE - 2;
    draw_rect(bb, x, y, w, h, color);
}
```

**Coordinate conversion:**  
The field is `FIELD_WIDTH × FIELD_HEIGHT` cells. Each cell is `CELL_SIZE` pixels (30px). Cell `(col, row)` maps to pixel `(col * 30, row * 30)`.

**Why `+1` and `-2`?**  
`col * CELL_SIZE + 1` starts 1px inside the cell. `CELL_SIZE - 2` ends 1px before the cell's right/bottom edge. This leaves a 1px gap between adjacent cells — you see grid lines for free without drawing them explicitly:

```
Cell (0,0):  x=1,  y=1,  w=28, h=28
Cell (1,0):  x=31, y=1,  w=28, h=28
Gap between: 1px of black (background)
```

`static` means this function is private to `tetris.c` — not declared in `tetris.h`. Only `tetris.c` needs it.

---

## Step 3 — Color lookup for tetromino types

```c
static uint32_t get_tetromino_color(TETROMINO_BY_IDX index) {
    switch (index) {
    case TETROMINO_I_IDX: return COLOR_CYAN;
    case TETROMINO_J_IDX: return COLOR_BLUE;
    case TETROMINO_L_IDX: return COLOR_ORANGE;
    case TETROMINO_O_IDX: return COLOR_YELLOW;
    case TETROMINO_S_IDX: return COLOR_GREEN;
    case TETROMINO_T_IDX: return COLOR_MAGENTA;
    case TETROMINO_Z_IDX: return COLOR_RED;
    default:              return COLOR_GRAY;
    }
}
```

The original course stored colors as `PIECE_COLORS[]` arrays in each platform backend. Now one function in `tetris.c` owns the piece-color mapping — one place to change all colors.

---

## Step 4 — `draw_piece`: draw a full tetromino

```c
static void draw_piece(TetrisBackbuffer *bb, int piece_index,
                       int field_col, int field_row,
                       uint32_t color, TETROMINO_R_DIR rotation) {
    for (int py = 0; py < TETROMINO_LAYER_COUNT; py++) {
        for (int px = 0; px < TETROMINO_LAYER_COUNT; px++) {
            int pi = tetromino_pos_value(px, py, rotation);
            if (TETROMINOES[piece_index][pi] == TETROMINO_BLOCK) {
                draw_cell(bb, field_col + px, field_row + py, color);
            }
        }
    }
}
```

This iterates all 16 positions in the 4×4 bounding box. For each `'X'` cell, it calls `draw_cell` at the field position `(field_col + px, field_row + py)`.

`tetromino_pos_value(px, py, rotation)` — from Lesson 06 — applies the rotation to map `(px, py)` to the correct index in the tetromino string.

---

## Step 5 — Test it in `tetris_render` (first version)

Add a skeleton `tetris_render` to `tetris.c`:

```c
void tetris_render(TetrisBackbuffer *backbuffer, GameState *state) {
    /* Clear to black */
    for (int i = 0; i < backbuffer->width * backbuffer->height; i++)
        backbuffer->pixels[i] = COLOR_BLACK;

    /* Draw the field */
    for (int row = 0; row < FIELD_HEIGHT; row++) {
        for (int col = 0; col < FIELD_WIDTH; col++) {
            unsigned char cell = state->field[row * FIELD_WIDTH + col];
            if (cell == TETRIS_FIELD_EMPTY) continue;
            if (cell == TETRIS_FIELD_WALL) {
                draw_cell(backbuffer, col, row, COLOR_GRAY);
            } else if (cell == TETRIS_FIELD_TMP_FLASH) {
                draw_cell(backbuffer, col, row, COLOR_WHITE);
            } else {
                /* cell = piece_index + 1, so piece_index = cell - 1 */
                draw_cell(backbuffer, col, row,
                          get_tetromino_color(cell - 1));
            }
        }
    }

    /* Draw the falling piece */
    draw_piece(backbuffer,
               state->current_piece.index,
               state->current_piece.x,
               state->current_piece.y,
               get_tetromino_color(state->current_piece.index),
               state->current_piece.rotation);
}
```

Build and run — you should see:
- Gray walls on the left, right, and bottom
- A colored tetromino falling from the top
- Black background (1px grid lines visible between cells)

---

## Step 6 — Declare `tetris_render` in `tetris.h`

```c
void tetris_render(TetrisBackbuffer *backbuffer, GameState *state);
```

And the drawing primitives that platforms may also use:

```c
void draw_rect(TetrisBackbuffer *bb, int x, int y, int w, int h,
               uint32_t color);
void draw_text(TetrisBackbuffer *bb, int x, int y, const char *text,
               uint32_t color, int scale);  /* added in Lesson 09 */
```

---

## Visual: Coordinate System

```
Pixel (0,0) ─── top-left corner of the window
                 ↓ Y increases downward
col=0, row=0 → pixel (1, 1)  to  pixel (28, 28)  ← draw_cell(bb, 0, 0, color)
col=1, row=0 → pixel (31, 1) to  pixel (58, 28)  ← draw_cell(bb, 1, 0, color)
              ← 1px gap (background) →

Full field: cols 0..11, rows 0..17
Wall cells: col 0, col 11, row 17
```

---

## How `draw_rect` Writes a Single Pixel

```c
/* Pixel at column px, row py: */
uint32_t *row = bb->pixels + py * (bb->pitch / 4);
row[px] = color;

/* Equivalent: */
bb->pixels[py * (bb->pitch / 4) + px] = color;

/* For tightly-packed buffer (pitch = width * 4): */
bb->pixels[py * bb->width + px] = color;
```

One integer assignment. No function call overhead. The inner loop of `draw_rect` is essentially:

```c
for (int px = x0; px < x1; px++)
    row[px] = color;
```

This is a tight memory write — the CPU can potentially vectorize it (write 4 pixels at once with SIMD). Very fast for the sizes we use.

---

## Key Concepts

- `draw_rect` writes directly into `backbuffer->pixels` — no platform API
- Clipping prevents out-of-bounds writes (mandatory for safety)
- `draw_cell` converts grid (col, row) to pixel (x, y) with 1px padding for grid lines
- `get_tetromino_color` centralizes piece-color mapping in one place
- `draw_piece` iterates the 4×4 box, calls `draw_cell` for each `'X'`
- `static` functions are private to the translation unit

---

## Exercise

Modify `draw_cell` to add a 1px bright border on the top and left edges of each cell (a "bevel" effect). What additional `draw_rect` calls do you need? Does this change the cell interior size?
