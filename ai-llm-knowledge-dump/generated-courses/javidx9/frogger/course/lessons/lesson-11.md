# Lesson 11 — Danger Buffer + Collision Detection

## By the end of this lesson you will have:

A cell-granular `danger` array that marks each screen cell as safe or deadly, collision detection using the frog's center cell, and win detection when the frog reaches a home tile.

---

## Why a Danger Buffer?

Direct sprite-overlap collision would require testing the frog's pixel rectangle against every car/log. With 10 lanes each up to 64 tiles wide, that's up to 640 tile checks per frame.

A pre-built danger buffer inverts this: one pass builds the map; one lookup detects collision. `O(SCREEN_CELLS)` build + `O(1)` lookup vs. `O(tiles_per_lane × num_lanes)` per frame.

```c
int8_t danger[SCREEN_CELLS_W * SCREEN_CELLS_H];
```

`1` = deadly, `0` = safe. Start dangerous, carve out safe tiles.

---

## Step 1 — Building the danger buffer

```c
void frogger_build_danger(GameState *state) {
    /* Default: all cells dangerous */
    memset(state->danger, 1, sizeof(state->danger));

    for (int y = 0; y < NUM_LANES; y++) {
        int tile_start, px_offset;
        lane_scroll(state->time, lane_speeds[y], &tile_start, &px_offset);
        (void)px_offset;   /* only need tile index, not pixel offset */

        for (int x = 0; x < SCREEN_CELLS_W; x++) {
            int pattern_pos = (tile_start + x) % LANE_PATTERN_LEN;
            char c = lane_patterns[y][pattern_pos];

            if (tile_is_safe(y, c))
                state->danger[y * SCREEN_CELLS_W + x] = 0;
        }
    }
}
```

`tile_is_safe(row, tile_char)` returns 1 for:
- Road rows: `.` (empty road, no car)
- River rows: `j`, `l`, `k` (log tiles)
- Home/median: always safe

The danger map is rebuilt every tick so it always reflects the current scroll position.

---

## Step 2 — `tile_is_safe` logic

```c
static int tile_is_safe(int row, char c) {
    if (row == 4 || row == 0) return 1;  /* median, home row */
    if (row >= 5) {
        /* road: safe on empty road, deadly on car tiles */
        return (c == '.');
    } else {
        /* river (rows 1-3): safe on log, deadly on water */
        return (c == 'j' || c == 'l' || c == 'k');
    }
}
```

**Home row (row 0):** all tiles are safe — the frog can land anywhere on the top row without dying.

**Median (row 4):** always safe — it's a divider with no hazards.

**Road (rows 5–9):** `.` is safe (empty road). Car tiles (`a`,`s`,`d`,`f` = bus; others = cars) are dangerous.

**River (rows 1–3):** log tiles safe. Everything else (water `'~'`, gap `'.'`) deadly.

---

## Step 3 — Center-cell collision check

```c
void frogger_check_collision(GameState *state) {
    if (state->dead || state->won) return;

    float frog_px_x = state->frog_x * TILE_PX;
    float frog_px_y = state->frog_y * TILE_PX;

    /* Center point of frog */
    int cx = (int)(frog_px_x + TILE_PX / 2.0f) / CELL_PX;
    int cy = (int)(frog_px_y + TILE_PX / 2.0f) / CELL_PX;

    if (cx < 0 || cx >= SCREEN_CELLS_W || cy < 0 || cy >= SCREEN_CELLS_H)
        return;

    if (state->danger[cy * SCREEN_CELLS_W + cx]) {
        state->dead = 1;
        state->dead_timer = DEAD_ANIM_TIME;
    }
}
```

**Why center-cell, not 4-corner?**

Four-corner detection (`cx0,cy0` to `cx3,cy3`) would check the corners of the frog's 8×8 pixel tile. At log edges, the log might cover 7 of those 8 pixels — the corner is technically off the log — causing phantom death at the visual edge.

Center-cell uses `frog_px + TILE_PX/2` → midpoint → one cell lookup. If the frog's center is on a log tile, frog is safe. This matches the visual impression: if you can see the frog on the log, you're safe.

**Log-edge flickering:**
The danger map uses integer cell indices, but log position is fractional (pixel scroll). A log tile that's 4 pixels past its cell boundary: `cx` hits it before `cx+1` clears it. Without center-cell, one frame of death is possible during that 4-pixel transition.

---

## Step 4 — Win detection

```c
void frogger_check_win(GameState *state) {
    int fy_int = (int)(state->frog_y + 0.5f);  /* nearest row */
    if (fy_int != 0) return;   /* not on home row */

    /* Which cell is the frog's center on? */
    int cx = (int)(state->frog_x * TILE_PX + TILE_PX / 2.0f) / CELL_PX;
    if (cx < 0 || cx >= SCREEN_CELLS_W) return;

    int pattern_pos = /* tile at cx on home row */;
    char c = lane_patterns[0][pattern_pos];

    if (c == 'h') {  /* home tile */
        state->homes_reached++;
        state->frog_x = START_X;
        state->frog_y = START_Y;

        if (state->homes_reached >= NUM_HOMES) {
            state->won = 1;
        }
    }
}
```

The home row has `'h'` at valid landing spots. Reaching row 0 on a non-`'h'` tile is still deadly (water between homes). `homes_reached` counts successful landings; `NUM_HOMES` completions triggers the win state.

---

## Step 5 — Interaction of all collision systems

```
frogger_tick:
  1. process_hop         (move frog)
  2. carry_river         (move frog on river)
  3. check_off_screen    (river out-of-bounds → dead)
  4. build_danger        (rebuild from current scroll)
  5. check_collision     (danger lookup → dead)
  6. check_win           (home row landing → win/reset)
```

`check_off_screen` runs before `build_danger` — an off-screen frog doesn't need a danger lookup, it's already dead.

`check_win` runs after `check_collision` — if the frog lands on a home tile that's somehow dangerous (e.g., an already-claimed home), collision kills it first.

---

## Key Concepts

- `danger[y * W + x]` — pre-built collision map, rebuilt every tick
- `memset(danger, 1, ...)` — start deadly, carve out safe tiles
- Center-cell lookup `(frog_px + TILE_PX/2) / CELL_PX` — avoids log-edge flickering
- `tile_is_safe(row, char)` — road uses empty-road check, river uses log-tile check
- Win detection: row 0 + `'h'` tile at frog's center cell
- Build order matters: carrying → off-screen kill → danger rebuild → collision check → win check

---

## Exercise

Change the collision check from center-cell to all-4-corners:
```c
int corners[4][2] = {
    {cx0, cy0}, {cx1, cy0}, {cx0, cy1}, {cx1, cy1}
};
for (int i = 0; i < 4; i++) {
    if (state->danger[corners[i][1] * W + corners[i][0]]) { /* dead */ }
}
```
Run the game and try crossing the river. Do you see more phantom deaths at log edges? Can you construct a scenario where center-cell succeeds but 4-corner fails?
