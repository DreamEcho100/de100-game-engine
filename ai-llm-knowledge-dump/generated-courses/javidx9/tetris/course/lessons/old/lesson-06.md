# Lesson 06 — GameState: The Field as Real Data

---

## By the end of this lesson you will have:

A `GameState` struct that holds the entire play field as an array of numbers.
The field is **initialized with boundary walls** (value 9 along the edges).
The renderer **reads from the field array** to draw walls — no more hardcoded
drawing loops. The falling piece is also part of `GameState`.

Press Z to rotate. Left/right to move. The walls are solid data now.

---

## The Problem

Right now, your renderer draws walls like this:

```c
/* draw left and right walls */
for (int y = 0; y < FIELD_HEIGHT; y++) {
    draw_cell(0, y, GRAY);
    draw_cell(FIELD_WIDTH - 1, y, GRAY);
}
/* draw floor */
for (int x = 0; x < FIELD_WIDTH; x++) {
    draw_cell(x, FIELD_HEIGHT - 1, GRAY);
}
```

This draws walls, but it's just pixels. The **game logic** — collision
detection, locking pieces — needs to *read* the field as data. It needs to ask:
"Is cell (3, 14) occupied?" and get a yes/no answer. A drawing call can't
answer that question. An array can.

We need the field as actual data: an array where each element represents one
cell. 0 means empty. 9 means wall. 1–7 will mean a locked piece (the piece
number + 1). Later, when a piece lands, we write into this array. When we
check collision, we read from it.

---

## Step 1 — Add `GameState` to `tetris.h`

Open `src/tetris.h`. After the constants and before the function declarations,
add the `GameState` struct:

```c
/* ── Game state ──────────────────────────────────────── */

typedef struct {
    unsigned char field[FIELD_WIDTH * FIELD_HEIGHT]; /* the play field */
    int current_piece;
    int current_rotation;
    int current_x;
    int current_y;
    int score;
    int game_over;
} GameState;
```

### Unpacking each field

**`unsigned char field[FIELD_WIDTH * FIELD_HEIGHT]`**

This is the play field — a flat array of bytes. Let's break it down:

- `unsigned char` — one byte, value 0 to 255. "Unsigned" means no negative numbers.
  Compare to `int` (4 bytes, −2 billion to +2 billion) — we don't need that range.
  One byte per cell is enough. Values we'll use:
  - `0` = empty
  - `1`–`7` = locked piece (piece index + 1)
  - `9` = wall

- `field[FIELD_WIDTH * FIELD_HEIGHT]` — the array size. With `FIELD_WIDTH=12`
  and `FIELD_HEIGHT=18`, that's 216 bytes total. The field is 12 cells wide and
  18 cells tall, stored as one long row after another:

  ```
  field[0]   = cell (col=0,  row=0)
  field[1]   = cell (col=1,  row=0)
  ...
  field[11]  = cell (col=11, row=0)
  field[12]  = cell (col=0,  row=1)
  ...
  field[215] = cell (col=11, row=17)
  ```

  To get the index for column `x`, row `y`:
  `index = y * FIELD_WIDTH + x`

  This is the same formula we use for tetromino cells — consistent!

**`int current_piece`** — which of the 7 tetrominoes is falling (0–6).

**`int current_rotation`** — the rotation counter (0, 1, 2, or 3).

**`int current_x` / `int current_y`** — the column and row of the top-left
corner of the current piece's 4×4 bounding box.

**`int score`** — points accumulated. Starts at 0.

**`int game_over`** — 1 when the game ends, 0 while playing.

### What is `typedef struct`?

Without `typedef`:
```c
struct GameState gs;
gs.score = 0;
```

With `typedef`:
```c
GameState gs;
gs.score = 0;
```

`typedef struct { ... } GameState;` creates a type alias. It's exactly like
`type GameState = { ... }` in TypeScript — you can now use `GameState` as the
type name everywhere without writing `struct GameState` every time.

---

## Step 2 — Declare `tetris_init()` in `tetris.h`

Add this declaration below the `GameState` definition:

```c
void tetris_init(GameState *state);
int  tetris_does_piece_fit(GameState *state, int piece, int rotation,
                           int pos_x, int pos_y);
```

Don't worry about `tetris_does_piece_fit` yet — we'll implement it at the end
of this lesson. Declaring it now means the compiler knows it exists.

**What is `GameState *state`?**

The `*` means "pointer to" — `state` is the *address* of a `GameState`, not
a copy of it. This matters because:

```c
void tetris_init(GameState state)  { state.score = 0; } /* modifies a COPY — caller sees nothing */
void tetris_init(GameState *state) { state->score = 0; } /* modifies the ORIGINAL — caller sees it */
```

In JavaScript, objects are always passed by reference. In C, everything is
passed by value (copied) by default. You use a pointer (`*`) when you want the
function to modify the original struct.

`state->score` is the same as `(*state).score` — it dereferences the pointer
then accesses the field. The `->` arrow is just a shortcut.

---

## Step 3 — Implement `tetris_init()` in `tetris.c`

Add `#include <string.h>` and `#include <stdlib.h>` at the top of `tetris.c`
(needed for `memset` and `rand`/`srand`). Then add `#include <time.h>` for
`time(NULL)`. Then add the function:

```c
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "tetris.h"

/* ... TETROMINOES array and tetris_rotate() from before ... */

void tetris_init(GameState *state)
{
    /* Zero out the entire struct. Every field becomes 0. */
    memset(state, 0, sizeof(GameState));

    /* Build the boundary walls. */
    for (int y = 0; y < FIELD_HEIGHT; y++) {
        for (int x = 0; x < FIELD_WIDTH; x++) {
            /* Left wall, right wall, or floor → value 9 */
            if (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) {
                state->field[y * FIELD_WIDTH + x] = 9;
            }
            /* Everything else stays 0 (empty) from the memset */
        }
    }

    /* Pick a random starting piece. */
    srand((unsigned int)time(NULL));
    state->current_piece    = rand() % 7;
    state->current_rotation = 0;
    state->current_x        = FIELD_WIDTH / 2 - 2;  /* center-ish */
    state->current_y        = 0;
}
```

### `memset(state, 0, sizeof(GameState))`

`memset` sets every byte of a memory region to a given value.
- `state` — where to start
- `0` — fill with zeros
- `sizeof(GameState)` — how many bytes to fill

`sizeof(GameState)` is the size of the struct in bytes — C computes this
automatically. This call zeros every field: score=0, game_over=0, every cell
in the field array=0. It's like `Object.assign(state, { score: 0, game_over: 0, ... })`
but for the entire struct at once.

### `rand()` and `srand(time(NULL))`

`rand()` returns a random integer. `rand() % 7` gives a number from 0 to 6 —
perfect for picking one of the 7 pieces.

`srand(...)` seeds the random number generator. Without a seed, `rand()` returns
the same sequence every run. `time(NULL)` returns the current time as a number,
giving a different seed each run → different starting pieces.

You only call `srand` once, at initialization.

### Building the walls

Let's trace through a small example. Suppose `FIELD_WIDTH = 4, FIELD_HEIGHT = 3`:

```
(0,0) (1,0) (2,0) (3,0)
(0,1) (1,1) (2,1) (3,1)
(0,2) (1,2) (2,2) (3,2)
```

After init:
```
9 0 0 9   ← x=0 and x=3 are walls
9 0 0 9
9 9 9 9   ← y=2 is the floor
```

The conditions:
- `x == 0` → left wall
- `x == FIELD_WIDTH - 1` → right wall (x=3 in this example)
- `y == FIELD_HEIGHT - 1` → floor (y=2 in this example)

The field index for `(x, y)` is `y * FIELD_WIDTH + x`.
For `(x=3, y=0)`: `0 * 4 + 3 = 3`. So `field[3] = 9`. ✓

---

## Step 4 — Implement `tetris_does_piece_fit()` in `tetris.c`

```c
int tetris_does_piece_fit(GameState *state, int piece, int rotation,
                          int pos_x, int pos_y)
{
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            /* Which character in the piece string? */
            int pi = tetris_rotate(px, py, rotation);

            /* Skip empty cells in the piece */
            if (TETROMINOES[piece][pi] == '.') continue;

            /* Where would this cell land in the field? */
            int field_x = pos_x + px;
            int field_y = pos_y + py;

            /* Skip if outside the field bounds
               (piece's 4×4 box can hang outside on the sides) */
            if (field_x < 0 || field_x >= FIELD_WIDTH)  continue;
            if (field_y < 0 || field_y >= FIELD_HEIGHT) continue;

            /* Field index for this position */
            int fi = field_y * FIELD_WIDTH + field_x;

            /* If the field cell is not empty → doesn't fit */
            if (state->field[fi] != 0) return 0;
        }
    }
    return 1; /* all solid cells checked — it fits */
}
```

### Walk through the logic step by step

For each of the 16 cells in the piece's 4×4 bounding box:

1. **Get the piece character.** `pi = tetris_rotate(px, py, rotation)` gives
   us the index into the TETROMINOES string for this rotation.

2. **Skip empty cells.** If `TETROMINOES[piece][pi] == '.'`, this part of the
   box is empty — nothing to collide. Move on.

3. **Compute field position.** `field_x = pos_x + px`, `field_y = pos_y + py`.
   The piece is at `(pos_x, pos_y)` in field coordinates, and `(px, py)` is
   the offset within the 4×4 box.

4. **Skip out-of-bounds.** If `field_x < 0` or `field_x >= FIELD_WIDTH`, this
   cell is outside the field. We *skip* (not fail) because pieces can hang
   partially off the edge during spawn or movement — that's okay unless they'd
   overlap something solid.

5. **Check the field.** `fi = field_y * FIELD_WIDTH + field_x`. If `field[fi]`
   is not 0 (i.e., it's a wall=9 or a locked piece=1–7), the piece can't go
   here. Return `0` (doesn't fit) immediately.

6. If we check all 16 cells without returning early, the piece fits. Return `1`.

### Why `!= 0` instead of `== 9`?

`9` is the wall value. But locked pieces have values 1–7. If we only checked
for `9`, a falling piece could overlap with previously locked pieces. `!= 0`
catches walls AND locked pieces in one check.

---

## Step 5 — Update `platform.h`

The render function needs to know about `GameState` to draw from it.
Update `platform.h`:

```c
#ifndef PLATFORM_H
#define PLATFORM_H

#include "tetris.h"   /* needed for GameState */

typedef struct {
    int move_left;
    int move_right;
    int move_down;
    int rotate;
    int quit;
} PlatformInput;

void platform_init(const char *title, int width, int height);
PlatformInput platform_get_input(void);
void platform_render(GameState *state);   /* ← now takes GameState */
void platform_sleep_ms(int ms);
int  platform_should_quit(void);
void platform_shutdown(void);

#endif /* PLATFORM_H */
```

---

## Step 6 — Update `platform_render()` in `main_x11.c`

Replace the old `platform_render` (which took piece/rotation/col/row as
separate parameters) with one that reads from `GameState`:

```c
void platform_render(GameState *state)
{
    /* Clear the window */
    XClearWindow(display, window);

    /* Draw the field */
    for (int y = 0; y < FIELD_HEIGHT; y++) {
        for (int x = 0; x < FIELD_WIDTH; x++) {
            unsigned char cell = state->field[y * FIELD_WIDTH + x];
            if (cell == 0) continue;   /* empty — skip */

            /* Pick a color based on cell value */
            if (cell == 9) {
                XSetForeground(display, gc, 0x808080); /* gray wall */
            } else {
                XSetForeground(display, gc, 0x00AAFF); /* blue for locked pieces */
            }

            XFillRectangle(display, window, gc,
                           x * CELL_SIZE, y * CELL_SIZE,
                           CELL_SIZE - 1, CELL_SIZE - 1);
        }
    }

    /* Draw the current falling piece on top */
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            int pi = tetris_rotate(px, py, state->current_rotation);
            if (TETROMINOES[state->current_piece][pi] == 'X') {
                int sx = (state->current_x + px) * CELL_SIZE;
                int sy = (state->current_y + py) * CELL_SIZE;
                XSetForeground(display, gc, 0xFF4444); /* red for falling piece */
                XFillRectangle(display, window, gc,
                               sx, sy, CELL_SIZE - 1, CELL_SIZE - 1);
            }
        }
    }

    XFlush(display);
}
```

The field drawing loop replaces the old hardcoded wall loops.
The walls are now drawn because `field[y * FIELD_WIDTH + x] == 9` — the data
drives the display.

Do the same change in `main_raylib.c`, substituting `XFillRectangle` calls
with the equivalent Raylib draw calls:

```c
void platform_render(GameState *state)
{
    BeginDrawing();
    ClearBackground(BLACK);

    for (int y = 0; y < FIELD_HEIGHT; y++) {
        for (int x = 0; x < FIELD_WIDTH; x++) {
            unsigned char cell = state->field[y * FIELD_WIDTH + x];
            if (cell == 0) continue;

            Color color = (cell == 9) ? GRAY : BLUE;
            DrawRectangle(x * CELL_SIZE, y * CELL_SIZE,
                          CELL_SIZE - 1, CELL_SIZE - 1, color);
        }
    }

    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            int pi = tetris_rotate(px, py, state->current_rotation);
            if (TETROMINOES[state->current_piece][pi] == 'X') {
                DrawRectangle((state->current_x + px) * CELL_SIZE,
                              (state->current_y + py) * CELL_SIZE,
                              CELL_SIZE - 1, CELL_SIZE - 1, RED);
            }
        }
    }

    EndDrawing();
}
```

---

## Step 7 — Update `main()`

In both `main_x11.c` and `main_raylib.c`, update `main()`:

```c
int main(void)
{
    platform_init("Tetris", FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE);

    GameState state;
    tetris_init(&state);   /* initialize the field + pick first piece */

    while (!platform_should_quit()) {
        PlatformInput input = platform_get_input();

        if (input.quit)       break;
        if (input.rotate)     state.current_rotation++;
        if (input.move_left)  state.current_x--;
        if (input.move_right) state.current_x++;

        platform_render(&state);   /* pass a pointer to state */
        platform_sleep_ms(16);
    }

    platform_shutdown();
    return 0;
}
```

Key changes:
- `GameState state;` declares the game state on the stack.
- `tetris_init(&state)` — `&state` is the *address* of `state`. We pass the
  address so `tetris_init` can modify the original (not a copy).
- `platform_render(&state)` — same idea.
- Movement now changes `state.current_x` / `state.current_y` directly.

---

## Step 8 — Build & Run

```bash
./build_x11.sh && ./tetris_x11
```

or

```bash
./build_raylib.sh && ./tetris_raylib
```

**What to verify:**

1. **Gray walls appear** on the left, right, and bottom edges.
   These are drawn because the field array has value 9 at those positions.

2. **A colored falling piece** appears near the top-center.

3. **Z rotates** the piece through all four orientations.

4. **Left/right arrows move** the piece (still no collision — it can move
   through walls for now). We'll add collision next lesson.

5. **No hardcoded wall drawing loops** remain in your render function.
   The walls come from `state.field`.

---

## Mental Model

> **`GameState` is the single source of truth** — like Redux state in a React app.
>
> - The **renderer** only reads from `GameState`. It never writes to it.
>   It asks: "What's in cell (x, y)?" and draws accordingly.
> - **Game logic** writes to `GameState`. When a piece locks, it writes values
>   into `state.field`. When a line clears, it removes rows.
> - `main()` is just the coordinator: read inputs → update state → render state → repeat.
>
> This separation means you can add a completely new renderer (say, terminal
> output) by just writing a new function that reads `GameState`. The game logic
> doesn't care how it's displayed.

---

## Common Confusions

**"Why `unsigned char` and not `int` for the field?"**

`int` is 4 bytes; `unsigned char` is 1 byte. The field array has 216 cells.
With `int` that's 864 bytes; with `unsigned char` that's 216 bytes. Not a huge
difference at this scale, but it's good habit: use the smallest type that fits
the data. The values 0–9 fit easily in one byte.

**"Why do we pass `&state` instead of just `state`?"**

When you pass a struct by value, C copies all of it — 216+ bytes every call.
When you pass a pointer, C copies just the address — 8 bytes. More importantly,
functions like `tetris_init` need to modify the struct, and modifying a copy
does nothing to the original. Pass `&state` (the address) when you want the
function to see or modify the real thing.

**"Can `rand()` ever return the same piece every time?"**

Without `srand`, yes — same sequence every run. With `srand(time(NULL))`, the
seed changes each second. If you run the game twice within the same second, you'll
get the same starting piece. That's fine for a tutorial.

---

## Exercise

After calling `tetris_init(&state)`, add a temporary debug print to `main()`:

```c
printf("First 3 rows of field:\n");
for (int y = 0; y < 3; y++) {
    for (int x = 0; x < FIELD_WIDTH; x++) {
        printf("%d ", state.field[y * FIELD_WIDTH + x]);
    }
    printf("\n");
}
```

Add `#include <stdio.h>` at the top of `main_x11.c` if it's not there.

**Expected output:**

```
First 3 rows of field:
9 0 0 0 0 0 0 0 0 0 0 9
9 0 0 0 0 0 0 0 0 0 0 9
9 0 0 0 0 0 0 0 0 0 0 9
```

- `9` at position 0 (left wall)
- `0` for all interior cells
- `9` at position 11 (right wall, since `FIELD_WIDTH - 1 = 11`)

If you see a different pattern, check the wall-building loops in `tetris_init`.

Remove the `printf` block after you've verified. Debug prints left in the code
slow down the render loop (printing to terminal on every frame is slow).

---

**Next lesson:** We'll add gravity — the piece falls one row at a time on a
timer — and use `tetris_does_piece_fit()` to lock it when it hits the floor or
another piece.
