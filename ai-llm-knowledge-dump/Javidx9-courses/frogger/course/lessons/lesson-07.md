# Lesson 7 — Frog Placement and Keyboard Input

**By the end of this lesson you will have:**
The frog responding to arrow keys — hop up, down, left, right, one tile per
keypress. The movement fires on key *release* (not press), which gives the
classic Frogger "hop" feel: one tap = one hop, no holding.

---

## The frog in GameState

Open `frogger.h`. Find `GameState`:

```c
typedef struct {
    float  frog_x;    /* tile column — float so river movement is smooth  */
    float  frog_y;    /* tile row    — float for same reason               */
    float  time;
    int    homes_reached;
    int    dead;
    float  dead_timer;
    uint8_t danger[SCREEN_CELLS_W * SCREEN_CELLS_H];
    SpriteBank sprites;
} GameState;
```

**Why `float` for tile position?**
The river carries the frog sideways at fractional-tile speeds (e.g., 0.048 tiles
per frame). An `int` would round that to zero — the frog would never move with
the current. `float` preserves the sub-tile position.

**New C concept — `typedef struct`:**
```c
typedef struct { float x; float y; } Vec2;
Vec2 pos = {1.0f, 2.0f};
```
`typedef` gives the struct a name so you can use `Vec2` instead of `struct Vec2`.
JS analogy: like `interface Vec2 { x: number; y: number }` — defines a shape.

---

## Step 1 — How frog position becomes a pixel

In `platform_render`:
```c
int frog_px = (int)(state->frog_x * (float)TILE_PX);
int frog_py = (int)(state->frog_y * (float)TILE_PX);
```

Frog at (8.0, 9.0):
```
frog_px = (int)(8.0 × 64) = 512
frog_py = (int)(9.0 × 64) = 576
```

`TILE_PX = TILE_CELLS * CELL_PX = 8 * 8 = 64`. The `(int)` truncates at the
**pixel** level, giving 1-pixel precision. This is important: `frog_x` drifts
as a float when the frog rides a log, so the render must track that fractional
position smoothly.

> **Common mistake — premature `(int)` cast:**
> `(int)(frog_x * TILE_CELLS) * CELL_PX` inserts the cast one step too early:
> `frog_x=8.05` → `(int)(64.4)=64` → `512 px`. Then `frog_x=8.12` → `(int)(65.0)=65`
> → `520 px` — a sudden 8px jump! The frog stutters in 8-pixel hops instead of
> gliding smoothly. Always multiply to the finest unit first, then truncate once.

---

## Step 2 — InputState

In `frogger.h`:
```c
typedef struct {
    int up_released;
    int down_released;
    int left_released;
    int right_released;
} InputState;
```

**Why "released" and not "pressed"?**
- "pressed" fires every frame while held → frog moves continuously (too fast)
- "released" fires once per tap → frog hops exactly one tile per keypress

This matches the original Frogger feel: each tap = one hop.

**How it's populated in Raylib (`main_raylib.c`):**
```c
void platform_get_input(InputState *input) {
    memset(input, 0, sizeof(InputState));  /* clear last frame's state  */
    if (IsKeyReleased(KEY_UP))    input->up_released    = 1;
    if (IsKeyReleased(KEY_DOWN))  input->down_released  = 1;
    if (IsKeyReleased(KEY_LEFT))  input->left_released  = 1;
    if (IsKeyReleased(KEY_RIGHT)) input->right_released = 1;
    if (IsKeyReleased(KEY_ESCAPE)) g_quit = 1;
}
```

`memset(input, 0, sizeof(InputState))` zeroes the struct every frame.
JS analogy: `Object.assign(input, { upReleased: false, ... })` at start of frame.

**How it's populated in X11 (`main_x11.c`):**

X11 sends discrete events. We track held keys and fire "released" when the event
arrives. X11 also sends fake auto-repeat pairs (`KeyRelease`+`KeyPress`) while a
key is held. Without detection, every fake release would make the frog hop — so
we peek at the next event to skip auto-repeat:

```c
if (e.type == KeyPress) {
    KeySym sym = XLookupKeysym(&e.xkey, 0);
    if (sym == XK_Up || sym == XK_w) g_key_down[0] = 1;
    ...
}
if (e.type == KeyRelease) {
    /* Peek: if next event is a KeyPress with same keycode+time, it's
       auto-repeat — consume it and skip this release entirely.       */
    int is_repeat = 0;
    if (XEventsQueued(g_display, QueuedAfterReading)) {
        XEvent ahead;
        XPeekEvent(g_display, &ahead);
        if (ahead.type == KeyPress &&
            ahead.xkey.keycode == e.xkey.keycode &&
            ahead.xkey.time    == e.xkey.time) {
            XNextEvent(g_display, &ahead); /* consume fake KeyPress */
            is_repeat = 1;
        }
    }
    if (!is_repeat) {
        KeySym sym = XLookupKeysym(&e.xkey, 0);
        if (sym == XK_Up || sym == XK_w) { input->up_released = 1; g_key_down[0] = 0; }
        ...
    }
}
```

Without the peek, holding a key fires `KeyRelease` at ~30 Hz → the frog hops
repeatedly from one tap. The peek-ahead detects and discards all fake pairs.

**New C concept — `memset(ptr, value, size)`:**
Sets `size` bytes starting at `ptr` to `value`. Here, zeroes the whole struct.
JS analogy: like `Object.keys(obj).forEach(k => obj[k] = 0)` but instant.

---

## Step 3 — Input in frogger_tick

In `frogger.c`, `frogger_tick`:

```c
if (input->up_released)    state->frog_y -= 1.0f;
if (input->down_released)  state->frog_y += 1.0f;
if (input->left_released)  state->frog_x -= 1.0f;
if (input->right_released) state->frog_x += 1.0f;
```

Y increases downward (row 0 = top, row 9 = bottom). Moving "up" towards the goal
means *decreasing* frog_y — same as CSS where y=0 is the top.

**Why does this live in `frogger_tick` and not in `platform_get_input`?**
Platform code should only handle OS events — not game logic. The decision
"what does pressing UP do?" is game logic. This keeps the platform code reusable:
you could use the same X11 backend for a different game with different controls.

---

## Step 4 — Bounds (off-screen = death)

When the frog drifts off screen (river carries it to the edge), `frogger_tick`
kills it using a pixel-level check:

```c
int frog_px_x = (int)(state->frog_x * (float)TILE_PX);
int frog_px_y = (int)(state->frog_y * (float)TILE_PX);

if (frog_px_x < 0 || frog_px_y < 0 ||
    frog_px_x + TILE_PX > SCREEN_PX_W ||
    frog_px_y + TILE_PX > SCREEN_PX_H) {
    state->dead = 1;
    state->dead_timer = 0.4f;
    return;
}
```

We check whether the frog **sprite** (not just its top-left corner) fits on
screen. `frog_px_x + TILE_PX > SCREEN_PX_W` means the right edge of the frog
has gone past the window's right edge.

Why pixel-level (not tile-level)?
With river drift, `frog_x` can be 15.9 before the integer tile check would
fire. At that point the frog visually runs off the screen for a whole tile-width
before dying. The pixel check kills the moment any edge leaves the window.

---

## Build & Run

```sh
./build_x11.sh && ./frogger_x11
```

**What you should see:**
Press the arrow keys. The frog hops one tile per keypress.
Lanes still scroll underneath, but no collision happens yet (Lesson 9).

**If keys don't respond in X11:**
Make sure the window has focus (click on it first). X11 only delivers keyboard
events to the focused window.

---

## Mental Model

Input handling in three layers:

```
OS event (KeyRelease)
       ↓
platform_get_input()   — translates OS event to InputState
       ↓
frogger_tick()         — translates InputState to GameState change
       ↓
platform_render()      — reads GameState, draws frog at new position
```

Each layer does exactly one thing and doesn't know about the others.
This is the same as MVC in web dev: Model (GameState) ← Controller (tick) ← View (render).

---

## Exercise

The frog can walk off the left/right edges before the out-of-bounds kill fires.
Add explicit clamping after the input section in `frogger_tick`:

```c
if (state->frog_x < 0.0f)  state->frog_x = 0.0f;
if (state->frog_x > 15.0f) state->frog_x = 15.0f;
if (state->frog_y < 0.0f)  state->frog_y = 0.0f;
if (state->frog_y > 9.0f)  state->frog_y = 9.0f;
```

Rebuild and verify: the frog can no longer hop off the top/bottom/sides.
Think about whether this is better UX than the "death on out-of-bounds" approach.
