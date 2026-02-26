# Lesson 10 — Polish: Bounds, Homes, and Win State

**By the end of this lesson you will have:**
A complete, playable Frogger. The frog can't walk off the edges, reaching a home
tile increments a counter, and a "YOU WIN" message appears when all home slots
are filled. The game is feature-complete.

---

## What's left to polish?

The game already works from Lesson 9 — you can die and reset. This lesson
adds the remaining features that make it feel like a real game:

1. **Edge clamping** — frog can't hop off the left/right edges of the screen
2. **Home detection** — landing on row 0, tile 'h' counts as a win
3. **Win condition** — reaching a certain number of homes ends the game
4. **HUD** — homes counter displayed on screen (already in platform_render)

---

## Step 1 — Read the win detection in `frogger_tick`

```c
/* Win detection: if frog reaches row 0 on a home tile */
if (fy_int == 0) {
    int pattern_pos = (int)(state->frog_x + 0.5f) % LANE_PATTERN_LEN;
    if (pattern_pos < 0)
        pattern_pos += LANE_PATTERN_LEN;
    char c = lane_patterns[0][pattern_pos % LANE_PATTERN_LEN];
    if (c == 'h') {
        state->homes_reached++;
        state->frog_x = 8.0f;
        state->frog_y = 9.0f;
    }
}
```

**The `+ 0.5f` trick:**
`frog_x` is a float. `(int)(8.4f) = 8` and `(int)(8.6f) = 8` — both truncate to 8.
Adding 0.5 first: `(int)(8.4f + 0.5f) = 8`, `(int)(8.6f + 0.5f) = 9`.
This rounds to the nearest integer instead of always truncating down.

**Why `% LANE_PATTERN_LEN`?**
The pattern is 64 tiles. `frog_x` could in theory exceed 63 (though the screen
is only 16 tiles wide and clamping prevents this). The modulo ensures we don't
read out of bounds.

**Home tile layout in lane 0:**
```
"wwwhhwwwhhwwwhhwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww"
 0123456789...

Position 3: 'h'  ← home slot 1
Position 4: 'h'  ← home slot 1 (2 tiles wide)
Position 9: 'h'  ← home slot 2
...
```

---

## Step 2 — Edge handling: pixel-bounds kill

The frog does NOT get clamped to the screen. `frogger_tick` kills it the moment
any part of the sprite leaves the window — this is how the river "sweeps you off
the edge" mechanic works:

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

`frog_px_x + TILE_PX > SCREEN_PX_W` means the *right edge* of the frog sprite
has passed the window boundary. This fires the moment the frog starts to leave.

**Why pixel-level, not tile-level?**
An integer tile check (`fx >= 16`) only fires when `frog_x >= 16.0` — a full
tile (64px) off-screen. The river can push the frog visually out of the window
for an entire tile-width before the tile check fires. The pixel check is immediate.

**Exercise — try clamping instead:**
Replace the death block with clamps and feel the difference:
```c
if (state->frog_x < 0.0f)  state->frog_x = 0.0f;
if (state->frog_x > 15.0f) state->frog_x = 15.0f;
if (state->frog_y < 0.0f)  state->frog_y = 0.0f;
if (state->frog_y > 9.0f)  state->frog_y = 9.0f;
```
The frog sticks at the edge instead of dying. Revert when done — river-edge
death is intentional game design.

---

## Step 3 — Win condition

Currently `homes_reached` just counts up. Add a win condition.
The top row (lane 0) has 3 home slots (each 2 tiles wide): positions 3-4, 9-10, 15-16.
So maximum homes = 3 (but you can count them multiple times in this simplified version).

Add to `frogger_tick`, after homes_reached is incremented:

```c
if (c == 'h') {
    state->homes_reached++;
    state->frog_x = 8.0f;
    state->frog_y = 9.0f;
    /* Optional: add a win flash when homes_reached reaches a target */
}
```

In `platform_render`, the HUD already shows the count:
```c
/* Raylib: */
snprintf(score_buf, sizeof(score_buf), "Homes: %d", state->homes_reached);
DrawText(score_buf, 8, 8, 16, WHITE);

/* X11: */
XDrawString(g_display, g_backbuffer, g_gc, 8, 20, score_buf, strlen(score_buf));
```

Add a WIN message (3 homes = win):
In `platform_render`, after the score line:

**Raylib version:**
```c
if (state->homes_reached >= 3) {
    DrawText("YOU WIN!", SCREEN_PX_W/2 - 60, SCREEN_PX_H/2 - 20, 32, YELLOW);
}
```

**X11 version:**
```c
if (state->homes_reached >= 3) {
    XSetForeground(g_display, g_gc, 0xFFFF00); /* yellow */
    const char *win = "YOU WIN!";
    XDrawString(g_display, g_backbuffer, g_gc,
        SCREEN_PX_W/2 - 30, SCREEN_PX_H/2, win, strlen(win));
}
```

---

## Step 4 — DOD summary: what lives where

Now that the game is complete, let's review the data layout:

```
GameState (flat struct, ~34 KB, no pointers):
  frog_x, frog_y  — position (2 floats, 8 bytes)
  time            — timer (1 float, 4 bytes)
  homes_reached   — score (int, 4 bytes)
  dead, dead_timer — death state (int + float, 8 bytes)
  danger[128*80]  — collision map (10,240 bytes)
  sprites         — all sprite data (24 KB)

Read-only globals (in .rodata, not in GameState):
  lane_speeds[10]          — 40 bytes
  lane_patterns[10][65]    — 650 bytes

Separation of concerns:
  frogger_tick()       — ONLY writes to GameState, ONLY reads input
  platform_render()    — ONLY reads GameState (const *), ONLY draws
  platform_get_input() — ONLY reads OS events, ONLY writes InputState
```

This is the core of the Handmade Hero philosophy: know exactly what data you
have, where it lives, and which function is allowed to touch it.

---

## Build & Run

```sh
./build_x11.sh && ./frogger_x11
```

**What you should see:**
- Complete playable Frogger
- Arrow keys move the frog
- Cars and buses kill the frog
- Water kills the frog, logs carry it
- Reaching a home tile increments "Homes: N"
- At 3 homes, "YOU WIN!" appears

---

## Mental Model

Data flows in one direction, every frame:

```
OS / Hardware
     ↓  (events)
platform_get_input() → InputState (snapshot, lives one frame)
     ↓
frogger_tick(state, input, dt) → mutates GameState
     ↓
platform_render(const state) → pixels on screen
     ↓
human eyes → human brain → fingers → OS / Hardware
```

`GameState` is the single source of truth. If you want to add a feature (lives,
timer, multiplayer), you add it to `GameState` and handle it in `frogger_tick`.
The platform code doesn't need to change.

---

## Final Exercise — Add Lives

Add a `lives` field to `GameState` in `frogger.h`:

```c
int lives;   /* starts at 3 */
```

In `frogger_init`, set `state->lives = 3`.

In `frogger_tick`, when the frog dies (sets `state->dead = 1`), decrement lives:
```c
state->dead = 1;
state->dead_timer = 0.4f;
state->lives--;
if (state->lives <= 0) {
    /* Game over — reset everything */
    state->lives = 3;
    state->homes_reached = 0;
    state->frog_x = 8.0f;
    state->frog_y = 9.0f;
}
```

In `platform_render`, display lives next to the homes counter.

This is a real feature addition with zero changes to platform code — only
`frogger.h` (data), `frogger.c` (logic), and a one-line UI change in each
platform render. That's the power of clean separation.

---

## What you've built

From a C++ OOP console engine demo, you've built:

- A fully playable Frogger in plain C
- Two platform backends (X11 and Raylib) from one game logic file
- A binary .spr file loader
- A time-based scrolling system
- A flat danger buffer for O(1) collision detection
- Data-oriented game state with no heap allocation

The same patterns — flat state, separate arrays, isolated update/render —
are what systems like the Unity Job System, Unreal's ECS, and id Software's
Doom/Quake engines use at their core. You now understand why.
