# Lesson 11 — Collision Detection + Asteroid Splitting

## By the end of this lesson you will have:
Bullets that destroy asteroids. A large asteroid (radius 64) splits into two medium ones (radius 32); a medium splits into two small ones (radius 16); a small disappears entirely. The score increases with each hit: smaller asteroids that required more shots are worth proportionally more points.

---

## Step 1 — Circle collision: the squared-distance optimisation

**Why circles?** Asteroids are irregularly shaped polygons, but testing a point against a circle (radius = asteroid's `size` field) is fast, simple, and close enough to accurate for arcade gameplay.

The naive distance formula between a bullet at `(px, py)` and an asteroid centre at `(cx, cy)` is:

```
distance = sqrt( (px - cx)² + (py - cy)² )
```

If `distance < r`, the bullet is inside the asteroid. **But `sqrt` is slow.** We can avoid it by squaring both sides:

```
(px - cx)² + (py - cy)²  <  r²
```

The inequality holds in the same direction because both sides are non-negative.

**Worked example — hit:**
```
cx = 200, cy = 200, r = 32
px = 220, py = 210

dx = 220 - 200 = 20
dy = 210 - 200 = 10
dx² + dy² = 400 + 100 = 500

r² = 32² = 1024

500 < 1024  →  INSIDE (hit!) ✓
```

**Worked example — miss:**
```
cx = 200, cy = 200, r = 32
px = 240, py = 220

dx = 40, dy = 20
dx² + dy² = 1600 + 400 = 2000

2000 < 1024  →  NOT INSIDE (miss) ✓
```

The function in `asteroids.c`:

```c
static int is_point_inside_circle(float cx, float cy, float r,
                                   float px, float py)
{
    float dx = px - cx, dy = py - cy;
    return (dx * dx + dy * dy) < (r * r);
}
```

Returns `1` (true) or `0` (false) — C has no `bool` type by default, so `int` is used. In JS this would return a `boolean`.

---

## Step 2 — The double loop: bullet × asteroid

We need to test every active bullet against every active asteroid:

```c
for (i = 0; i < state->bullet_count; i++) {
    SpaceObject *b = &state->bullets[i];
    if (!b->active) continue;

    for (j = 0; j < state->asteroid_count; j++) {
        SpaceObject *a = &state->asteroids[j];
        if (!a->active) continue;

        if (is_point_inside_circle(a->x, a->y, (float)a->size, b->x, b->y)) {
            b->active = 0;
            /* ... splitting and scoring ... */
            a->active = 0;
            break; /* one bullet can only hit one asteroid */
        }
    }
}
```

**Why `if (!b->active) continue` at the top of the outer loop?** A bullet that already hit an asteroid in an earlier iteration of the outer loop has `active = 0`. Without this check, it could "ghost-hit" a second asteroid on the same frame. The `break` at the end of the inner loop prevents a single bullet hitting two asteroids in one frame, but the `continue` handles the case where the same dead bullet is encountered again after `compact_pool` hasn't run yet.

**Why not remove objects inside the loop?** If we called `compact_pool` mid-loop, `asteroid_count` would shrink and we'd skip the entry that swapped into the slot we just compacted. Instead: mark inactive, compact *after* both loops finish.

**JS analogy:** This is like calling `array.filter()` on both arrays after the nested loop, rather than splicing during iteration — which would silently skip entries.

---

## Step 3 — Asteroid splitting logic

When a bullet hits an asteroid with `size > SMALL_SIZE` (i.e., `size > 16`):

```c
if (a->size > SMALL_SIZE) {
    int   new_size  = a->size / 2;
    float new_speed = 60.0f;
    float angle1 = ((float)rand() / (float)RAND_MAX) * 2.0f * 3.14159265f;
    float angle2 = ((float)rand() / (float)RAND_MAX) * 2.0f * 3.14159265f;
    add_asteroid(state,
                 a->x, a->y,
                 new_speed * sinf(angle1),
                 new_speed * cosf(angle1),
                 new_size);
    add_asteroid(state,
                 a->x, a->y,
                 new_speed * sinf(angle2),
                 new_speed * cosf(angle2),
                 new_size);
}
```

**Size halving:**

| Hit | `size` before | `size` after (each child) |
|-----|--------------|--------------------------|
| Large hit | 64 | 32 (medium) |
| Medium hit | 32 | 16 (small) |
| Small hit | 16 | — (no children, just removed) |

**Random direction:** `rand() / RAND_MAX` gives a float in `[0, 1)`. Multiplying by `2π` gives an angle in `[0, 2π)`. The two children fly off in completely random directions — not the original direction, which gives the chaotic feel of the original arcade game.

**Why `sinf` for x and `cosf` for y?** The asteroid velocity uses the same convention as bullet velocity: `dx = speed * sin(angle)`, `dy = speed * cos(angle)` (note: *positive* cos for asteroids, since they don't need the y-down ship direction convention — they can go any direction, and both sin and cos together cover the full circle).

`add_asteroid` bounds-checks against `MAX_ASTEROIDS = 64` before writing:

```c
static void add_asteroid(GameState *state,
                          float x, float y,
                          float dx, float dy,
                          int size)
{
    if (state->asteroid_count >= MAX_ASTEROIDS) return;
    SpaceObject *a = &state->asteroids[state->asteroid_count++];
    a->x      = x;
    a->y      = y;
    a->dx     = dx;
    a->dy     = dy;
    a->angle  = 0.0f;
    a->timer  = 0.0f;
    a->size   = size;
    a->active = 1;
}
```

---

## Step 4 — Score calculation

```c
state->score += (LARGE_SIZE / a->size) * 100;
```

`LARGE_SIZE = 64`. The score depends on the asteroid's size *before* splitting:

| Asteroid size | `64 / size` | Points |
|---------------|-------------|--------|
| Large (64)    | `64 / 64 = 1` | 100 |
| Medium (32)   | `64 / 32 = 2` | 200 |
| Small (16)    | `64 / 16 = 4` | 400 |

Smaller asteroids are harder to hit and worth more — a classic arcade incentive. The formula is integer division in C: `64 / 64 = 1` exactly (no rounding needed here since all sizes are powers of two that divide evenly into 64).

**JS analogy:** `Math.floor(64 / size) * 100` — but because `size` is always 16, 32, or 64, the division is always exact so `Math.floor` is irrelevant.

---

## Step 5 — `compact_pool` after the collision loop

After the nested bullet × asteroid loop, we call `compact_pool` on **both** pools:

```c
compact_pool(state->bullets,   &state->bullet_count);
compact_pool(state->asteroids, &state->asteroid_count);
```

This handles:
- Bullets consumed by hits
- Asteroids destroyed by hits
- Newly added child asteroids from splits are *always* `active = 1`, so they survive compaction

**Order matters:** We compact bullets first, then asteroids. Either order is correct here, but being explicit is good practice — some future change might make the order matter.

---

## Build & Run

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11 -lX11 -lxkbcommon -lGL -lGLX -lm
./build/asteroids_x11
```

**What you should see:** Shoot a large yellow asteroid — it splits into two medium ones that fly off in random directions. Shoot a medium one — it splits into two small ones. Shoot a small one — it vanishes. The "SCORE: 0" HUD counter updates with each hit: 100 for large, 200 for medium, 400 for small.

---

## Key Concepts

- **Circle collision without `sqrt`** — compare squared distances: `dx*dx + dy*dy < r*r`.
- **Mark-then-compact** — set `active = 0` inside loops; call `compact_pool` after all loops finish. Never modify the pool mid-iteration.
- **Splitting tree** — `size / 2` per generation; `size > SMALL_SIZE` guard prevents infinite sub-division.
- **Random angle** — `(float)rand() / (float)RAND_MAX * 2π` gives a uniform random direction.
- **Inverse-size scoring** — `(LARGE_SIZE / size) * 100` rewards hitting smaller, harder targets more.
- **`MAX_ASTEROIDS` guard** — splitting can at most double the count per wave; 64 slots is safely above any realistic maximum.
- **Integer division** — `64 / size` in C truncates toward zero; since all valid sizes divide 64 exactly, no rounding error occurs.

---

## Exercise

After `compact_pool`, add a "chain reaction" effect: when any asteroid is destroyed, give all *remaining* asteroids a small speed boost:

```c
for (int k = 0; k < state->asteroid_count; k++) {
    state->asteroids[k].dx *= 1.05f;
    state->asteroids[k].dy *= 1.05f;
}
```

Does this make the game harder? What happens after several chain reactions? Think about whether you'd want to cap the speed with an `if (speed > MAX_SPEED)` check, and how you'd compute `speed` from `dx`/`dy`.
