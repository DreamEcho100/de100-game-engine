# Lesson 09: Tower Barrel and Targeting

## What we're building

Placed towers grow a barrel — a short white line that rotates to point at the
**nearest-to-exit** active creep within range.  No shooting yet; this lesson is
purely about visual tracking.  Towers with no creep in range keep their barrel
pointing right (angle = 0).

## What you'll learn

- `draw_line()` via Bresenham's algorithm added to `draw-shapes.c`
- How `atan2f(dy, dx)` converts a direction vector to an angle in radians
- How to compute a line tip from an angle: `tip_x = cx + cosf(angle) * len`
- An **O(n)** `find_best_target()` in TARGET_FIRST mode: iterate all creeps,
  pick the one with the smallest `dist[]` value (closest to exit) that is also
  within range
- The `in_range()` distance-squared check (avoids `sqrtf`)

## Prerequisites

- Lesson 07 complete: creeps move and the pool is active
- Lesson 08 complete: `GAME_PHASE_WAVE` is wired up
- Tower struct with `angle`, `cx`, `cy`, `range`, `target_id` fields

---

## Step 1: Add `draw_line()` to `draw-shapes.c`

Bresenham's line algorithm draws a 1-pixel-wide line between two integer
endpoints.

```c
/* src/utils/draw-shapes.c  —  draw_line() addition */

void draw_line(Backbuffer *bb, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx =  abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;  /* error accumulator */

    for (;;) {
        /* Plot current point */
        if (x0 >= 0 && x0 < bb->width && y0 >= 0 && y0 < bb->height)
            bb->pixels[y0 * bb->width + x0] = color;

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
```

Declare it in `draw-shapes.h`:

```c
/* src/utils/draw-shapes.h  —  addition */
void draw_line(Backbuffer *bb, int x0, int y0, int x1, int y1, uint32_t color);
```

---

## Step 2: Barrel length constant

```c
/* src/game.c  —  at the top, with other constants */
#define BARREL_LEN 8   /* pixels from tower centre to barrel tip */
```

---

## Step 3: `in_range()` — distance-squared check

```c
/* src/game.c  —  in_range() */

/* Returns 1 if the creep's float position is within the tower's range.
 * Uses distance² to avoid sqrtf — valid because both sides are squared. */
static int in_range(const Tower *tower, const Creep *creep) {
    float dx = creep->x - (float)tower->cx;
    float dy = creep->y - (float)tower->cy;
    float r  = tower->range;
    return (dx * dx + dy * dy) <= (r * r);
}
```

**Why distance²?**  `sqrtf` is accurate but slow in a tight inner loop.  Because
we only need `d <= r`, squaring both sides gives `d² <= r²`, which requires no
square root and produces identical results for positive distances.

---

## Step 4: `find_best_target()` — TARGET_FIRST mode

TARGET_FIRST returns the active in-range creep with the **lowest** `dist[]` value
— that is, the creep that is closest to the exit and therefore will deal lives
damage soonest if ignored.

```c
/* src/game.c  —  find_best_target() */

/* Returns the index into state->creeps[] of the best target, or -1 if none.
 * TARGET_FIRST mode only (other modes added in Lesson 15). */
static int find_best_target(const GameState *state, const Tower *tower) {
    int   best_idx  = -1;
    int   best_dist = INT_MAX;

    for (int i = 0; i < state->creep_count; i++) {
        const Creep *c = &state->creeps[i];
        if (!c->active)          continue;
        if (!in_range(tower, c)) continue;

        int d = state->dist[c->row * GRID_COLS + c->col];
        if (d < best_dist) {
            best_dist = d;
            best_idx  = i;
        }
    }

    return best_idx;
}
```

Add `#include <limits.h>` at the top of `game.c` for `INT_MAX`.

---

## Step 5: `tower_update()` — rotate barrel toward target

```c
/* src/game.c  —  tower_update() */
#include <math.h>   /* atan2f, cosf, sinf */

static void tower_update(GameState *state, Tower *tower, float dt) {
    (void)dt; /* no firing yet — added in Lesson 10 */

    int target_idx = find_best_target(state, tower);

    if (target_idx >= 0) {
        const Creep *target = &state->creeps[target_idx];
        float dx = target->x - (float)tower->cx;
        float dy = target->y - (float)tower->cy;
        /* atan2f(y, x) returns angle in [-π, π] from positive-x axis */
        tower->angle     = atan2f(dy, dx);
        tower->target_id = target->id;
    } else {
        tower->target_id = -1;
        /* angle unchanged — barrel keeps pointing in last known direction */
    }
}
```

JS analogy: `Math.atan2(dy, dx)` is identical to `atan2f(dy, dx)`.  The only
difference is the `f` suffix, which tells the C compiler to use the `float`
overload instead of `double`.

---

## Step 6: Call `tower_update()` from `game_update()`

Add this loop inside `GAME_PHASE_WAVE` in `game_update()`:

```c
/* Inside GAME_PHASE_WAVE case of game_update() */

/* Update towers (targeting, firing — firing added in Lesson 10) */
for (int i = 0; i < state->tower_count; i++) {
    Tower *t = &state->towers[i];
    if (t->active) tower_update(state, t, dt);
}
```

---

## Step 7: Render towers with barrel lines in `game_render()`

Replace the plain `draw_rect` tower body with a body + barrel:

```c
/* src/game.c  —  tower rendering (replaces earlier plain draw_rect) */

for (int i = 0; i < state->tower_count; i++) {
    const Tower *t = &state->towers[i];
    if (!t->active) continue;

    uint32_t color = TOWER_DEFS[t->type].color;

    /* Tower body: (CELL_SIZE-2) × (CELL_SIZE-2) centred on cx,cy */
    int body = CELL_SIZE - 2;
    int px   = t->cx - body / 2;
    int py   = t->cy - body / 2;
    draw_rect(bb, px, py, body, body, color);

    /* Barrel line: from centre toward angle, length BARREL_LEN */
    int tip_x = t->cx + (int)(cosf(t->angle) * BARREL_LEN);
    int tip_y = t->cy + (int)(sinf(t->angle) * BARREL_LEN);
    draw_line(bb, t->cx, t->cy, tip_x, tip_y, COLOR_BARREL);
}
```

---

## Build and run

```bash
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG \
      -fsanitize=address,undefined \
      -o build/dtd \
      src/main_raylib.c \
      src/game.c \
      src/utils/draw-shapes.c \
      src/utils/draw-text.c \
      -lraylib -lm -lpthread -ldl

./build/dtd
```

**Expected output:** Placed towers show a grey/coloured square body with a short
white barrel line.  When creeps enter range, each barrel smoothly rotates to point
at the creep with the lowest BFS distance (closest to exit).  When the last creep
leaves range the barrel freezes at its final angle.  Towers with no creeps in
range point right (angle = 0, barrel extends to the right).

---

## Exercises

**Beginner:** Add a thin grey **range circle** outline around each tower.  Draw it
using a simple loop: for angles 0..360 step 5°, plot a pixel at
`(cx + cosf(rad)*range, cy + sinf(rad)*range)`.  Make it only visible during
`GAME_PHASE_PLACING` so it does not clutter the wave view.

**Intermediate:** Implement `TARGET_LAST` mode in `find_best_target()`:  
`TARGET_LAST` returns the creep with the **highest** `dist[]` value (farthest
from exit).  Add a `target_mode` toggle to the tower via a right-click handler.
Verify that barrels now track the creep farthest from the exit.

**Challenge:** `atan2f` is called every frame for every active tower.  Profile
the cost by adding a per-frame counter: how many `atan2f` calls happen per second
at wave 1 with 5 towers and 10 creeps?  Then implement the incremental update:
only recalculate the angle when `target_id` changes **or** when the target's cell
changes (i.e., on `best_col != prev_col || best_row != prev_row`).  Does the
barrel still look smooth?

---

## What's next

In Lesson 10 we add the **firing system**: a cooldown timer triggers
`spawn_projectile()`, projectiles fly toward their target's current position, and
`apply_damage()` deducts HP.  Dead creeps award gold and increment the counter.
