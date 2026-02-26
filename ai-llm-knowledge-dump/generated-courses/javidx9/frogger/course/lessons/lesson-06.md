# Lesson 06 — Data-Oriented Design: Lane Data

## By the end of this lesson you will have:

An understanding of why `lane_speeds[]` and `lane_patterns[][]` are stored as separate arrays (DOD) instead of a `struct Lane` (AOS), and what this means for cache performance.

---

## The 10-Lane World

Frogger has 10 lanes, each with:
- A scroll speed (float)
- A 64-tile pattern string (64 chars)

Lane map (top = row 0, bottom = row 9):

```
Row 0: home row   — "wwwhhwww..." (wall/home tiles, speed 0)
Row 1: river      — ",,,jllk,..." (logs on water, speed -3)
Row 2: river      — ",,,,jllk,." (logs on water, speed +3)
Row 3: river      — ",,jlk,,,,." (logs on water, speed +2)
Row 4: pavement   — "pppppppppp" (safe strip, speed 0)
Row 5: road       — "....asdf.." (bus, speed -3)
Row 6: road       — ".....ty..." (car2, speed +3)
Row 7: road       — "..zx....." (car1, speed -4)
Row 8: road       — "..ty....." (car2, speed +2)
Row 9: pavement   — "pppppppppp" (safe start, speed 0)
```

---

## Step 1 — AOS vs DOD

### Array of Structures (AOS):
```c
typedef struct {
    float speed;
    char  pattern[65];
} Lane;

Lane lanes[10];
/* Size: 10 × (4 + 65) = 690 bytes */
```

When `frogger_tick` reads `lane_speeds` to compute platform riding:
```c
for (y = 0; y < 3; y++)
    state->frog_x -= lanes[y].speed * dt;
```

Memory access: `lanes[0]` (69 bytes), then jump 65 bytes of pattern to get `lanes[1].speed`, etc. Total touched: ~207 bytes for 3 speeds. Cache lines are 64 bytes. You load ~4 cache lines but only read 12 bytes of useful data (3 floats).

### Structure of Arrays (DOD):
```c
static const float lane_speeds  [10];       /* 40 bytes */
static const char  lane_patterns[10][65];   /* 650 bytes */
```

Accessing `lane_speeds[0..2]`: touches exactly 12 bytes — fits in one cache line. The 650 bytes of pattern data are never touched.

---

## Step 2 — The actual data

```c
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
```

**Speed units:** tiles per second. Speed `3.0f` at `TILE_PX=64` pixels/tile means the lane scrolls at `3 × 64 = 192` pixels/second.

**Sign convention:** negative = moves left, positive = moves right. This matches the frog-riding formula: `frog_x -= lane_speeds[row] * dt` pushes the frog in the same direction the lane moves.

```c
static const char lane_patterns[NUM_LANES][LANE_PATTERN_LEN + 1] = {
    "wwwhhwwwhhwwwhhwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww",
    ...
};
```

**Tile characters:**
| Char | Sprite | Safe? |
|------|--------|-------|
| `w` | wall | ✗ deadly |
| `h` | home | ✓ (goal) |
| `,` | water | ✗ deadly |
| `j/l/k` | log start/mid/end | ✓ |
| `p` | pavement | ✓ |
| `.` | road (black) | ✓ |
| `z/x` | car1 back/front | ✗ deadly |
| `t/y` | car2 back/front | ✗ deadly |
| `a/s/d/f` | bus segments | ✗ deadly |

---

## Step 3 — `static const` at file scope

```c
static const float lane_speeds[NUM_LANES] = { ... };
```

- `static`: the array is only visible in `frogger.c` — not accessible from other files
- `const`: data is read-only; the compiler can place it in `.rodata` (read-only segment, possibly in flash/ROM)
- No `static const` in C99 doesn't guarantee compile-time evaluation (unlike C++ `constexpr`), but the data is still placed in the binary and never modified

**Why not `#define` the speeds?**
`#define LANE_SPEED_1 (-3.0f)` would require 10 named constants and no loops. The array form allows `lane_speeds[y]` inside loops — essential for iterating all 10 lanes.

---

## Step 4 — Both arrays are used in two places

`lane_speeds` and `lane_patterns` are used in **both** `frogger_tick` (danger buffer rebuild) and `frogger_render` (drawing). Because both now live in `frogger.c`, there's no duplication.

The old code duplicated these arrays:
- `frogger.c` had `lane_speeds` + `lane_patterns`
- `main_x11.c` had `RENDER_LANE_SPEEDS` + `RENDER_LANE_PATTERNS` (exact copy)
- `main_raylib.c` had the same copies

Three copies of the same data → risk of one getting out of sync. The new architecture has exactly one copy in `frogger.c`.

---

## Key Concepts

- DOD (Structure of Arrays) vs AOS (Array of Structures): separate arrays improve cache locality when accessing one field across all elements
- `lane_speeds[10]` — 40 bytes, fits in one cache line; danger buffer and riding code only touch this
- `lane_patterns[10][65]` — 650 bytes; only touched during rendering
- `static const` at file scope: private to `frogger.c`, read-only, placed in `.rodata`
- Negative speed = moves left; `frog_x -= speed * dt` applies riding in the correct direction
- Old code duplicated lane data in 3 files — new code has exactly one copy

---

## Exercise

The danger buffer rebuild iterates all 10 lanes and all `LANE_WIDTH=18` tiles per lane. That's 180 iterations per frame. In the AOS `Lane` struct version, each iteration fetches `lane.speed` (offset 0) and `lane.pattern[tile_idx]` (offset 4+). How many cache lines does one pass over `lane.pattern` touch? Compare to the DOD version where `lane_patterns[y]` is a 65-byte array — how many cache lines does one row's pattern fit in?
