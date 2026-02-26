# Lesson 12 — Full HUD: Score, Level, Pieces, Next-Piece Preview

## By the end of this lesson you will have:

A complete sidebar HUD showing score (yellow), level and pieces count (cyan), a next-piece preview at half cell size, and a controls hint block at the bottom — all drawn by `tetris_render` using the bitmap font from Lesson 09.

---

## HUD Architecture

The HUD lives in `tetris_render()` in `tetris.c`. It uses only `draw_text`, `draw_rect`, and `get_tetromino_color` — functions we already have. No platform API is needed.

The sidebar occupies the area to the right of the field:
- X start: `FIELD_WIDTH * CELL_SIZE` = 12 × 30 = **360px**
- Width: `SIDEBAR_WIDTH` = **200px**
- Total window: 360 + 200 = **560px** wide

---

## Step 1 — Score display

```c
int sx = FIELD_WIDTH * CELL_SIZE + 10;  /* left margin in sidebar */
int sy = 10;                             /* top margin */
char buf[32];

/* Label in white, value in yellow */
draw_text(backbuffer, sx, sy, "SCORE", COLOR_WHITE, 2);
snprintf(buf, sizeof(buf), "%d", state->score);
draw_text(backbuffer, sx, sy + 16, buf, COLOR_YELLOW, 2);
```

At `scale=2`, each character glyph is 10×14 pixels. The label "SCORE" takes 5 characters × 12px spacing = 60px width, 14px height. Value appears `16px` below (row height) to leave 2px breathing room.

**Score formula** (from Lesson 07 locking logic):
- +25 per piece locked
- +(1 << lines_cleared) × 100 bonus (200/400/800/1600 for 1/2/3/4 lines)

---

## Step 2 — Level and pieces count

```c
/* Level and pieces on the same horizontal row */
draw_text(backbuffer, sx, sy + 40, "LEVEL", COLOR_WHITE, 2);
snprintf(buf, sizeof(buf), "%d", state->level);
draw_text(backbuffer, sx, sy + 56, buf, COLOR_CYAN, 2);

draw_text(backbuffer, sx + 80, sy + 40, "PIECES", COLOR_WHITE, 2);
snprintf(buf, sizeof(buf), "%d", state->pieces_count);
draw_text(backbuffer, sx + 80, sy + 56, buf, COLOR_CYAN, 2);
```

"LEVEL" and "PIECES" share a row (`sy + 40`), offset 80px apart. Both values appear `16px` below their labels in `COLOR_CYAN`.

**Level formula** (from `tetris_update`):
```c
if (state->pieces_count % 25 == 0)
    state->level++;
```
Every 25 pieces locked, level increments. Starting level is 0.

---

## Step 3 — Next-piece preview

```c
draw_text(backbuffer, sx, sy + 85, "NEXT", COLOR_WHITE, 2);

int preview_x         = sx;
int preview_y         = sy + 105;
int preview_cell_size = CELL_SIZE / 2;  /* 15px — half the normal cell */
```

The preview iterates the 4×4 bounding box at default rotation (0°):

```c
for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
        int pi = tetromino_pos_value(px, py, TETROMINO_R_0);
        if (TETROMINOES[state->current_piece.next_index][pi]
                == TETROMINO_BLOCK) {
            uint32_t color =
                get_tetromino_color(state->current_piece.next_index);
            draw_rect(backbuffer,
                      preview_x + px * preview_cell_size + 1,
                      preview_y + py * preview_cell_size + 1,
                      preview_cell_size - 2,
                      preview_cell_size - 2,
                      color);
        }
    }
}
```

**Why `draw_rect` instead of `draw_cell`?**  
`draw_cell` uses the fixed `CELL_SIZE` (30px). The preview needs `CELL_SIZE / 2` (15px). By calling `draw_rect` directly with `preview_cell_size`, we reuse the same `+1`/`-2` padding pattern but with the smaller cell size.

The 4×4 preview at 15px/cell = 60×60px total — compact but readable.

---

## Step 4 — Controls hint block

Anchored from the bottom of the field:

```c
int hint_y = FIELD_HEIGHT * CELL_SIZE - 90;
draw_text(backbuffer, sx, hint_y,      "CONTROLS",            COLOR_DARK_GRAY, 1);
draw_text(backbuffer, sx, hint_y + 18, "{} MOVE LEFT/RIGHT",  COLOR_DARK_GRAY, 1);
draw_text(backbuffer, sx, hint_y + 28, "v  SOFT DROP",        COLOR_DARK_GRAY, 1);
draw_text(backbuffer, sx, hint_y + 38, "Z  ROTATE LEFT",      COLOR_DARK_GRAY, 1);
draw_text(backbuffer, sx, hint_y + 48, "X  ROTATE RIGHT",     COLOR_DARK_GRAY, 1);
draw_text(backbuffer, sx, hint_y + 58, "R  RESTART",          COLOR_DARK_GRAY, 1);
draw_text(backbuffer, sx, hint_y + 68, "Q  QUIT",             COLOR_DARK_GRAY, 1);
```

`COLOR_DARK_GRAY` at `scale=1` renders small, unobtrusive text. At scale=1, each character is 6px wide, 7px tall. Lines are spaced 10px apart.

**Custom arrow characters:**
- `{` → ← (left arrow)
- `}` → → (right arrow)
- `v` → ↓ (down arrow)

From `FONT_SPECIAL` in `tetris.c` (Lesson 09). This lets `"{} MOVE LEFT/RIGHT"` render with visible arrow glyphs.

**Anchor calculation:**  
`FIELD_HEIGHT * CELL_SIZE` = 18 × 30 = 540px (window height). Subtracting 90px puts the hint block at y=450. The hint block is 7 rows × 10px spacing = ~80px tall. This fits cleanly above the floor.

---

## Step 5 — Layout summary

```
Sidebar layout (x = 360+10 = 370):

y= 10: SCORE
y= 26: [score value]
y= 40: LEVEL          PIECES
y= 56: [level value]  [pieces value]
y= 85: NEXT
y=105: [4×4 preview at 15px/cell]
...
y=450: CONTROLS
y=468: {} MOVE LEFT/RIGHT
y=478: v  SOFT DROP
y=488: Z  ROTATE LEFT
y=498: X  ROTATE RIGHT
y=508: R  RESTART
y=518: Q  QUIT
```

---

## Step 6 — Verify the complete render

After implementing, build and run. You should see:
- Gray boundary walls on left, right, and floor
- Colored falling piece
- Sidebar with "SCORE 0", "LEVEL 0", "PIECES 1"
- 4×4 next-piece preview in correct piece color
- Controls hint in dark gray at the bottom

Lock a piece to verify `pieces_count` increments. Clear a line to verify score jumps.

---

## Key Concepts

- HUD entirely in `tetris.c` — no platform drawing code
- `sx = FIELD_WIDTH * CELL_SIZE + 10` — sidebar left edge with margin
- Score label in `COLOR_WHITE`, value in `COLOR_YELLOW` — visual hierarchy
- Level and pieces share a row, offset 80px — compact layout
- Preview uses `CELL_SIZE / 2 = 15px` and `draw_rect` directly
- Controls hint at `FIELD_HEIGHT * CELL_SIZE - 90` — anchored to bottom
- `{`, `}`, `v` render as arrows via `FONT_SPECIAL` custom mappings

---

## Exercise

Add a "HIGH SCORE" display that persists across games within one session (stored as a local variable in `main()`). Show it below the score. When the game is over and the player's score exceeds the high score, update it and display "NEW!" next to the high score label.

How would you pass the high score to `tetris_render`? Would you add it to `GameState` or pass it as a separate parameter?
