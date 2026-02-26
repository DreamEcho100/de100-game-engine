# Lesson 07 — SpaceObject + Entity Pools

## By the end of this lesson you will have:
A working entity system: asteroids split when shot, bullets expire after 2.5 seconds, and destroyed objects are removed from the pool without shifting the array. You will see bullets disappear after flying across the screen and asteroids split into two smaller pieces.

---

## Step 1 — The SpaceObject struct

```c
typedef struct {
    float x, y;    /* world position (pixels) */
    float dx, dy;  /* velocity (pixels/sec)   */
    float angle;   /* heading / rotation (radians) */
    float timer;   /* bullet: lifetime remaining; asteroid: unused */
    int   size;    /* asteroid: radius; bullet: 0                  */
    int   active;  /* 1 = alive, 0 = dead (triggers compact_pool)  */
} SpaceObject;
```

One struct covers both asteroids and bullets. The fields mean different things depending on usage:

| Field   | Asteroid                        | Bullet                          |
|---------|---------------------------------|---------------------------------|
| `x, y`  | world position                  | world position                  |
| `dx,dy` | drift velocity (px/sec)         | travel velocity (px/sec)        |
| `angle` | visual rotation (for rendering) | heading angle (for velocity)    |
| `timer` | unused (always 0)               | lifetime remaining (seconds)    |
| `size`  | radius (16, 32, or 64)          | 0 (just a dot)                  |
| `active`| 1 = alive, 0 = destroyed        | 1 = alive, 0 = expired          |

**Why one struct for two different object types?** In this game, bullets and asteroids are nearly identical in their physical simulation (position, velocity, wrapping). Sharing one struct avoids duplicating the update loop. If the differences grew larger (bullets had 10 unique fields, asteroids had 10 different ones), you'd split them.

**JS analogy:** Like using one TypeScript interface for both and narrowing with a discriminant field. Here `size === 0` identifies bullets.

---

## Step 2 — Fixed C arrays vs dynamic vectors: why no heap in the hot path

```c
#define MAX_ASTEROIDS  64
#define MAX_BULLETS    32

typedef struct {
    SpaceObject asteroids[MAX_ASTEROIDS];
    int         asteroid_count;
    SpaceObject bullets[MAX_BULLETS];
    int         bullet_count;
    /* ... */
} GameState;
```

`GameState` is allocated once (as a stack variable in `main`). The asteroid and bullet arrays live inside it — no `malloc` at object creation time.

**Why not use a dynamically-sized array (like a JS array or a C++ `std::vector`)?**

1. **No allocation in the game loop hot path.** Every frame runs update + render in under 16 ms. A `malloc`/`free` call can take 1–10 µs and is non-deterministic (the allocator may need to coalesce blocks). A fixed array has O(1) predictable access.

2. **No fragmentation.** A game that runs for 10 minutes at 60 fps processes 36,000 frames. Dynamic allocation across 36,000 frames can fragment the heap, causing allocation latency to grow over time.

3. **The pool size is knowable.** Large asteroid (1) → 2 medium (2) → 4 small (4): one original asteroid spawns at most 7 total. Starting with 2 large = at most 14. `MAX_ASTEROIDS = 64` gives plenty of headroom. 

4. **Cache efficiency.** `SpaceObject asteroids[64]` is a single contiguous block of memory. Iterating it accesses memory sequentially — cache-friendly. A linked list or heap-allocated objects scattered around memory would cause cache misses.

**JS analogy:** The difference between `const items = new Array(64).fill(null)` (pre-allocated, contiguous) vs `const items = []` with `.push()/.splice()` (dynamic, potentially scattered). In a game loop you want the former.

---

## Step 3 — The active flag pattern

When an object should be removed (bullet expires, asteroid is shot), we don't immediately erase it from the array. Instead we set its `active` flag to 0:

```c
/* Expire bullet when its lifetime runs out */
if (b->timer <= 0.0f)
    b->active = 0;

/* Hit! Remove bullet and asteroid */
b->active = 0;
a->active = 0;
```

We then call `compact_pool` once **after** the loop, not during it.

**Why not remove immediately during iteration?**

If you remove slot [2] by shifting [3]→[2], [4]→[3], etc., then the loop index `i` now points at what was [3] — you just skipped it. You'd need to decrement `i` after every removal, and the logic becomes error-prone.

The flag approach keeps the loop index stable during iteration. All removal happens in one clean pass afterward.

---

## Step 4 — compact_pool: swap-with-last

```c
static void compact_pool(SpaceObject *pool, int *count) {
    int i = 0;
    while (i < *count) {
        if (!pool[i].active) {
            pool[i] = pool[*count - 1]; /* overwrite with last */
            (*count)--;
        } else {
            i++;
        }
    }
}
```

This is the "swap-with-last" removal strategy. Let's trace it with a real example.

**Before:**
```
Index:  [ 0 ] [ 1 ] [ 2 ] [ 3 ] [ 4 ]
Active:   A     A     X     A     A      count=5
```
(X = inactive, A = active)

**Step i=0:** pool[0].active=1 → i++ → i=1  
**Step i=1:** pool[1].active=1 → i++ → i=2  
**Step i=2:** pool[2].active=0 →  
- `pool[2] = pool[4]` (copy last active entry over the dead slot)  
- `count--` → count=4  
- Do NOT increment i (the slot we just filled might itself be inactive)

**After copy:**
```
Index:  [ 0 ] [ 1 ] [ 2 ] [ 3 ] [ . ]
Active:   A     A     A     A          count=4
```

**Step i=2 again:** pool[2].active=1 (it's now the copy of what was [4]) → i++ → i=3  
**Step i=3:** pool[3].active=1 → i++ → i=4  
**i=4 == count=4** → loop ends ✓

**What if two adjacent slots are inactive?**

```
Before: [ A | X | X | A | A ]  count=5
```

i=0: skip (active)  
i=1: pool[1] inactive → copy pool[4] here → count=4 → don't increment  
```
[ A | A | X | A | . ]  count=4
```
i=1: pool[1] now active (was pool[4]) → i++  
i=2: pool[2] inactive → copy pool[3] here → count=3 → don't increment  
```
[ A | A | A | . | . ]  count=3
```
i=2: pool[2] now active → i++  
i=3 == count=3 → done ✓

**Order is not preserved.** The last element may be moved to fill a gap. This is intentional — asteroids and bullets don't have a meaningful "order" and the order changes every frame anyway.

**Complexity:** O(n) per call, where n is the current pool size. One pass through the array regardless of how many items are removed.

---

## Step 5 — add_asteroid and add_bullet helpers

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

`state->asteroid_count++` — post-increment: first use the current count as the index, then increment it. So if count is 2, we write to `asteroids[2]` and count becomes 3.

The guard `if (state->asteroid_count >= MAX_ASTEROIDS) return;` prevents writing past the end of the array. In JavaScript you'd never worry about this because arrays resize automatically — in C, writing past the end is undefined behavior (typically a crash or memory corruption).

```c
static void add_bullet(GameState *state, float x, float y,
                       float angle)
{
    if (state->bullet_count >= MAX_BULLETS) return;
    SpaceObject *b = &state->bullets[state->bullet_count++];
    b->x      = x;
    b->y      = y;
    b->dx     = BULLET_SPEED * sinf(angle);
    b->dy     = -BULLET_SPEED * cosf(angle);
    b->angle  = angle;
    b->timer  = BULLET_LIFE;
    b->size   = 0;
    b->active = 1;
}
```

`b->dy = -BULLET_SPEED * cosf(angle)` — the negative sign is because angle 0 means "pointing up" (toward negative y in screen coordinates). `cos(0) = 1`, so without the `-`, a bullet fired upward would move down. The `-` flips it.

**Worked example — firing at angle 0 (pointing up):**
```
dx = 300 * sin(0) = 300 * 0.0   =   0.0  (no horizontal movement)
dy = -300 * cos(0) = -300 * 1.0 = -300.0 (moving up = decreasing y) ✓
```

**Worked example — firing at angle π/2 (pointing right):**
```
dx = 300 * sin(π/2) = 300 * 1.0   = 300.0  (moving right) ✓
dy = -300 * cos(π/2) = -300 * 0.0 =   0.0  (no vertical movement) ✓
```

---

## Step 6 — The full update flow using pools

In `asteroids_update`, the pool lifecycle per frame is:

```c
/* 1. Move bullets, decrement timers, mark expired */
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

/* 2. Move asteroids */
for (i = 0; i < state->asteroid_count; i++) {
    SpaceObject *a = &state->asteroids[i];
    a->x     += a->dx * dt;
    a->y     += a->dy * dt;
    a->angle += 0.5f * dt;
    wrap_coordinates(&a->x, &a->y);
}

/* 3. Bullet ↔ Asteroid collision: mark inactive, don't modify pools yet */
for (i = 0; i < state->bullet_count; i++) {
    SpaceObject *b = &state->bullets[i];
    if (!b->active) continue;
    for (j = 0; j < state->asteroid_count; j++) {
        SpaceObject *a = &state->asteroids[j];
        if (!a->active) continue;
        if (is_point_inside_circle(a->x, a->y, (float)a->size, b->x, b->y)) {
            b->active = 0;
            if (a->size > SMALL_SIZE) {
                /* spawn two smaller fragments */
                int new_size = a->size / 2;
                float new_speed = 60.0f;
                float angle1 = ((float)rand() / (float)RAND_MAX) * 2.0f * 3.14159265f;
                float angle2 = ((float)rand() / (float)RAND_MAX) * 2.0f * 3.14159265f;
                add_asteroid(state, a->x, a->y,
                             new_speed * sinf(angle1), new_speed * cosf(angle1), new_size);
                add_asteroid(state, a->x, a->y,
                             new_speed * sinf(angle2), new_speed * cosf(angle2), new_size);
            }
            a->active = 0;
            state->score += (LARGE_SIZE / a->size) * 100;
            break;
        }
    }
}
/* 4. Now compact both pools */
compact_pool(state->bullets,   &state->bullet_count);
compact_pool(state->asteroids, &state->asteroid_count);
```

**Why `compact_pool` is called after step 3, not inside it:**  
During the collision loop, `add_asteroid` may add new entries to `state->asteroids` (the split fragments). If we called `compact_pool` inside the loop, we might remove or reorder entries that the outer loop is still iterating. Mark first, compact once after all modifications are done.

---

## Build & Run

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11 -lX11 -lxkbcommon -lGL -lGLX -lm
./build/asteroids_x11
```

**What you should see:** Shoot a large yellow asteroid — it splits into two medium asteroids. Shoot a medium — it splits into two small. Shoot a small — it disappears. Bullets disappear after flying across the screen (2.5 seconds). The score increases: 100 pts for large, 200 pts for medium, 400 pts for small. Clearing all asteroids adds a 1000-point bonus and spawns two new large ones.

---

## Key Concepts

- `SpaceObject` covers both bullets and asteroids; `size == 0` identifies bullets, `timer` is only used by bullets.
- Fixed-size C arrays (`SpaceObject asteroids[64]`) — no heap allocation per object, contiguous memory, cache-friendly.
- `active` flag — mark objects dead without modifying the array during iteration.
- `compact_pool` uses swap-with-last: copy the last element over the dead slot, decrement count. O(n), preserves no ordering.
- Do not increment `i` after a removal — the slot you just filled may itself be inactive.
- `compact_pool` must be called *after* all marking and adding is done for the frame, not during loops.
- `add_asteroid`/`add_bullet` guard against overflow with `if (count >= MAX) return;`.
- Bullet velocity: `dx = SPEED * sin(angle)`, `dy = -SPEED * cos(angle)` — the `-` makes angle 0 mean "up" in y-down screen coordinates.

## Exercise

Change `BULLET_LIFE` from `2.5f` to `0.5f` in `asteroids.h`. Rebuild and observe that bullets now disappear after half a second. Notice how it changes the gameplay feel (shorter range, must aim more carefully). Then change it back to `2.5f`.
