# Lesson 5 — Lane Data and DOD Layout

**By the end of this lesson you will have:**
All 10 lanes drawn statically on screen — pavement at top and bottom, water tiles
in the middle rows, road (black) below that. No scrolling yet, but the full game
board layout is visible.

---

## What is Data-Oriented Design (DOD)?

The original C++ code used:
```cpp
vector<pair<float, wstring>> vecLanes = {
    {0.0f, L"wwwhhwww..."},
    {-3.0f, L",,,jllk..."},
    ...
};
```

Each element bundles a speed (float, 4 bytes) with a pattern (64+ bytes) into one
pair. When you access just speeds in a loop, you skip over 64 bytes of pattern data
per element — wasting CPU cache.

Our C port separates them:
```c
static const float lane_speeds[10];           /* 40 bytes — one cache line! */
static const char  lane_patterns[10][65];     /* 650 bytes — separate       */
```

**Why does cache matter?**
The CPU cache holds 64 bytes at a time (one "cache line"). When you read
`lane_speeds[0]`, the CPU fetches bytes 0-63 of the array — loading 16 floats at
once, all ready for use. With the bundled struct, 64 of those bytes would be pattern
data you don't need yet.

JS analogy: like the difference between iterating a flat `Float32Array` of speeds
vs an array of objects `[{speed: 0.0, pattern: "www..."}, ...]`.

---

## Step 1 — Read the lane data in `frogger.c`

Open `src/frogger.c` and find the two arrays near the top:

```c
/* DOD: speeds separate from patterns — cache-friendly when looping speeds */
static const float lane_speeds[NUM_LANES] = {
     0.0f,  /* 0 home row    */
    -3.0f,  /* 1 river left  */
     3.0f,  /* 2 river right */
     2.0f,  /* 3 river right */
     0.0f,  /* 4 pavement    */
    -3.0f,  /* 5 road left   */
     3.0f,  /* 6 road right  */
    -4.0f,  /* 7 road left   */
     2.0f,  /* 8 road right  */
     0.0f,  /* 9 pavement    */
};

static const char lane_patterns[NUM_LANES][LANE_PATTERN_LEN + 1] = {
    "wwwhhwwwhhwwwhhwww...",   /* row 0: wall (w) and home (h) */
    ",,,jllk,,jllllk,,,",      /* row 1: water (,) and logs (j/l/k) */
    ...
};
```

**Tile character legend:**
```
w = wall (top border, deadly)        h = home (goal, safe)
, = water (deadly)                   j/l/k = log start/mid/end (safe)
p = pavement (safe)
. = road  (safe, rendered black)     a/s/d/f = bus (deadly)
z/x = car1 back/front (deadly)       t/y = car2 back/front (deadly)
```

**New C concept — `static const` array at file scope:**
This lives in the `.rodata` (read-only data) segment of the binary. It's loaded
once when the program starts and never changes. No heap, no stack.
JS analogy: like `Object.freeze([...])` but enforced at compile time.

---

## Step 2 — The tile-to-sprite mapping

The platform render functions contain `local_tile_to_sprite()`. Read it:

```c
static int local_tile_to_sprite(char c, int *src_x) {
    *src_x = 0;          /* default: start at left edge of sprite */
    switch (c) {
        case 'w': return SPR_WALL;
        case 'h': return SPR_HOME;
        case ',': return SPR_WATER;
        case 'p': return SPR_PAVEMENT;
        case 'j': *src_x =  0; return SPR_LOG;   /* log start  */
        case 'l': *src_x =  8; return SPR_LOG;   /* log middle */
        case 'k': *src_x = 16; return SPR_LOG;   /* log end    */
        ...
    }
    return -1;  /* road '.' = black, no sprite */
}
```

**New C concept — `int *src_x` (output parameter):**
C can only return one value. To return two values, we pass a pointer and write
to it with `*src_x = value`. The caller passes `&src_x` (address of their variable).
JS analogy: like passing an object and mutating it — `obj.src_x = value`.

**New C concept — `switch/case`:**
Same as JS. Matches `c` against each case. Falls through unless `return` or `break`.

**Why `src_x`?**
The log sprite is 24 cells wide (3 tiles side-by-side). `j`, `l`, `k` each occupy
8 cells within it. `src_x` says "start reading from this column offset":
- `j` → `src_x = 0`  (left 8 cells)
- `l` → `src_x = 8`  (middle 8 cells)
- `k` → `src_x = 16` (right 8 cells)

The bus similarly has 4 segments (`a/s/d/f`) in a 32-cell-wide sheet.

---

## Step 3 — Read the lane rendering loop

In `platform_render()` (both platform files):

```c
for (int y = 0; y < NUM_LANES; y++) {
    int tile_start, px_offset;
    /* lane_scroll() handles time-based scroll correctly for all speeds.
       At state->time == 0, tile_start=0 and px_offset=0 — no scrolling. */
    lane_scroll(state->time, RENDER_LANE_SPEEDS[y], &tile_start, &px_offset);

    int dest_py = y * TILE_PX;  /* pixel Y for this lane */

    for (int i = 0; i < LANE_WIDTH; i++) {
        char c    = RENDER_LANE_PATTERNS[y][(tile_start + i) % LANE_PATTERN_LEN];
        int  src_x = 0;
        int  spr   = local_tile_to_sprite(c, &src_x);
        /* Sub-tile pixel offset — smooth scrolling once time > 0 */
        int  dest_px = (-1 + i) * TILE_PX - px_offset;

        if (spr >= 0) {
            draw_sprite_partial(&state->sprites, spr,
                src_x, 0, TILE_CELLS, TILE_CELLS,
                dest_px, dest_py);
        }
    }
}
```

At `state->time = 0`, `lane_scroll` returns `tile_start=0, px_offset=0`, so
the loop draws the first 18 tiles of each pattern with no offset — the static
board. Once `time` advances (Lesson 6), the tiles start scrolling.

---

## Step 4 — Verify the game board visually

At `state->time = 0`, the first 18 characters of each pattern are drawn:

```
Row 0: wwwhhwwwhhwwwhhwww  → wall-wall-wall-home-home-wall-...
Row 1: ,,,jllk,,jllllk,,, → water-water-water-log-log-log-...
Row 4: pppppppppppppppppp  → all pavement
Row 9: pppppppppppppppppp  → all pavement
```

---

## Build & Run

```sh
./build_x11.sh && ./frogger_x11
```

**What you should see:**
- Row 0 (top): gray wall tiles + gold home slots
- Rows 1-3: cyan water tiles with brown/green log tiles
- Row 4: gray pavement
- Rows 5-8: black (road)
- Row 9 (bottom): gray pavement
- Frog sitting in the bottom pavement at tile (8,9)

Nothing moves yet — that's Lesson 6.

---

## Mental Model

The lane rendering loop in pseudocode:
```
for each lane row y (0..9):
    pixel_y = y × 64

    for each tile slot i (0..17):
        tile_char = pattern[y][ tile_index ]
        sprite_id = lookup(tile_char)
        pixel_x   = (i - 1) × 64    ← starts one tile left of screen

        draw sprite at (pixel_x, pixel_y)
```

The `(i - 1)` offset is why we draw `LANE_WIDTH = 18` tiles for a screen that
only fits `128 / 8 = 16` tiles — the extra 2 tiles are for the left-edge bleed.

---

## Exercise

Look at lane 5's pattern: `"....asdf.......asdf....asdf..."`
Count how many tiles until the first bus (`asdf`). Starting at i=4, the `a` is at
position 4. What does `tile_start + 4` give when `state->time = 0`? (Answer: 4.)

Change `state->frog_x = 8.0f` in `frogger_init` to `state->frog_x = 0.0f`.
Rebuild. Where does the frog appear? Verify with the formula.
Restore to 8.0f when done.
