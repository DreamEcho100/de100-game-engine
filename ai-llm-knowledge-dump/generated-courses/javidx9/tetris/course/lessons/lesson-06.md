# Lesson 06 — Collision Detection

## By the end of this lesson you will have:

`tetromino_pos_value()` and `tetromino_does_piece_fit()` implemented — the two functions that make rotation and collision work. Pieces can be moved and rotated without passing through walls or locked cells.

---

## What Changed from the Original

This is mostly a **rename lesson**. The collision algorithm is the same; only the function names and parameter types changed:

| Original | New | Change |
|----------|-----|--------|
| `tetris_rotate(px, py, r)` | `tetromino_pos_value(px, py, r)` | Renamed for clarity |
| `tetris_does_piece_fit(state, piece, rotation, x, y)` | `tetromino_does_piece_fit(state, piece, rotation, pos_x, pos_y)` | Renamed |
| `int r` (raw integer) | `TETROMINO_R_DIR r` (typed enum) | Typed parameter |
| `const GameState *` | `GameState *` | Minor |

The algorithms inside are identical.

---

## Step 1 — `tetromino_pos_value`: coordinate-space rotation

```c
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r) {
    switch (r) {
    case TETROMINO_R_0:   return py * TETROMINO_LAYER_COUNT + px;
    case TETROMINO_R_90:  return 12 + py - (px * TETROMINO_LAYER_COUNT);
    case TETROMINO_R_180: return 15 - (py * TETROMINO_LAYER_COUNT) - px;
    case TETROMINO_R_270: return 3 - py + (px * TETROMINO_LAYER_COUNT);
    }
    return 0;
}
```

**What this function does:**  
Given a column `px` and row `py` within a 4×4 bounding box, returns the flat array index into the 16-character tetromino string — after rotating by `r * 90°` clockwise.

**The 4×4 grid layout (R_0, no rotation):**

```
     col: 0  1  2  3
row 0:    0  1  2  3
row 1:    4  5  6  7
row 2:    8  9 10 11
row 3:   12 13 14 15

Formula: index = row * 4 + col = py * 4 + px
```

**After 90° clockwise (R_90):**  
Position `(px, py)` maps to `(3 - py, px)` in the rotated grid.  
Flat index = `new_row * 4 + new_col = px * 4 + (3 - py) = 12 + py - px * 4`.

Derivation for each case:
```
R_0:   (px, py)         → py*4 + px              (identity)
R_90:  (3-py, px)       → 12 + py - px*4
R_180: (3-px, 3-py)     → 15 - py*4 - px
R_270: (py, 3-px)       → 3 - py + px*4
```

**Key concept:** No trigonometry. Rotation of discrete grid positions is just arithmetic. The formulas above are derived by applying the standard 2D rotation matrix `[cos θ, -sin θ; sin θ, cos θ]` to integer grid coordinates and simplifying.

---

## Step 2 — `tetromino_does_piece_fit`

```c
int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                              int pos_x, int pos_y) {
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            /* Map (px, py) in the 4x4 bounding box to the tetromino string */
            int pi = tetromino_pos_value(px, py, rotation);

            /* Skip empty cells in the piece */
            if (TETROMINOES[piece][pi] == TETROMINO_SPAN)
                continue;

            /* Field position this piece cell would occupy */
            int field_x = pos_x + px;
            int field_y = pos_y + py;

            /* Out-of-bounds cells are silently skipped.
             * This allows a piece to partially hang off the left/right edges
             * during spawn without triggering a false collision. */
            if (field_x < 0 || field_x >= FIELD_WIDTH)  continue;
            if (field_y < 0 || field_y >= FIELD_HEIGHT) continue;

            /* In bounds — check for overlap with any non-empty cell */
            int fi = field_y * FIELD_WIDTH + field_x;
            if (state->field[fi] != TETRIS_FIELD_EMPTY)
                return 0;  /* doesn't fit */
        }
    }
    return 1;  /* all solid cells checked — it fits */
}
```

**Why check `!= TETROMINO_SPAN` instead of `== TETROMINO_BLOCK`?**  
The check `TETROMINOES[piece][pi] == TETROMINO_SPAN` skips any cell that is '.' (empty space in the 4×4 box). The alternative `== TETROMINO_BLOCK` checks for 'X'. They should be equivalent — but the `!= SPAN` form is defensive: if the piece string ever contains a third character, `!= SPAN` would still process it correctly.

**Why skip out-of-bounds cells instead of returning 0?**  
When a piece spawns at the top center, its 4×4 bounding box may extend to negative X or Y. Those cells don't exist in the field, but they also aren't occupied — the piece fits. Returning 0 for out-of-bounds would prevent spawning. Skipping them is correct.

**Boundary collision:**  
Walls and the floor are stored in the field array as `TETRIS_FIELD_WALL` (value 8 ≠ 0 = `TETRIS_FIELD_EMPTY`). The check `field[fi] != TETRIS_FIELD_EMPTY` catches both walls and locked pieces with one comparison.

---

## Step 3 — Declare in `tetris.h`

```c
/* Returns the flat index into a tetromino string for position (px,py)
 * after rotating r*90° clockwise within a 4×4 bounding box. */
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r);

/* Returns 1 if piece fits at (pos_x, pos_y) with given rotation.
 * Returns 0 if any solid cell overlaps a non-empty field cell. */
int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                              int pos_x, int pos_y);
```

---

## How Collision is Used

Every piece movement attempt goes through `tetromino_does_piece_fit`:

```c
/* Try to move right: */
if (tetromino_does_piece_fit(state, index, rotation, x + 1, y))
    state->current_piece.x++;

/* Try to rotate clockwise: */
TETROMINO_R_DIR new_rot = (rotation + 1) % 4;  /* simple modulo wrap */
if (tetromino_does_piece_fit(state, index, new_rot, x, y))
    state->current_piece.rotation = new_rot;

/* Gravity drop: */
if (tetromino_does_piece_fit(state, index, rotation, x, y + 1))
    state->current_piece.y++;
else
    /* lock the piece — can't drop further */
```

The function is called many times per frame (up to ~5 per input + 1 for gravity). It only iterates 16 cells with simple arithmetic — very fast.

---

## Step 4 — Verify with a test

In `main()` after `game_init`, print a sanity check:

```c
/* Piece 0 (I) should fit at x=4, y=0, rotation 0 */
int fits = tetromino_does_piece_fit(
    &game_state,
    TETROMINO_I_IDX,
    TETROMINO_R_0,
    4, 0
);
printf("Piece fits: %d (expected 1)\n", fits);

/* Should NOT fit where a wall is — try near the right edge */
int fits_wall = tetromino_does_piece_fit(
    &game_state,
    TETROMINO_I_IDX,
    TETROMINO_R_0,
    FIELD_WIDTH - 1, 0
);
printf("Piece at wall: %d (expected 0)\n", fits_wall);
```

---

## Visual: Rotation of the I-Piece

```
String: "..X...X...X...X."

R_0 (no rotation):    R_90 (90° CW):
. . X .               . . . .
. . X .               X X X X
. . X .               . . . .
. . X .               . . . .

tetromino_pos_value(2, 0, R_0)  = 2       (column 2, row 0)
tetromino_pos_value(0, 1, R_90) = 4       (column 0, row 1 of rotated grid)
```

---

## Key Concepts

- `tetromino_pos_value(px, py, r)` maps grid coordinates to string index under rotation
- Four rotation formulas derived from 2D rotation matrix applied to integer coordinates
- `tetromino_does_piece_fit` iterates the 4×4 box, skips empty cells and out-of-bounds
- Walls stored as `TETRIS_FIELD_WALL` (≠ 0) are caught by the same `!= EMPTY` check
- Out-of-bounds cells are skipped (not failed) — allows spawning with partial overhang

---

## Exercise

Trace through `tetromino_pos_value` for the T-piece (`"..X..XX...X....."`) at rotation `TETROMINO_R_90`. Draw the resulting shape on a 4×4 grid. Does it match what you'd expect a T rotated 90° clockwise to look like?
