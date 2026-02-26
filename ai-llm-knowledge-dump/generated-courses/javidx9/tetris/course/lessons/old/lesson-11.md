# Lesson 11 — Score, Speed Progression & HUD

## What You'll Build

By the end of this lesson, the game looks like a real Tetris game:

```
+────────────────────+  ┌──────────────────┐
│  . . . . . . . . . │  │ SCORE            │
│  . . . . . . . . . │  │ 1450             │
│  . . X . . . . . . │  │                  │
│  . X X X . . . . . │  │ LEVEL            │
│  . . . . . . X . . │  │ 2                │
│  . . . . X X X . . │  │                  │
│  . X X . X X X X . │  │ NEXT             │
│  X X X X X X X X . │  │  . X .           │
+────────────────────+  │  X X X           │
                        └──────────────────┘
```

Three things to add:

1. **Score** — points for locking pieces and clearing lines
2. **Speed progression** — game speeds up every 50 pieces
3. **HUD sidebar** — draws score, level, and the next-piece preview

---

## Before You Start

You should have working line clearing from lesson 10. The fields we add here
build on the `GameState` struct that already has `speed`, `next_piece`, and
`line_count`.

---

## Step 1 — Add `score` and `piece_count` to `GameState`

Open `src/tetris.h`. Find the `GameState` struct. Add these two lines:

```c
int score;        /* total score */
int piece_count;  /* total pieces locked since game start */
```

`score` accumulates points. `piece_count` tracks how many pieces have locked
so we know when to speed up.

✅ **Build checkpoint:** Compile. No behavior change yet.

---

## Step 2 — Calculate Score After Locking

Open `src/tetris.c`. Find the spot in `tetris_tick()` where you added line
detection in lesson 10 (right after the locking loop). Add the scoring block
**after** line detection and **before** spawning the next piece:

```c
/* ── SCORING ─────────────────────────────────────────────────────── */
state->piece_count++;
state->score += 25;   /* 25 points for every piece locked */
if (state->line_count > 0)
    state->score += (1 << state->line_count) * 100;
```

**What `1 << state->line_count` means:**

`<<` is the **left-shift** operator. Each shift doubles the number. Starting
from 1:

```
1 << 0  =   1   (no shift)
1 << 1  =   2   (doubled once)
1 << 2  =   4   (doubled twice)
1 << 3  =   8   (doubled three times)
1 << 4  =  16   (doubled four times)
```

Multiply by 100:

| Lines cleared | Formula | Points |
|--------------|---------|--------|
| 1 | `(1 << 1) * 100` = `2 * 100` | 200 |
| 2 | `(1 << 2) * 100` = `4 * 100` | 400 |
| 3 | `(1 << 3) * 100` = `8 * 100` | 800 |
| 4 | `(1 << 4) * 100` = `16 * 100` | 1600 |

Clearing 4 lines at once (a "Tetris") gives 8× the points of clearing 1 line.
That's the incentive to stack flat and wait for the I-piece.

In JavaScript, `<<` works exactly the same:

```js
console.log(1 << 3);  // 8 — identical to C
```

✅ **Build checkpoint:** Compile and run. Play a few pieces. The score isn't
displayed yet, but it's being tracked. We'll verify it when we draw the HUD.

---

## Step 3 — Speed Up Every 50 Pieces

Still in the scoring block, add speed progression right after scoring:

```c
/* ── SPEED PROGRESSION ───────────────────────────────────────────── */
if (state->piece_count % 50 == 0 && state->speed > 10)
    state->speed--;
```

**What `% 50 == 0` means:**

`%` is the **modulo** operator. It gives you the remainder of division.
`piece_count % 50 == 0` is true whenever `piece_count` is a multiple of 50:

```
piece_count:  1   2  ...  50   51  ...  100  101  ...  150
% 50:         1   2  ...   0    1  ...    0    1  ...    0
speed up?    no  no  ...  YES   no  ...  YES   no  ...  YES
```

`state->speed` is the number of ticks between forced drops. Smaller speed =
fewer ticks between drops = piece falls faster:

| `speed` | Ticks/drop | At 50ms/tick | Feel |
|---------|-----------|-------------|------|
| 20 | 20 | 1 drop/sec | Starting pace |
| 18 | 18 | 1.1 drops/sec | Slightly faster |
| 15 | 15 | 1.3 drops/sec | Noticeably quicker |
| 10 | 10 | 2 drops/sec | Minimum — stops here |

We stop at `speed > 10` so the game doesn't become literally unplayable. Feel
free to lower the floor if you want more of a challenge.

✅ **Build checkpoint:** Compile. Play 50 pieces (or lower the threshold to 5
for testing) and verify the drop speed increases.

---

## Step 4 — Widen the Window for the Sidebar

The game window is currently `FIELD_WIDTH * CELL_SIZE` pixels wide — just wide
enough for the field. The HUD needs extra space to the right.

Open `src/tetris.h`. Add this constant near the other `#define`s:

```c
#define SIDEBAR_WIDTH 200   /* extra pixels to the right of the field */
```

Now update the window creation in **both** backends to use the wider width.

**In `src/main_x11.c`**, find the `XCreateSimpleWindow` call and change the
width argument from `FIELD_WIDTH * CELL_SIZE` to
`FIELD_WIDTH * CELL_SIZE + SIDEBAR_WIDTH`:

```c
g_window = XCreateSimpleWindow(
    g_display, DefaultRootWindow(g_display),
    0, 0,
    FIELD_WIDTH * CELL_SIZE + SIDEBAR_WIDTH,   /* ← updated */
    FIELD_HEIGHT * CELL_SIZE,
    0, 0, g_colors[0]
);
```

**In `src/main_raylib.c`**, find the `InitWindow` call and do the same:

```c
InitWindow(FIELD_WIDTH * CELL_SIZE + SIDEBAR_WIDTH,   /* ← updated */
           FIELD_HEIGHT * CELL_SIZE,
           "Tetris — Raylib");
```

✅ **Build checkpoint:** Compile and run. The window should be wider now —
200 pixels of empty space to the right of the field.

---

## Step 5 — Draw the HUD in `main_x11.c`

Open `src/main_x11.c`. Find `platform_render()`. The function already draws
the field and the current piece. We'll add the sidebar at the end.

First, add `#include <stdio.h>` at the top of the file (for `snprintf`), if it
isn't there already.

At the end of `platform_render()`, before the final `XFlush`, add:

```c
/* ── HUD SIDEBAR ─────────────────────────────────────────────────── */
char buf[64];
int sx = FIELD_WIDTH * CELL_SIZE + 12;   /* X start of sidebar text */
int sy = 30;                              /* Y start of first line   */

/* Set text color to white */
XSetForeground(g_display, g_gc, g_colors[8]);

/* Score */
snprintf(buf, sizeof(buf), "SCORE");
XDrawString(g_display, g_window, g_gc, sx, sy, buf, strlen(buf));
snprintf(buf, sizeof(buf), "%d", state->score);
XDrawString(g_display, g_window, g_gc, sx, sy + 18, buf, strlen(buf));

/* Level (computed from speed — not stored directly) */
int level = (20 - state->speed) / 2;
snprintf(buf, sizeof(buf), "LEVEL");
XDrawString(g_display, g_window, g_gc, sx, sy + 54, buf, strlen(buf));
snprintf(buf, sizeof(buf), "%d", level);
XDrawString(g_display, g_window, g_gc, sx, sy + 72, buf, strlen(buf));

/* NEXT label */
snprintf(buf, sizeof(buf), "NEXT");
XDrawString(g_display, g_window, g_gc, sx, sy + 108, buf, strlen(buf));

/* Next-piece preview: draw a 4x4 grid of the next piece at rotation 0 */
int preview_x = FIELD_WIDTH * CELL_SIZE + 12;
int preview_y = sy + 120;
for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
        if (tetromino[state->next_piece][rotate(px, py, 0)] == 'X') {
            int cx = preview_x + px * (CELL_SIZE / 2);
            int cy = preview_y + py * (CELL_SIZE / 2);
            int cs = CELL_SIZE / 2 - 2;
            XSetForeground(g_display, g_gc,
                           g_colors[state->next_piece + 1]);
            XFillRectangle(g_display, g_window, g_gc, cx, cy, cs, cs);
        }
    }
}
```

**Key explanations:**

`snprintf(buf, sizeof(buf), "SCORE: %d", state->score)` — converts the integer
score to a string and writes it into `buf`. `sizeof(buf)` is 64; `snprintf`
guarantees it never writes more than 64 bytes, so the buffer can't overflow.
Never use plain `sprintf` in C — it doesn't have this safety limit.

`XDrawString(g_display, g_window, g_gc, x, y, text, strlen(text))` — draws a
string. The `y` coordinate is the **text baseline** (the bottom of most
letters). So if you want the top of the text at pixel 30, the baseline is
around pixel 42 (depends on font). Add 12–18 pixels to your intended Y.

`int level = (20 - state->speed) / 2` — derives the display level from the
current speed. If `speed = 20` (start), level = 0. If `speed = 18`, level = 1.
We don't store level in `GameState` because it's always derivable from speed.

The preview loop uses half-size cells (`CELL_SIZE / 2 = 16px`) so the preview
fits in the sidebar without overwhelming it.

✅ **Build and test the X11 version.** You should see score, level, and the
next-piece preview in the sidebar.

---

## Step 6 — Draw the HUD in `main_raylib.c`

Open `src/main_raylib.c`. Find `platform_render()`. At the end, before
`EndDrawing()`, add:

```c
/* ── HUD SIDEBAR ─────────────────────────────────────────────────── */
char buf[64];
int sx = FIELD_WIDTH * CELL_SIZE + 12;
int sy = 20;

/* Score */
DrawText("SCORE", sx, sy, 18, WHITE);
snprintf(buf, sizeof(buf), "%d", state->score);
DrawText(buf, sx, sy + 22, 22, WHITE);

/* Level */
int level = (20 - state->speed) / 2;
DrawText("LEVEL", sx, sy + 60, 18, WHITE);
snprintf(buf, sizeof(buf), "%d", level);
DrawText(buf, sx, sy + 82, 22, WHITE);

/* Next piece */
DrawText("NEXT", sx, sy + 120, 18, WHITE);

int preview_x = sx;
int preview_y = sy + 142;
int cs = CELL_SIZE / 2 - 2;
for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
        if (tetromino[state->next_piece][rotate(px, py, 0)] == 'X') {
            DrawRectangle(
                preview_x + px * (CELL_SIZE / 2),
                preview_y + py * (CELL_SIZE / 2),
                cs, cs,
                PIECE_COLORS[state->next_piece + 1]
            );
        }
    }
}
```

**Comparing X11 and Raylib text drawing:**

| | X11 `XDrawString` | Raylib `DrawText` |
|---|---|---|
| Parameters | 7 (`display, window, gc, x, y, text, len`) | 5 (`text, x, y, size, color`) |
| Y coordinate | Text **baseline** | Text **top** |
| Font size | Set on GC beforehand | Passed directly |
| Color | `XSetForeground` beforehand | Passed directly |

Raylib is simpler. X11 requires more setup but gives you more low-level control.
Both draw the same thing — we've been living this tradeoff since lesson 05.

`DrawText` Y is the top of the text, not the baseline. So if you want text to
start 20 pixels from the top, just pass `y = 20`. No arithmetic needed.

✅ **Build and test the Raylib version:**

```sh
gcc -o tetris_raylib src/main_raylib.c src/tetris.c -lraylib
./tetris_raylib
```

You should see identical HUD information in both versions.

---

## ✅ Final Build and Play Test

```sh
gcc -o tetris_x11    src/main_x11.c    src/tetris.c -lX11
gcc -o tetris_raylib src/main_raylib.c src/tetris.c -lraylib
```

Play both versions and verify:

- [ ] Score starts at 0 and increases when pieces lock (+25 each)
- [ ] Clearing a line gives a bigger bonus (200 / 400 / 800 / 1600)
- [ ] LEVEL in the sidebar changes after every 50 pieces
- [ ] NEXT preview shows the correct upcoming piece
- [ ] The next piece in the preview matches what actually spawns next

---

## Exercise: Add a Piece Counter to the HUD

The `piece_count` field is being tracked in `GameState`. Display it in the
sidebar below the level counter.

**Steps:**

1. In both `platform_render()` functions, add a "PIECES" label and value after
   the level display.
2. Use `snprintf(buf, sizeof(buf), "%d", state->piece_count)`.
3. Space it far enough below the level so the text doesn't overlap.

Bonus: add visual level bars. Draw N filled rectangles where N equals the level.

---

## Mental Model

The HUD is just more drawing. Everything on screen — field, pieces, HUD — is
rectangles and text at specific pixel coordinates. There's no magic framework.

Score is an integer in `GameState`. Drawing it means: convert to string with
`snprintf`, then call the platform's text drawing function with an X/Y position.
Level is derived from speed — it's computed fresh each frame.

The next-piece preview is the same loop you use to render the current piece —
just pointed at `next_piece` instead of `current_piece`, and offset to the
sidebar position. Same data, different coordinates.

---

## Summary

| What you added | Where | Effect |
|----------------|-------|--------|
| `score`, `piece_count` | `tetris.h` — `GameState` | Track score and piece count |
| Scoring block | `tetris.c` — after locking | +25/lock, +200/400/800/1600 for lines |
| Speed progression | `tetris.c` — after scoring | Speed up every 50 pieces |
| `SIDEBAR_WIDTH` | `tetris.h` | Widens both backend windows |
| HUD rendering | Both `main_*.c` | Draws score, level, next-piece preview |
