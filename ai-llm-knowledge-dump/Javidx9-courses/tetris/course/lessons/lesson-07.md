# Lesson 07 — Input: Arrow Keys Move the Piece

---

## What You'll Have Working by the End

Press ← → to slide the piece left and right. Press ↓ to drop it faster. Press Z
to rotate it — **exactly once per press**, no matter how long you hold it. Push
the piece into the wall and it stops dead.

Both X11 and Raylib respond identically.

The piece still doesn't fall on its own. That's lesson 08.

---

## Where We're Starting

At the end of lesson 06, two platform functions are stubs:

```c
/* main_x11.c AND main_raylib.c — current stub: */
void platform_get_input(PlatformInput *input) {
    memset(input, 0, sizeof(PlatformInput));  /* always returns all zeros */
}

void platform_sleep_ms(int ms) { (void)ms; } /* does nothing yet */
int  platform_should_quit(void) { return 0; }/* never quits    */
```

And both `main()` functions look like this:

```c
int main(void) {
    platform_init();
    tetris_init(&state);
    while (1) {
        platform_render(&state);  /* draws the field — static, no logic */
    }
    platform_shutdown();
    return 0;
}
```

`tetris_apply_input()` and `tetris_tick()` do not exist yet.

---

## Step 1: Understand the Plan

We need two new pieces working together:

```
KEYBOARD
   │
   ▼
platform_get_input()   ← platform layer (X11 or Raylib)
   │ fills PlatformInput struct
   ▼
tetris_apply_input()   ← game layer (tetris.c — same for both platforms)
   │ updates GameState
   ▼
platform_render()      ← draws the updated state
```

The platform layer speaks "keyboard." The game layer speaks "piece position."
`PlatformInput` is the shared language between them.

---

## Step 2: Add Key State Flags to `main_x11.c`

X11 does not have an API like "is the left arrow held down right now?" Instead,
it sends you a `KeyPress` event when a key goes down and a `KeyRelease` event when
it comes up. We track the current held state ourselves with five `static` flags.

Open `src/main_x11.c`. Find the existing global variable block (the section with
`static int g_should_quit = 0;`). Add these five lines right after it:

```c
/* Key state — maintained from X11 event queue.
 * move_* : 1 while held, 0 when released (continuous).
 * rotate/restart : "latched" — 1 on KeyPress, cleared after one tick read. */
static int g_key_left    = 0;
static int g_key_right   = 0;
static int g_key_down    = 0;
static int g_key_rotate  = 0;
static int g_key_restart = 0;
```

---

## Step 3: Implement `platform_get_input()` in `main_x11.c`

Find the stub `platform_get_input()` and **replace the entire function** with this:

```c
void platform_get_input(PlatformInput *input) {
    memset(input, 0, sizeof(PlatformInput));

    /* Drain all events that arrived since last tick.
     * XPending() returns count of waiting events — 0 means nothing to read.
     * We drain ALL of them, not just one: between 50ms ticks the user can
     * press multiple keys, and we don't want to miss any. */
    while (XPending(g_display)) {
        XEvent event;
        XNextEvent(g_display, &event);

        if (event.type == KeyPress) {
            /* XLookupKeysym converts a raw hardware keycode into a portable
             * named constant: XK_Left, XK_Right, XK_z, XK_Escape, etc.
             * Like event.key in a browser keydown listener. */
            KeySym key = XLookupKeysym(&event.xkey, 0);

            if (key == XK_Left  || key == XK_a)  g_key_left    = 1;
            if (key == XK_Right || key == XK_d)  g_key_right   = 1;
            if (key == XK_Down  || key == XK_s)  g_key_down    = 1;
            if (key == XK_z     || key == XK_x)  g_key_rotate  = 1; /* latch */
            if (key == XK_r)                     g_key_restart = 1; /* latch */
            if (key == XK_Escape || key == XK_q) g_should_quit = 1;
        }

        if (event.type == KeyRelease) {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            /* Clear movement keys on release so sliding stops when you let go. */
            if (key == XK_Left  || key == XK_a)  g_key_left  = 0;
            if (key == XK_Right || key == XK_d)  g_key_right = 0;
            if (key == XK_Down  || key == XK_s)  g_key_down  = 0;
            /* g_key_rotate is NOT cleared here — see the latch explanation below. */
        }

        /* "User clicked the ✕ button" is sent as a ClientMessage, not a KeyPress. */
        if (event.type == ClientMessage)
            g_should_quit = 1;
    }

    /* Copy current state to the output struct. */
    input->move_left  = g_key_left;
    input->move_right = g_key_right;
    input->move_down  = g_key_down;
    input->rotate     = g_key_rotate;
    input->restart    = g_key_restart;
    input->quit       = g_should_quit;

    /* Clear the latched keys AFTER reading them.
     * This ensures rotate/restart fire exactly once per press. */
    g_key_rotate  = 0;
    g_key_restart = 0;
}
```

### Why `g_key_rotate` gets a latch and `g_key_left` doesn't

Open a text editor and hold the letter A. You get one "a", a brief pause, then
"aaaaaaaaaaaaaaaa" floods in. That's OS **auto-repeat** — the OS resends `KeyPress`
continuously while any key is held.

For left/right/down: perfect. Hold left = piece slides continuously. That's what
you want.

For rotation: catastrophe. Hold Z = piece spins wildly 10 times per second.
The player tapped Z once. The OS sent 15 `KeyPress` events.

The latch fixes this with one insight: a boolean flag can only be `1` once.

```
User presses Z and holds for 0.5 seconds:

OS sends:   KeyPress(Z)  KeyPress(Z)  KeyPress(Z)  KeyPress(Z)  KeyRelease(Z)
              (real)      (repeat)     (repeat)     (repeat)

g_key_rotate: 0→1          1            1            1
                           ↑ already 1, setting to 1 again does nothing

Tick reads g_key_rotate = 1 → rotates once → clears to 0

All the repeat events already fired — but the flag was already 1.
No extra rotations. ✅
```

---

## Step 4: Implement `platform_get_input()` in `main_raylib.c`

Find the stub and **replace the entire function** with:

```c
void platform_get_input(PlatformInput *input) {
    input->move_left  = IsKeyDown(KEY_LEFT)    || IsKeyDown(KEY_A);
    input->move_right = IsKeyDown(KEY_RIGHT)   || IsKeyDown(KEY_D);
    input->move_down  = IsKeyDown(KEY_DOWN)    || IsKeyDown(KEY_S);
    input->rotate     = IsKeyPressed(KEY_Z)    || IsKeyPressed(KEY_X);
    input->restart    = IsKeyPressed(KEY_R);
    input->quit       = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q);
}
```

6 lines. Compare to the ~35-line X11 version. Same behavior.

`IsKeyDown(key)` — returns `1` every single frame the key is physically held.
Good for movement (continuous while held).

`IsKeyPressed(key)` — returns `1` only on the **first frame** of a new press.
Returns `0` for all subsequent frames while held. Naturally solves auto-repeat
without any latch flag.

Raylib processes the OS event queue internally (inside `EndDrawing()`). By the
time your code calls `IsKeyPressed()`, Raylib has already done the equivalent
of our X11 drain loop. You just query the result.

---

## Step 5: Add `tetris_apply_input()` to `tetris.c`

This is pure game logic — no X11, no Raylib. Open `src/tetris.c`.

Add this function **after** `tetris_does_piece_fit()`:

```c
/* Apply one frame of player input to the game state.
 * Every move is gated by tetris_does_piece_fit():
 *   - Try the new position
 *   - Only commit if it's valid (empty cells, in bounds)
 * Walls, floor, and stacked pieces all block movement automatically. */
void tetris_apply_input(GameState *state, const PlatformInput *input) {

    /* Move left: try current_x - 1 */
    if (input->move_left &&
        tetris_does_piece_fit(state, state->current_piece, state->current_rotation,
                              state->current_x - 1, state->current_y))
        state->current_x--;

    /* Move right: try current_x + 1 */
    if (input->move_right &&
        tetris_does_piece_fit(state, state->current_piece, state->current_rotation,
                              state->current_x + 1, state->current_y))
        state->current_x++;

    /* Soft drop: try current_y + 1 (down = positive Y) */
    if (input->move_down &&
        tetris_does_piece_fit(state, state->current_piece, state->current_rotation,
                              state->current_x, state->current_y + 1))
        state->current_y++;

    /* Rotate clockwise: try rotation + 1. (% 4 wraps 3 back to 0.) */
    if (input->rotate &&
        tetris_does_piece_fit(state, state->current_piece, state->current_rotation + 1,
                              state->current_x, state->current_y))
        state->current_rotation++;
}
```

Now open `src/tetris.h`. In the `Public API` section, add the declaration:

```c
/* Apply player input (move/rotate) to state for one tick.
 * Called from tetris_tick(). Each move is gated by tetris_does_piece_fit(). */
void tetris_apply_input(GameState *state, const PlatformInput *input);
```

### How collision-gated movement eliminates wall-check code

For left movement, the pattern is always:

```
IF (left key held) AND (piece fits at x-1) THEN move left
```

The field array already has walls encoded as value `9`. When you call
`tetris_does_piece_fit()` with `x - 1`, it checks every solid cell of the piece
against every field cell at the destination. If any field cell is non-zero
(wall = 9, locked piece = 1–7), it returns `0` — blocked.

You never write:
```c
if (state->current_x > 0) { /* manual left-wall check — DON'T DO THIS */
```

The walls are in the data. The data answers the question for you.

---

## Step 6: Update `main()` in Both Files

### `src/main_x11.c` — replace the `main()` function:

```c
int main(void) {
    srand((unsigned int)time(NULL));

    GameState     state;
    PlatformInput input;

    platform_init();
    tetris_init(&state);

    while (!platform_should_quit()) {
        platform_sleep_ms(50);           /* ~50ms per tick — still a stub, OK for now */
        platform_get_input(&input);      /* drain X11 event queue, fill input struct  */

        if (input.quit) break;

        tetris_apply_input(&state, &input); /* move/rotate piece if valid */

        platform_render(&state);         /* draw updated state */
    }

    platform_shutdown();
    return 0;
}
```

### `src/main_raylib.c` — replace `main()` with the same structure:

```c
int main(void) {
    srand((unsigned int)time(NULL));

    GameState     state;
    PlatformInput input;

    platform_init();
    tetris_init(&state);

    while (!platform_should_quit()) {
        platform_sleep_ms(50);
        platform_get_input(&input);

        if (input.quit) break;

        tetris_apply_input(&state, &input);

        platform_render(&state);
    }

    platform_shutdown();
    return 0;
}
```

Both loops are word-for-word identical. Different files, same code. The platform
differences are entirely inside `platform_get_input()` — not in the loop itself.

`platform_sleep_ms()` is still a stub (does nothing). The loop runs as fast as
possible. Input will work, but the loop might be slightly snappy. That's fine for
this lesson — we fix pacing in lesson 08.

---

## Step 7: Build and Run

```bash
# From the course/ directory:
./build_x11.sh    && ./tetris_x11
./build_raylib.sh && ./tetris_raylib
```

### What to test

| Action | Expected result |
|--------|-----------------|
| Press ← | Piece slides one column left |
| Hold ← | Piece keeps sliding left (continuous) |
| Push piece against left wall | Stops at the wall — cannot go further |
| Press → | Piece slides one column right |
| Press ↓ | Piece drops one row down |
| Press ↓ at floor | Nothing — piece stops at floor |
| Press Z once | Piece rotates once |
| Hold Z | Piece rotates once total (latch working) |
| Press Q or Escape | Game exits |
| Click ✕ (X11) | Game exits cleanly |

### Common issue: compile error about `tetris_apply_input`

If you get `undefined reference to tetris_apply_input`, check that:
1. The declaration is in `tetris.h` (the `void tetris_apply_input(...)` line)
2. The definition is in `tetris.c` (the actual function body)
3. Your build command compiles `tetris.c` — check `build_x11.sh`

---

## Key Concept: "Ask before moving"

```
WRONG: move → check if valid → if bad, move back
RIGHT: check if valid → if OK, move
```

The "right" pattern is simpler and correct by construction. You can never get
into an invalid state because you only ever move to valid states.

This pattern — validate before committing, not after — shows up everywhere in
systems programming: memory allocation checks, file open checks, network send
checks. Never assume success. Always gate on the result.

---

## Exercise

**Add WASD as alternative controls in X11.**

The Raylib version already supports it (notice the `|| IsKeyDown(KEY_A)` etc.).
In the X11 version's `platform_get_input()`, add to the `KeyPress` block:

```c
/* WASD — same as arrow keys */
if (key == XK_a) g_key_left   = 1;
if (key == XK_d) g_key_right  = 1;
if (key == XK_s) g_key_down   = 1;
if (key == XK_w) g_key_rotate = 1;  /* W rotates (like some Tetris games use up-arrow) */
```

And in the `KeyRelease` block:

```c
if (key == XK_a) g_key_left  = 0;
if (key == XK_d) g_key_right = 0;
if (key == XK_s) g_key_down  = 0;
/* W/rotate is latched — no KeyRelease handler needed */
```

Build and verify WASD works identically to arrow keys.

---

## Checkpoint

Files changed in this lesson:

| File | What changed |
|------|-------------|
| `src/main_x11.c` | Added 5 key flags; implemented `platform_get_input()`; updated `main()` |
| `src/main_raylib.c` | Implemented `platform_get_input()`; updated `main()` |
| `src/tetris.c` | Added `tetris_apply_input()` |
| `src/tetris.h` | Declared `tetris_apply_input()` |

**Next:** Lesson 08 — Game Loop & Automatic Falling
