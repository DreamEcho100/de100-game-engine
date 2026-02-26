# Lesson 10 — tetris_render(): One Function for All Platforms

## By the end of this lesson you will have:

`tetris_render(TetrisBackbuffer *, GameState *)` fully implemented in `tetris.c` — clearing the backbuffer, drawing the field, drawing the current piece, and rendering the HUD sidebar. Both X11 and Raylib backends remove all their drawing code and call this one function instead.

---

## The Architectural Pivot

Before this lesson, each platform backend had its own rendering code. After this lesson, both backends reduce to:

```c
/* Platform backend — after this lesson: */
tetris_render(&backbuffer, &state);   /* game draws itself */
platform_display_backbuffer(&backbuffer);  /* platform shows it */
```

The rendering relationship inverts: the game draws itself to a portable pixel buffer; each platform only needs to display it.

---

## Step 1 — Clear the backbuffer

```c
void tetris_render(TetrisBackbuffer *backbuffer, GameState *state) {
    /* Fill every pixel with black */
    for (int i = 0; i < backbuffer->width * backbuffer->height; i++) {
        backbuffer->pixels[i] = COLOR_BLACK;
    }
```

This is a simple memory fill. `memset` would be faster but it only sets bytes — not 32-bit values. A plain loop is clear and correct; the compiler often optimizes it to a SIMD fill anyway.

---

## Step 2 — Draw the field

```c
    for (int row = 0; row < FIELD_HEIGHT; row++) {
        for (int col = 0; col < FIELD_WIDTH; col++) {
            unsigned char cell_value =
                state->field[row * FIELD_WIDTH + col];

            switch (cell_value) {
            case TETRIS_FIELD_EMPTY:
                continue;  /* skip — background already black */
            case TETRIS_FIELD_WALL:
                draw_cell(backbuffer, col, row, COLOR_GRAY);
                break;
            case TETRIS_FIELD_TMP_FLASH:
                draw_cell(backbuffer, col, row, COLOR_WHITE);
                break;
            default:
                /* TETRIS_FIELD_I through TETRIS_FIELD_Z */
                /* cell_value = piece_index + 1 */
                draw_cell(backbuffer, col, row,
                          get_tetromino_color(cell_value - 1));
                break;
            }
        }
    }
```

The `default` case handles values 1–7 (`TETRIS_FIELD_I` through `TETRIS_FIELD_Z`). The formula `cell_value - 1` converts back to a `TETROMINO_BY_IDX` for color lookup.

---

## Step 3 — Draw the current falling piece

```c
    draw_piece(backbuffer,
               state->current_piece.index,
               state->current_piece.x,
               state->current_piece.y,
               get_tetromino_color(state->current_piece.index),
               state->current_piece.rotation);
```

This draws the live (not yet locked) falling piece on top of the field. Since we clear the backbuffer every frame, there's no need to "erase" the piece's previous position — it's simply not drawn at the old position in the new frame.

---

## Step 4 — HUD sidebar layout

The sidebar starts at `x = FIELD_WIDTH * CELL_SIZE` (right edge of the field) + a small margin:

```c
    int sx = FIELD_WIDTH * CELL_SIZE + 10;  /* sidebar x start */
    int sy = 10;                             /* sidebar y start */
    char buf[32];
```

**Score:**
```c
    draw_text(backbuffer, sx, sy, "SCORE", COLOR_WHITE, 2);
    snprintf(buf, sizeof(buf), "%d", state->score);
    draw_text(backbuffer, sx, sy + 16, buf, COLOR_YELLOW, 2);
```

At scale=2, each glyph is 10×14 pixels. "SCORE" is 5 characters × 12px = 60px wide, 14px tall. The value appears 16px below the label.

**Level and pieces count side by side:**
```c
    draw_text(backbuffer, sx, sy + 40, "LEVEL", COLOR_WHITE, 2);
    snprintf(buf, sizeof(buf), "%d", state->level);
    draw_text(backbuffer, sx, sy + 56, buf, COLOR_CYAN, 2);

    draw_text(backbuffer, sx + 80, sy + 40, "PIECES", COLOR_WHITE, 2);
    snprintf(buf, sizeof(buf), "%d", state->pieces_count);
    draw_text(backbuffer, sx + 80, sy + 56, buf, COLOR_CYAN, 2);
```

"LEVEL" and "PIECES" share a row (same `sy + 40`), offset by 80px horizontally.

---

## Step 5 — Next piece preview

```c
    draw_text(backbuffer, sx, sy + 85, "NEXT", COLOR_WHITE, 2);

    int preview_x         = sx;
    int preview_y         = sy + 105;
    int preview_cell_size = CELL_SIZE / 2;  /* 15px cells for the mini preview */

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

The preview uses `draw_rect` directly (not `draw_cell`) because the cell size is different (`CELL_SIZE / 2 = 15px` vs `CELL_SIZE = 30px`). The `+1`/`-2` padding is the same as `draw_cell` — 1px gaps between preview cells.

The preview is always shown at rotation 0 (`TETROMINO_R_0`), regardless of the piece's actual spawn rotation.

---

## Step 6 — Controls hint

```c
    int hint_y = FIELD_HEIGHT * CELL_SIZE - 90;
    draw_text(backbuffer, sx, hint_y,      "CONTROLS",         COLOR_DARK_GRAY, 1);
    draw_text(backbuffer, sx, hint_y + 18, "{} MOVE LEFT/RIGHT", COLOR_DARK_GRAY, 1);
    draw_text(backbuffer, sx, hint_y + 28, "v  SOFT DROP",     COLOR_DARK_GRAY, 1);
    draw_text(backbuffer, sx, hint_y + 38, "Z  ROTATE LEFT",   COLOR_DARK_GRAY, 1);
    draw_text(backbuffer, sx, hint_y + 48, "X  ROTATE RIGHT",  COLOR_DARK_GRAY, 1);
    draw_text(backbuffer, sx, hint_y + 58, "R  RESTART",       COLOR_DARK_GRAY, 1);
    draw_text(backbuffer, sx, hint_y + 68, "Q  QUIT",          COLOR_DARK_GRAY, 1);
```

`COLOR_DARK_GRAY` makes the controls subtle — visible but not distracting. `scale=1` (small text) fits the hint block in the lower sidebar.

`{` and `}` render as ← and → arrows (from `FONT_SPECIAL` in Lesson 09). `v` renders as ↓.

The hint block is anchored from the bottom: `hint_y = FIELD_HEIGHT * CELL_SIZE - 90` puts it 90px above the bottom of the field.

---

## Step 7 — Remove drawing code from platform backends

In `main_x11.c`, delete the old `platform_render` function body (keep the declaration stub if needed). The display path becomes:

```c
/* After tetris_update: */
tetris_render(&backbuffer, &game_state);
platform_display_backbuffer(&backbuffer);
```

In `main_raylib.c`, remove `DrawRectangle`, `DrawText`, and `PIECE_COLORS[]`. Replace with:

```c
tetris_render(&backbuffer, &state);
UpdateTexture(texture, backbuffer.pixels);
BeginDrawing();
ClearBackground(BLACK);
DrawTexture(texture, 0, 0, WHITE);
EndDrawing();
```

Both backends now contain zero game-specific drawing code.

---

## Complete `tetris_render` skeleton

```c
void tetris_render(TetrisBackbuffer *backbuffer, GameState *state) {
    /* 1. Clear */
    for (int i = 0; i < backbuffer->width * backbuffer->height; i++)
        backbuffer->pixels[i] = COLOR_BLACK;

    /* 2. Field */
    for (int row = 0; row < FIELD_HEIGHT; row++)
        for (int col = 0; col < FIELD_WIDTH; col++)
            /* ... switch on field[row*FIELD_WIDTH+col] ... */

    /* 3. Falling piece */
    draw_piece(backbuffer, ...);

    /* 4. HUD */
    /* SCORE, LEVEL, PIECES, NEXT, controls */

    /* 5. Game over overlay (Lesson 13) */
    if (state->game_over) { /* ... */ }
}
```

---

## Key Concepts

- `tetris_render` owns all rendering — both backends call it, then display the result
- Clear every frame with a pixel loop (memset alternative for 32-bit values)
- Field switch: EMPTY → skip, WALL → gray, FLASH → white, piece values → colored
- `cell_value - 1` converts field encoding back to `TETROMINO_BY_IDX` for color lookup
- Preview uses `CELL_SIZE / 2` and `draw_rect` directly (different cell size)
- Custom arrow chars in control hints: `{` = ←, `}` = →, `v` = ↓
- Platforms lose all drawing code — they only call `tetris_render` + display

---

## Exercise

Add a "ghost piece" to `tetris_render`: raycast the current piece downward until it no longer fits, then draw it at that position with `COLOR_DARK_GRAY` (before drawing the real piece). Hint: use `tetromino_does_piece_fit` in a loop. This gives players visual feedback about where the piece will land.
