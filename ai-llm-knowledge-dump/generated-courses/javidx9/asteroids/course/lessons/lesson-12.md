# Lesson 12 — Death + Reset State Machine

## By the end of this lesson you will have:
A death sequence: touching an asteroid freezes the game, flashes the ship red at 10 Hz for 1.5 seconds over a translucent red overlay showing "GAME OVER", then automatically resets everything and restarts. The player's score resets but the asteroid models (the jagged polygon shapes) are preserved.

---

## Step 1 — The `GAME_PHASE` enum

**Why an enum?** The game has two distinct modes of operation: normal play and the death animation. Using a plain `int` with magic numbers like `0` and `1` would work, but it makes code opaque. A `typedef enum` gives each state a readable name and prevents accidentally passing an arbitrary integer where a phase is expected.

```c
typedef enum {
    PHASE_PLAYING,
    PHASE_DEAD
} GAME_PHASE;
```

`PHASE_PLAYING = 0` and `PHASE_DEAD = 1` by default (C enums are integers starting from 0). The `typedef` means we can write `GAME_PHASE phase;` instead of `enum GAME_PHASE phase;` — a minor convenience that makes the type feel like a first-class citizen.

**JS analogy:**
```js
const PHASE = Object.freeze({ PLAYING: 'playing', DEAD: 'dead' });
```
In C we don't have `Object.freeze` but a `typedef enum` achieves the same intent: a closed set of named values.

In `GameState`:
```c
GAME_PHASE  phase;
float       dead_timer;
```

---

## Step 2 — Player ↔ Asteroid collision triggers death

After the bullet-asteroid loop and the asteroid position updates, we check whether the player's centre is inside any asteroid circle:

```c
for (i = 0; i < state->asteroid_count; i++) {
    SpaceObject *a = &state->asteroids[i];
    if (is_point_inside_circle(a->x, a->y, (float)a->size,
                               state->player.x, state->player.y)) {
        state->phase     = PHASE_DEAD;
        state->dead_timer = DEAD_ANIM_TIME;
        return;
    }
}
```

`DEAD_ANIM_TIME = 1.5f` seconds. The `return` exits `asteroids_update` immediately — no more physics, no shooting, no new asteroid movement this frame. The game is frozen in the dead state.

**Why `return` and not `break`?** `break` would only exit the `for` loop; the rest of `asteroids_update` (level-clear detection, etc.) would still run. `return` stops everything.

---

## Step 3 — PHASE_DEAD: draining the timer

At the very top of `asteroids_update`, before any game logic runs, we check the phase:

```c
if (state->phase == PHASE_DEAD) {
    state->dead_timer -= dt;
    if (state->dead_timer <= 0.0f) {
        reset_game(state);
    }
    return; /* freeze all game logic during death animation */
}
```

Each frame, `dead_timer` decreases by `dt`. At 60 Hz:
```
Start: dead_timer = 1.5 s
After 1 frame (dt=0.016):  1.5 - 0.016 = 1.484 s
After 90 frames (~1.5 s):  ≈ 0.0 s  → reset_game called
```

When the timer reaches zero, `reset_game` is called and the game restarts. The `return` after the block guarantees that no movement, collision, or input is processed while dead.

---

## Step 4 — `reset_game`: what resets and what doesn't

```c
static void reset_game(GameState *state) {
    state->player.x      = SCREEN_W / 2.0f;
    state->player.y      = SCREEN_H / 2.0f;
    state->player.dx     = 0.0f;
    state->player.dy     = 0.0f;
    state->player.angle  = 0.0f;
    state->player.timer  = 0.0f;
    state->player.size   = 0;
    state->player.active = 1;

    state->asteroid_count = 0;
    state->bullet_count   = 0;

    add_asteroid(state, 100.0f, 100.0f,  40.0f, -30.0f, LARGE_SIZE);
    add_asteroid(state, 500.0f, 100.0f, -25.0f,  15.0f, LARGE_SIZE);

    state->score     = 0;
    state->phase     = PHASE_PLAYING;
    state->dead_timer = 0.0f;
}
```

**What resets:**
- Player position, velocity, angle — back to centre, stationary, pointing up.
- Both object pools — `asteroid_count = 0` and `bullet_count = 0` effectively clears them (the old data is still in the array but will never be read because `count` says there's nothing there).
- Score — back to zero.
- Phase — back to `PHASE_PLAYING`.

**What does NOT reset:**
- `ship_model` and `asteroid_model` — these are set once in `asteroids_init` and never touched again. The jagged asteroid shape stays the same across deaths. This is intentional: `asteroids_init` is only called once (from `main`), and `reset_game` is the "restart" path.

**JS analogy:** Like resetting a Redux store to its initial state — you rebuild the mutable data, but constants (like the static model geometry) are untouched.

---

## Step 5 — Death flash: the 10 Hz blink formula

During `PHASE_DEAD`, the ship blinks red at roughly 10 Hz. Here is the render code:

```c
} else {
    /* Death flash: blink at 10Hz using dead_timer */
    int flash = (int)(state->dead_timer / 0.05f);
    if (flash % 2 == 0) {
        draw_wireframe(bb,
                       state->ship_model, SHIP_VERTS,
                       state->player.x, state->player.y,
                       state->player.angle, SHIP_SCALE,
                       COLOR_RED);
    }
}
```

**How the formula works — step by step:**

`dead_timer` starts at 1.5 and counts down to 0.

```
dead_timer = 1.5 → (int)(1.5 / 0.05) = (int)30 = 30  → 30 % 2 = 0 → DRAW red
dead_timer = 1.45 → (int)(1.45 / 0.05) = (int)29 = 29 → 29 % 2 = 1 → SKIP
dead_timer = 1.4 → (int)28 = 28 → 28 % 2 = 0 → DRAW red
...
```

Every 0.05 seconds the integer changes by 1, toggling the `% 2` result. That's 1/0.05 = **20 toggles per second** — the ship flips between visible and invisible 20 times per second, so you see 10 full on/off cycles per second = **10 Hz**.

**JS analogy:**
```js
const flash = Math.floor(deadTimer / 0.05);
const visible = flash % 2 === 0;
```

The key insight is using `(int)(timer / interval)` to convert a continuous timer into a discrete step count, then `% 2` to toggle between two states.

---

## Step 6 — `draw_rect_blend`: the red overlay

During death, a translucent red rectangle covers the entire screen:

```c
if (state->phase == PHASE_DEAD) {
    draw_rect_blend(bb, 0, 0, bb->width, bb->height,
                    ASTEROIDS_RGBA(160, 0, 0, 80));
    draw_text(bb,
              bb->width / 2 - 42,
              bb->height / 2 - 7,
              "GAME OVER", 2, COLOR_WHITE);
}
```

`ASTEROIDS_RGBA(160, 0, 0, 80)` — a dark red with alpha 80 (out of 255, so roughly 31% opacity).

The blend formula in `draw_rect_blend`:

```
out = src * alpha/255 + dst * (255 - alpha)/255
```

**Worked example — blending red over a white pixel:**
```
src = (160, 0, 0),  alpha = 80
dst = (255, 255, 255)  (white)

out.r = (160 * 80 + 255 * 175) / 255 = (12800 + 44625) / 255 = 57425 / 255 ≈ 225
out.g = (0   * 80 + 255 * 175) / 255 = 44625 / 255 ≈ 175
out.b = (0   * 80 + 255 * 175) / 255 ≈ 175
→ (225, 175, 175)  — a pinkish tint
```

The overlay is drawn *after* the asteroid and ship render passes so it appears on top of everything.

---

## Build & Run

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11 -lX11 -lxkbcommon -lGL -lGLX -lm
./build/asteroids_x11
```

**What you should see:** Steer the ship into an asteroid. The game immediately freezes, a red overlay appears with "GAME OVER" in the centre, and the ship blinks red rapidly. After 1.5 seconds everything resets: ship back to centre, two fresh large asteroids, score back to zero.

---

## Key Concepts

- **`GAME_PHASE` enum** — a named, closed set of integer constants; prevents magic-number bugs when comparing phase state.
- **Early `return`** — exiting `asteroids_update` at the top of the dead phase skips all game logic cleanly.
- **Timer drain** — `timer -= dt` per frame; when `<= 0`, trigger the next state. No OS timers, no callbacks, no threads.
- **`reset_game` ≠ `asteroids_init`** — `init` builds models and seeds RNG once; `reset` only restores mutable game state. They are deliberately separate.
- **Blink formula** — `(int)(timer / interval) % 2` converts a countdown into a toggling boolean at a fixed frequency.
- **Alpha blending** — `out = src * a + dst * (1 - a)` is standard "over" compositing; integer version divides by 255.

---

## Exercise

Change `DEAD_ANIM_TIME` to `3.0f` and `0.025f` in the flash formula (replacing `0.05f`):

```c
int flash = (int)(state->dead_timer / 0.025f);
```

What frequency does the ship now blink at? (Calculate: 1 / 0.025 / 2 = ?) Does the faster blink feel more or less alarming? Revert to the original values afterwards.
