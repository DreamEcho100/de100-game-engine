# Lesson 04 — Typed Enums: TETROMINO_BY_IDX, TETROMINO_R_DIR, TETRIS_FIELD_CELL

## By the end of this lesson you will have:

All "magic numbers" in the game replaced by self-documenting named enums. A rotation direction is no longer `int r = 1` — it's `TETROMINO_R_DIR rotation = TETROMINO_R_90`. A field cell value is no longer `9` — it's `TETRIS_FIELD_WALL`.

---

## The Problem with Magic Numbers

The original code used plain integers for everything:

```c
state->current_piece    = 3;      /* What piece is 3? T? J? */
state->current_rotation = 2;      /* Is 2 "180 degrees"? Which way? */
state->field[idx]       = 9;      /* What does 9 mean? */
```

When you read `if (state->field[i] == 8)` three months later, you have to remember that 8 means "line complete, flashing." Named enums make the code self-documenting.

---

## Step 1 — Tetromino piece index: `TETROMINO_BY_IDX`

Add to `tetris.h`:

```c
typedef enum {
    TETROMINO_I_IDX,   /* 0 */
    TETROMINO_J_IDX,   /* 1 */
    TETROMINO_L_IDX,   /* 2 */
    TETROMINO_O_IDX,   /* 3 */
    TETROMINO_S_IDX,   /* 4 */
    TETROMINO_T_IDX,   /* 5 */
    TETROMINO_Z_IDX,   /* 6 */
} TETROMINO_BY_IDX;
```

**New C concept — `typedef enum`:**  
An `enum` defines a set of named integer constants. `TETROMINO_I_IDX` = 0, `TETROMINO_J_IDX` = 1, etc. by default. `typedef enum { ... } NAME` creates a type alias so you can write `TETROMINO_BY_IDX piece` instead of `enum TETROMINO_BY_IDX piece`.

This enum matches the order of the `TETROMINOES[]` array. `TETROMINOES[TETROMINO_I_IDX]` = the I-piece string.

---

## Step 2 — Rotation direction: `TETROMINO_R_DIR`

```c
typedef enum {
    TETROMINO_R_0,     /* 0° — default orientation */
    TETROMINO_R_90,    /* 90° clockwise */
    TETROMINO_R_180,   /* 180° — upside down */
    TETROMINO_R_270,   /* 270° clockwise = 90° counter-clockwise */
} TETROMINO_R_DIR;
```

Before: `int rotation` where 0=0°, 1=90°, 2=180°, 3=270°.  
After: `TETROMINO_R_DIR rotation` where `TETROMINO_R_90` makes the intent obvious.

In `tetromino_pos_value`, the switch cases become:

```c
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r) {
    switch (r) {
    case TETROMINO_R_0:   return py * 4 + px;
    case TETROMINO_R_90:  return 12 + py - (px * 4);
    case TETROMINO_R_180: return 15 - (py * 4) - px;
    case TETROMINO_R_270: return 3 - py + (px * 4);
    }
    return 0;
}
```

The compiler will warn if a case is missing — an enum switch is a safety net.

---

## Step 3 — Field cell values: `TETRIS_FIELD_CELL`

```c
typedef enum {
    TETRIS_FIELD_EMPTY    = 0,  /* must be zero (memset-compatible) */
    TETRIS_FIELD_I        = 1,  /* locked I piece */
    TETRIS_FIELD_J        = 2,
    TETRIS_FIELD_L        = 3,
    TETRIS_FIELD_O        = 4,
    TETRIS_FIELD_S        = 5,
    TETRIS_FIELD_T        = 6,
    TETRIS_FIELD_Z        = 7,
    TETRIS_FIELD_WALL     = 8,  /* boundary wall */
    TETRIS_FIELD_TMP_FLASH = 9, /* completed line, showing flash animation */
} TETRIS_FIELD_CELL;
```

**Why `EMPTY = 0`?**  
When we initialize `GameState` with `memset(state, 0, sizeof(*state))`, every byte is set to 0. If `TETRIS_FIELD_EMPTY = 0`, the entire field starts empty for free — no initialization loop needed. This is a common C idiom: **make the zero value the "nothing" or "default" state**.

**Why `piece_index + 1` for locked pieces?**  
`TETRIS_FIELD_I = 1` means a locked I-piece. The pattern: `field_value = piece_index + 1`. So when rendering:
```c
draw_cell(bb, col, row, get_tetromino_color(cell_value - 1));
```
`cell_value - 1` converts back to the piece index for color lookup. Zero (`TETRIS_FIELD_EMPTY`) is never passed to `get_tetromino_color`.

---

## Step 4 — Named tetromino shape constants

In `tetris.c`, replace the anonymous inline strings with named constants:

```c
#define TETROMINO_SIZE  16
#define TETROMINOS_COUNT 7

const char TETROMINO_I[TETROMINO_SIZE] = "..X...X...X...X.";
const char TETROMINO_J[TETROMINO_SIZE] = "..X...X..XX.....";
const char TETROMINO_L[TETROMINO_SIZE] = ".X...X...XX.....";
const char TETROMINO_O[TETROMINO_SIZE] = ".....XX..XX.....";
const char TETROMINO_S[TETROMINO_SIZE] = ".X...XX...X.....";
const char TETROMINO_T[TETROMINO_SIZE] = "..X..XX...X.....";
const char TETROMINO_Z[TETROMINO_SIZE] = "..X..XX..X......";

const char *TETROMINOES[TETROMINOS_COUNT] = {
    TETROMINO_I,
    TETROMINO_J,
    TETROMINO_L,
    TETROMINO_O,
    TETROMINO_S,
    TETROMINO_T,
    TETROMINO_Z,
};
```

**New C concept — `extern` in `tetris.h`:**  
Add this declaration to `tetris.h` so other files can use the array:
```c
#define TETROMINO_SIZE   16
#define TETROMINO_SPAN   '.'
#define TETROMINO_BLOCK  'X'
#define TETROMINOS_COUNT  7

extern const char *TETROMINOES[7];
```

`extern` says "this variable is defined in some other `.c` file — trust me." The linker connects the declaration to the definition in `tetris.c`.

**New ordering:** The original course used I/S/Z/T/J/L/O order. The new order is **I/J/L/O/S/T/Z** — matching the `TETROMINO_BY_IDX` enum and the Tetris Guideline convention. This means `TETROMINOES[TETROMINO_I_IDX]` is the I-piece, `TETROMINOES[TETROMINO_Z_IDX]` is the Z-piece, etc.

---

## Step 5 — `TETROMINOS_COUNT` and `SIDEBAR_WIDTH`

Add to `tetris.h`:

```c
#define FIELD_WIDTH    12
#define FIELD_HEIGHT   18
#define CELL_SIZE      30   /* pixels per grid cell */
#define SIDEBAR_WIDTH  200  /* extra pixels to the right of the field */
#define TETROMINO_LAYER_COUNT 4  /* each tetromino fits in a 4x4 bounding box */
```

`SIDEBAR_WIDTH` was previously hardcoded in each platform backend. Centralizing it means changing the sidebar width in one place affects both backends.

Total window width: `FIELD_WIDTH * CELL_SIZE + SIDEBAR_WIDTH` = 12×30 + 200 = **560 pixels**.  
Total window height: `FIELD_HEIGHT * CELL_SIZE` = 18×30 = **540 pixels**.

---

## Step 6 — `TETROMINO_ROTATE_X_VALUE`

For rotation input direction:

```c
typedef enum {
    TETROMINO_ROTATE_X_NONE,
    TETROMINO_ROTATE_X_GO_LEFT,   /* Z key: rotate counter-clockwise */
    TETROMINO_ROTATE_X_GO_RIGHT,  /* X key: rotate clockwise */
} TETROMINO_ROTATE_X_VALUE;
```

Before: `int rotate_direction` where 1=clockwise, -1=counter-clockwise.  
After: `TETROMINO_ROTATE_X_VALUE value` where the intent is explicit.

---

## Visual: Field Cell Encoding

```
Field array index: row * FIELD_WIDTH + col

Value → Meaning
─────────────────────────────
  0   → TETRIS_FIELD_EMPTY     (empty space)
  1   → TETRIS_FIELD_I         (locked I piece, cyan)
  2   → TETRIS_FIELD_J         (locked J piece, blue)
  3   → TETRIS_FIELD_L         (locked L piece, orange)
  4   → TETRIS_FIELD_O         (locked O piece, yellow)
  5   → TETRIS_FIELD_S         (locked S piece, green)
  6   → TETRIS_FIELD_T         (locked T piece, magenta)
  7   → TETRIS_FIELD_Z         (locked Z piece, red)
  8   → TETRIS_FIELD_WALL      (boundary — drawn gray)
  9   → TETRIS_FIELD_TMP_FLASH (completed line, flash white)
```

When rendering (Lesson 10), you switch on this value:
```c
switch (cell_value) {
case TETRIS_FIELD_EMPTY: continue;
case TETRIS_FIELD_WALL:  draw_cell(bb, col, row, COLOR_GRAY); break;
/* TETRIS_FIELD_I...Z: draw with piece color */
/* TETRIS_FIELD_TMP_FLASH: draw white */
}
```

---

## Key Concepts

- `typedef enum` creates self-documenting integer constants
- `EMPTY = 0` — zero value is the "null" state, compatible with `memset`
- `piece_index + 1` encoding — 0 unambiguously means empty
- Named shape constants make `TETROMINOES[TETROMINO_T_IDX]` readable
- `extern` declaration in `.h` connects to the definition in `.c`
- `TETROMINOS_COUNT` and `SIDEBAR_WIDTH` in `tetris.h` — not per-backend

---

## Exercise

Map the original course's tetromino order (I/S/Z/T/J/L/O) to the new order (I/J/L/O/S/T/Z) by writing a translation table:

```
Old index 0 (I) → New TETROMINO_I_IDX (0) ✓
Old index 1 (S) → New TETROMINO_S_IDX (?)
...
```

Complete the mapping. Why does the ordering matter for the `TETRIS_FIELD_*` enum values?
