# Lesson 09 — Toroidal Space

## By the end of this lesson you will have:
A complete understanding of *why* objects and the lines that draw them wrap seamlessly across screen edges. You'll see that the wrapping already works for object *positions* (from Lesson 08) but that wireframe *lines* need per-pixel wrapping to avoid a diagonal slash across the screen when a polygon straddles two edges.

---

## Step 1 — The problem: a polygon that straddles an edge

**Why this matters:** `wrap_coordinates` keeps the *centre* of an object inside the screen. But a large asteroid near `x = 790` has its right side at roughly `x = 854` (radius 64). If we draw a line from `x = 786` to `x = 854`, the standard Bresenham algorithm dutifully draws pixels all the way across the middle of the screen — a long diagonal slash through everything. We need those pixels to appear on the *left* edge instead.

**JS analogy:** It's the same problem you hit in Canvas when you try to draw an image that partially exits the right edge — you'd have to draw it twice, clipped. Our solution is simpler: let every pixel wrap individually.

---

## Step 2 — `wrap_coordinates`: the four cases revisited

Here is the complete function from `asteroids.c` (introduced in Lesson 08, now examined in detail):

```c
static void wrap_coordinates(float *x, float *y) {
    if (*x <  0.0f)            *x += (float)SCREEN_W;
    if (*x >= (float)SCREEN_W) *x -= (float)SCREEN_W;
    if (*y <  0.0f)            *y += (float)SCREEN_H;
    if (*y >= (float)SCREEN_H) *y -= (float)SCREEN_H;
}
```

These are four **independent** `if` statements, not `if / else if`. That means a position like `x = -810` (two full widths off-screen) only gets corrected once. For this game that never happens because `dt` is small enough that an object can't travel more than one screen width per frame. The four cases are:

| Case | Condition | Action | Example |
|------|-----------|--------|---------|
| Left exit   | `x < 0`        | `x += 800` | `x = -5   → 795` |
| Right exit  | `x >= 800`     | `x -= 800` | `x = 803  → 3`   |
| Top exit    | `y < 0`        | `y += 600` | `y = -12  → 588` |
| Bottom exit | `y >= 600`     | `y -= 600` | `y = 601  → 1`   |

This is called a **torus topology** — mathematically identical to gluing the left edge to the right edge and the top edge to the bottom edge, forming a donut shape.

**Pac-Man metaphor:** When Pac-Man walks off the left side of the maze, he reappears on the right. That *corridor* connecting the two sides isn't drawn — the game just teleports him. Our screen works the same way in both axes simultaneously.

---

## Step 3 — `draw_pixel_w`: per-pixel toroidal wrapping

`draw_pixel_w` ("w" = wrapping) is called once per pixel from `draw_line`. It applies the same modular arithmetic to pixel coordinates before writing to the backbuffer:

```c
static void draw_pixel_w(AsteroidsBackbuffer *bb, int x, int y, uint32_t color) {
    x = ((x % bb->width)  + bb->width)  % bb->width;
    y = ((y % bb->height) + bb->height) % bb->height;
    bb->pixels[y * bb->width + x] = color;
}
```

**Why `(x % W + W) % W` and not just `x % W`?**

In C, the `%` operator on a negative number gives a negative result (unlike JavaScript, where `%` also gives a negative result — both languages agree here):

```
-3 % 800  →  -3   in C (and JS)
```

That would be a negative array index — undefined behaviour in C (and an out-of-bounds access in JS). Adding `W` before the second `%` fixes it:

```
((-3 % 800) + 800) % 800
= (-3 + 800) % 800
= 797 % 800
= 797   ✓
```

**Worked example — positive out-of-bounds:**
```
x = 803, W = 800
((803 % 800) + 800) % 800
= (3 + 800) % 800
= 803 % 800
= 3   ✓
```

---

## Step 4 — `draw_line`: Bresenham passes out-of-bounds coordinates through

`draw_line` does not clamp coordinates — it lets `draw_pixel_w` handle them:

```c
static void draw_line(AsteroidsBackbuffer *bb,
                      int x0, int y0, int x1, int y1,
                      uint32_t color)
{
    int dx  =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy  = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        draw_pixel_w(bb, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
```

The algorithm just steps `x0` from start to end one pixel at a time. When `x0` goes past 799, `draw_pixel_w` wraps it to the left side. The line appears to "continue" on the opposite edge.

---

## Step 5 — Tracing the wrapping line example

**Scenario:** Screen width = 800. We draw a horizontal line from `x = 795` to `x = 810` (15 pixels wide), `y = 300`.

The Bresenham loop emits pixels at these x values: `795, 796, 797, 798, 799, 800, 801, 802, 803, 804, 805, 806, 807, 808, 809, 810`.

`draw_pixel_w` wraps each one:

```
795 → 795   (in bounds)
796 → 796
797 → 797
798 → 798
799 → 799   (last in-bounds pixel on the right edge)
800 → 0     ← wraps!
801 → 1
802 → 2
...
810 → 10
```

**Result:** Pixels 795–799 appear on the right side; pixels 0–10 appear on the left side. The line is split at the edge and continues seamlessly — exactly what you see when a large asteroid sits near the right wall with part of its ring overlapping the left edge.

---

## Step 6 — Why `draw_wireframe` needs no changes

`draw_wireframe` simply calls `draw_line` with the transformed vertex coordinates. Those coordinates are floating-point world positions — they can be outside `[0, SCREEN_W)`. `draw_line` passes them straight to `draw_pixel_w`, which wraps them. No special-casing needed in the polygon code.

```c
for (int i = 0; i < n_verts; i++) {
    int j = (i + 1) % n_verts;
    draw_line(bb,
              (int)t[i].x, (int)t[i].y,
              (int)t[j].x, (int)t[j].y,
              color);
}
```

The wrapping is entirely contained inside `draw_pixel_w`. That's good design: the caller doesn't need to know about screen bounds at all.

---

## Build & Run

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11 -lX11 -lxkbcommon -lGL -lGLX -lm
./build/asteroids_x11
```

**What you should see:** Thrust the ship into a corner, then watch it emerge from the opposite edge with no glitching. If there are large asteroids on screen (added in Lesson 11+), their wireframe rings will visibly split across both edges at once. No stray diagonal lines across the middle of the screen.

---

## Key Concepts

- **Torus topology** — connecting opposite edges mathematically, not visually; objects teleport instead of clip.
- **`(x % W + W) % W`** — the standard C idiom for a non-negative modulo that handles negative inputs correctly.
- **Per-pixel wrapping** — `draw_pixel_w` is called for every pixel of every line; the wrapping cost is two integer additions and two modulos per pixel.
- **Separation of concerns** — `draw_wireframe` is unaware of screen bounds; `draw_pixel_w` owns all wrapping logic.
- **Pac-Man corridor** — the conceptual model: the right edge of the screen and the left edge are the same corridor, just shown in two different places.

---

## Exercise

Change `draw_pixel_w` to use simple clamping instead of wrapping:

```c
static void draw_pixel_w(AsteroidsBackbuffer *bb, int x, int y, uint32_t color) {
    if (x < 0 || x >= bb->width || y < 0 || y >= bb->height) return;
    bb->pixels[y * bb->width + x] = color;
}
```

Build and run. Steer an asteroid near the right edge. You should now see the polygon getting clipped — part of the ring disappears instead of reappearing on the left. This demonstrates exactly why per-pixel wrapping is necessary. Restore the original modulo version afterwards.
