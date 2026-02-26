# Lesson 04 — Grain vs Line Collision + Slide

**By the end of this lesson you will have:**
Draw any line or bowl shape. The single grain falls, hits the line, and slides down the slope. Draw a bowl and the grain rolls down one side, up the other, and settles at the bottom. Press R to reset.

---

## Step 1 — The Collision Check: O(1) Lookup

How do you know if a grain has hit a drawn line?

The `LineBitmap` already has the answer. Every pixel on the canvas is either empty (`0`) or has a line (`1`). Checking if position `(x, y)` is occupied is one array read:

```c
int has_line = lb->pixels[iy * CANVAS_W + ix];
/*
 * iy = (int)grain_y     ← truncated to pixel row
 * ix = (int)grain_x     ← truncated to pixel column
 *
 * Example: grain at (213.7, 145.2)
 *   iy = 145
 *   ix = 213
 *   index = 145 * 640 + 213 = 93013
 *   lb->pixels[93013] is 0 (empty) or 1 (line)
 */
```

This is O(1) — constant time regardless of how many lines you have drawn. No loops, no distance calculations. This is why we stored lines in a flat bitmap instead of a list of segments.

Compare to what you'd do in JavaScript:

```js
// Slow: loop over every segment
for (const seg of drawnSegments) {
  if (pointNearSegment(grain, seg)) { ... }
}

// Fast equivalent: read one pixel from an ImageData
const pixel = ctx.getImageData(grain.x|0, grain.y|0, 1, 1).data[3];
```

Our LineBitmap is the C equivalent of that fast approach.

---

## Step 2 — Why We Need Sub-Steps (Tunneling)

Without sub-steps, a fast grain can skip over thin lines entirely.

Concrete example:

```
GRAVITY = 280 px/s²
At frame 20 (≈0.33s after spawn): vy ≈ 93 px/s
Displacement per frame: 93 * 0.0167 ≈ 1.6 pixels

That's safe — the grain moves 1–2 pixels per frame, can't skip a 1-pixel line.

But at TERM_VEL = 600 px/s:
  600 * 0.0167 = 10 pixels per frame

A grain could jump from y=139 to y=149, skipping y=144 entirely.
If the line is at y=144, the grain passes through it — tunneling.
```

The fix: break each frame's movement into multiple small steps, each at most 1 pixel long.

```
Total movement: (dx, dy) = (0.0, 10.0) pixels
Steps needed: ceil(|dx| + |dy|) = 10
Step size: (0.0/10, 10.0/10) = (0.0, 1.0) per sub-step

Check collision at each of the 10 sub-step positions.
```

The formula for step count:

```c
float abs_dx = dx < 0 ? -dx : dx;   /* fabsf(dx) */
float abs_dy = dy < 0 ? -dy : dy;
int steps = (int)(abs_dx + abs_dy) + 1;
float sdx = dx / (float)steps;
float sdy = dy / (float)steps;
```

`+1` ensures at least one step even when `dx = dy = 0`.

---

## Step 3 — Slide Resolution

When a grain hits a line, we don't just stop it. We try to slide it sideways.

Strategy:

```
Grain is at (ix, iy) and the pixel below is occupied.

Try sliding right: is (ix+1, iy+1) empty?
  Yes → move grain there (slide right-down)

Try sliding left: is (ix-1, iy+1) empty?
  Yes → move grain there (slide left-down)

Both blocked →
  Zero velocity. Grain comes to rest.
```

This produces natural pile-up behavior. Grains stack on each other because each grain that comes to rest becomes an obstacle for the next one.

The slide check happens at the bottom of the sub-step loop, only when a collision is detected.

---

## Step 4 — The Full Collision + Slide Function

Replace `game_update` in `game.c` with the version below. The new section is the sub-step loop inside `if (state->grain_active)`.

```c
void game_update(GameState *state, GameInput *input, float delta_time) {
    if (BUTTON_PRESSED(input->escape)) {
        state->should_quit = 1;
        return;
    }

    if (BUTTON_PRESSED(input->reset)) {
        memset(state->lines.pixels, 0, sizeof(state->lines.pixels));
        state->grain_x      = 320.0f;
        state->grain_y      = 20.0f;
        state->grain_vx     = 0.0f;
        state->grain_vy     = 0.0f;
        state->grain_active = 1;
    }

    if (input->mouse.left.ended_down) {
        draw_brush_line(
            &state->lines,
            input->mouse.prev_x, input->mouse.prev_y,
            input->mouse.x,      input->mouse.y,
            BRUSH_RADIUS
        );
    }

    if (!state->grain_active) return;

    /* ── 1. Integrate velocity ──────────────────────── */
    state->grain_vy += GRAVITY * (float)state->gravity_sign * delta_time;
    if (state->grain_vy >  TERM_VEL) state->grain_vy =  TERM_VEL;
    if (state->grain_vy < -TERM_VEL) state->grain_vy = -TERM_VEL;

    /* ── 2. Compute this frame's displacement ─────── */
    float dx = state->grain_vx * delta_time;
    float dy = state->grain_vy * delta_time;

    /* ── 3. Sub-step movement + collision ─────────── */
    float abs_dx = dx < 0.0f ? -dx : dx;
    float abs_dy = dy < 0.0f ? -dy : dy;
    int steps = (int)(abs_dx + abs_dy) + 1;
    float sdx = dx / (float)steps;
    float sdy = dy / (float)steps;

    LineBitmap *lb = &state->lines;

    for (int s = 0; s < steps; s++) {
        float nx = state->grain_x + sdx;
        float ny = state->grain_y + sdy;

        int ix = (int)nx;
        int iy = (int)ny;

        /* Off-screen → deactivate */
        if (iy >= CANVAS_H || iy < 0 || ix < 0 || ix >= CANVAS_W) {
            state->grain_active = 0;
            return;
        }

        /* No collision → accept the move */
        if (!lb->pixels[iy * CANVAS_W + ix]) {
            state->grain_x = nx;
            state->grain_y = ny;
            continue;
        }

        /* ── Collision detected ──────────────────────────────────────────
         *
         * Sugar Sugar "follow the line" rule:
         *
         * The grain was at (grain_x, grain_y) and tried to step to (nx, ny).
         * Position (ix, iy) is solid. We need to resolve in a way that lets
         * the grain slide along the surface.
         *
         * Key insight: distinguish two cases:
         *   (A) iy != cur_iy  →  grain was moving vertically into a surface
         *       (falling onto a slope / horizontal line).
         *       Resolution: try diagonal slide to (grain_x ± 1, iy).
         *       Slide speed = max(|vy|, GRAIN_SLIDE_MIN) — ensures the grain
         *       always moves fast enough to be detected as "moving" by the
         *       displacement-based settle check, even on shallow re-contacts.
         *       GRAIN_SLIDE_MIN only fires in this "free diagonal" branch;
         *       when both diagonals are blocked the !slid path zeros velocity
         *       normally — so flat piles still settle and bake correctly.
         *
         *   (B) iy == cur_iy  →  grain was moving only horizontally into a
         *       vertical surface (wall). Resolution: kill vx, keep falling.
         *
         * Always BREAK after any collision so that sub-step deltas from BEFORE
         * the collision are not re-used — that would cause jitter.
         */
        int cur_iy = (int)state->grain_y;
        int cur_ix = (int)state->grain_x;

        if (iy != cur_iy) {
            /* --- Vertically-induced collision: try diagonal slide ---
             * Check up to GRAIN_SLIDE_REACH (2) pixels in each direction.
             * ±1 only gives 45° angle of repose; ±2 gives ~27°, which
             * looks more like real sugar flowing off a heap.
             * The path-clear guard ensures no pixel is skipped over solid. */
            int d1 = (state->grain_vx >= 0.0f) ?  1 : -1;
            int d2 = -d1;
            int slid = 0;

            for (int dist = 1; dist <= GRAIN_SLIDE_REACH && !slid; dist++) {
                for (int attempt = 0; attempt < 2 && !slid; attempt++) {
                    int d  = (attempt == 0) ? d1 : d2;
                    int sx = cur_ix + d * dist;
                    if (sx < 0 || sx >= CANVAS_W) continue;
                    /* All pixels between cur_ix and sx must be free. */
                    int ok = 1;
                    for (int px = cur_ix + d; px != sx + d; px += d)
                        if (lb->pixels[iy * CANVAS_W + px]) { ok = 0; break; }
                    if (!ok || lb->pixels[iy * CANVAS_W + sx]) continue;
                    float spd = state->grain_vy < 0 ? -state->grain_vy
                                                    :  state->grain_vy;
                    if (spd < GRAIN_SLIDE_MIN) spd = GRAIN_SLIDE_MIN;
                    if (spd > MAX_VX) spd = MAX_VX;
                    state->grain_vx = (float)d * spd;
                    state->grain_vy = 0.0f;
                    state->grain_x  = (float)sx;
                    state->grain_y  = (float)iy;
                    slid = 1;
                }
            }

            if (!slid) {
                /* Grain comes to rest (piled on horizontal surface). */
                state->grain_vx = 0.0f;
                state->grain_vy = 0.0f;
            }
        } else {
            /* --- Pure horizontal collision: kill vx, keep falling --- */
            state->grain_vx = 0.0f;
        }

        /* Stop sub-stepping this frame after a collision.
         * The new velocity takes effect from the next frame cleanly. */
        break;
    }
}
```

---

## Step 5 — Why Grains Pile Up and Collide With Each Other

### Pile-up on a surface

When a grain settles (`vx = vy = 0`), gravity re-adds a tiny `vy` every frame:

```
vy = 0 + 280 * 0.0167 = 4.67 px/s
ny = grain_y + 4.67 * 0.0167 = grain_y + 0.078
```

The grain tries to move 0.08 pixels. The next integer row is occupied (the line
below). Collision → try diagonal → both blocked → bounce or zero velocity. Repeats 60×
per second. The grain appears still because it is continuously blocked.

### Grain-grain collision (occupancy bitmap)

The `LineBitmap` only stores player-drawn lines. Two grains can occupy the same
pixel without knowing it, so grains pass through each other and the pile-up
looks wrong (grains overlap rather than stack).

The fix is a **grain-occupancy bitmap** — a second `uint8_t[CANVAS_W*CANVAS_H]`
array that is rebuilt every frame from the current grain positions:

```c
/* In update_grains() — built once, at the top, each frame */
static uint8_t s_occ[CANVAS_W * CANVAS_H]; /* static = BSS, not stack */
memset(s_occ, 0, sizeof(s_occ));
for (int i = 0; i < pool->count; i++) {
    if (!pool->active[i]) continue;
    int ix = (int)pool->x[i], iy = (int)pool->y[i];
    s_occ[iy * CANVAS_W + ix] = 1;   /* mark this pixel as occupied */
}

/* Unified solid check used everywhere in the physics loop: */
#define IS_SOLID(xx, yy) \
    ((xx) < 0 || (xx) >= CANVAS_W || \
     lines->pixels[(yy)*CANVAS_W+(xx)] || s_occ[(yy)*CANVAS_W+(xx)])
```

Before updating grain `i`, unmark its current pixel (so it doesn't block
itself), then re-mark the new pixel after movement:

```c
s_occ[old_y * W + old_x] = 0;   /* unmark self before move */
/* ... move and resolve collision ... */
s_occ[new_y * W + new_x] = 1;   /* re-mark at new position */
```

Result: the grain at the bottom of a pile is "solid" to the grain falling on
top of it — natural stacking without any additional data structure.

> ⚠️ **Why static, not a stack variable?**
> `CANVAS_W * CANVAS_H = 640 * 480 = 307,200 bytes = 300 KB`.  Declaring that
> on the stack would exceed typical stack limits and corrupt the program.
> `static` inside a function puts the array in the BSS segment (zero-initialised
> global storage) — same lifetime as the process, no stack cost.

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
Draw a diagonal line. The grain hits it and slides to the lower end. Draw a shallow U-shape (bowl). The grain falls in, slides down one side, up the other, settles at the bottom. Press R to reset grain and lines.

---

## Key Concepts

- `lb->pixels[iy * CANVAS_W + ix]`: O(1) collision check — one array read. No loop over line segments.
- **Tunneling**: a fast grain can skip over a 1-pixel line in a single frame. Sub-steps prevent this by keeping each step ≤1 pixel of movement.
- `steps = (int)(|dx| + |dy|) + 1`: Manhattan distance gives the number of sub-steps needed. `+1` ensures at least one step.
- **Slide resolution and angle of repose**: on a vertical collision (`iy != cur_iy`), try `(cur_x ± dist, iy)` for `dist = 1..GRAIN_SLIDE_REACH`. The outermost free pixel wins. Checking only ±1 gives a 45° pile (any 2-wide obstacle blocks). Checking ±2 gives ~27° — closer to real sugar. A **path-clear guard** checks all intermediate pixels so the grain can't teleport through a wall. On a pure horizontal collision (`iy == cur_iy`), kill `vx` only.
- **`GRAIN_SLIDE_REACH (2)`**: maximum lateral distance checked for a diagonal slide. Increase to get a shallower, more fluid heap; decrease to get a steeper, more solid-looking pile. A reach of 1 = 45°, 2 = ~27°, 3 = ~18°.
- **`EMITTER_JITTER (8 px/s)`**: random horizontal spread applied at spawn. ±3 px/s looked fine but produced a visually narrow stream — all grains fell nearly straight down and piled up in a single column. ±8 px/s creates a small natural fan width visible from the spout.
- **Always `break` after a collision.** Sub-step deltas are computed before the velocity changes. Letting sub-steps continue after a collision re-uses stale deltas, causing jitter and tunnelling into the solid.
- **Occupancy bitmap**: replace `lb->pixels[…]` with `IS_SOLID(x,y)` that checks *both* the line bitmap and a per-frame grain-occupancy bitmap. Without this, grains pass through each other.
- **Horizontal damping**: multiply `vx` by `GRAIN_HORIZ_DRAG (0.97)` each frame. Grains decelerate naturally over ~1 s on a flat surface after sliding off a ramp.
- **`GRAIN_BOUNCE (0.25)` and `GRAIN_BOUNCE_MIN (20 px/s)`**: when a grain hits a solid surface with `|vy| > GRAIN_BOUNCE_MIN`, reflect a fraction of `vy` upward: `vy_after = -|vy_impact| × GRAIN_BOUNCE`. This gives grains a lively, light-bouncing quality before they settle. At 0.25 restitution a grain landing at 200 px/s bounces to 50 px/s, then to 12.5 px/s (below GRAIN_BOUNCE_MIN → stops). The threshold prevents perpetual micro-bouncing from gravity re-accumulation (≈4.7 px/s << 20).
- **`GRAIN_SLIDE_MIN (25 px/s)`**: the minimum slide speed assigned when a free diagonal pixel is found. Ensures grains always move with enough speed to be detected as "moving" by the displacement settle check. Safe to use here because the minimum only fires when a free diagonal exists — on a flat pile the `!slid` branch zeros velocity regardless. Value is 25 (not 50): at 50 px/s, slide-off grains flew far enough to create discrete "satellite" piles, producing three visually distinct streams. 25 px/s keeps sliding grains near the main pile.
- **Displacement-based bake-and-free (frame-rate-independent)**: capture grain position before sub-steps (`pre_x, pre_y`). After sub-steps, compute displacement distance². Compare against a velocity-based threshold `(GRAIN_SETTLE_SPEED × dt)²`. If the grain's average speed this frame < `GRAIN_SETTLE_SPEED (10 px/s)` for `GRAIN_SETTLE_FRAMES (8)` consecutive frames, write the pixel into the line bitmap and free the pool slot.

  - **Why velocity-based detection fails**: gravity re-adds ~8.3 px/s between frames — resetting the counter every single frame so stuck grains never baked.
  - **Why a fixed-distance check fails**: at uncapped FPS (thousands of frames per second), `dt ≈ 0.0001 s`. A grain at 50 px/s moves only 0.005 px per frame. `dist² = 0.000025 < 0.04` → every grain is immediately marked "still" and bakes before it is even rendered.
  - **The correct fix**: threshold scales with `dt²` so it is always *"moved less than GRAIN_SETTLE_SPEED × dt"* — the same speed check at 60 fps, 120 fps, or 10,000 fps. A free-falling grain (50 px/s) never triggers it; a truly blocked grain (0 px/s) always does.
  - The frame rate cap in `main_x11.c` (60 fps via `nanosleep`) is still needed for visual stability, but the settle check is now robust to any dt.

---

## Exercise

Add a second obstacle: instead of resetting both lines and grain on R, only reset the grain (keep the lines). This lets you build a complex course, then press R repeatedly to watch the grain navigate it from the top. Hint: split the R handler into two key checks — R clears lines, Space resets grain only.
