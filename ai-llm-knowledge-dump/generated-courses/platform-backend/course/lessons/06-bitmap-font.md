# Lesson 06 — Bitmap Font

## Overview

| Item | Value |
|------|-------|
| **Backend** | Both |
| **New concepts** | `FONT_8X8[128][8]`, LSB-first mask `(1 << col)`, `draw_char`, `draw_text` |
| **Observable outcome** | "PLATFORM READY" text label visible on canvas |
| **Files created** | `src/utils/draw-text.h`, `src/utils/draw-text.c` |
| **Files modified** | `src/game/demo.c` |

---

## What you'll build

A bitmap font renderer.  Every printable ASCII character is stored as 8 rows × 8 columns of bits.  `draw_char` reads those bits and calls `draw_rect` for each lit pixel.

---

## Background

### JS analogy

Imagine a 2D array `font[charCode][row]` where each `row` value is a number whose 8 bits are 8 boolean "pixel on/off" flags.  The least-significant bit (bit 0) is the **leftmost** pixel.

```js
// Pseudo-JS version — LSB-first font (bit 0 = leftmost pixel)
function drawChar(canvas, x, y, charCode, scale, color) {
    for (let row = 0; row < 8; row++) {
        const bits = FONT_8X8[charCode][row];
        for (let col = 0; col < 8; col++) {
            if (bits & (1 << col)) {   // (1 << col): col 0 tests bit 0 (leftmost)
                fillRect(canvas, x + col*scale, y + row*scale, scale, scale, color);
            }
        }
    }
}
```

### LSB-first encoding

The font uses **LSB-first** (Least Significant Bit first) encoding: **bit 0 = leftmost pixel (col 0), bit 7 = rightmost pixel (col 7)**.

Example — letter `'N'` (ASCII 78), row 0 = `0x63` = binary `0110 0011`:

```
col:   0  1  2  3  4  5  6  7
bit:   0  1  2  3  4  5  6  7   ← bit index (LSB first)
value: 1  1  0  0  0  0  1  1   ← 0x63 bits from LSB to MSB
pix:   X  X  .  .  .  .  X  X  ← left stroke + right stroke of 'N'
```

Test: `FONT_8X8['N'][0] & (1 << col)`
- `col=0`: `0x63 & 0x01 = 0x01` → on (left stroke) ✓
- `col=1`: `0x63 & 0x02 = 0x02` → on ✓
- `col=2`: `0x63 & 0x04 = 0` → off ✓
- `col=6`: `0x63 & 0x40 = 0x40` → on (right stroke) ✓

### Why `(1 << col)` and NOT `(0x80 >> col)`?

`(1 << col)` tests bit `col`, i.e. col 0 = bit 0 (leftmost pixel).  This matches the LSB-first encoding of the font data.

`(0x80 >> col)` tests bits from the MSB (bit 7) downward — it assumes BIT7-left encoding.  **Using `(0x80 >> col)` with our LSB-first font renders every character as its horizontal mirror image.**  (A `P` becomes a `q`-shape, `Z` diagonals go the wrong direction, etc.)

### `FONT_8X8[128][8]`

The table is indexed directly by ASCII code.  Control characters (0x00–0x1F) and DEL (0x7F) are stored as blank glyphs.

The full table is in `src/utils/draw-text.c` — 128 entries of 8 bytes each.

---

## What to type

### `src/utils/draw-text.h`

```c
#ifndef UTILS_DRAW_TEXT_H
#define UTILS_DRAW_TEXT_H
#include <stdint.h>
#include "utils/backbuffer.h"

#define GLYPH_W 8
#define GLYPH_H 8

// Font encoding: LSB-first — bit 0 = leftmost pixel (col 0).
// To test pixel (col, row): FONT_8X8[c][row] & (1 << col)
// Do NOT use (0x80 >> col) — that is BIT7-left and mirrors every glyph.
extern const uint8_t FONT_8X8[128][8];

void draw_char(Backbuffer *bb, int x, int y, int scale, char c, uint32_t color);
void draw_text(Backbuffer *bb, int x, int y, int scale, const char *text, uint32_t color);
#endif
```

### `src/utils/draw-text.c` — key functions (font table is pre-generated)

```c
#include "utils/draw-text.h"
#include "utils/draw-shapes.h"

// (full FONT_8X8[128][8] table here — see source file)

void draw_char(Backbuffer *bb, int x, int y, int scale, char c, uint32_t color) {
    int idx = (unsigned char)c;
    if (idx >= 128) idx = '?';

    for (int row = 0; row < GLYPH_H; row++) {
        uint8_t bits = FONT_8X8[idx][row];
        for (int col = 0; col < GLYPH_W; col++) {
            if (bits & (1 << col)) {   // LSB-first: bit 0 = leftmost pixel
                draw_rect(bb, x + col*scale, y + row*scale, scale, scale, color);
            }
        }
    }
}

void draw_text(Backbuffer *bb, int x, int y, int scale, const char *text, uint32_t color) {
    int cx = x, cy = y;
    for (const char *p = text; *p; p++) {
        if (*p == '\n') { cx = x; cy += GLYPH_H * scale; continue; }
        draw_char(bb, cx, cy, scale, *p, color);
        cx += GLYPH_W * scale;
    }
}
```

### `src/game/demo.c` — L06 addition

```c
void demo_render(Backbuffer *bb, GameInput *input, int fps) {
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BG);
    draw_rect(bb, 50,  50, 200, 100, COLOR_RED);
    draw_rect(bb, 300, 50, 200, 100, COLOR_GREEN);
    draw_rect(bb, 550, 50, 200, 100, COLOR_BLUE);
    draw_rect_blend(bb, 150, 30, 200, 140, GAME_RGBA(255,255,0,120));

    // L06 — static label
    draw_text(bb, 50, 200, 2, "PLATFORM READY", COLOR_WHITE);
}
```

---

## Explanation

| Concept | Detail |
|---------|--------|
| `FONT_8X8[128][8]` | 128 ASCII glyphs × 8 rows × 8 pixels per row (packed as bits, LSB-first) |
| `(1 << col)` | Tests bit `col` from LSB → bit 0 = leftmost pixel (col 0); correct for LSB-first font |
| `(0x80 >> col)` | Tests from MSB (bit 7) — BIT7-left; **wrong** for this font → mirror-image characters |
| `scale` parameter | Multiplies each pixel rect; scale=2 → 16×16 glyph |
| Newline in `draw_text` | `cy += GLYPH_H * scale; cx = x` — supports multi-line labels |

---

## Build and run

```sh
./build-dev.sh --backend=raylib -r
```

The text "PLATFORM READY" should appear in white at scale 2.

---

## Quiz

1. Write the bitmask test for pixel `(col=3, row=0)` of character `'N'` (0x63).  Is it on or off?
2. Why does `draw_char` use `draw_rect(scale, scale)` instead of writing a single pixel?  What does `scale=1` produce?
3. Explain what would happen if you accidentally used `(0x80 >> col)` instead of `(1 << col)` with this font.  Which characters would look most obviously wrong?
