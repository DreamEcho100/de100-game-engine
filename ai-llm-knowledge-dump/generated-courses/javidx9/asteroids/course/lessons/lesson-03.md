# Lesson 03 — Bresenham's Line Algorithm

## By the end of this lesson you will have:
The game draws straight white lines between any two pixel coordinates. You will see the ship's triangular wireframe and the asteroid outlines formed from many connected line segments. When a line crosses the screen edge — say, from x=790 to x=810 — it wraps seamlessly: pixels appear on the right edge *and* continue from the left edge, because every pixel call goes through `draw_pixel_w`.

---

## Step 1 — Why not just use `y = mx + b`?

The naive approach to drawing a line between (x0,y0) and (x1,y1) is:

```
slope = (y1 - y0) / (x1 - x0)
for x from x0 to x1:
    y = round(y0 + slope * (x - x0))
    drawPixel(x, y)
```

This has two problems:

1. **Floating-point rounding** introduces tiny errors. Pixels end up at slightly wrong positions, and the result looks slightly different on different CPUs due to floating-point precision differences.
2. **Steep lines break it.** When `|dy| > |dx|`, iterating over x produces gaps — consecutive x values map to y values more than 1 pixel apart.

Bresenham's algorithm fixes both: it uses only integers and always advances along the **major axis** (the axis with the larger span).

---

## Step 2 — The error term idea

Imagine walking from (2, 1) to (8, 4). The "ideal" line has slope 3/6 = 0.5. At each x step, the true y moves 0.5 pixels. We can't draw half a pixel, so we need to decide when to step y.

The insight: track accumulated fractional error as an integer, scaled up so we never divide. When it exceeds half a pixel, step.

Let's trace it manually with real numbers:

```
From (2,1) to (8,4):
  dx = |8-2| = 6    (positive, moving right)
  dy = |4-1| = 3    (positive, moving down)
  sx = +1            (x moves right)
  sy = +1            (y moves down)

  Bresenham initializes: err = dx + (-dy) = 6 + (-3) = 3
  Note: dy is stored as NEGATIVE in the algorithm for the error update.

Pixel 1: draw (2,1)
  e2 = 2*err = 6
  e2 >= dy(-3)?  6 >= -3 → YES → err += -3 = 0,  x += 1 → x=3
  e2 <= dx(6)?   6 <= 6  → YES → err += 6  = 6,  y += 1 → y=2

Pixel 2: draw (3,2)
  e2 = 2*6 = 12
  12 >= -3 → YES → err += -3 = 3,  x=4
  12 <= 6  → NO

Pixel 3: draw (4,2)
  e2 = 2*3 = 6
  6 >= -3 → YES → err += -3 = 0,  x=5
  6 <= 6  → YES → err += 6  = 6,  y=3

Pixel 4: draw (5,3)
  e2 = 12
  12 >= -3 → YES → err += -3 = 3, x=6
  12 <= 6  → NO

Pixel 5: draw (6,3)
  e2 = 6
  6 >= -3 → YES → err += -3 = 0, x=7
  6 <= 6  → YES → err += 6 = 6, y=4

Pixel 6: draw (7,4)
  e2 = 12
  12 >= -3 → YES → err += -3 = 3, x=8
  12 <= 6  → NO

Pixel 7: draw (8,4) → x == x1 and y == y1 → BREAK
```

Result pixels: (2,1), (3,2), (4,2), (5,3), (6,3), (7,4), (8,4) — a smooth staircase approximating the true line.

---

## Step 3 — The exact code

```c
static void draw_line(AsteroidsBackbuffer *bb,
                      int x0, int y0, int x1, int y1,
                      uint32_t color)
{
    int dx  =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy  = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy; /* accumulated error term */

    for (;;) {
        draw_pixel_w(bb, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
```

**Line by line:**

`dx = abs(x1 - x0)` — total horizontal span (always positive).  
`dy = -abs(y1 - y0)` — total vertical span, stored **negative** by convention. This is key: it means adding `dy` to `err` always decreases it, and adding `dx` always increases it. The error term balances them.

`sx = x0 < x1 ? 1 : -1` — which direction to step x (right or left).  
`sy = y0 < y1 ? 1 : -1` — which direction to step y (down or up).

`err = dx + dy` — initial error. Since `dy` is negative, this is `dx - abs(dy)`. It represents how far ahead we are in the x direction relative to y. A positive starting value means we need to catch up vertically.

**The step logic:**
```c
int e2 = 2 * err;
if (e2 >= dy) { err += dy; x0 += sx; }  // advance along x
if (e2 <= dx) { err += dx; y0 += sy; }  // advance along y
```

Both conditions can be true simultaneously — this happens when the line is exactly diagonal (45°). That's correct: we step both x and y in the same iteration.

**Why `2 * err`?** This is the standard Bresenham formulation. Doubling eliminates the need for a `0.5` comparison. The check `e2 >= dy` (where dy is negative) is equivalent to checking whether the fractional part exceeds half.

**JS analogy:** Think of `err` as the "debt" accumulator in a rate-limiting algorithm. Every step you earn a credit (`dx`) or spend a credit (`|dy|`). When your credit exceeds the threshold, you take a step.

---

## Step 4 — Toroidal wrapping in draw_line

`draw_line` takes coordinates that can be outside the screen bounds. Every pixel in the loop calls `draw_pixel_w(bb, x0, y0, color)`, and `draw_pixel_w` wraps `x0` and `y0` back into range.

**Example:** Drawing from (790, 300) to (810, 300) on an 800-wide screen.

`dx = 20, dy = 0` — a horizontal line.  
`draw_pixel_w` will be called at x = 790, 791, ..., 799, 800, 801, ..., 810.

For x=800: `((800 % 800) + 800) % 800 = (0 + 800) % 800 = 0` → draws at x=0.  
For x=801: `((801 % 800) + 800) % 800 = (1 + 800) % 800 = 1` → draws at x=1.  
...and so on.

The line emerges on the left side of the screen at the same y. The asteroid crosses the edge seamlessly.

**Why do it per-pixel instead of clipping the line to screen bounds?**  
Clipping is harder to implement and would require splitting the line into two segments at the boundary. Per-pixel wrapping is simpler and costs essentially nothing (two integer mod operations per pixel).

---

## Build & Run

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11 -lX11 -lxkbcommon -lGL -lGLX -lm
./build/asteroids_x11
```

**What you should see:** The white ship triangle and yellow asteroid outlines drawn on screen. Try pressing the arrow keys to move the ship toward an edge — when it reaches the boundary, it wraps around and continues from the other side.

---

## Key Concepts

- Bresenham's algorithm uses only integer addition and no division — it is pixel-exact and fast.
- `dx = abs(x1-x0)`, `dy = -abs(y1-y0)` — dy is stored negative by convention.
- `err = dx + dy` — the accumulated error term that decides when to step each axis.
- `e2 >= dy` and `e2 <= dx` can both be true simultaneously for 45° diagonals.
- Every pixel in `draw_line` calls `draw_pixel_w`, which toroidally wraps the coordinate.
- Wrapping per-pixel means lines crossing screen edges render correctly on both sides with no special-case clipping code.

## Exercise

Add a temporary call in `asteroids_render` to draw a yellow diagonal line from corner (0,0) to corner (799,599):

```c
draw_line(bb, 0, 0, 799, 599, COLOR_YELLOW);
```

Run the game and observe the diagonal. Then change the endpoint to (1000, 800) and observe that the line wraps onto the screen from both the bottom and right edges. Remove the temporary line before moving on.
