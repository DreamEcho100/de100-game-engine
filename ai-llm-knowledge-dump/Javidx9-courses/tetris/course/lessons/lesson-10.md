# Lesson 10 — Line Clearing

## What You'll Build

A piece locks. The row it completed turns **bright white** for about 400ms. Then
it vanishes. Everything above it falls down one row. That's real Tetris.

```
Before:                      Flash:                       After:
. . . . . . . . . .          . . . . . . . . . .          . . . . . . . . . .
. . X X . . X X . .          . . X X . . X X . .          . . . . . . . . . .
X X X X X X X X X X   →      ■ ■ ■ ■ ■ ■ ■ ■ ■ ■   →      . . X X . . X X . .
. X X X X X . . X X          . X X X X X . . X X          X X X X X X X X X X  ← (was the row above)
```

Three things to implement:

1. **Detect** — which rows are complete? (right after locking)
2. **Flash** — freeze the game, mark those rows white
3. **Collapse** — shift everything above the cleared row down by one

---

## Before You Start

Make sure you have these files from lesson 09:

```
src/tetris.h       ← game data types
src/tetris.c       ← tetris_tick(), tetris_init()
src/main_x11.c     ← X11 platform
src/main_raylib.c  ← Raylib platform
```

Good news: the rendering of the flash color is **already wired up**. In lesson
06, both backends defined color index 8 as white:

- **X11**: `g_colors[8] = WhitePixel(g_display, g_screen);`
- **Raylib**: `PIECE_COLORS[8] = {255, 255, 255, 255};`

When we tag a cell with value `8`, the renderer draws it white automatically.
You won't touch the rendering code at all in this lesson.

---

## Step 1 — Add Three Fields to `GameState`

Open `src/tetris.h`. Find the `GameState` struct. Add these three lines
**inside** the struct, near the bottom:

```c
int lines[4];       /* row indices of completed lines this lock */
int line_count;     /* how many entries in lines[] are valid */
int flash_timer;    /* countdown: while > 0, game is paused showing white flash */
```

**What each field does:**

| Field | What it stores |
|-------|---------------|
| `lines[4]` | The row numbers of completed rows. Max 4 because a piece is 4 cells tall — it can complete at most 4 rows in one lock. |
| `line_count` | How many of the `lines[]` entries are actually filled in. Starts at 0 each lock. |
| `flash_timer` | Counts down from 8 to 0. While it's above 0, the game is frozen and the white rows are visible. |

Why not just a single flag? We need the row *numbers* later when we collapse.
Just knowing "some row was cleared" isn't enough — we need to know *which* row.

✅ **Build checkpoint:** Compile now. Nothing should change in behavior yet.

```sh
gcc -o tetris_x11 src/main_x11.c src/tetris.c -lX11
```

---

## Step 2 — Freeze the Game While Rows Flash

Open `src/tetris.c`. Find `tetris_tick()`. Go to the very **top** of the
function body — before input handling, before movement, before gravity.

Add this block:

```c
/* ── FLASH TIMER ─────────────────────────────────────────────────── */
if (state->flash_timer > 0) {
    state->flash_timer--;
    if (state->flash_timer == 0) {
        /* timer hit zero: collapse all completed rows now */
        for (int i = 0; i < state->line_count; i++) {
            int v = state->lines[i];
            for (int px = 1; px < FIELD_WIDTH - 1; px++) {
                for (int py = v; py > 0; py--)
                    state->field[py * FIELD_WIDTH + px] =
                        state->field[(py - 1) * FIELD_WIDTH + px];
                state->field[px] = 0;   /* clear the top row */
            }
        }
        state->line_count = 0;
    }
    return;   /* freeze all game logic while flashing */
}
```

**Line by line:**

```
if (state->flash_timer > 0)
```
"Are we currently in a flash?" If yes, enter the block.

```
state->flash_timer--;
```
Tick the countdown down by 1. At 50ms per tick, 8 ticks = 400ms of flash.

```
if (state->flash_timer == 0) { ... }
```
The flash just finished. Now collapse the marked rows. (We'll look at the
collapse code closely in Step 4.)

```
return;
```
This is the key line. It skips the rest of `tetris_tick()` — no input, no
gravity, no piece movement. Everything is frozen. The renderer still runs
every tick, so the player sees the white rows the whole time.

**Timeline picture:**

```
Piece locks                 Flash timer = 8, rows tagged white
                            ↓
Tick +1  flash_timer = 7   └─ renderer draws white rows, game frozen
Tick +2  flash_timer = 6   └─ ...
Tick +3  flash_timer = 5   └─ ...
...
Tick +8  flash_timer = 0   └─ COLLAPSE ROWS NOW, then return
Tick +9  flash_timer = 0   └─ game unfreezes, new piece falls normally
```

✅ **Build checkpoint:** Compile again. Still no visible change (we haven't added
line detection yet), but the new code should compile cleanly.

---

## Step 3 — Detect Complete Rows After Locking

Still in `src/tetris.c`, find the code inside `tetris_tick()` where pieces lock.
It looks like this (from lesson 09):

```c
/* Lock piece into field */
for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
        if (tetromino[state->current_piece][rotate(px, py, state->current_rotation)] == 'X') {
            state->field[(state->current_y + py) * FIELD_WIDTH + (state->current_x + px)]
                = state->current_piece + 1;
        }
    }
}
```

**Right after** that locking loop (not inside it — after the closing `}}`), add
this block:

```c
/* ── LINE DETECTION ──────────────────────────────────────────────── */
state->line_count = 0;
for (int py = 0; py < 4; py++) {
    if (state->current_y + py >= FIELD_HEIGHT - 1) continue;   /* skip floor */

    int complete = 1;
    for (int px = 1; px < FIELD_WIDTH - 1; px++) {
        if (state->field[(state->current_y + py) * FIELD_WIDTH + px] == 0) {
            complete = 0;
            break;
        }
    }

    if (complete) {
        /* Tag this row: value 8 = white in the renderer */
        for (int px = 1; px < FIELD_WIDTH - 1; px++)
            state->field[(state->current_y + py) * FIELD_WIDTH + px] = 8;
        state->lines[state->line_count++] = state->current_y + py;
    }
}

if (state->line_count > 0)
    state->flash_timer = 8;   /* 8 ticks × 50ms = ~400ms of white flash */
```

**Walking through it carefully:**

```c
state->line_count = 0;
```
Reset the count every time we check. We're about to fill `lines[]` fresh.

```c
for (int py = 0; py < 4; py++) {
```
Loop over the 4 rows of the piece's bounding box. `py` is 0, 1, 2, 3 —
the piece's local rows. `state->current_y + py` is the actual field row.

```c
if (state->current_y + py >= FIELD_HEIGHT - 1) continue;
```
Skip the floor row (`FIELD_HEIGHT - 1` = row 17). The floor is always full
of wall cells — we'd detect it as "complete" every time if we checked it.
The `continue` keyword means "skip this loop iteration, go to the next `py`."

```c
int complete = 1;
for (int px = 1; px < FIELD_WIDTH - 1; px++) {
    if (state->field[(state->current_y + py) * FIELD_WIDTH + px] == 0) {
        complete = 0;
        break;
    }
}
```
Walk the 10 playable columns (1 through 10 — we skip column 0 and column 11
because those are wall cells, always non-zero). If we find even one empty cell
(`== 0`), the row is not complete. `break` stops the inner loop early — no
point checking more cells.

We start with `complete = 1` (assume yes) and disprove it if we find an empty
cell. This is the standard "innocent until proven guilty" pattern in C.

```c
if (complete) {
    for (int px = 1; px < FIELD_WIDTH - 1; px++)
        state->field[(state->current_y + py) * FIELD_WIDTH + px] = 8;
    state->lines[state->line_count++] = state->current_y + py;
}
```
Row is complete. Tag every interior cell with `8` (white). Store the row index
in `lines[]`. `state->line_count++` increments the count *after* using it as
the array index — so the first line goes into `lines[0]`, second into
`lines[1]`, etc.

```c
if (state->line_count > 0)
    state->flash_timer = 8;
```
If any rows were completed, start the flash. The `return` in Step 2 will now
kick in on the next tick and freeze everything.

---

## Step 4 — How the Collapse Works

Go back to the collapse code you added in Step 2 and read it alongside this
explanation. This is the most conceptually tricky part.

**The idea in plain English:**

"Deleting" a row means: copy every row above it down by one. The deleted row
gets overwritten by the row above it. That row gets overwritten by the one above
it. And so on, all the way to the top.

**Toy example — 6-column field, deleting row 4:**

```
Before:
  row 0: .  .  .  .  .  .       ← will become new empty top
  row 1: .  X  .  .  X  .
  row 2: .  X  X  .  X  .
  row 3: X  X  X  .  X  X
  row 4: X  X  X  X  X  X       ← FULL — delete this

Step: copy row 3 into row 4, row 2 into row 3, row 1 into row 2, row 0 into row 1.
Then clear row 0.

After:
  row 0: .  .  .  .  .  .       ← cleared
  row 1: .  .  .  .  .  .       ← was row 0
  row 2: .  X  .  .  X  .       ← was row 1
  row 3: .  X  X  .  X  .       ← was row 2
  row 4: X  X  X  .  X  X       ← was row 3 (row 4 is overwritten/gone)
```

**The code does this column by column:**

```c
for (int px = 1; px < FIELD_WIDTH - 1; px++) {       /* each playable column */
    for (int py = v; py > 0; py--)                    /* from the cleared row UP */
        state->field[py * FIELD_WIDTH + px] =
            state->field[(py - 1) * FIELD_WIDTH + px]; /* copy from one row above */
    state->field[px] = 0;                              /* clear row 0 of this column */
}
```

Tracing column 1, with `v = 4` (the cleared row):

```
py=4: field[4][1] = field[3][1]   → copies row 3 down into row 4
py=3: field[3][1] = field[2][1]   → copies row 2 down into row 3
py=2: field[2][1] = field[1][1]   → copies row 1 down into row 2
py=1: field[1][1] = field[0][1]   → copies row 0 down into row 1
(loop ends because py > 0 fails when py = 0)
field[1] = 0  → clears row 0, column 1   (field[0*12+1] = field[1])
```

**Why `state->field[px] = 0`?**

`field[px]` is the same as `field[0 * FIELD_WIDTH + px]` — that's row 0,
column `px`. After the loop, row 0 still contains whatever was there before
(the loop copies row 0 into row 1, but never zeros row 0). So we clear it
explicitly. Without this, a ghost copy of old data would appear at the very
top of the field.

**Multiple lines at once:**

If two rows are complete (e.g., rows 14 and 15), we collapse them one at a
time in the order they appear in `lines[]`. Process the lowest row first.
After collapsing row 15, the rows above shift down — but rows below row 15
are unaffected, and `lines[0]` (the lower one) was already processed.

For our loop that processes bottom-to-top, this works correctly as long as
lines are stored in order (which they are, since we scan py from 0 to 3).

---

## ✅ Final Build and Test

```sh
# X11
gcc -o tetris_x11 src/main_x11.c src/tetris.c -lX11

# Raylib
gcc -o tetris_raylib src/main_raylib.c src/tetris.c -lraylib
```

Run the game. Place pieces until you fill a row. You should see:

1. The completed row turns **white** for about half a second.
2. The game is frozen (no new input, no gravity).
3. The white row **disappears**.
4. Everything above it **drops down** one row.
5. The new piece starts falling.

If the row flashes but doesn't disappear: check that `state->flash_timer == 0`
triggers the collapse loop, not just `flash_timer > 0`.

If the wrong rows disappear: verify that `state->lines[]` is storing the right
field-row indices (not piece-local row indices).

---

## Exercise: Pre-fill a Row to Test Without Playing

Testing line clears by playing is slow — it takes 30 seconds to fill a row.
Instead, pre-fill one in `tetris_init()`.

In `src/tetris.c`, find `tetris_init()`. After the field is initialized (walls
set up), add:

```c
/* Pre-fill row 15 for testing line clear */
for (int px = 1; px < FIELD_WIDTH - 1; px++)
    state->field[15 * FIELD_WIDTH + px] = 1;
```

Now run the game. Drop any piece so it touches or lands near row 15. When row
15 is completely filled, it should immediately flash and collapse.

Remove this line when you're done testing.

---

## Mental Model

Line clearing is just array manipulation. The "physics" — pieces above a cleared
row falling down — is nothing more than copying array elements: `field[y] =
field[y-1]`. There's no physics engine. No gravity simulation. Just one line of
array assignment, repeated for every column, from the cleared row up to row 0.

The flash is even simpler: overwrite cells with value 8, freeze the game loop
for 8 ticks, then copy the rows. Two concepts (tagging and a timer) create the
entire visual effect.

---

## Summary

| What you added | Where | Effect |
|----------------|-------|--------|
| `lines[4]`, `line_count`, `flash_timer` | `tetris.h` — `GameState` | Stores which rows to clear and manages the pause |
| Line detection loop | `tetris.c` — after locking | Finds complete rows, tags them `8`, sets `flash_timer` |
| Flash timer handler | `tetris.c` — top of `tetris_tick` | Freezes game during flash, collapses rows when timer hits 0 |
| Flash color (already done) | Both renderers | `g_colors[8]` / `PIECE_COLORS[8]` = white |
