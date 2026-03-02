# Lesson 13 — Game Over Overlay and Alpha Compositing

## By the end of this lesson you will have:

A semi-transparent black overlay that dims the field when the game is over, a red border framing the field, and "GAME OVER" / "R RESTART" / "Q QUIT" text centered on screen — all rendered using a new `draw_rect_blend` function.

---

## Why a New Function?

`draw_rect` does solid fills. Every pixel becomes exactly the color you specify.

The game-over overlay needs to:

1. Darken the existing field (which already has pieces drawn)
2. Allow the text to read clearly over it
3. Not completely hide the final field state

Transparency (alpha compositing) solves this.

---

## Step 1 — Alpha compositing math

The standard "source over destination" formula:

```
out = (src * alpha + dst * (255 - alpha)) / 255
```

Where:

- `src` is the overlay pixel color (0–255 per channel)
- `dst` is what's already in the backbuffer
- `alpha` is the overlay opacity (0 = fully transparent, 255 = fully opaque)
- `out` is the blended result

Applied per channel (R, G, B). The alpha byte of `out` is discarded — pixels in the backbuffer don't need their own alpha, they just store `AARRGGBB` for convenience.

**Example:** Black overlay at alpha=200

```
src_r=0, src_g=0, src_b=0, alpha=200
dst_r=255, dst_g=100, dst_b=0   (a cyan piece cell)

out_r = (0 * 200 + 255 * 55) / 255 = 55
out_g = (0 * 200 + 100 * 55) / 255 = 21
out_b = (0 * 200 +   0 * 55) / 255 = 0
```

The cyan piece becomes a very dark red — visually dimmed, but still visible underneath the overlay.

---

## Step 2 — `draw_rect_blend` implementation

```c
void draw_rect_blend(TetrisBackbuffer *backbuffer,
                     int x, int y, int w, int h,
                     uint32_t color) {
    int alpha = (color >> 24) & 0xFF;

    /* Fast path: fully opaque — use draw_rect */
    if (alpha == 255) {
        draw_rect(backbuffer, x, y, w, h, color);
        return;
    }
    /* Fast path: fully transparent — nothing to draw */
    if (alpha == 0) return;

    int src_r = (color >> 16) & 0xFF;
    int src_g = (color >> 8)  & 0xFF;
    int src_b = (color)       & 0xFF;
    int inv_a = 255 - alpha;

    /* Clamp to backbuffer bounds */
    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = (x + w > backbuffer->width)  ? backbuffer->width  : x + w;
    int y1 = (y + h > backbuffer->height) ? backbuffer->height : y + h;

    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            uint32_t *dst_pixel =
                &backbuffer->pixels[py * backbuffer->width + px];

            int dst_r = (*dst_pixel >> 16) & 0xFF;
            int dst_g = (*dst_pixel >> 8)  & 0xFF;
            int dst_b = (*dst_pixel)       & 0xFF;

            int out_r = (src_r * alpha + dst_r * inv_a) / 255;
            int out_g = (src_g * alpha + dst_g * inv_a) / 255;
            int out_b = (src_b * alpha + dst_b * inv_a) / 255;

            *dst_pixel = GAME_RGB(out_r, out_g, out_b);
        }
    }
}
```

**Alpha extraction:** `(color >> 24) & 0xFF`  
Our color format is `0xAARRGGBB`. Shifting right 24 bits brings the `AA` byte to position 0-7, then `& 0xFF` masks off everything above.

**Precompute `inv_a = 255 - alpha`** outside the loop — it's constant for the whole rectangle, no need to recalculate per pixel.

**New C concept — bit-shifting for channel extraction:**

```c
(color >> 24) & 0xFF   /* alpha */
(color >> 16) & 0xFF   /* red   */
(color >> 8)  & 0xFF   /* green */
(color)       & 0xFF   /* blue  */
```

Each channel is 8 bits. Shifting moves the desired byte to bits 0-7, then `& 0xFF` keeps only those 8 bits.

---

## Step 3 — Game over overlay in `tetris_render`

```c
if (state->game_over) {
    int field_px_w = FIELD_WIDTH  * CELL_SIZE;
    int field_px_h = FIELD_HEIGHT * CELL_SIZE;

    /* 1. Semi-transparent black overlay */
    draw_rect_blend(backbuffer,
                    0, 0, field_px_w, field_px_h,
                    GAME_RGBA(0, 0, 0, 200));

    /* 2. Red border: 4 thin rectangles (top, bottom, left, right) */
    int border_thickness = 4;
    draw_rect(backbuffer, 0, 0, field_px_w, border_thickness, COLOR_RED);
    draw_rect(backbuffer, 0, field_px_h - border_thickness,
              field_px_w, border_thickness, COLOR_RED);
    draw_rect(backbuffer, 0, 0, border_thickness, field_px_h, COLOR_RED);
    draw_rect(backbuffer, field_px_w - border_thickness, 0,
              border_thickness, field_px_h, COLOR_RED);

    /* 3. "GAME OVER" text centered on field */
    int text_scale = 3;
    int char_w     = 6 * text_scale;
    int str_len    = 9;  /* strlen("GAME OVER") */
    int text_x     = (field_px_w - str_len * char_w) / 2;
    int text_y     = field_px_h / 2 - 30;
    draw_text(backbuffer, text_x, text_y, "GAME OVER", COLOR_RED, text_scale);

    /* 4. Instruction lines below */
    int small_x = (field_px_w - 9 * (6 * 2)) / 2;
    draw_text(backbuffer, small_x, text_y + 30, "R RESTART", COLOR_WHITE, 2);
    draw_text(backbuffer, small_x, text_y + 46, "Q QUIT",    COLOR_WHITE, 2);
}
```

**Why 4 thin rectangles for the border, not 1 hollow rect?**  
`draw_rect` fills solid rectangles. To draw a border, draw the top edge, bottom edge, left edge, right edge — each is a thin, full-width or full-height rectangle. This is a common pattern in software renderers.

**Centering formula:**

```
text_x = (total_width - text_pixel_width) / 2
```

`text_pixel_width = character_count × (5 glyphs + 1 gap) × scale = char_count × 6 × scale`

"GAME OVER" is 9 characters (including space). At scale 3: `9 × 6 × 3 = 162px`. Field is `360px`. `(360 - 162) / 2 = 99px`.

---

## Step 4 — `GAME_RGBA` macro

Defined in `tetris.h`:

```c
#define GAME_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) << 8)  |  (uint32_t)(b))
```

`GAME_RGB` just uses `0xFF` as alpha:

```c
#define GAME_RGB(r, g, b) GAME_RGBA(r, g, b, 0xFF)
```

For `GAME_RGBA(0, 0, 0, 200)`:

- alpha = 200 → `0xC8000000`
- red = 0 → `0x00000000`
- green = 0 → `0x00000000`
- blue = 0 → `0x00000000`
- Result: `0xC8000000`

---

## Step 5 — Performance note

`draw_rect_blend` is a CPU blend. For each pixel in the rectangle, it reads the existing pixel, does 6 integer multiplications and 3 additions, and writes the result.

At 360×540 pixels (the full field), that's 194,400 pixels per frame. At 60fps, that's ~11 million blend operations per second.

On modern hardware this is fast enough. But you can optimize with the fast paths:

- `alpha == 255` → call `draw_rect` (memset-like write, no read-modify)
- `alpha == 0` → return immediately

These fast paths are already in our implementation. The game-over path only blends once per frame during game over state.

---

## Key Concepts

- `draw_rect_blend`: reads existing pixel, blends with `src_a / 255` weight
- `(color >> 24) & 0xFF` — alpha from `0xAARRGGBB` format
- `inv_a = 255 - alpha` precomputed outside pixel loop
- `out = (src * a + dst * inv_a) / 255` — standard alpha composite formula
- Red border: 4 calls to `draw_rect` (4 edges, not 1 hollow rect)
- Centering: `(total_width - text_width) / 2`; text_width = `char_count × 6 × scale`
- `game_over` branch in `tetris_render` — only runs when `state->game_over` is true

---

## Exercise

Add a "flash" effect when the overlay first appears: for the first 10 frames of game over, draw the border in white. After frame 10, switch to red. What field in `GameState` would you use to track frame count? How would you reset it when `game_init` is called?
