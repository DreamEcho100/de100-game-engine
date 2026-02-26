# Lesson 12 — Restart, Polish & Final Integration

## What You'll Build

A complete, polished Tetris game:

- Game over shows a dark overlay with your final score and "Press R to restart"
- Pressing R resets everything and lets you play again immediately
- The window never needs to be closed and reopened
- Both X11 and Raylib versions work identically
- Valgrind shows zero memory leaks

This is the last lesson. After this you have a finished game.

---

## Step 1 — Add `restart` to `PlatformInput`

Open `src/tetris.h`. Find the `PlatformInput` struct:

```c
typedef struct {
    int move_left;
    int move_right;
    int move_down;
    int rotate;
    int quit;
} PlatformInput;
```

Add one field:

```c
    int restart;    /* 1 if R key was just pressed, 0 otherwise */
```

Like `rotate`, this should fire **once per press** — not every tick while
held. The player presses R once; the game restarts once.

✅ **Build checkpoint:** Compile. No visible change yet.

---

## Step 2 — Wire Up R in `main_x11.c`

Open `src/main_x11.c`. Find `platform_get_input()`. Look for where `rotate`
is set — it uses the same latch pattern we're about to copy.

The latch pattern: set on `KeyPress`, clear on `KeyRelease`. This fires exactly
once per press, no matter how long the key is held.

Find the `KeyPress` handling block. Add `restart` alongside the other keys:

```c
case XK_r:
case XK_R:
    input->restart = 1;
    break;
```

Find the `KeyRelease` handling block. Add the matching clear:

```c
case XK_r:
case XK_R:
    input->restart = 0;
    break;
```

We handle both lowercase and uppercase because the X key symbol depends on
whether Shift is held. Handling both means R works regardless.

✅ **Build checkpoint:** Compile the X11 version.

---

## Step 3 — Wire Up R in `main_raylib.c`

Open `src/main_raylib.c`. Find `platform_get_input()`. It uses `IsKeyPressed`
for the rotate action. Add restart the same way:

```c
input->restart = IsKeyPressed(KEY_R);
```

`IsKeyPressed` returns 1 only on the frame the key transitions from up to down.
Holding R down returns 0 after the first frame. That's the same behavior as our
X11 latch — one press, one restart.

✅ **Build checkpoint:** Compile the Raylib version.

---

## Step 4 — Handle Restart in the Main Loop

Open whichever platform file you're working in. Find the main game loop. It
should look like this from lesson 08:

```c
while (!platform_should_quit()) {
    platform_sleep_ms(50);
    platform_get_input(&input);
    if (input.quit) break;

    tetris_tick(&state, &input);
    platform_render(&state);
}
```

Update the tick section to handle restart:

```c
    if (state.game_over && input.restart)
        tetris_init(&state);
    else
        tetris_tick(&state, &input);
```

**What this does:** if the game is over AND the player pressed R, reset
everything by calling `tetris_init()`. Otherwise, run normal game logic.

`tetris_init()` zeros the entire `GameState` struct and re-initializes it —
field walls, score, speed, pieces, all of it. The window stays open. The X11
connection stays open. The Raylib window stays open. Only the game *data* resets.

Do this in **both** `main_x11.c` and `main_raylib.c`.

---

## Step 5 — Improve the Game Over Overlay

The game already shows "GAME OVER" text. Let's make it look polished: a dark
box in the center of the field, showing the final score and restart instructions.

### In `main_x11.c`

Find where the game over overlay is drawn in `platform_render()`. Replace it with:

```c
if (state->game_over) {
    int fw = FIELD_WIDTH  * CELL_SIZE;
    int fh = FIELD_HEIGHT * CELL_SIZE;
    int bw = 220, bh = 90;
    int bx = (fw - bw) / 2;
    int by = (fh - bh) / 2;

    /* Dark box */
    XSetForeground(g_display, g_gc, g_colors[0]);   /* black */
    XFillRectangle(g_display, g_window, g_gc, bx, by, bw, bh);

    /* Border */
    XSetForeground(g_display, g_gc, g_colors[8]);   /* white */
    XDrawRectangle(g_display, g_window, g_gc, bx, by, bw, bh);

    /* Text */
    char buf[64];
    XDrawString(g_display, g_window, g_gc,
                bx + 70, by + 24, "GAME OVER", 9);

    snprintf(buf, sizeof(buf), "Score: %d", state->score);
    XDrawString(g_display, g_window, g_gc,
                bx + 60, by + 48, buf, strlen(buf));

    XDrawString(g_display, g_window, g_gc,
                bx + 32, by + 70, "Press R to restart", 18);
}
```

**Coordinate math for centering:**

```
field_width  = FIELD_WIDTH  * CELL_SIZE = 12 * 32 = 384px
field_height = FIELD_HEIGHT * CELL_SIZE = 18 * 32 = 576px

box_width  = 220
box_height = 90

box_x = (384 - 220) / 2 = 82    ← 82px from left, leaving 82px on right
box_y = (576 -  90) / 2 = 243   ← 243px from top, leaving 243px on bottom
```

```
+───────────── 384px ──────────────+
│                                  │
│  ←82px→  +──────────────+        │
│           │  GAME OVER   │ ←90px │
│           │  Score: 450  │       │
│           │  Press R...  │       │
│           +──────────────+       │
│                 ↑ centered       │
+──────────────────────────────────+
```

`XFillRectangle` fills with the current foreground color — set it to black first.
`XDrawRectangle` draws only the outline — use white for contrast.

### In `main_raylib.c`

Find the game over section and replace it with:

```c
if (state->game_over) {
    int fw = FIELD_WIDTH  * CELL_SIZE;
    int fh = FIELD_HEIGHT * CELL_SIZE;
    int bw = 220, bh = 90;
    int bx = (fw - bw) / 2;
    int by = (fh - bh) / 2;

    /* Semi-transparent dark overlay on the whole field */
    DrawRectangle(0, 0, fw, fh, (Color){0, 0, 0, 160});

    /* Solid box */
    DrawRectangle(bx, by, bw, bh, BLACK);
    DrawRectangleLines(bx, by, bw, bh, WHITE);

    char buf[64];
    DrawText("GAME OVER",          bx + 52, by + 8,  20, WHITE);
    snprintf(buf, sizeof(buf), "Score: %d", state->score);
    DrawText(buf,                  bx + 60, by + 35, 16, GRAY);
    DrawText("Press R to restart", bx + 22, by + 62, 14, GRAY);
}
```

`(Color){0, 0, 0, 160}` is a semi-transparent black overlay — alpha 160 out of
255 means about 63% opaque. You can still see the frozen board underneath.
X11 doesn't support per-pixel alpha without extensions, so we use a solid box
there instead.

`DrawRectangleLines` draws only the outline, just like `XDrawRectangle`.

✅ **Build both versions. Press Escape or let pieces stack until game over.**
You should see the overlay. Press R — the field clears, score resets, game
starts fresh.

---

## Step 6 — Clean Shutdown

When the player quits (Escape or closes the window), we should free OS resources
in the right order. This isn't strictly required — the OS reclaims everything on
process exit — but it's good practice and makes Valgrind output clean.

### In `main_x11.c`

Find `platform_shutdown()`. Make sure it has these three calls, in this order:

```c
void platform_shutdown(void) {
    XFreeGC(g_display, g_gc);
    XDestroyWindow(g_display, g_window);
    XCloseDisplay(g_display);
}
```

**Why this exact order?**

| Call | Frees | Must come before |
|------|-------|-----------------|
| `XFreeGC(g_display, g_gc)` | The drawing context (fonts, colors) | `XCloseDisplay` |
| `XDestroyWindow(g_display, g_window)` | The window handle | `XCloseDisplay` |
| `XCloseDisplay(g_display)` | The connection to the X server | (last) |

After `XCloseDisplay`, the display pointer is invalid. You can't use it to free
the GC — that would be accessing freed memory. Always free *through* a resource
before closing the connection that owns it.

### In `main_raylib.c`

Find `platform_shutdown()`:

```c
void platform_shutdown(void) {
    CloseWindow();
}
```

Raylib wraps everything in `CloseWindow()`. It undoes everything `InitWindow()`
set up — the window, the OpenGL context, the audio device, all of it.

---

## Step 7 — Run Valgrind

Valgrind watches your program run and reports memory problems. Let's check for
leaks.

**Compile with debug symbols** (the `-g` flag), then run under Valgrind:

```sh
gcc -g -o tetris_x11 src/main_x11.c src/tetris.c -lX11
valgrind --leak-check=full ./tetris_x11
```

Play for a few seconds, then press Escape to exit cleanly. Valgrind prints a
summary at exit. Here's what to look for:

```
LEAK SUMMARY:
   definitely lost: 0 bytes in 0 blocks    ← THIS IS THE GOAL
   indirectly lost: 0 bytes in 0 blocks
     possibly lost: 0 bytes in 0 blocks
   still reachable: 8,192 bytes in 3 blocks ← NORMAL (X11 internals)
```

| Category | Meaning | Problem? |
|----------|---------|---------|
| `definitely lost` | You called `malloc` but never `free` | **Yes — fix it** |
| `still reachable` | Memory freed when process exits (X11 internal state) | No — normal |
| `possibly lost` | Valgrind isn't certain | Usually no |

Our game allocates no heap memory — `GameState` lives on the stack in `main()`.
The X11 library allocates a small internal buffer that shows up as `still
reachable`. That's fine. We want `definitely lost: 0`.

If you see `definitely lost > 0`, Valgrind shows the call stack where that
memory was allocated. Follow that back to find the `malloc` without a `free`.

---

## Step 8 — Final Integration Checklist

Go through this list. Test each feature:

| Feature | How to test |
|---------|------------|
| Window opens (both backends) | Run both binaries |
| Field boundary visible | Walls shown on all sides |
| Piece spawns at top center | Watch where each piece appears |
| Left/right movement | One tap = one move; holding doesn't slide |
| Rotation | Up/Z rotates; blocked near walls stays put |
| Soft drop | Hold down — falls faster |
| Piece locks when it can't fall | Drop a piece; watch it freeze |
| Locked cells stay | Stack multiple pieces; earlier ones don't move |
| Line detection fires | Fill a row; it turns white |
| Flash animation | White row visible for ~400ms |
| Row collapses | After flash, row disappears; pieces above drop |
| Score increases | +25 per piece; +200/400/800/1600 per line(s) |
| Level shows correctly | Increases every 50 pieces |
| Speed increases at higher levels | Drops feel faster |
| Next-piece preview | Sidebar preview matches what actually spawns |
| Game over triggers | Fill field to top; overlay appears |
| Game over shows score | Final score in the overlay |
| R restarts | Board clears, score resets, pieces start again |
| Escape quits | Window closes, no crash |
| Valgrind clean | `definitely lost: 0` |

---

## What You've Built

```
src/tetris.h       — game data types (GameState, PlatformInput, constants)
src/tetris.c       — all game logic (tick, init, collision, line clear, score)
src/platform.h     — the 6-function contract both platforms must implement
src/main_x11.c     — X11 implementation of the contract
src/main_raylib.c  — Raylib implementation of the contract
```

The game logic in `tetris.c` has **zero platform dependencies**. It doesn't
know whether it's running in a terminal, a window, or on a game console. It
just does game stuff: move pieces, check collisions, clear lines, update score.

The platform files are adapters. Each one wraps a completely different system
(X11's event queue vs. Raylib's polling, X11's pixel colors vs. Raylib's RGBA
structs) in the same 6-function shape. The game logic never needs to change when
you add a new platform.

**Mental model:** You built two programs that share one brain. The brain
(`tetris.c`) doesn't know if it's running on X11 or Raylib. It just does game
stuff. The platform files are two different bodies for the same brain.

---

## Where to Go Next

The game works. Here are 8 natural directions to take it:

| Extension | What to add | New concept |
|-----------|------------|-------------|
| **Ghost piece** | Draw a faded preview of where the piece will land. | Raycast downward: keep calling `DoesPieceFit` with `y+1`, `y+2`, ... until it fails. The last valid Y is the ghost position. |
| **Wall kicks** | When rotating fails, try shifting 1 cell left or right before giving up. | Retry logic: `DoesPieceFit(piece, rot, x-1, y)` and `DoesPieceFit(piece, rot, x+1, y)` as fallbacks. |
| **Hard drop** | Space bar instantly drops the piece to the lowest valid position. | Same raycast as the ghost piece — find the drop Y, then lock immediately. |
| **7-bag randomizer** | Shuffle all 7 pieces into a "bag," deal them out one at a time, then reshuffle. | Fisher-Yates shuffle. Guarantees you see each piece exactly once per 7. |
| **Hold piece** | Press C to save the current piece; press again to swap it back. | One more field in `GameState`: `int hold_piece`. Swap `current_piece` and `hold_piece` on C press. |
| **High score table** | Write the top 10 scores to a file on disk. | `fopen`, `fprintf`, `fscanf`. Good first file I/O exercise. |
| **Sound effects** | Play a sound when a piece locks, when lines clear, and on game over. | Raylib: `PlaySound()`. X11: link against a sound library or use the PC speaker. |
| **SDL3 backend** | Write `src/main_sdl3.c` implementing the same 6 functions. | SDL3 runs on Linux, macOS, and Windows. The game logic compiles unchanged. |

---

## What You Now Know

You came in as a full-stack JS/TS developer. You leave knowing:

- **C fundamentals**: structs, pointers, arrays, `#define`, `typedef`, `memset`
- **Manual memory**: stack allocation, why `sizeof` matters, `snprintf` vs `sprintf`
- **Platform programming**: how X11 works at the event-queue level, how Raylib wraps it
- **Game architecture**: separating platform-specific I/O from platform-agnostic logic
- **2D rendering**: pixel coordinates, color arrays, drawing rectangles and text
- **Bit operations**: bitmasks, left-shift for powers of two
- **Data representation**: 1D arrays as 2D grids, sentinel values, flag fields
- **Debugging**: Valgrind, the difference between `definitely lost` and `still reachable`

Every game you build from here — no matter the language or framework — uses these
same ideas: a game loop, input → update → render, separating logic from display.
You've seen how it works at the metal. That understanding doesn't expire.
