# Lesson 03 — Color System: uint32_t, SNAKE_RGB, Named Constants

## By the end of this lesson you will have:

A color system where every color in the game is a `uint32_t` constant defined once in `snake.h`, and a `SNAKE_RGB` macro that packs red/green/blue values into the `0xAARRGGBB` format used by the backbuffer pipeline.

---

## The Format: `0xAARRGGBB`

Each pixel is one 32-bit integer. The 32 bits are divided into four 8-bit channels:

```
Bit: 31──────24  23──────16  15───────8  7────────0
     AAAAAAAA    RRRRRRRR    GGGGGGGG    BBBBBBBB
     alpha       red         green       blue
```

Written as a hex literal: `0xAARRGGBB`.

Examples:
```
0xFFFF0000 = fully opaque red
0xFF00FF00 = fully opaque green
0xFF0000FF = fully opaque blue
0xFFFFFFFF = fully opaque white
0xFF000000 = fully opaque black
0x80FF0000 = 50% transparent red (alpha = 0x80 = 128)
```

---

## Step 1 — Bit-packing with shifts

To pack four separate byte values into one `uint32_t`:

```c
uint32_t pixel = ((uint32_t)a << 24) | ((uint32_t)r << 16) |
                 ((uint32_t)g <<  8) |  (uint32_t)b;
```

**New C concept — bit shifts and bitwise OR:**
- `<< 24` moves value to bits 31–24 (alpha position)
- `<< 16` moves value to bits 23–16 (red position)
- `<< 8` moves value to bits 15–8 (green position)
- `|` combines all four non-overlapping chunks into one integer

Concrete example for green (`r=0, g=255, b=0, a=255`):
```
a=255: 0xFF000000 (shifted left 24)
r=0:   0x00000000 (shifted left 16)
g=255: 0x0000FF00 (shifted left 8)
b=0:   0x00000000 (no shift)
OR:    0xFF00FF00  ← final pixel value
```

**New C concept — `(uint32_t)` cast before shifting:**
In C, shifting an `int` left by 24 can overflow if `int` is 32 bits. Casting to `uint32_t` first makes the shift safe by working in unsigned 32-bit space.

---

## Step 2 — `SNAKE_RGB` and `SNAKE_RGBA` macros

```c
#define SNAKE_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) <<  8) |  (uint32_t)(b))

#define SNAKE_RGB(r, g, b) SNAKE_RGBA(r, g, b, 0xFF)
```

`SNAKE_RGB` always sets alpha = `0xFF` (fully opaque). `SNAKE_RGBA` lets you specify any alpha for transparency effects.

**New C concept — multi-line macros with `\`:**
A backslash at the end of a line continues the macro definition onto the next line. The `do { } while (0)` pattern isn't needed here because this macro evaluates to a single expression.

**New C concept — function-like macros vs functions:**
`SNAKE_RGB(255, 0, 0)` looks like a function call but is a text substitution at compile time. No function call overhead. The downside: no type checking. We use it here because the computation is simple and called at startup for constants.

---

## Step 3 — Named color constants

```c
#define COLOR_BLACK     SNAKE_RGB(  0,   0,   0)
#define COLOR_WHITE     SNAKE_RGB(255, 255, 255)
#define COLOR_RED       SNAKE_RGB(220,  50,  50)
#define COLOR_DARK_RED  SNAKE_RGB(139,   0,   0)
#define COLOR_GREEN     SNAKE_RGB( 50, 205,  50)
#define COLOR_YELLOW    SNAKE_RGB(255, 215,   0)
#define COLOR_DARK_GRAY SNAKE_RGB( 64,  64,  64)
#define COLOR_GRAY      SNAKE_RGB(128, 128, 128)
```

**Why named constants instead of raw hex?**
- `COLOR_GREEN` is readable; `0xFF32CD32` is not
- Changing the exact shade of green means updating one line, not hunting for `0xFF32CD32` everywhere
- Compiler sees them as compile-time constants — no runtime cost

**Comparison to JS:**
```js
const COLOR_GREEN = 0xFF32CD32;  // closest JS equivalent
```

---

## Step 4 — Extracting channels (for blending)

To read individual channels back out of a packed pixel:

```c
uint32_t pixel = 0xFF32CD32;

int a = (pixel >> 24) & 0xFF;  /* alpha: 255 */
int r = (pixel >> 16) & 0xFF;  /* red:    50 */
int g = (pixel >>  8) & 0xFF;  /* green: 205 */
int b = (pixel)       & 0xFF;  /* blue:   50 */
```

Shift right to bring the desired byte to bits 7–0, then `& 0xFF` masks off everything above. Used in `draw_rect_blend` (Lesson 12).

---

## Step 5 — Little-endian and `GL_RGBA`

The backbuffer uses `0xAARRGGBB`. On x86 (little-endian), this 32-bit value is stored in memory as bytes `[BB, GG, RR, AA]`.

OpenGL's `GL_RGBA` reads bytes in memory order: `[R, G, B, A]`. This means our blue and red channels appear swapped when OpenGL reads them.

In practice, you'll see colors look correct visually after verifying with the test colors. If not, switch the `glTexImage2D` format argument to `GL_BGRA`. Our implementation uses `GL_RGBA` and the colors render correctly for the chosen pixel layout.

---

## Key Concepts

- `0xAARRGGBB` — alpha in high byte, blue in low byte
- `(uint32_t)r << 16` — shift channel value to its position in the integer
- `SNAKE_RGB(r, g, b)` — packs three bytes into one `uint32_t` with full opacity
- `SNAKE_RGBA(r, g, b, a)` — same but with explicit alpha (for transparency)
- `(pixel >> 16) & 0xFF` — extract red channel back out
- Named color constants (`COLOR_GREEN` etc.) — defined once, readable everywhere

---

## Exercise

Try adding `COLOR_ORANGE = SNAKE_RGB(255, 165, 0)` and `COLOR_PURPLE = SNAKE_RGB(128, 0, 128)`. Then verify: what is the hex value of `COLOR_ORANGE`? What would the `uint32_t` value be if you used `SNAKE_RGBA(255, 165, 0, 128)` (50% transparent orange)? Work it out with the shift formulas above.
