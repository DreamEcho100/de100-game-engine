# Lesson 06 — Scrolling Lanes: Time-Based Motion and the Danger Buffer

## What You'll Build

Lanes that scroll left or right at varying speeds using real elapsed time, so
motion is frame-rate independent. A flat `danger[]` array that mirrors the
on-screen positions of obstacles — so collision detection is just one
array-lookup, not a per-entity loop.

---

## 1. Why Time-Based Motion?

If we moved sprites by 1 pixel per frame, a 60 Hz machine runs at double the
speed of a 30 Hz machine. Instead we store **elapsed time** and compute
position from `time × speed`:

```c
// Where is tile i in the pattern?
position = time * speed   // advances proportionally to real time
```

Both `frogger_tick` and `platform_render` read the same `state->time`, so the
collision grid and the drawn sprites are always in sync.

---

## 2. The Broken Naive Approach

A first attempt might look like:

```c
// ❌ Broken — wrong for negative speeds AND coarse pixel steps
int tile_start  = (int)(time * speed) % PATTERN_LEN;
int cell_offset = (int)(TILE_CELLS * time * speed) % TILE_CELLS;
if (tile_start  < 0) tile_start  = LANE_PATTERN_LEN - (-tile_start  % LANE_PATTERN_LEN);
if (cell_offset < 0) cell_offset = TILE_CELLS       - (-cell_offset % TILE_CELLS);
```

This has **two bugs**:

### Bug 1 — C Truncates Toward Zero (sprites jump at tile boundaries)

`(int)` truncates toward zero, not toward negative infinity:

| expression        | C `(int)` | floor()  |
|-------------------|-----------|----------|
| `(int)(-0.5)`     | 0         | -1       |
| `(int)(-4.5)`     | -4        | -5       |
| `(int)(4.5)`      | 4         | 4 ✓      |

For **negative speeds** this makes `tile_start` stay at the same value for two
consecutive frames near integer crossings, then skip by 2. You see the sprite
stall then jump.

### Bug 2 — Cell-Granular Offset (8-pixel snapping)

`cell_offset` was in CELL units (0–7). Converting to pixels: `0,8,16,…,56`.
The sprite snaps to the nearest 8-pixel grid — you see it teleport by 8 pixels
at a time instead of gliding smoothly.

---

## 3. The Fix: `lane_scroll()` — Work in Pixels, Use Positive Modulo

```c
// frogger.h
static inline void lane_scroll(float time, float speed,
                                int *out_tile_start, int *out_px_offset) {
    const int PATTERN_PX = LANE_PATTERN_LEN * TILE_PX;  /* 64 × 64 = 4096 */

    int sc = (int)(time * speed * (float)TILE_PX) % PATTERN_PX;
    if (sc < 0) sc += PATTERN_PX;   /* always [0, PATTERN_PX) */

    *out_tile_start = sc / TILE_PX;
    *out_px_offset  = sc % TILE_PX;  /* 0..63 pixels */
}
```

#### How it works

1. **Multiply by `TILE_PX` (64) first** — convert tile units to pixel units
   before truncating with `(int)`. Now `sc` is a continuous integer-pixel
   position that advances smoothly every frame.

2. **Positive modulo** — `if (sc < 0) sc += PATTERN_PX` — maps any negative
   `sc` to the equivalent position in `[0, PATTERN_PX)`. No branches needed
   in the caller.

3. **Separate tile and sub-tile** — `tile_start = sc / TILE_PX` is the
   pattern index; `px_offset = sc % TILE_PX` is a **pixel** offset (0–63).

#### Speed at 60 fps

```
speed = 3.0, dt ≈ 0.016 s
pixel advance per frame = 3.0 × 64 × 0.016 = 3.07 pixels
```

Smooth, sub-pixel fractional advance every frame.

---

## 4. Using `lane_scroll()` in the Render Loop

```c
for (int y = 0; y < NUM_LANES; y++) {
    int tile_start, px_offset;
    lane_scroll(state->time, RENDER_LANE_SPEEDS[y], &tile_start, &px_offset);

    for (int i = 0; i < LANE_WIDTH; i++) {
        char c = RENDER_LANE_PATTERNS[y][(tile_start + i) % LANE_PATTERN_LEN];
        int dest_px = (-1 + i) * TILE_PX - px_offset;  /* pixels, may be < 0 */
        /* draw tile at dest_px ... */
    }
}
```

We start at tile `i = -1` so there is always a partial tile bleeding in from
the left edge. `dest_px` is allowed to be negative — the drawing functions
just clip to the window. As `px_offset` increases toward 64, the tiles slide
left one pixel at a time. When it wraps back to 0, `tile_start` increments by
1, and the pattern advances.

---

## 5. The Danger Buffer: Collision at Cell Resolution

Rendering is pixel-precise (sub-cell), but collision detection only needs
**cell** resolution (each cell is 8×8 px). We convert `px_offset` to cells:

```c
// frogger.c — frogger_tick
for (int y = 0; y < NUM_LANES; y++) {
    int tile_start, px_offset;
    lane_scroll(state->time, lane_speeds[y], &tile_start, &px_offset);
    int cell_offset = px_offset / CELL_PX;   /* 0..TILE_CELLS-1 */

    for (int i = 0; i < LANE_WIDTH; i++) {
        char c    = lane_patterns[y][(tile_start + i) % LANE_PATTERN_LEN];
        int  safe = tile_is_safe(c);
        int cx_start = (-1 + i) * TILE_CELLS - cell_offset;

        /* Each lane is TILE_CELLS (8) cell-rows tall.
           Lane y=9 is at cell rows 72..79 — NOT row 9!
           The dy loop fills all 8 rows so the frog starts on safe ground. */
        for (int dy = 0; dy < TILE_CELLS; dy++) {
            int cy = y * TILE_CELLS + dy;
            for (int cx = cx_start; cx < cx_start + TILE_CELLS; cx++) {
                if (cx >= 0 && cx < SCREEN_CELLS_W)
                    state->danger[cy * SCREEN_CELLS_W + cx] = (uint8_t)(!safe);
            }
        }
    }
}
```

> **Common mistake**: writing `state->danger[y * SCREEN_CELLS_W + cx]` — `y`
> is a *lane index* (0–9) not a *cell row*. Lane 9 = cell rows 72–79. Without
> the `dy` loop the entire lower screen stays at the `memset(1)` = deadly
> value → frog is dead on the first frame.

Same scroll math → collision grid always matches what's on screen exactly.

---

## 6. Collision: Center-Cell Check vs 4-Corner Check

### The 4-corner approach breaks on log edges

A first attempt checks the four inner corner cells of the frog's tile:

```c
// ❌ Breaks at log edges
int tl = danger[(base_cy+1)            * W + (base_cx+1)];
int tr = danger[(base_cy+1)            * W + (base_cx+TILE_CELLS-1)];
int bl = danger[(base_cy+TILE_CELLS-1) * W + (base_cx+1)];
int br = danger[(base_cy+TILE_CELLS-1) * W + (base_cx+TILE_CELLS-1)];
if (tl || tr || bl || br) → die;
```

The corner cells are 1 cell (8px) from the tile edge. Logs scroll at pixel
granularity but the danger buffer updates at cell granularity (8px steps). At
the exact frame a log's leading or trailing edge crosses a cell boundary, that
border cell flips to "dangerous". The frog's corner cells are exactly at that
boundary → **false death flash** every ~2.5 frames while riding a log.

### The fix: check the center cell only

```c
// ✓ Immune to tile-edge artifacts
int cx = (frog_px_x + TILE_PX/2) / CELL_PX;
int cy = (frog_px_y + TILE_PX/2) / CELL_PX;
if (danger[cy * SCREEN_CELLS_W + cx]) → die;
```

The center is `TILE_PX/2 = 32px = 4 cells` inset from every edge. A log would
have to be shorter than half a tile for the center check to be on a boundary
cell — no such tile exists in this game.

---

## 7. Render Precision: Don't Insert a Premature `(int)` Cast

When the frog rides a log, `frog_x` drifts as a float. A naive render:

```c
// ❌ Snaps to 8-pixel grid
int frog_px = (int)(frog_x * TILE_CELLS) * CELL_PX;
//                  ^^^^^^^^^^^^^^^^^^^
//   (int)(8.05 * 8) = 64  → pixel 512   (same as 8.0!)
//   (int)(8.12 * 8) = 65  → pixel 520   (jumped 8px suddenly!)
```

At river speed 3 (3 tiles/sec, ~3px/frame), the frog visually freezes for
~2.5 frames then jumps 8px — the "stuttering on log" bug.

The fix: move the `(int)` to the outermost position so truncation is in pixels:

```c
// ✓ 1-pixel precision
int frog_px = (int)(frog_x * (float)TILE_PX);
//   (int)(8.05 * 64) = (int)(515.2) = 515  — advances 3px every frame
```

`TILE_PX = TILE_CELLS * CELL_PX`. Multiplying first keeps all precision.

---

## 8. DOD Patterns Used This Lesson

| Pattern | Applied here |
|---------|-------------|
| Flat arrays for parallel data | `lane_speeds[]`, `lane_patterns[][]` separate |
| No per-entity structs in hot path | Danger buffer is a flat `uint8_t[]` |
| Math isolated in a helper | `lane_scroll()` used by both tick and render |
| State drives both update and render | Same `state->time` → same scroll position |

---

## 9. Exercises

1. Add a new lane with `speed = 5.0f` pointing right. Notice the scroll
   advances ~5 pixels/frame at 60 Hz.

2. Comment out the `dy` loop so the danger buffer only writes row `y` (not
   rows `y*TILE_CELLS .. y*TILE_CELLS+7`). Run the game — what happens?
   (Answer: frog is dead immediately because cell rows 72–79 stay at the
   initial danger=1 value.)

3. Change the collision back to the 4-corner check, then ride a log. Count
   how many frames pass between each flash. You should see it match the
   formula: `CELL_PX / (|speed| * TILE_PX * dt)` ≈ `8 / (3*64*0.016)` ≈ 2.6 frames.

---

*Next lesson: Sprite loading — reading the binary `.spr` format into our flat
`SpriteBank` pool.*
