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

| Field         | What it stores                                                                                                         |
| ------------- | ---------------------------------------------------------------------------------------------------------------------- |
| `lines[4]`    | The row numbers of completed rows. Max 4 because a piece is 4 cells tall — it can complete at most 4 rows in one lock. |
| `line_count`  | How many of the `lines[]` entries are actually filled in. Starts at 0 each lock.                                       |
| `flash_timer` | Counts down from 8 to 0. While it's above 0, the game is frozen and the white rows are visible.                        |

Why not just a single flag? We need the row _numbers_ later when we collapse.
Just knowing "some row was cleared" isn't enough — we need to know _which_ row.

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

        /* IMPORTANT: Process lines from bottom to top (highest row index first).
         * Why? When we collapse a row, everything ABOVE it shifts down by 1.
         * If we processed top-to-bottom, the stored indices would become stale.
         *
         * Example: lines[0]=14, lines[1]=15 (two adjacent complete rows)
         * If we collapse row 14 first, row 15 shifts to row 14.
         * But lines[1] still says 15 — wrong!
         *
         * By processing bottom-to-top (row 15 first), we avoid this problem.
         * After collapsing row 15, row 14 is still at row 14.
         */
        for (int i = state->line_count - 1; i >= 0; i--) {
            int line_y = state->lines[i];

            /* Collapse this row: copy each row above it down by one.
             * Start at the completed row, work upward to row 1.
             * We skip row 0 in the copy (nothing above it to copy from).
             */
            for (int y = line_y; y > 0; y--) {
                for (int x = 1; x < FIELD_WIDTH - 1; x++) {
                    state->field[y * FIELD_WIDTH + x] =
                        state->field[(y - 1) * FIELD_WIDTH + x];
                }
            }

            /* Clear the top row (row 0) — it has no row above to copy from */
            for (int x = 1; x < FIELD_WIDTH - 1; x++) {
                state->field[x] = 0;
            }

            /* Adjust remaining line indices.
             * All lines we haven't processed yet (indices 0 to i-1) are
             * ABOVE the line we just collapsed. They've all shifted down
             * by 1 row, so increment their stored indices.
             */
            for (int j = i - 1; j >= 0; j--) {
                state->lines[j]++;
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
for (int i = state->line_count - 1; i >= 0; i--)
```

Process lines from bottom to top. If `line_count` is 2 and we have rows 14 and
15, we process `i=1` (row 15) first, then `i=0` (row 14).

```
for (int y = line_y; y > 0; y--)
```

Start at the completed row, move upward. We copy from `y-1` into `y`, so row 14
gets row 13's contents, row 13 gets row 12's contents, etc.

**Why `y > 0` and not `y >= 0`?** When `y` is 1, we copy from `y-1 = 0`. That's
the last valid copy. If we continued to `y = 0`, we'd copy from `y-1 = -1` —
out of bounds!

```
for (int x = 1; x < FIELD_WIDTH - 1; x++)
```

Only copy the playable columns (1 through 10). Columns 0 and 11 are walls —
we never touch them.

```
state->field[x] = 0;
```

Clear row 0. After the copy loop, row 0 still has its old data (we copied it
into row 1, but nothing copied into row 0). We clear it explicitly.

```
for (int j = i - 1; j >= 0; j--) { state->lines[j]++; }
```

**Critical fix for multiple lines!** After collapsing a row, all rows above it
have shifted down by 1. The remaining entries in `lines[]` (indices 0 to i-1)
point to rows that are now 1 position lower than before. We increment them to
keep them accurate.

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
    int row_y = state->current_y + py;

    /* Skip if outside playable area (above top or at/below floor) */
    if (row_y < 0 || row_y >= FIELD_HEIGHT - 1) continue;

    int complete = 1;
    for (int px = 1; px < FIELD_WIDTH - 1; px++) {
        if (state->field[row_y * FIELD_WIDTH + px] == 0) {
            complete = 0;
            break;
        }
    }

    if (complete) {
        /* Tag this row: value 8 = white in the renderer */
        for (int px = 1; px < FIELD_WIDTH - 1; px++)
            state->field[row_y * FIELD_WIDTH + px] = 8;
        state->lines[state->line_count++] = row_y;
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
int row_y = state->current_y + py;
if (row_y < 0 || row_y >= FIELD_HEIGHT - 1) continue;
```

Skip rows outside the playable area. `row_y < 0` handles pieces at the very top.
`row_y >= FIELD_HEIGHT - 1` skips the floor row (row 17). The floor is always
full of wall cells — we'd detect it as "complete" every time if we checked it.

```c
int complete = 1;
for (int px = 1; px < FIELD_WIDTH - 1; px++) {
    if (state->field[row_y * FIELD_WIDTH + px] == 0) {
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
        state->field[row_y * FIELD_WIDTH + px] = 8;
    state->lines[state->line_count++] = row_y;
}
```

Row is complete. Tag every interior cell with `8` (white). Store the row index
in `lines[]`. `state->line_count++` increments the count _after_ using it as
the array index — so the first line goes into `lines[0]`, second into
`lines[1]`, etc.

**Note:** Lines are stored in top-to-bottom order (py goes 0, 1, 2, 3). This is
fine because we process them bottom-to-top in Step 2.

```c
if (state->line_count > 0)
    state->flash_timer = 8;
```

If any rows were completed, start the flash. The `return` in Step 2 will now
kick in on the next tick and freeze everything.

---

## Step 4 — How the Collapse Works (Detailed)

This is the most conceptually tricky part. Let's trace through examples.

### Single Line Clear

**Deleting row 15:**

```
Before:
  row 0:  . . . . . . . . . .       ← will become empty (cleared)
  row 1:  . . . . . . . . . .
  ...
  row 13: . X . . . . . . X .
  row 14: X X X . . . . X X X
  row 15: X X X X X X X X X X       ← COMPLETE — delete this
  row 16: X X X . . . . . X X
  row 17: # # # # # # # # # #       ← floor (never touched)

Collapse loop (y = 15 down to 1):
  y=15: field[15] = field[14]   → row 14 copies into row 15
  y=14: field[14] = field[13]   → row 13 copies into row 14
  ...
  y=1:  field[1] = field[0]     → row 0 copies into row 1

Then clear row 0.

After:
  row 0:  . . . . . . . . . .       ← cleared
  row 1:  . . . . . . . . . .       ← was row 0
  ...
  row 14: . X . . . . . . X .       ← was row 13
  row 15: X X X . . . . X X X       ← was row 14
  row 16: X X X . . . . . X X       ← unchanged (below cleared line)
  row 17: # # # # # # # # # #       ← floor unchanged
```

### Multiple Line Clear (The Tricky Part)

**Deleting rows 14 AND 15 (adjacent):**

```
Before:
  row 13: . X . . . . . . X .
  row 14: X X X X X X X X X X       ← COMPLETE (lines[0] = 14)
  row 15: X X X X X X X X X X       ← COMPLETE (lines[1] = 15)
  row 16: X X X . . . . . X X

Detection stores: lines[0] = 14, lines[1] = 15, line_count = 2

Processing (i = 1 down to 0):

─── i = 1: Collapse row 15 ───
  y=15: field[15] = field[14]   → row 14 (complete!) copies into row 15
  y=14: field[14] = field[13]   → row 13 copies into row 14
  ...
  Clear row 0.

  Adjust remaining indices: lines[0]++ → lines[0] = 15

  After this step:
    row 13: . . . . . . . . . .     ← was row 12
    row 14: . X . . . . . . X .     ← was row 13
    row 15: X X X X X X X X X X     ← was row 14 (STILL COMPLETE!)
    row 16: X X X . . . . . X X     ← unchanged

  lines[0] is now 15 (adjusted from 14)

─── i = 0: Collapse row 15 (adjusted) ───
  y=15: field[15] = field[14]   → row 14 copies into row 15
  y=14: field[14] = field[13]   → row 13 copies into row 14
  ...
  Clear row 0.

  After this step:
    row 14: . . . . . . . . . .     ← was row 13
    row 15: . X . . . . . . X .     ← was row 14
    row 16: X X X . . . . . X X     ← unchanged

Final result: Both complete rows are gone!
```

**Why the index adjustment is critical:**

Without `lines[0]++`, we would try to collapse row 14 on the second iteration.
But row 14 now contains what was row 13 — not a complete row! The actual
complete row shifted down to row 15. The adjustment keeps our indices accurate.

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

### Testing Multiple Lines

To test multi-line clears, try to set up a situation where an I-piece (the long
bar) completes 2, 3, or even 4 rows at once. All rows should flash, then all
should collapse correctly.

### Common Issues

**Row flashes but doesn't disappear:**
Check that `flash_timer == 0` triggers the collapse loop. Make sure you have
the `return` statement after the flash timer block.

**Wrong rows disappear:**
Verify that `lines[]` stores field row indices (`state->current_y + py`), not
piece-local indices (`py`).

**Only one row clears when multiple are complete:**
Make sure you have the index adjustment loop:

```c
for (int j = i - 1; j >= 0; j--) {
    state->lines[j]++;
}
```

**Floor disappears:**
Check that your collapse loop uses `y > 0` (not `y >= 0`) and that your
detection skips `row_y >= FIELD_HEIGHT - 1`.

**Walls get corrupted:**
Check that your loops use `x = 1` to `x < FIELD_WIDTH - 1` (skipping columns
0 and 11).

---

## Exercise: Pre-fill Rows to Test Without Playing

Testing line clears by playing is slow — it takes 30 seconds to fill a row.
Instead, pre-fill some in `tetris_init()`.

In `src/tetris.c`, find `tetris_init()`. After the field is initialized (walls
set up), add:

```c
/* Pre-fill rows 15 and 16 for testing multi-line clear */
for (int px = 1; px < FIELD_WIDTH - 1; px++) {
    state->field[15 * FIELD_WIDTH + px] = 1;
    state->field[16 * FIELD_WIDTH + px] = 1;
}
/* Leave one gap so it's not immediately complete */
state->field[15 * FIELD_WIDTH + 5] = 0;
state->field[16 * FIELD_WIDTH + 5] = 0;
```

Now run the game. Drop a piece into column 5 to complete both rows at once.
Both should flash and collapse correctly.

Remove this code when you're done testing.

---

## Mental Model

Line clearing is just array manipulation. The "physics" — pieces above a cleared
row falling down — is nothing more than copying array elements: `field[y] =
field[y-1]`. There's no physics engine. No gravity simulation. Just one line of
array assignment, repeated for every column, from the cleared row up to row 1.

The flash is even simpler: overwrite cells with value 8, freeze the game loop
for 8 ticks, then copy the rows. Two concepts (tagging and a timer) create the
entire visual effect.

The tricky part is handling multiple lines. When you collapse a row, everything
above it shifts down — including other completed rows you haven't processed yet.
The solution is simple: process bottom-to-top, and adjust the remaining indices
after each collapse.

---

## Summary

| What you added                          | Where                             | Effect                                                      |
| --------------------------------------- | --------------------------------- | ----------------------------------------------------------- |
| `lines[4]`, `line_count`, `flash_timer` | `tetris.h` — `GameState`          | Stores which rows to clear and manages the pause            |
| Line detection loop                     | `tetris.c` — after locking        | Finds complete rows, tags them `8`, sets `flash_timer`      |
| Flash timer handler                     | `tetris.c` — top of `tetris_tick` | Freezes game during flash, collapses rows when timer hits 0 |
| Index adjustment                        | `tetris.c` — inside collapse loop | Keeps remaining line indices valid after each collapse      |
| Flash color (already done)              | Both renderers                    | `g_colors[8]` / `PIECE_COLORS[8]` = white                   |
