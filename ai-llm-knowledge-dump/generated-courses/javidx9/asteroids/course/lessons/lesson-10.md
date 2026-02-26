# Lesson 10 — Bullets

## By the end of this lesson you will have:
A working fire mechanic: pressing Space spawns a white dot at the ship's nose that travels at 300 px/s in the direction the ship faces, wraps around the screen, and vanishes after 2.5 seconds. Holding Space does *not* rapid-fire — each press fires exactly one bullet.

---

## Step 1 — "Just pressed" vs "held": why the distinction matters

**The problem:** If we fire a bullet every frame the Space key is down at 60 Hz, the player fires 60 bullets per second and instantly floods the bullet pool. We want one bullet per *key press*, no matter how long the key is held.

**JS analogy:** This is the difference between listening to `keydown` (fires once per OS repeat, roughly 10× per second) vs listening to `keypress` once per physical press. Our `GameButtonState` captures this distinction more precisely.

The check in `asteroids_update`:

```c
if (input->fire.ended_down && input->fire.half_transition_count > 0) {
```

- `ended_down == 1` — the key is currently down at the end of this frame.
- `half_transition_count > 0` — the key *changed state* at least once this frame, meaning it was just pressed (not held from a previous frame).

If the player has held Space since the last frame, `ended_down` is still 1 but `half_transition_count` is 0 (no new events) — the condition is false, so no bullet fires.

`prepare_input_frame` (called at the top of every loop iteration) resets `half_transition_count` to zero before new events are processed:

```c
void prepare_input_frame(GameInput *input) {
    input->left.half_transition_count  = 0;
    input->right.half_transition_count = 0;
    input->up.half_transition_count    = 0;
    input->fire.half_transition_count  = 0;
    input->quit                        = 0;
}
```

This is why `ended_down` (the "is it held" state) persists across frames but `half_transition_count` (the "did it change this frame" counter) resets every frame.

---

## Step 2 — Computing the nose position

**Why:** We don't want the bullet to spawn at the ship's centre — it should emerge from the tip (nose) of the triangle. The ship model's nose vertex is `(0, -5)` in model space; after `draw_wireframe` scales by `SHIP_SCALE = 10`, that becomes 50 pixels above the centre in world space.

We replicate that transform in `asteroids_update`:

```c
float nose_x = state->player.x + sinf(state->player.angle) * SHIP_SCALE * 5.0f;
float nose_y = state->player.y - cosf(state->player.angle) * SHIP_SCALE * 5.0f;
```

`SHIP_SCALE * 5.0f = 10 * 5 = 50` pixels. The model nose is at model-y = −5; after scale × 10 that is −50 pixels. On a y-down screen, −50 pixels in the "up" direction is `−cosf(angle) * 50`.

**Worked example — ship pointing up (angle = 0), position (400, 300):**
```
nose_x = 400 + sin(0) * 50  = 400 + 0   = 400
nose_y = 300 - cos(0) * 50  = 300 - 50  = 250   ← 50px above centre ✓
```

**Worked example — ship pointing right (angle = π/2), position (400, 300):**
```
nose_x = 400 + sin(π/2) * 50  = 400 + 50  = 450
nose_y = 300 - cos(π/2) * 50  = 300 - 0   = 300   ← 50px right of centre ✓
```

---

## Step 3 — `add_bullet`: velocity from angle

`add_bullet` receives the nose position and the ship's current angle, then computes the bullet velocity using the same sin/cos direction formula from Lesson 08:

```c
static void add_bullet(GameState *state, float x, float y, float angle)
{
    if (state->bullet_count >= MAX_BULLETS) return;
    SpaceObject *b = &state->bullets[state->bullet_count++];
    b->x      = x;
    b->y      = y;
    b->dx     = BULLET_SPEED * sinf(angle);
    b->dy     = -BULLET_SPEED * cosf(angle); /* -cos: angle 0 = up (y-down screen) */
    b->angle  = angle;
    b->timer  = BULLET_LIFE;
    b->size   = 0;
    b->active = 1;
}
```

`BULLET_SPEED = 300.0f` px/s. At 60 Hz the bullet moves `300 × 0.016 = 4.8` pixels per frame.

**Worked example — angle = 0 (pointing up):**
```
dx = 300 * sin(0)  =  0    px/s
dy = -300 * cos(0) = -300  px/s   ← moves upward (negative y)
```

**Worked example — angle = π/4 (pointing upper-right, 45°):**
```
dx = 300 * sin(π/4) =  300 * 0.707 = +212.1  px/s
dy = -300 * cos(π/4) = -300 * 0.707 = -212.1  px/s
```

`b->size = 0` — bullets have no radius for collision purposes; they're a single pixel.

The guard `if (state->bullet_count >= MAX_BULLETS) return;` prevents writing past the end of the array. `MAX_BULLETS = 32` is enough because fire rate is one-per-press.

---

## Step 4 — Bullet update loop and lifetime countdown

Every frame, active bullets are moved and their lifetime timer is decremented:

```c
for (i = 0; i < state->bullet_count; i++) {
    SpaceObject *b = &state->bullets[i];
    b->x     += b->dx * dt;
    b->y     += b->dy * dt;
    b->timer -= dt;
    wrap_coordinates(&b->x, &b->y);

    if (b->timer <= 0.0f)
        b->active = 0;
}
compact_pool(state->bullets, &state->bullet_count);
```

`BULLET_LIFE = 2.5f` seconds. At 300 px/s, a bullet travels `300 × 2.5 = 750` pixels before expiring — just under one full screen width.

When `timer` reaches zero, `active` is set to 0 (not removed immediately — that would shift indices mid-loop). After the loop, `compact_pool` removes all inactive bullets.

**JS analogy:** This is like filtering an array — but we don't create a new array. `compact_pool` does an in-place compaction by swapping each dead entry with the last live entry, keeping the array dense.

---

## Step 5 — `compact_pool`: in-place removal without shifting

```c
static void compact_pool(SpaceObject *pool, int *count) {
    int i = 0;
    while (i < *count) {
        if (!pool[i].active) {
            pool[i] = pool[*count - 1]; /* overwrite dead slot with last */
            (*count)--;
        } else {
            i++;
        }
    }
}
```

**Before (count = 4, X = inactive):**
```
Index: [ 0 | 1 | 2 | 3 ]
Data:  [ A | B | X | C ]
```

**Step — slot 2 is inactive → copy slot 3 here, count = 3:**
```
Index: [ 0 | 1 | 2 ]
Data:  [ A | B | C ]   ← X is gone; C moved to slot 2
```

No memory allocation. No shifting. O(n) over the whole array, but each removal is effectively O(1) because we don't shift — we just swap with the tail.

**Why not use `remove_if` or `filter`?** In C there's no garbage collector and no dynamic array (unless you bring one in). A fixed-size pool with `compact_pool` gives the same logical result with zero heap traffic in the game loop hot path.

---

## Build & Run

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11 -lX11 -lxkbcommon -lGL -lGLX -lm
./build/asteroids_x11
```

**What you should see:** Press Space once — one white pixel fires from the nose at 300 px/s in the direction the ship is facing. Press and hold Space — only one bullet fires. Each bullet disappears after 2.5 seconds. Bullets wrap around screen edges just like the ship.

---

## Key Concepts

- **"Just pressed" detection** — `ended_down && half_transition_count > 0` fires once per physical key press, not once per frame.
- **`prepare_input_frame`** — resets `half_transition_count` to zero before polling; `ended_down` persists.
- **Nose position** — `SHIP_SCALE * model_nose_y` gives the offset in pixels; same sin/cos transform as thrust.
- **Bullet velocity** — `dx = BULLET_SPEED * sin(angle)`, `dy = -BULLET_SPEED * cos(angle)` (y-down convention).
- **`timer` countdown** — `timer -= dt` each frame; `active = 0` when expired, removed by `compact_pool`.
- **`compact_pool`** — swap-dead-with-last keeps the array dense without shifting or heap allocation.
- **`MAX_BULLETS` guard** — `if (count >= MAX) return;` prevents array overrun; a hard upper limit is safer than dynamic allocation in a game loop.

---

## Exercise

Add a small "ship velocity bonus" to bullets: when firing, add the player's current `dx`/`dy` to the bullet's velocity:

```c
b->dx = BULLET_SPEED * sinf(angle) + state->player.dx;
b->dy = -BULLET_SPEED * cosf(angle) + state->player.dy;
```

This is physically correct (momentum conservation). Does it make the game easier or harder? Does the bullet lifetime need to change? Try `BULLET_LIFE = 1.5f` and observe the effect.
