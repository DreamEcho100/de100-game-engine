# Lesson 05 — GrainPool SoA + Continuous Stream

**By the end of this lesson you will have:**
An emitter at the top center of the screen sprays a continuous stream of grains. Grains accumulate up to 4096. They fall, hit drawn lines, and slide. When the pool is full, no new grains spawn until old ones exit the screen.

---

## Step 1 — The Problem With One Grain

In Lesson 04 we had one `grain_x`, one `grain_y`, one `grain_vy`. To add a second grain we'd need `grain_x2`, `grain_y2`, `grain_vy2`. For 4096 grains this is insane.

The natural instinct from JavaScript would be an array of objects:

```js
// JS: Array of Objects (AoS)
const grains = [
  { x: 10.0, y: 20.0, vx: 0.0, vy: 8.3, color: 0, active: true },
  { x: 15.0, y: 30.0, vx: 0.0, vy: 5.1, color: 0, active: true },
  ...
];
```

In C, that is:

```c
// C: Array of Structs (AoS)
typedef struct { float x, y, vx, vy; uint8_t color, active; } Grain;
Grain grains[4096];
```

This works. But there is a faster layout.

---

## Step 2 — Struct of Arrays vs Array of Structs

Consider what the physics loop does every frame:

```
For each grain:
  vy[i] += GRAVITY * dt
  y[i]  += vy[i] * dt
```

It reads `vy[i]` and `y[i]` for every grain, every frame.

**Array of Structs (AoS) memory layout:**

```
[x0|y0|vx0|vy0|color0|active0|pad] [x1|y1|vx1|vy1|color1|active1|pad] ...
 ←──── grain 0, 24 bytes ────────→  ←──── grain 1, 24 bytes ────────→
```

To read `vy[0]` and `vy[1]`, the CPU must load 24 bytes per grain to get each `vy` field. Cache lines are 64 bytes, so you get ~2–3 grains' worth of `vy` per cache line — but the rest of each cache line (x, y, color, active fields you didn't need right now) is wasted bandwidth.

**Struct of Arrays (SoA) memory layout:**

```
x[]:      [x0|x1|x2|x3|x4|x5|x6|x7|x8|x9|x10|x11|x12|x13|x14|x15|...]
y[]:      [y0|y1|y2|y3|...]
vx[]:     [vx0|vx1|vx2|vx3|...]
vy[]:     [vy0|vy1|vy2|vy3|...]
color[]:  [c0|c1|c2|c3|...]
active[]: [a0|a1|a2|a3|...]
```

When the physics loop reads `vy[i]` for grains 0–15, it loads a 64-byte cache line of 16 consecutive `vy` floats. No wasted bandwidth. This is **cache-friendly** access.

Our `GrainPool` is the SoA layout (already defined in `game.h`):

```c
typedef struct {
    float   x[MAX_GRAINS];
    float   y[MAX_GRAINS];
    float   vx[MAX_GRAINS];
    float   vy[MAX_GRAINS];
    uint8_t color[MAX_GRAINS];
    uint8_t active[MAX_GRAINS];
    uint8_t tpcd[MAX_GRAINS];   /* teleport cooldown */
    int     count;              /* high-watermark index */
} GrainPool;
```

`count` is the **high-watermark**: the highest index ever used plus one. We never shrink it. This avoids scanning the whole pool to find the extent.

Memory: `4 * 4096 * 4 + 3 * 4096 * 1 + 4 = 69,644 bytes ≈ 68 KB`. Fits comfortably.

---

## Step 3 — Allocating a Grain: grain_alloc

To add a grain, find the first inactive slot:

```c
static int grain_alloc(GrainPool *p) {
    /*
     * Scan from 0 to count for an inactive slot.
     * If none found, extend count (up to MAX_GRAINS).
     *
     * This is an O(n) scan worst-case.
     * With 4096 grains and tight inner loops, it's still fast.
     * A free-list would be O(1) but isn't needed at this scale.
     */
    for (int i = 0; i < p->count; i++) {
        if (!p->active[i]) return i;
    }
    if (p->count < MAX_GRAINS) {
        int idx = p->count++;
        return idx;
    }
    return -1;   /* pool full */
}
```

Usage:

```c
int idx = grain_alloc(&state->grains);
if (idx >= 0) {
    state->grains.x[idx]      = 320.0f;
    state->grains.y[idx]      = 20.0f;
    state->grains.vx[idx]     = 0.0f;
    state->grains.vy[idx]     = 0.0f;
    state->grains.color[idx]  = GRAIN_WHITE;
    state->grains.active[idx] = 1;
    state->grains.tpcd[idx]   = 0;
}
```

In JavaScript terms, this is like `grains.push({...})` — but we manage the array ourselves, so grains can be "removed" (deactivated) without shifting the array.

---

## Step 4 — Emitter: Timed Spawning

An `Emitter` spawns grains at a fixed rate using a timer accumulator. Add this function to `game.c`:

```c
static void update_emitter(GameState *state, Emitter *em, float dt) {
    /*
     * spawn_timer accumulates real seconds.
     * When it exceeds 1/grains_per_second, spawn one grain and subtract.
     *
     * Example: grains_per_second = 30
     *   interval = 1/30 = 0.0333 s
     *   Frame 1 (dt=0.0167): timer = 0.0167  → no spawn
     *   Frame 2 (dt=0.0167): timer = 0.0333  → spawn, timer = 0.0
     *
     * This gives exactly 30 grains/second regardless of frame rate.
     */
    if (em->grains_per_second <= 0) return;

    float interval = 1.0f / (float)em->grains_per_second;
    em->spawn_timer += dt;

    while (em->spawn_timer >= interval) {
        em->spawn_timer -= interval;

        int idx = grain_alloc(&state->grains);
        if (idx < 0) return;   /* pool full, skip */

        /* Random jitter: ±8 px/s horizontal spread */
        float jitter = ((float)(rand() % 17) - 8.0f);

        state->grains.x[idx]      = (float)em->x;
        state->grains.y[idx]      = (float)em->y;
        state->grains.vx[idx]     = jitter;
        state->grains.vy[idx]     = 10.0f;   /* small downward push */
        state->grains.color[idx]  = GRAIN_WHITE;
        state->grains.active[idx] = 1;
        state->grains.tpcd[idx]   = 0;
    }
}
```

Add `#include <stdlib.h>` at the top of `game.c` if not already present — it provides `rand()`.

---

## Step 5 — The Grain Physics Loop (Pool Version)

This replaces the single-grain physics from Lesson 03/04. The loop structure is identical to before, but now we iterate over all slots up to `count`:

```c
static void update_grains(GameState *state, float dt) {
    GrainPool *p  = &state->grains;
    LineBitmap *lb = &state->lines;
    int W = CANVAS_W, H = CANVAS_H;

    /* ── Grain-occupancy bitmap ─────────────────────────────────────────
     * Tracks where active grains currently sit so they collide with EACH
     * OTHER, not just with drawn lines.  Static → lives in BSS (not stack).
     * Rebuilt from scratch every frame. */
    static uint8_t s_occ[CANVAS_W * CANVAS_H];
    __builtin_memset(s_occ, 0, sizeof(s_occ));
    for (int i = 0; i < p->count; i++) {
        if (!p->active[i]) continue;
        int ix = (int)p->x[i], iy = (int)p->y[i];
        if (ix >= 0 && ix < W && iy >= 0 && iy < H)
            s_occ[iy * W + ix] = 1;
    }

/* Unified solid check: line pixel OR another grain.  Out-of-bounds X = wall. */
#define IS_SOLID(xx, yy) \
    ((xx) < 0 || (xx) >= W || \
     lb->pixels[(yy)*W+(xx)] || s_occ[(yy)*W+(xx)])

    for (int i = 0; i < p->count; i++) {
        if (!p->active[i]) continue;

        /* Unmark this grain's pixel so it doesn't block itself. */
        {
            int cx = (int)p->x[i], cy = (int)p->y[i];
            if (cx >= 0 && cx < W && cy >= 0 && cy < H)
                s_occ[cy * W + cx] = 0;
        }

        /* ── Integrate ───────────────────────────── */
        p->vy[i] += GRAVITY * (float)state->gravity_sign * dt;
        if (p->vy[i] >  TERM_VEL) p->vy[i] =  TERM_VEL;
        if (p->vy[i] < -TERM_VEL) p->vy[i] = -TERM_VEL;

        float dx = p->vx[i] * dt;
        float dy = p->vy[i] * dt;

        /* ── Sub-step ────────────────────────────── */
        float abs_dx = dx < 0.0f ? -dx : dx;
        float abs_dy = dy < 0.0f ? -dy : dy;
        int steps = (int)(abs_dx + abs_dy) + 1;
        float sdx = dx / (float)steps;
        float sdy = dy / (float)steps;

        for (int s = 0; s < steps; s++) {
            float nx = p->x[i] + sdx;
            float ny = p->y[i] + sdy;
            int ix = (int)nx;
            int iy = (int)ny;

            /* Off-screen → deactivate */
            if (iy >= H || iy < 0 || ix < 0 || ix >= W) {
                p->active[i] = 0;
                break;
            }

            /* No collision → move */
            if (!IS_SOLID(ix, iy)) {
                p->x[i] = nx;
                p->y[i] = ny;
                continue;
            }

            /* ── Collision → resolve ─────────────────────────────────────
             * (A) Vertical collision (iy != cur_iy): grain hitting a surface
             *     from above or below. Try diagonal slides up to
             *     GRAIN_SLIDE_REACH (2) pixels to each side.
             *     Slide speed = max(|vy|, GRAIN_SLIDE_MIN=25).  The minimum
             *     keeps grains moving on slopes without causing the perpetual-
             *     attractor problem: it ONLY fires when a free diagonal exists.
             *     When all diagonals are blocked (flat pile) the !slid path
             *     zeros velocity and the displacement settle check bakes the grain.
             *     Path-clear guard: all intermediate pixels must be free so the
             *     grain cannot teleport across a 1-wide wall.
             * (B) Horizontal collision (iy == cur_iy): grain moving sideways
             *     into a vertical wall. Zero vx, keep falling.
             * Always break — stale sub-step deltas must not be re-used.
             */
            int cur_iy = (int)p->y[i];
            int cur_ix = (int)p->x[i];

            if (iy != cur_iy) {
                int d1 = (p->vx[i] >= 0.0f) ? 1 : -1;
                int d2 = -d1;
                int slid = 0;
                for (int dist = 1; dist <= GRAIN_SLIDE_REACH && !slid; dist++) {
                    for (int a = 0; a < 2 && !slid; a++) {
                        int d = (a == 0) ? d1 : d2;
                        int sx = cur_ix + d * dist;
                        if (sx < 0 || sx >= W) continue;
                        int ok = 1;
                        for (int px = cur_ix + d; px != sx + d; px += d)
                            if (IS_SOLID(px, iy)) { ok = 0; break; }
                        if (!ok || IS_SOLID(sx, iy)) continue;
                        float spd = p->vy[i] < 0 ? -p->vy[i] : p->vy[i];
                        if (spd < GRAIN_SLIDE_MIN) spd = GRAIN_SLIDE_MIN;
                        if (spd > MAX_VX) spd = MAX_VX;
                        p->vx[i] = (float)d * spd;
                        p->vy[i] = 0.0f;
                        p->x[i]  = (float)sx;
                        p->y[i]  = (float)iy;
                        slid = 1;
                    }
                }
                if (!slid) { p->vx[i] = 0.0f; p->vy[i] = 0.0f; }
            } else {
                p->vx[i] = 0.0f;  /* horizontal wall */
            }
            break; /* always stop sub-stepping after any collision */
        }

        /* Re-mark final position. */
        {
            int fx = (int)p->x[i], fy = (int)p->y[i];
            if (fx >= 0 && fx < W && fy >= 0 && fy < H)
                s_occ[fy * W + fx] = 1;
        }
    }
#undef IS_SOLID
}
```

---

## Step 6 — Rendering All Grains

Each active grain is a single colored pixel. Add a static array of grain colors near the top of `game.c`:

```c
static const uint32_t GRAIN_COLORS[GRAIN_COLOR_COUNT] = {
    GAME_RGB(210, 180, 140),   /* GRAIN_WHITE  → tan/wheat  */
    GAME_RGB(220,  60,  60),   /* GRAIN_RED                 */
    GAME_RGB( 60, 180,  60),   /* GRAIN_GREEN               */
    GAME_RGB(230, 140,  40),   /* GRAIN_ORANGE              */
};

static void render_grains(const GrainPool *p, GameBackbuffer *bb) {
    for (int i = 0; i < p->count; i++) {
        if (!p->active[i]) continue;
        int px = (int)p->x[i];
        int py = (int)p->y[i];
        if (px >= 0 && px < bb->width && py >= 0 && py < bb->height) {
            bb->pixels[py * bb->width + px] = GRAIN_COLORS[p->color[i]];
        }
    }
}
```

---

## Step 7 — Wire It All Together

Remove the single-grain fields from `GameState` in `game.h` (the `grain_x`, `grain_y`, `grain_vx`, `grain_vy`, `grain_active` fields added in Lesson 03).

Update `game_init` to set up a hardcoded emitter (levels come in Lesson 06):

```c
void game_init(GameState *state, GameBackbuffer *backbuffer) {
    (void)backbuffer;
    memset(state, 0, sizeof(*state));
    state->gravity_sign   = 1;
    state->unlocked_count = 1;

    /* Temporary emitter until level system is wired in lesson 06 */
    state->level.emitter_count        = 1;
    state->level.emitters[0].x        = CANVAS_W / 2;
    state->level.emitters[0].y        = 30;
    state->level.emitters[0].grains_per_second = 30;
    state->level.emitters[0].spawn_timer       = 0.0f;
}
```

Update `game_update`:

```c
void game_update(GameState *state, GameInput *input, float delta_time) {
    if (BUTTON_PRESSED(input->escape)) { state->should_quit = 1; return; }

    if (BUTTON_PRESSED(input->reset)) {
        memset(state->lines.pixels, 0, sizeof(state->lines.pixels));
        memset(&state->grains, 0, sizeof(state->grains));
    }

    if (input->mouse.left.ended_down) {
        draw_brush_line(
            &state->lines,
            input->mouse.prev_x, input->mouse.prev_y,
            input->mouse.x,      input->mouse.y,
            BRUSH_RADIUS
        );
    }

    /* Tick emitters */
    for (int e = 0; e < state->level.emitter_count; e++) {
        update_emitter(state, &state->level.emitters[e], delta_time);
    }

    /* Update all grains */
    update_grains(state, delta_time);
}
```

Update `game_render`:

```c
void game_render(const GameState *state, GameBackbuffer *backbuffer) {
    draw_rect(backbuffer, 0, 0, backbuffer->width, backbuffer->height, COLOR_BG);
    render_lines(&state->lines, backbuffer);
    render_grains(&state->grains, backbuffer);
}
```

---

## Build & Run

```bash
cd course
clang -Wall -Wextra -O2 -o sugar \
    src/main_x11.c src/game.c src/levels.c \
    -lX11 -lm
./sugar
```

**What you should see:**
A stream of cream-colored grains falls from the top center. Draw a ramp — grains slide down it at a speed proportional to how fast they were falling. Draw a bowl — grains fill it and pile up naturally. Grains that come to rest bake into the line bitmap (freeing their pool slot) so the stream is continuous. Press R to clear everything and restart.

---

## Key Concepts

- **Array of Structs (AoS)**: natural but cache-unfriendly for bulk operations. Reading field `vy` means skipping over x, y, vx, color, active for each grain.
- **Struct of Arrays (SoA)**: all `vy` values are contiguous in memory. The physics loop loads 16 `vy` floats per cache line — 16x better cache utilisation than AoS for that loop.
- `count` (high-watermark): the pool only grows, never shrinks. Inactive slots are reused. `count` tells the loop "don't bother checking past here."
- `grain_alloc`: scans from 0 to `count` for the first inactive slot. O(n) worst case, but fast enough at 4096 elements.
- Timer accumulator (`spawn_timer += dt; while(timer >= interval) { spawn; timer -= interval; }`): the `while` (not `if`) handles the case where dt is large and multiple grains should spawn in one frame.
- `rand() % 17 - 8`: gives integers in [-8, 8]. Not cryptographically random — just enough jitter to make the stream look natural. Cast to `float` for the velocity field.
- **`GRAIN_SLIDE_MIN = 25 px/s`**: minimum slide speed on a surface-contact. Ensures grains always have enough velocity to register as "moving" in the displacement settle check. Safe to use: the minimum only fires when a free diagonal pixel is found; flat piles zero velocity via the `!slid` path regardless. Value reduced from 50 → 25 px/s: at 50 px/s sliding grains flew far from the pile and formed discrete satellite streams, giving the appearance of three separate trickles instead of one pour.
- **`EMITTER_SPREAD = ±3 px`**: random position offset applied at spawn (in addition to velocity jitter). Without it, all grains start at the exact same pixel `(em->x, em->y)`. The first grain that stalls there marks that pixel solid in the occupancy bitmap; every subsequent grain collides immediately and slides to exactly ±1 or ±2 pixels — producing three discrete "satellite streams" instead of a smooth pour. Adding ±3 px of spawn position distributes the initial impact across 7 pixels so the pile grows uniformly.
- **`EMITTER_JITTER = ±8 px/s`**: produces a slight natural velocity fan from the spout. ±3 px/s was almost invisible — all grains fell nearly straight down even with EMITTER_SPREAD. ±8 px/s creates a small visible spread while still looking like a focused pour.
- **Displacement-based bake-and-free (frame-rate-independent)**: velocity-based detection failed because gravity re-adds ~8.3 px/s between frames — resetting the counter every single frame so grains in piles never baked. A fixed-distance check failed at uncapped FPS: `dt ≈ 0.0001s` → grain moves 0.005 px → `dist² = 0.000025 < 0.04` → all grains bake instantly. Fix: capture `(pre_x, pre_y)` before sub-steps, compare after. If `dist² < (GRAIN_SETTLE_SPEED × dt)²` (< 10 px/s equivalent) for `GRAIN_SETTLE_FRAMES (8)` frames → bake. The threshold scales with `dt²`, making it frame-rate-independent at any FPS. The 60fps cap in `main_x11.c` ensures visual stability, but the settle check itself is robust.
- **GRAIN_HORIZ_DRAG = 0.97**: 0.97^60 ≈ 16%/s decay. More aggressive than 0.98 (30%/s), keeping slide-off grains closer to the main pile.
- **`GRAIN_BOUNCE (0.25)` + `GRAIN_BOUNCE_MIN (20 px/s)`**: small coefficient of restitution on vertical impact. Gives grains a lively feel — they pop slightly before settling. The `BOUNCE_MIN` threshold ensures gravity micro-accumulation (~4.7 px/s per frame with GRAVITY=280) never triggers a bounce.
- Pool full: `grain_alloc` returns `-1`. The caller silently skips the spawn. Safety net only — the bake-and-free mechanism normally keeps the pool well under capacity.

---

## Exercise

Add a second emitter at `(160, 30)` by inserting one more element in the emitter array (set `emitter_count = 2`). Give it 15 grains per second. Observe that both streams interact correctly with the same `LineBitmap`. You have two emitters with zero additional collision logic.
