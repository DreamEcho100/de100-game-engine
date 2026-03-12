# Lesson 04 — Drawing Primitives

## Overview

| Item | Value |
|------|-------|
| **Backend** | Both |
| **New concepts** | `draw_rect` (clip + fill), `draw_rect_blend` (alpha composite), pitch-based row arithmetic |
| **Observable outcome** | Colored rectangles visible; semi-transparent overlay blends correctly |
| **Files created** | `src/utils/math.h`, `src/utils/draw-shapes.h`, `src/utils/draw-shapes.c` |
| **Files modified** | `src/game/demo.c` |

---

## What you'll build

Two utility functions (`draw_rect` and `draw_rect_blend`) that write pixel-aligned rectangles into the backbuffer.  Everything in this game engine is built on top of these two primitives.

---

## Background

### JS analogy

`draw_rect(bb, x, y, w, h, color)` is like `ctx.fillRect(x, y, w, h)` on a 2D canvas — except we're writing directly into a `Uint32Array` of pixel data instead of calling a browser API.

`draw_rect_blend` is the equivalent of setting `ctx.globalAlpha` before drawing.

### Why clip before writing?

If a rectangle extends outside the canvas (e.g., x = -5), we'd write before the start of the pixel array — a classic buffer overrun.  Clipping to `[0, width] × [0, height]` prevents this.

### The pitch stride trick

```c
int stride = bb->pitch / bb->bytes_per_pixel;
uint32_t *row = bb->pixels + y * stride + x;
```

Multiplying `row_index × stride` instead of `row_index × width` handles the case where the platform aligns rows to 16-byte boundaries (common in production renderers).  It costs nothing and is always correct.

### `math.h` macros

```c
#define MIN(a, b)   ((a) < (b) ? (a) : (b))
#define MAX(a, b)   ((a) > (b) ? (a) : (b))
#define CLAMP(v, lo, hi)  MIN(MAX(v, lo), hi)
#define ABS(x)       ((x) < 0 ? -(x) : (x))
```

These expand inline — no function call overhead.  Each argument is evaluated once (no side-effect double-evaluation risk if you avoid `++i` inside).

---

## What to type

### `src/utils/math.h`

```c
#ifndef UTILS_MATH_H
#define UTILS_MATH_H
#define MIN(a, b)         ((a) < (b) ? (a) : (b))
#define MAX(a, b)         ((a) > (b) ? (a) : (b))
#define CLAMP(val, lo, hi) MIN(MAX(val, lo), hi)
#define ABS(x)            ((x) < 0 ? -(x) : (x))
#endif
```

### `src/utils/draw-shapes.h`

```c
#ifndef UTILS_DRAW_SHAPES_H
#define UTILS_DRAW_SHAPES_H
#include <stdint.h>
#include "utils/backbuffer.h"

void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);
#endif
```

### `src/utils/draw-shapes.c` — `draw_rect`

```c
#include "utils/draw-shapes.h"
#include "utils/math.h"

void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
    if (!bb || !bb->pixels) return;

    // Clip to backbuffer bounds
    int x0 = MAX(x, 0);
    int y0 = MAX(y, 0);
    int x1 = MIN(x + w, bb->width);
    int y1 = MIN(y + h, bb->height);
    if (x0 >= x1 || y0 >= y1) return;

    int stride = bb->pitch / bb->bytes_per_pixel;
    for (int row = y0; row < y1; row++) {
        uint32_t *dst = bb->pixels + row * stride + x0;
        for (int col = x0; col < x1; col++) {
            *dst++ = color;
        }
    }
}
```

### `draw_rect_blend` — alpha composite

```c
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
    int src_a = (color >> 24) & 0xFF;
    if (src_a == 0)   return;
    if (src_a == 255) { draw_rect(bb, x, y, w, h, color); return; }

    int x0 = MAX(x, 0), y0 = MAX(y, 0);
    int x1 = MIN(x+w, bb->width), y1 = MIN(y+h, bb->height);
    if (x0 >= x1 || y0 >= y1) return;

    int inv_a = 255 - src_a;
    int src_r = (color      ) & 0xFF;
    int src_g = (color >>  8) & 0xFF;
    int src_b = (color >> 16) & 0xFF;
    int stride = bb->pitch / bb->bytes_per_pixel;

    for (int row = y0; row < y1; row++) {
        uint32_t *dst = bb->pixels + row * stride + x0;
        for (int col = x0; col < x1; col++) {
            uint32_t d  = *dst;
            int dr = (d      ) & 0xFF;
            int dg = (d >>  8) & 0xFF;
            int db = (d >> 16) & 0xFF;
            int out_r = (src_r * src_a + dr * inv_a) >> 8;
            int out_g = (src_g * src_a + dg * inv_a) >> 8;
            int out_b = (src_b * src_a + db * inv_a) >> 8;
            *dst++ = (uint32_t)out_r | ((uint32_t)out_g<<8) |
                     ((uint32_t)out_b<<16) | 0xFF000000u;
        }
    }
}
```

### `src/game/demo.c` — L04 additions

```c
void demo_render(Backbuffer *bb, GameInput *input, int fps) {
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BG);

    // L04 — solid rectangles
    draw_rect(bb, 50,  50, 200, 100, COLOR_RED);
    draw_rect(bb, 300, 50, 200, 100, COLOR_GREEN);
    draw_rect(bb, 550, 50, 200, 100, COLOR_BLUE);

    // L04 — semi-transparent yellow overlay
    draw_rect_blend(bb, 150, 30, 200, 140, GAME_RGBA(255, 255, 0, 120));
}
```

---

## Explanation

| Concept | Detail |
|---------|--------|
| **Two-step clip** | Compute `(x0,y0)` and `(x1,y1)`, clamp both, early-exit if empty |
| **`stride = pitch / bpp`** | Number of uint32_t slots per row — safe for padded rows |
| **Alpha formula** | `out = (src×a + dst×(255-a)) >> 8` — fast integer approximation of `src×(a/255) + dst×(1-a/255)` |
| **`>> 8` vs `/ 255`** | The `>> 8` divides by 256 (≈ 0.4% error).  Acceptable for a game; production code uses `/255` or a lookup table |
| **`0xFF000000u` in blend** | Forces alpha to 255 in the output (opaque result) |

---

## Build and run

```sh
./build-dev.sh --backend=raylib -r
```

You should see three colored rectangles with a semi-transparent yellow overlay spanning the middle one.

---

## Quiz

1. Why does `draw_rect` compute `x1 = MIN(x + w, bb->width)` rather than `x1 = x + w`?
2. The alpha blend uses `>> 8` instead of `/ 255`.  What is the maximum per-channel error?  Does it matter for 8-bit color?
3. Why is `stride = pitch / bytes_per_pixel` safer than `stride = width`?
