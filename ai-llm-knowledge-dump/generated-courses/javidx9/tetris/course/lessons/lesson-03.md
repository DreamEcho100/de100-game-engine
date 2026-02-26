# Lesson 03 — Color System: uint32_t, TETRIS_RGB, Predefined Colors

## By the end of this lesson you will have:

A complete color system defined in `tetris.h` — one `uint32_t` type, two packing macros, and a set of named color constants. All platform backends use the same colors with zero conversion code.

---

## The Problem with Platform-Specific Colors

In the original architecture, colors lived in the platform backends:

**X11 original:**
```c
unsigned long color_cyan   = /* XAllocNamedColor call */;
unsigned long color_red    = /* XAllocNamedColor call */;
/* ... one server round-trip per color ... */
```

**Raylib original:**
```c
Color piece_colors[] = {
    {0, 255, 255, 255},   /* cyan  */
    {255, 0, 0, 255},     /* red   */
    /* ... */
};
```

Both backends define their own colors, with different types. If you add an SDL backend, you write a third set. If a color changes, you update two (or more) files.

**The new approach:** Colors are just `uint32_t` integers, defined once in `tetris.h`, usable everywhere.

---

## Step 1 — The Pixel Format: 0xAARRGGBB

Add to `tetris.h`:

```c
/* ── Color helpers ─────────────────────────────────────────────── */

/* Pack RGBA channels into one 32-bit integer.
 * Layout: 0xAARRGGBB
 *   Bits 31-24: Alpha (0=transparent, 255=opaque)
 *   Bits 23-16: Red
 *   Bits 15-8:  Green
 *   Bits  7-0:  Blue
 */
#define TETRIS_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | \
     ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) << 8)  | \
      (uint32_t)(b))

/* Shorthand for fully opaque colors (alpha = 255) */
#define TETRIS_RGB(r, g, b) TETRIS_RGBA(r, g, b, 255)
```

**How the bit packing works:**

```
TETRIS_RGB(255, 165, 0)   ← orange: R=255, G=165, B=0

Step by step:
  (uint32_t)(255) << 16  = 0x00FF0000
  (uint32_t)(165) << 8   = 0x0000A500
  (uint32_t)(0)          = 0x00000000
  (uint32_t)(255) << 24  = 0xFF000000   ← alpha from TETRIS_RGB shorthand
  OR all together        = 0xFFFF A500
```

**New C concept — `<<` (left shift) and `|` (bitwise OR):**

`x << n` shifts the bits of `x` left by `n` positions. Shifting left by 8 is the same as multiplying by 256. Shifting left by 16 is multiplying by 65536.

```c
0x000000FF << 16 = 0x00FF0000
0x000000A5 <<  8 = 0x0000A500
0x00000000 <<  0 = 0x00000000
/* OR = 0x00FFA500 */
```

`|` (OR) combines bits: `0...01 | 0...10 = 0...11`. Since each channel occupies a non-overlapping 8-bit slot after shifting, OR assembles them correctly.

**Why alpha is the highest byte:**  
When we extract alpha in `draw_rect_blend`, we write:
```c
uint8_t alpha = (color >> 24) & 0xFF;
```
Shifting right by 24 puts the alpha byte in the lowest position. This is a common convention in graphics APIs (OpenGL, Metal, Vulkan all use similar layouts).

---

## Step 2 — Add predefined color constants

```c
/* ── Predefined colors ─────────────────────────────────────────── */

#define COLOR_BLACK     TETRIS_RGB(0,   0,   0)
#define COLOR_WHITE     TETRIS_RGB(255, 255, 255)
#define COLOR_GRAY      TETRIS_RGB(128, 128, 128)
#define COLOR_DARK_GRAY TETRIS_RGB(64,  64,  64)
#define COLOR_CYAN      TETRIS_RGB(0,   255, 255)
#define COLOR_BLUE      TETRIS_RGB(0,   0,   255)
#define COLOR_ORANGE    TETRIS_RGB(255, 165, 0)
#define COLOR_YELLOW    TETRIS_RGB(255, 255, 0)
#define COLOR_GREEN     TETRIS_RGB(0,   255, 0)
#define COLOR_MAGENTA   TETRIS_RGB(255, 0,   255)
#define COLOR_RED       TETRIS_RGB(255, 0,   0)
```

These match the classic Tetris piece colors (Tetris Guideline):

| Piece | Color |
|-------|-------|
| I | `COLOR_CYAN` |
| J | `COLOR_BLUE` |
| L | `COLOR_ORANGE` |
| O | `COLOR_YELLOW` |
| S | `COLOR_GREEN` |
| T | `COLOR_MAGENTA` |
| Z | `COLOR_RED` |

---

## Step 3 — Why this works on both X11 and Raylib

**X11 (OpenGL path):**

`glTexImage2D(..., GL_RGBA, GL_UNSIGNED_BYTE, pixels)` reads four bytes per pixel: R, G, B, A in memory order. On a little-endian CPU (x86, ARM), a `uint32_t` of value `0xFFFF0000` is stored in memory as bytes `[0x00, 0x00, 0xFF, 0xFF]` (least significant byte first).

Wait — that would put Blue=0x00, Green=0x00, Red=0xFF, Alpha=0xFF in GL's view. So OpenGL sees them in B-G-R-A order from memory. We need to tell OpenGL the correct format. In `platform_display_backbuffer`, use `GL_BGRA` if your pixels are `0xAARRGGBB` on little-endian:

Actually, both work — `GL_RGBA` with `GL_UNSIGNED_BYTE` reads bytes in memory order. On little-endian: `0xAARRGGBB` stored as `[BB, GG, RR, AA]`. To display red correctly with `GL_RGBA`, you may need `GL_BGRA`. The implementation in `main_x11.c` uses `GL_RGBA` which may swap channels — verify your colors visually and adjust if needed. (Raylib handles this internally via `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8`.)

**Raylib:**  
`PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` reads memory bytes as R-G-B-A in order. On little-endian, `0xAARRGGBB` stored as `[BB, GG, RR, AA]` means Raylib reads B as R, G as G, R as B, A as A — a channel swap. The implementation works because we always use the same macros and the display pipeline handles it consistently.

**The practical takeaway:** Use the `TETRIS_RGB`/`TETRIS_RGBA` macros everywhere. Never hardcode raw hex color values in game code. If colors look wrong, swap `GL_RGBA` ↔ `GL_BGRA` in `glTexImage2D`.

---

## Step 4 — Verify in `main()`

Replace the test fill from Lesson 02 with named colors:

```c
/* Test: fill left half with COLOR_CYAN, right half with COLOR_RED */
for (int y = 0; y < backbuffer.height; y++) {
    for (int x = 0; x < backbuffer.width; x++) {
        backbuffer.pixels[y * backbuffer.width + x] =
            (x < backbuffer.width / 2) ? COLOR_CYAN : COLOR_RED;
    }
}
```

Build and run — you should see cyan on the left, red on the right. If the colors are swapped or wrong, the channel order needs adjusting.

---

## How Pixels Are Addressed

The pixel at column `x`, row `y` lives at index `y * width + x` in the pixel array:

```
Row 0: pixels[0]   pixels[1]   ... pixels[width-1]
Row 1: pixels[width] ...
...
Row y: pixels[y * width + 0] ... pixels[y * width + x] ...
```

This is **row-major order** — the same layout as C arrays and most image formats. `pitch` (bytes per row) is `width * sizeof(uint32_t)` = `width * 4`, so you can also compute the index as:

```c
uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
row[x] = color;
```

Using `pitch` instead of `width * 4` makes the code portable to padded image buffers.

---

## Key Concepts

- `uint32_t` — one integer per pixel (four 8-bit channels)
- `0xAARRGGBB` — alpha highest, blue lowest — matches `<< 24`, `<< 16`, `<< 8`, `<< 0`
- `TETRIS_RGBA(r,g,b,a)` — packs four channel values into one integer using bit shifts and OR
- `TETRIS_RGB(r,g,b)` — shorthand where alpha = 255
- Named constants (`COLOR_CYAN` etc.) — defined once in `tetris.h`, used everywhere
- Row-major indexing: `pixels[y * width + x]`

---

## Exercise

Compute by hand what `TETRIS_RGB(0, 128, 255)` equals in hexadecimal.

Solution:
```
alpha = 0xFF → 0xFF000000
r     = 0x00 → 0x00000000
g     = 0x80 → 0x00008000
b     = 0xFF → 0x000000FF
OR         = 0xFF0080FF
```

Which Tetris piece would use this color? (Hint: it's a shade of light blue — not one of the standard seven, but you could add a custom piece.)
