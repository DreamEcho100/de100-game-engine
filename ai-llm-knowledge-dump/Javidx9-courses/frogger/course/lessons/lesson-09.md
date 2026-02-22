# Lesson 9 — Danger Buffer and Collision Detection

**By the end of this lesson you will have:**
The frog resetting to the start whenever it touches water or a vehicle.
The game is now fully playable — hop across the road, ride logs across the river,
reach the home row.

---

## What is the danger buffer?

The danger buffer is `uint8_t danger[128 * 80]` inside `GameState`. It's a 2D
grid (128 wide, 80 tall) where:
- `0` = safe to stand on
- `1` = deadly

One entry per cell of the virtual screen. Every frame, `frogger_tick` rebuilds
this buffer based on the current scroll positions of all lanes. Then it checks
the frog's position against it.

**DOD perspective:**
The danger buffer is a flat byte array. Checking collision = reading 4 bytes
at computed indices. The CPU can do this in a few nanoseconds.

Compare to OOP: checking "is frog touching any car?" would loop over all cars
as objects, calling `car.getBounds()`, etc. A flat buffer makes it O(1).

---

## Step 1 — Filling the danger buffer

In `frogger_tick`, the buffer is rebuilt every frame using `lane_scroll()`
(the same helper used by `platform_render`, so collision always matches visuals):

```c
/* Default everything to deadly */
memset(state->danger, 1, SCREEN_CELLS_W * SCREEN_CELLS_H);

for (int y = 0; y < NUM_LANES; y++) {
    int tile_start, px_offset;
    lane_scroll(state->time, lane_speeds[y], &tile_start, &px_offset);
    int cell_offset = px_offset / CELL_PX;   /* 0..TILE_CELLS-1 */

    for (int i = 0; i < LANE_WIDTH; i++) {
        char c    = lane_patterns[y][(tile_start + i) % LANE_PATTERN_LEN];
        int  safe = tile_is_safe(c);
        int cx_start = (-1 + i) * TILE_CELLS - cell_offset;

        /* Each lane is TILE_CELLS (8) cell-rows tall.
           Lane y is at cell rows y*TILE_CELLS .. y*TILE_CELLS+7.
           The dy loop fills ALL 8 rows.                          */
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

The `tile_is_safe` function:
```c
static int tile_is_safe(char c) {
    return (c == '.' || c == 'j' || c == 'l' || c == 'k' ||
            c == 'p' || c == 'h');
}
```

Safe tiles: road (`.`), logs (`j`, `l`, `k`), pavement (`p`), home (`h`).
Deadly tiles: water (`,`), cars (`z`,`x`,`t`,`y`), bus (`a`,`s`,`d`,`f`), wall (`w`).

**The index formula:**
```
danger[cy * SCREEN_CELLS_W + cx]
     = danger[(y * TILE_CELLS + dy) * 128 + cx]

Example: lane y=9 (bottom pavement), dy=4, cx=64:
  cy    = 9 * 8 + 4 = 76
  index = 76 * 128 + 64 = 9792
```

> **Common mistake**: `danger[y * SCREEN_CELLS_W + cx]` — `y` is a *lane index*
> (0–9), not a *cell row*. Lane 9 = cell rows 72–79, not row 9. Without the
> `dy` loop the frog's starting rows are never cleared → instant death frame 1.

**Why rebuild every frame?**
Rebuilding is cheap: `memset` 10,240 bytes + 10 × 18 × 8 × 8 = 11,520 cell
writes. No caching needed.

---

## Step 2 — The collision check

After filling the buffer, we check the **center cell** of the frog sprite:

```c
int frog_px_x = (int)(state->frog_x * (float)TILE_PX);
int frog_px_y = (int)(state->frog_y * (float)TILE_PX);

/* Center of frog sprite: TILE_PX/2 = 32px = 4 cells inset from every edge.
   Far from any tile boundary — immune to log-edge cell artifacts.         */
int cx = (frog_px_x + TILE_PX / 2) / CELL_PX;
int cy = (frog_px_y + TILE_PX / 2) / CELL_PX;

if (state->danger[cy * SCREEN_CELLS_W + cx]) {
    state->dead       = 1;
    state->dead_timer = 0.4f;
    return;
}
```

**Visualizing the center-cell check:**

```
Frog pixel position (512, 320). TILE_PX=64, CELL_PX=8.

cx = (512 + 32) / 8 = 68   (col 68, inside tile columns 64-71)
cy = (320 + 32) / 8 = 44   (row 44, inside tile rows  40-47)

  tile boundary                  tile boundary
  col 64                         col 71
  |                              |
  ┌──┬──┬──┬──┬──┬──┬──┬──┐
  │  │  │  │  │  │  │  │  │ row 40
  ├──┼──┼──┼──┼──┼──┼──┼──┤
  │  │  │  │  │  │  │  │  │ row 41
  ├──┼──┼──┼──┼──┼──┼──┼──┤
  │  │  │  │  │  │  │  │  │ row 42
  ├──┼──┼──┼──┼──┼──┼──┼──┤
  │  │  │  │  │  │  │  │  │ row 43
  ├──┼──┼──┼──┼──┼──┼──┼──┤
  │  │  │  │  [X]│  │  │  │ row 44 ← cx=68, cy=44 checked
  ├──┼──┼──┼──┼──┼──┼──┼──┤
  ...
  └──┴──┴──┴──┴──┴──┴──┴──┘ row 47
```

**Why center-cell, not 4 corners?**

The danger buffer updates at 8px (cell) granularity but logs scroll at 1px.
At the frame a log edge crosses a cell boundary, that border cell flips dangerous.
Corner checks (1 cell from the tile edge) land on that border → false death
flash every ~2.5 frames while riding a log.

The center is 4 cells from every edge — safe from any boundary artifact.

---

## Step 3 — The death state

When `state->dead = 1`, `frogger_tick` stops processing input and logic:

```c
if (state->dead) {
    state->dead_timer -= dt;
    if (state->dead_timer <= 0.0f) {
        /* Flash done — reset frog */
        state->frog_x  = 8.0f;
        state->frog_y  = 9.0f;
        state->dead    = 0;
    }
    return;   /* skip everything else */
}
```

And `platform_render` shows a flash:
```c
if (state->dead) {
    int flash = (int)(state->dead_timer / 0.05f);
    show_frog = (flash % 2 == 0);   /* blink every 0.05s */
}
```

**New C concept — `return` in the middle of a function:**
`return;` in a void function exits immediately. Here it skips input handling and
collision checking while the death timer counts down.
JS analogy: same — `return;` exits the function.

---

## Build & Run

```sh
./build_x11.sh && ./frogger_x11
```

**What you should see:**
The frog can now die! Navigate into the road — a car hits you, frog flashes and
resets. Walk into the water — same. The game is playable.

**Reaching home:**
Navigate up through road (dodge cars) and river (ride logs) to row 0.
Land on an 'h' tile — `homes_reached` counter goes up and frog resets.
Land on 'w' tile — death.

---

## Mental Model

The danger buffer turns a complex spatial query ("is frog touching any moving object?")
into a simple array lookup. Pre-compute it once per frame, check 4 values.

```
Naive OOP approach:
  for each car in cars[]:
      if frog_rect.overlaps(car.rect): die
  → O(n) checks per frame, n grows with number of objects

Danger buffer approach:
  rebuild danger[] from all tiles  → O(lanes × tiles) once per frame
  check 4 cells                    → O(1) per frog, scales to N frogs
```

This scales: add 10 more frogs? Still just 4 array reads per frog.
Add 100 more cars? Still just rebuilding the same 10,240-byte buffer.

---

## Exercise

The danger buffer is `uint8_t` (1 byte per cell). What if instead you wanted to
store the exact tile character that's at each cell (so you could tell "water" from
"car" at collision time)?

Change `uint8_t danger[...]` to `char tile_at[SCREEN_CELLS_W * SCREEN_CELLS_H]`
in `GameState` (update both `.h` and the fill loop in `frogger_tick`). Then in
the collision check, instead of just dying, print what character killed the frog:

```c
/* In the fill loop, store the pattern char instead of 0/1: */
state->tile_at[cy * SCREEN_CELLS_W + cx] = c;   /* 'c' is the pattern char */

/* In the collision check: */
char what = state->tile_at[cy * SCREEN_CELLS_W + cx];
if (what != '.' && what != 'j' && what != 'l' && what != 'k' &&
    what != 'p' && what != 'h') {
    printf("Killed by: '%c'\n", what);
    state->dead = 1; state->dead_timer = 0.4f;
}
```

Revert to `uint8_t danger` when done (simpler is better for the final game).
