# Lesson 09 — Locking, Spawning & Game Over

---

## What You'll Have Working by the End

Pieces have a full life cycle:

1. Appear at the top center
2. Fall one row per second (auto) — you can move and rotate while falling
3. Land on the floor or another piece
4. **Lock** — their cells are copied permanently into the field array
5. **New piece spawns** at the top — you can see pieces stacking up
6. When the stack reaches the top — **GAME OVER** displayed, press R to restart

---

## Where We're Starting

At the end of lesson 08:
- Pieces fall at one row per second
- When a piece hits the floor, it stops and stays there forever
- No new piece ever appears
- `tetris_tick()` exists but has this stub where locking should be:

```c
/* If it doesn't fit, the piece has landed.
 * For now: do nothing. Piece just sits at the floor.
 * Lesson 09 adds locking and spawning here. */
```

---

## Step 1: Understand What "Locking" Means

When a piece lands, it becomes part of the field permanently. We **copy** its
solid cells into the `field` array.

Before this, the field stores only walls (value `9`) and previously locked
pieces (values `1–7`). The falling piece lives only in `current_piece`,
`current_x`, `current_y`, and `current_rotation` — it's not in the field.

After locking, the piece's cells are written into the field, and those four
variables are set to a new piece. The old piece is now indistinguishable from
a wall. The renderer draws it, and the collision system treats it as an obstacle.

Visual before and after locking a T-piece at the bottom:

```
BEFORE: field has only walls and previously locked pieces.
        The T-piece is "floating" — tracked by current_* variables.

  . . . . . . . . . .
  . . . . . . . . . .
  . . . . T . . . . .   ← T-piece tracked in current_* vars, NOT in field
  . T T T T . . . . .   ← collision/rendering reads current_* vars for this
  # # # # # # # # # #   ← floor (value 9)

AFTER locking: T-piece cells written into field. current_* vars now hold new piece.

  . . . . . . . . . .
  . . . . . . . . . .
  . . . . 4 . . . . .   ← field[...] = 4  (T-piece index 3 + 1)
  . 4 4 4 4 . . . . .   ← field[...] = 4
  # # # # # # # # # #   ← floor (value 9)
```

The field is one flat array. Everything in it — walls, locked pieces — blocks
movement exactly the same way. The collision system doesn't know or care whether
a cell was a wall from the start or a locked piece from last tick.

---

## Step 2: Add `next_piece` and `piece_count` to `GameState`

Open `src/tetris.h`. Your `GameState` currently has `speed`, `speed_count`, and
`piece_count` (added in lesson 08). Verify `next_piece` is there — it may have
been added in lesson 06's init. If not, add it:

```c
typedef struct {
    unsigned char field[FIELD_WIDTH * FIELD_HEIGHT];

    int current_piece;
    int current_rotation;
    int current_x;
    int current_y;
    int next_piece;     /* must be present — preview and spawn use this */

    int speed;
    int speed_count;
    int piece_count;    /* total pieces locked — used for difficulty scaling */

    int score;
    int game_over;
} GameState;
```

---

## Step 3: Initialize `next_piece` in `tetris_init()`

Open `src/tetris.c`. In `tetris_init()`, ensure `next_piece` is initialized:

```c
state->current_piece    = rand() % 7;
state->next_piece       = rand() % 7;  /* pre-generate so preview works */
state->current_x        = FIELD_WIDTH / 2;
state->current_y        = 0;
state->current_rotation = 0;
```

`rand() % 7` picks a random number from 0 to 6 — one of the seven piece types.

`FIELD_WIDTH / 2` = 12 / 2 = 6. The piece spawns at column 6, row 0 — the top
center of the playfield. That's column 6 of 12, so the 4-wide piece bounding
box spans columns 6–9, which is roughly the center.

---

## Step 4: Add Locking to `tetris_tick()`

Open `src/tetris.c`. Find `tetris_tick()`. Locate the comment that says
`/* Lesson 09 adds locking and spawning here. */`. Replace that entire comment
with this code:

```c
    /* ── Piece can't drop: LOCK IT ──
     * Write each solid cell of the piece into the field array permanently.
     * After this, the piece exists only as numbers in state->field.
     * The current_* variables will be reassigned to the new piece below. */
    for (int px = 0; px < 4; px++) {
        for (int py = 0; py < 4; py++) {
            int pi = tetris_rotate(px, py, state->current_rotation);
            if (TETROMINOES[state->current_piece][pi] != '.') {
                state->field[(state->current_y + py) * FIELD_WIDTH +
                             (state->current_x + px)] = state->current_piece + 1;
            }
        }
    }
```

Walk through this line by line:

`for px = 0..3, py = 0..3` — we iterate every cell of the 4×4 bounding box.

`tetris_rotate(px, py, state->current_rotation)` — converts the (px, py)
grid position to a flat index into the 16-character tetromino string, accounting
for the current rotation.

`TETROMINOES[state->current_piece][pi] != '.'` — is this cell solid? If it's
`'X'`, yes. If it's `'.'`, skip it (empty cell in the bounding box).

`state->field[...] = state->current_piece + 1` — write the piece's color code
into the field. We store `index + 1` because `0` means empty. If we stored
piece index `0` directly, we couldn't tell empty from the I-piece.

```
Piece index:  0   1   2   3   4   5   6
Stored as:    1   2   3   4   5   6   7   ← always > 0, so 0 = empty
```

---

## Step 5: Add Scoring

Right after the locking loop, add a simple score increment:

```c
    /* Score: 25 points for every piece successfully placed. */
    state->score += 25;
    state->piece_count++;
```

We'll add proper line-clear bonuses in lesson 10. For now, 25 points per piece
gives satisfying feedback: each lock is worth something.

---

## Step 6: Add Difficulty Scaling

After incrementing `piece_count`, add the speed increase logic:

```c
    /* Difficulty: every 50 pieces placed, drop speed increases by 1 tick.
     * Lower speed value = fewer ticks between drops = faster falling.
     * Minimum speed is 10 (10 × 50ms = 500ms per drop = max difficulty). */
    if (state->piece_count % 50 == 0 && state->speed > 10)
        state->speed--;
```

The speed progression:

| Pieces placed | `speed` | Drop interval | Feel |
|---------------|---------|---------------|------|
| 0 – 49 | 20 | 1000ms | Easy, comfortable |
| 50 – 99 | 19 | 950ms | Slightly faster |
| 100 – 149 | 18 | 900ms | Noticeably quicker |
| … | … | … | … |
| 500+ | 10 | 500ms | Maximum — relentless |

`% 50 == 0` fires when piece_count is exactly 50, 100, 150, etc. The `&& speed > 10`
guard prevents it from going below 10 (which would make pieces fall every 500ms —
already very fast).

---

## Step 7: Add Spawning

After the scoring and difficulty code, spawn the next piece:

```c
    /* Spawn the next piece at the top center of the field.
     * Promote next_piece → current_piece, generate a fresh next_piece. */
    state->current_piece    = state->next_piece;
    state->next_piece       = rand() % 7;
    state->current_x        = FIELD_WIDTH / 2;
    state->current_y        = 0;
    state->current_rotation = 0;
```

`FIELD_WIDTH / 2 = 6`. The new piece starts at column 6, row 0 — top center.

`state->next_piece = rand() % 7` pre-generates the piece *after* this one,
so the preview panel (rendered in the sidebar) always shows what's coming.

---

## Step 8: Add the Game Over Check

Immediately after spawning, check if the new piece fits:

```c
    /* Game over: if the new piece doesn't fit at the spawn position,
     * the board is full. Set game_over = 1 and stop. */
    if (!tetris_does_piece_fit(state, state->current_piece, state->current_rotation,
                               state->current_x, state->current_y))
        state->game_over = 1;
```

Why this check works: we just locked a piece and spawned a new one at the top
center. If the top center is already occupied by locked pieces, there's no room
for the new piece. The board is full. Game over.

`tetris_does_piece_fit()` returns `1` if it fits, `0` if blocked. We flip it
with `!` to set `game_over = 1` when it's blocked.

### The complete locking block, in order

Here is everything you just added, in sequence, so you can check the order is right:

```c
    /* if (tetris_does_piece_fit(..., current_y + 1)) {
     *     state->current_y++;
     *     return;
     * }
     * ← we're here when the piece CAN'T drop */

    /* 1. Lock */
    for (int px = 0; px < 4; px++) {
        for (int py = 0; py < 4; py++) {
            int pi = tetris_rotate(px, py, state->current_rotation);
            if (TETROMINOES[state->current_piece][pi] != '.') {
                state->field[(state->current_y + py) * FIELD_WIDTH +
                             (state->current_x + px)] = state->current_piece + 1;
            }
        }
    }

    /* 2. Score and difficulty */
    state->score += 25;
    state->piece_count++;
    if (state->piece_count % 50 == 0 && state->speed > 10)
        state->speed--;

    /* 3. Spawn */
    state->current_piece    = state->next_piece;
    state->next_piece       = rand() % 7;
    state->current_x        = FIELD_WIDTH / 2;
    state->current_y        = 0;
    state->current_rotation = 0;

    /* 4. Game over check */
    if (!tetris_does_piece_fit(state, state->current_piece, state->current_rotation,
                               state->current_x, state->current_y))
        state->game_over = 1;
```

---

## Step 9: Add `GAME OVER` to `platform_render()`

Both backends already draw the field and the current piece. Now add a game-over
overlay. Your `platform_render()` in both files likely already has a game-over
section from lesson 05/06. Verify it looks like this.

### In `main_x11.c` — inside `platform_render()`, before `XFlush()`:

```c
    /* Game over overlay */
    if (state->game_over) {
        int cx = FIELD_WIDTH * CELL_SIZE / 2;
        int cy = FIELD_HEIGHT * CELL_SIZE / 2;
        /* Dark background box */
        XSetForeground(g_display, g_gc, BlackPixel(g_display, g_screen));
        XFillRectangle(g_display, g_window, g_gc, cx - 60, cy - 30, 120, 60);
        /* "GAME OVER" in red */
        XSetForeground(g_display, g_gc, alloc_color("red"));
        XDrawString(g_display, g_window, g_gc, cx - 28, cy - 8, "GAME OVER", 9);
        /* Restart hint in white */
        XSetForeground(g_display, g_gc, WhitePixel(g_display, g_screen));
        XDrawString(g_display, g_window, g_gc, cx - 42, cy + 12, "R=Restart  Q=Quit", 17);
    }
```

`cx` and `cy` are the pixel center of the play field. We draw a black box there,
then text on top of it.

`XDrawString` takes: display, window, GC, x, y, string, length.
The length (9 for "GAME OVER") is just `strlen(string)` — we hardcode it for
simplicity, or you can use `strlen()` from `<string.h>`.

### In `main_raylib.c` — inside `platform_render()`, before `EndDrawing()`:

```c
    /* Game over overlay */
    if (state->game_over) {
        int cx = FIELD_WIDTH * CELL_SIZE / 2;
        int cy = FIELD_HEIGHT * CELL_SIZE / 2;
        DrawRectangle(cx - 70, cy - 36, 140, 72, (Color){0, 0, 0, 200});
        DrawText("GAME OVER",    cx - 52, cy - 22, 24, RED);
        DrawText("R = Restart",  cx - 46, cy +  4, 14, WHITE);
        DrawText("Q/Esc = Quit", cx - 46, cy + 22, 14, WHITE);
    }
```

`(Color){0, 0, 0, 200}` — black with alpha 200 out of 255. Semi-transparent:
you can still faintly see the board underneath. That's something X11 basic
drawing can't easily do without extensions.

---

## Step 10: Build and Run

```bash
./build_x11.sh    && ./tetris_x11
./build_raylib.sh && ./tetris_raylib
```

### What to test

| What to do | Expected result |
|------------|----------------|
| Wait without pressing anything | Piece falls, hits floor, locks, new piece appears |
| Let pieces stack up | You see colored blocks building from the bottom |
| Keep stacking | Stack gets taller over time |
| Stack reaches the top | New piece can't fit → GAME OVER overlay appears |
| Press R when game over | Board clears, new game starts |
| Press Q when game over | Game exits |
| Move piece into corner while falling | Locks in the corner, new piece at top |
| Rotate piece close to wall | Either rotates or doesn't — never clips through |

### Common issue: pieces disappear through the floor

If pieces vanish instead of locking, check the `field` write:

```c
state->field[(state->current_y + py) * FIELD_WIDTH + (state->current_x + px)]
```

If `current_y + py` reaches `FIELD_HEIGHT - 1` (the floor row), you're writing
into the floor. The floor row has value `9` (wall), so collision stops the piece
before it ever reaches row `FIELD_HEIGHT - 1`. If pieces ARE reaching the floor
row, `tetris_does_piece_fit()` has a bug — check that boundary.

### Common issue: compile error `int px, py` — mixed declarations

In C89/C90, you can't declare variables in the middle of a function. The
`for (int px = ...)` syntax requires C99 or later. Make sure your build script
uses `-std=c99` or that it defaults to C99 (GCC defaults to gnu11 on modern
Linux, so this is usually fine). If you see errors, add `-std=c99` to your
`gcc` command in the build script.

---

## Key Concept: The Field as a Single Source of Truth

After locking, a piece's cells are just numbers in `state->field`. The game
has no list of "placed pieces." No array of piece objects. No struct for each
locked piece.

Everything in the field is equal: walls, locked cells, empty space — all just
values `0–9` in a flat array of bytes. The entire 12×18 board is 216 bytes.

This is why the collision system is so simple: `tetris_does_piece_fit()` checks
one array. It doesn't need to know about walls vs locked pieces. If the value is
non-zero, it's blocked. Done.

When you lock a piece and spawn a new one, the transition is:

```
Before: current_* vars point to the falling piece
        field contains walls + previously locked pieces

After:  current_* vars point to the new piece (at the top)
        field contains walls + previously locked pieces + THIS piece
```

The old piece is gone from `current_*`. It lives forever in `field`.

---

## Exercise

**Display a piece counter.**

Add a `"PIECES: N"` line to the sidebar in both backends. Show how many pieces
have been locked so far.

In `platform_render()`, after the score display:

```c
/* X11 version: */
XDrawString(g_display, g_window, g_gc, sx, 95, "PIECES", 6);
snprintf(buf, sizeof(buf), "%d", state->piece_count);
XDrawString(g_display, g_window, g_gc, sx, 111, buf, (int)strlen(buf));

/* Raylib version: */
DrawText("PIECES", sx, 95, 16, WHITE);
snprintf(buf, sizeof(buf), "%d", state->piece_count);
DrawText(buf, sx, 115, 20, CYAN);
```

Build and run. Watch the counter increment with each piece placed. When you reach
50, the speed increases by 1 — you should feel the game get slightly faster.

---

## Checkpoint

Files changed in this lesson:

| File | What changed |
|------|-------------|
| `src/tetris.h` | Verified `next_piece`, `piece_count` in `GameState` |
| `src/tetris.c` | `tetris_init()` sets `next_piece`; `tetris_tick()` now locks, scores, scales difficulty, spawns, checks game over |
| `src/main_x11.c` | `platform_render()` draws GAME OVER overlay |
| `src/main_raylib.c` | `platform_render()` draws GAME OVER overlay |

The game is now playable from start to game over. Pieces fall, stack, and end the
game when the board fills up. What's still missing: line clearing (lesson 10),
a score multiplier for multi-line clears (lesson 10), and the next-piece preview
panel (lesson 11).

**Next:** Lesson 10 — Line Clearing & Scoring
