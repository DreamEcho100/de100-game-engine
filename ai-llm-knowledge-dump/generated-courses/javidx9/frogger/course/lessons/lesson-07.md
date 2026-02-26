# Lesson 07 — `lane_scroll`: Pixel-Accurate Scrolling

## By the end of this lesson you will have:

A thorough understanding of the `lane_scroll` function — why it works in pixels, how it handles negative speeds, and why it makes the scroll sub-pixel smooth instead of cell-snapping.

---

## The Bug It Fixes

The naive scrolling code used integer tile counting:

```c
/* OLD — broken */
int tile_start  = (int)(time * speed) % LANE_PATTERN_LEN;
int cell_offset = (int)(TILE_CELLS * time * speed) % TILE_CELLS;
```

**Problem 1 — Negative modulo in C:**
C truncates `(int)` toward zero. `(int)(-4.5) = -4`, not `-5`.

For `speed = -3` (moves left), `time = 1.5`:
```
(int)(1.5 * -3) % 64
= (int)(-4.5) % 64
= -4 % 64
= -4       ← C gives negative result for negative dividend
```

`tile_start = -4` is an invalid array index — wraps to `lane_patterns[y][-4]` → undefined behavior, and visually the start tile is off by one.

**Problem 2 — Cell-granular offset (8px jumps):**
`cell_offset` was in CELL units (8 pixels each). So the sprite's sub-tile position jumped in 8-pixel steps. At 60fps with speed=3, the lane moves `3 * 64 * 0.016 = 3.07 pixels/frame`. The old code would hold position for ~2-3 frames then snap 8 pixels → visible stuttering.

---

## Step 2 — The fix: work entirely in pixels

```c
static inline void lane_scroll(float time, float speed,
                                int *out_tile_start, int *out_px_offset) {
    const int PATTERN_PX = LANE_PATTERN_LEN * TILE_PX;  /* 64 * 64 = 4096 */

    int sc = (int)(time * speed * (float)TILE_PX) % PATTERN_PX;
    if (sc < 0) sc += PATTERN_PX;  /* make positive */

    *out_tile_start = sc / TILE_PX;
    *out_px_offset  = sc % TILE_PX;
}
```

**Step by step with `speed=-3, time=1.5`:**
```
sc = (int)(1.5 * -3 * 64) % 4096
   = (int)(-288.0) % 4096
   = -288 % 4096
   = -288       ← still negative
sc += 4096
   = 3808       ← now in [0, 4096)
tile_start = 3808 / 64 = 59
px_offset  = 3808 % 64 = 32
```

`tile_start=59` is a valid index `[0, 63]`. `px_offset=32` is 32 pixels into the tile — sub-pixel smooth.

---

## Step 3 — How the output is used in rendering

```c
int dest_px = (-1 + i) * TILE_PX - px_offset;
char c = lane_patterns[y][(tile_start + i) % LANE_PATTERN_LEN];
```

- `(-1 + i) * TILE_PX` places tile `i` at its column position (tile -1 = just off left edge, for partial tile visibility)
- `- px_offset` shifts everything left by `px_offset` pixels — the sub-pixel scroll offset
- Result: sprites slide smoothly in sub-pixel increments each frame

**Example:** `speed=3, time=0.1`:
```
sc = (int)(0.1 * 3 * 64) % 4096 = (int)(19.2) % 4096 = 19
tile_start = 19 / 64 = 0
px_offset  = 19 % 64 = 19

Tile 0 renders at: (-1 + 0) * 64 - 19 = -83 px (off left edge)
Tile 1 renders at: (-1 + 1) * 64 - 19 = -19 px (partially visible)
Tile 2 renders at: (-1 + 2) * 64 - 19 =  45 px
...
```

Each frame `sc` increases by `speed * TILE_PX * dt ≈ 3.07` pixels → sprites slide right by ~3 pixels/frame.

---

## Step 4 — `static inline`

```c
static inline void lane_scroll(...)
```

- `static`: only visible within `frogger.h` (technically the translation units that include it)
- `inline`: hint to the compiler to expand the function body at each call site instead of making an actual function call — avoids call overhead in the hot path
- Defined in `frogger.h` (not `frogger.c`) so both `frogger_tick` and `frogger_render` can call it with the function body available for inlining

---

## Step 5 — Danger buffer uses the same math

Critically, `frogger_tick` (danger buffer) and `frogger_render` (drawing) both call `lane_scroll` with the same `state->time` and `lane_speeds`. This guarantees:

> The visual position of every sprite exactly matches the collision cell it occupies.

If the danger buffer used tile-granular math but rendering used pixel-granular math, the frog could visually be on a log but the danger buffer sees it in a water cell → false death. The single source of truth (`lane_scroll`) prevents this.

---

## Key Concepts

- Naive `(int)(time * speed) % PATTERN_LEN` is broken: negative C modulo and cell-granular jumps
- Fix: work in pixels, then `if (sc < 0) sc += PATTERN_PX` to ensure `[0, PATTERN_PX)`
- `tile_start = sc / TILE_PX` — which tile to start from (valid index)
- `px_offset = sc % TILE_PX` — sub-pixel horizontal offset (0..63 pixels)
- `dest_px = (-1 + i) * TILE_PX - px_offset` — smooth pixel position per tile
- Both `frogger_tick` and `frogger_render` call `lane_scroll` — collision and rendering always agree

---

## Exercise

The first tile drawn is at index `i=0`, which positions it at `dest_px = -TILE_PX - px_offset`. Why start at `i=0` (which draws off-screen to the left) instead of `i=1`? What would you miss if you started at `i=1`?

Hint: consider the case where `px_offset = 63` (the lane has scrolled 63 pixels into the first tile). Where would tile `i=1` appear on screen?
