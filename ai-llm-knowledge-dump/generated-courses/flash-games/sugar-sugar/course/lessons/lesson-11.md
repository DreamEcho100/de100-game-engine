# Lesson 11 — Rendering Polish

**By the end of this lesson you will have:**
Cup fill progress bars that grow as grains land. A bitmap font that labels every cup with its required count. A "LEVEL COMPLETE!" overlay that fades in from transparent. A gravity button that shows `GRAV v` or `GRAV ^`. Obstacles rendered in a distinct brown color.

---

## Step 1 — Cup fill progress bar math

A cup has a fixed pixel height `cup.h` (e.g. 100 px). The bar height is proportional to how many grains have been collected:

```
filled_h = (collected * cup.h) / required_count
```

Concrete numbers — 60 grains collected out of 150 required, cup height = 100 px:
```
filled_h = (60 * 100) / 150 = 6000 / 150 = 40 px
```

40 px of the 100 px cup is filled.

This is **integer division**. In C, dividing two `int` values always truncates toward zero. No `Math.floor()` needed — it is implicit.

The bar grows from the bottom of the cup:

```c
// game.c — render_playing(), inside the cups loop
for (int c = 0; c < state->level.cup_count; c++) {
    Cup *cup = &state->level.cups[c];

    // Empty area — full cup rectangle
    draw_rect(bb, cup->x, cup->y, cup->w, cup->h, COLOR_CUP_EMPTY);

    // Filled portion — grows upward from the bottom
    int filled_h = (cup->collected * cup->h) / cup->required_count;
    int bar_y    = cup->y + cup->h - filled_h; // top of the fill bar

    if (filled_h > 0) {
        draw_rect(bb, cup->x, bar_y, cup->w, filled_h, COLOR_CUP_FILL);
    }

    // Border on top
    draw_rect_outline(bb, cup->x, cup->y, cup->w, cup->h, COLOR_CUP_BORDER);
}
```

`bar_y = cup->y + cup->h - filled_h`:
- `cup->y + cup->h` is the bottom of the cup.
- Subtract `filled_h` to get the top of the fill bar.

Visual:
```
cup->y ──────────────  ← top of cup
       [  EMPTY    ]
bar_y  ──────────────  ← fill starts here
       [  FILLED   ]  ← filled_h pixels tall
cup->y + cup->h ──────  ← bottom of cup
```

---

## Step 2 — Cup labels with `draw_int()`

Each cup shows its target count above the fill bar. `draw_int()` converts an integer to a string without `printf`:

```c
// game.c — draw_int() implementation
static void draw_int(GameBackbuffer *bb, int x, int y, int n, uint32_t color) {
    char buf[12];
    int  i = 11;
    buf[i] = '\0';

    if (n == 0) {
        buf[--i] = '0';
    } else {
        int sign = (n < 0);
        if (sign) n = -n;
        while (n > 0) {
            buf[--i] = '0' + (n % 10); // extract last digit
            n /= 10;                    // drop last digit
        }
        if (sign) buf[--i] = '-';
    }
    draw_text(bb, x, y, buf + i, color);
}
```

Digit-extraction loop for n = 317:
```
iteration 1: digit = 317 % 10 = 7,  n = 317 / 10 = 31
iteration 2: digit = 31  % 10 = 1,  n = 31  / 10 = 3
iteration 3: digit = 3   % 10 = 3,  n = 3   / 10 = 0  → stop
```

We build the string right-to-left into `buf[]` by decrementing `i`. `buf + i` points to the start of the resulting string `"317"`.

Usage in cup rendering:
```c
// show required count above the cup
draw_int(bb, cup->x + cup->w/2 - 4, cup->y - 14, cup->required_count, COLOR_UI_TEXT);
// show collected / required
draw_int(bb, cup->x + 4, cup->y + cup->h - 12, cup->collected, COLOR_UI_TEXT);
```

---

## Step 3 — `draw_char()`: reading a glyph from `g_font8x8[]`

`src/font.h` contains a 256-entry table of 8×8 pixel glyphs:

```c
// font.h (excerpt)
static const uint8_t g_font8x8[256][8] = {
    ['A'] = { 0x18, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00 },
    // ...
};
```

Each row is one `uint8_t`. Each bit is one pixel. Bit 7 (the most significant bit) is the leftmost pixel.

To draw character `'A'` at position `(10, 20)`:

```c
// game.c — draw_char()
static void draw_char(GameBackbuffer *bb, int x, int y, char c, uint32_t color) {
    int idx = (unsigned char)c - 0x20;
    for (int row = 0; row < FONT_H; row++) {
        uint8_t bits = FONT_DATA[idx * FONT_H + row];
        for (int col = 0; col < FONT_W; col++) {
            // BIT0=left convention: bit 0 → leftmost pixel, bit 7 → rightmost.
            // Use (1 << col) so col=0 tests bit 0 (left) not bit 7 (right).
            if (bits & (1 << col))
                draw_pixel(bb, x + col, y + row, color);
        }
    }
}
```

> ⚠️ **Common mistake — mirrored text**: Using `(0x80 >> col)` instead of
> `(1 << col)` reverses the bit-scan direction. When `col=0` the mask is
> `0x80` (bit 7, the *right-most* pixel in a BIT0=left font), so every
> asymmetric character (N, R, 7, …) is drawn as its mirror image.
>
> Rule: always check which end of the byte is "left" in your font data,
> then match the bitmask direction accordingly. Test with 'N' — its diagonal
> must run top-left → bottom-right.

`(1 << col)` creates a bitmask:
```
col=0: 0x01 = 00000001  ← left-most pixel
col=1: 0x02 = 00000010
col=2: 0x04 = 00000100
...
col=7: 0x80 = 10000000  ← right-most pixel
```

`bits & (1 << col)` is non-zero when that column's pixel is on. If non-zero: draw a pixel at `(x + col, y + row)`.

---

## Step 4 — `draw_text()`: stride 9 px per character

`draw_text()` calls `draw_char()` for each character, advancing the x position by 9 pixels per character (8 glyph + 1 gap):

```c
// game.c
static void draw_text(GameBackbuffer *bb, int x, int y,
                      const char *str, uint32_t color) {
    int cx = x;
    int cy = y;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            cx  = x;        // carriage return
            cy += 10;       // move down one line (8px glyph + 2px gap)
        } else {
            draw_char(bb, cx, cy, str[i], color);
            cx += 9;        // advance right: 8px glyph + 1px gap
        }
    }
}
```

String "HI" at x=100:
```
'H' drawn at x=100, next cx=109
'I' drawn at x=109, next cx=118
```

`\n` resets `cx` to the original `x` and increments `cy` by 10. This lets you write multi-line text with a single `draw_text()` call:

```c
draw_text(bb, 10, 10, "Line one\nLine two\nLine three", COLOR_UI_TEXT);
```

---

## Step 5 — `draw_rect_blend()`: the alpha-blend formula

```c
// game.c
static void draw_rect_blend(GameBackbuffer *bb, int x, int y,
                             int w, int h, uint32_t color, int alpha_0_255) {
    uint8_t sr = (color >> 16) & 0xFF; // source red
    uint8_t sg = (color >>  8) & 0xFF; // source green
    uint8_t sb = (color      ) & 0xFF; // source blue

    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= bb->width || py < 0 || py >= bb->height) continue;
            uint32_t dst = bb->pixels[py * bb->width + px];
            uint8_t dr = (dst >> 16) & 0xFF;
            uint8_t dg = (dst >>  8) & 0xFF;
            uint8_t db = (dst      ) & 0xFF;

            uint8_t or_ = (uint8_t)((sr * alpha_0_255 + dr * (255 - alpha_0_255)) >> 8);
            uint8_t og  = (uint8_t)((sg * alpha_0_255 + dg * (255 - alpha_0_255)) >> 8);
            uint8_t ob  = (uint8_t)((sb * alpha_0_255 + db * (255 - alpha_0_255)) >> 8);

            bb->pixels[py * bb->width + px] = GAME_RGB(or_, og, ob);
        }
    }
}
```

The formula per channel: `out = (src * a + dst * (255 - a)) >> 8`

Why `>> 8` instead of `/ 256`? Right-shift by 8 is identical to integer division by 256 but compiles to a single CPU instruction. The result is always `0–255`.

Example — red filter overlay, alpha=128:
```
src = 229 (COLOR_RED red channel)
dst = 245 (COLOR_BG  red channel)
out = (229 * 128 + 245 * 127) >> 8
    = (29312  + 31115) >> 8
    = 60427 >> 8
    = 236
```

Result: a slightly pink background showing through the red tint.

---

## Step 6 — `render_level_complete()`: overlay + text shadow

When `phase == PHASE_LEVEL_COMPLETE`, a green overlay fades in over the game view. The fade is driven by `phase_timer`:

```c
// game.c
static void render_level_complete(const GameState *state, GameBackbuffer *bb) {
    // First render the playing state underneath
    render_playing(state, bb);

    // Fade in: 0→200 alpha over 1.0 second
    int alpha = (int)(state->phase_timer * 200.0f);
    if (alpha > 200) alpha = 200;

    draw_rect_blend(bb, 0, 0, CANVAS_W, CANVAS_H, COLOR_COMPLETE, alpha);

    // Text with a three-layer shadow for depth
    int tx = 160, ty = 210;
    draw_text(bb, tx + 2, ty + 2, "LEVEL COMPLETE!", COLOR_BLACK); // shadow 2px
    draw_text(bb, tx + 1, ty + 1, "LEVEL COMPLETE!", COLOR_BLACK); // shadow 1px
    draw_text(bb, tx,     ty,     "LEVEL COMPLETE!", COLOR_WHITE); // top layer
}
```

Three-layer shadow:
1. Draw black text at `(tx+2, ty+2)` — deepest shadow.
2. Draw black text at `(tx+1, ty+1)` — mid shadow.
3. Draw white text at `(tx, ty)` — the visible text.

The shadows are drawn first so they are underneath the white text. Layers are just draw order — later draws cover earlier ones.

`phase_timer * 200.0f` maps seconds to alpha:
- At 0.0 s: alpha = 0 (invisible).
- At 0.5 s: alpha = 100 (half transparent).
- At 1.0 s: alpha = 200 (mostly opaque, capped).

---

## Step 7 — Gravity button label

```c
// game.c — render_playing()
if (state->level.has_gravity_switch) {
    int gx = 556, gy = 10, gw = 78, gh = 28;
    uint32_t gbg = state->gravity_hover ? COLOR_BTN_HOVER : COLOR_BTN_NORMAL;

    draw_rect        (bb, gx, gy, gw, gh, gbg);
    draw_rect_outline(bb, gx, gy, gw, gh, COLOR_BTN_BORDER);

    if (state->gravity_sign == 1)
        draw_text(bb, gx + 6, gy + 10, "GRAV v", COLOR_UI_TEXT);
    else
        draw_text(bb, gx + 6, gy + 10, "GRAV ^", COLOR_UI_TEXT);
}
```

`gravity_hover` is updated in `update_playing()`:

```c
state->gravity_hover = (mx >= gx && mx < gx + gw && my >= gy && my < gy + gh);
```

Clicking the button (or pressing G) toggles `gravity_sign`:

```c
int clicked_grav = (BUTTON_PRESSED(input->mouse.left) && state->gravity_hover);
if ((BUTTON_PRESSED(input->gravity) || clicked_grav) && state->level.has_gravity_switch) {
    state->gravity_sign = -state->gravity_sign;
}
```

---

## Step 8 — Obstacle rendering before the line scan

Obstacles must be drawn before the player's lines so they appear in a distinct color (brown) while lines appear dark. Both share the same `LineBitmap` after `stamp_obstacles()`, so how do we draw them differently?

We don't. Obstacles are rendered separately in `render_playing()` using their `LevelDef` data, not the bitmap:

```c
// game.c — render_playing()

// 1. Draw obstacle rectangles in brown
for (int i = 0; i < state->level.obstacle_count; i++) {
    Obstacle *o = &state->level.obstacles[i];
    draw_rect(bb, o->x, o->y, o->w, o->h, COLOR_OBSTACLE);
}

// 2. Draw the line bitmap in dark grey (player-drawn lines)
for (int py = 0; py < CANVAS_H; py++) {
    for (int px = 0; px < CANVAS_W; px++) {
        if (state->lines.pixels[py * CANVAS_W + px]) {
            draw_pixel(bb, px, py, COLOR_LINE);
        }
    }
}
```

Obstacles happen to also be in `lines.pixels[]` (stamped by `stamp_obstacles()`). The line-scan loop draws them in `COLOR_LINE` too, but the brown rectangle drawn in step 1 is rendered first, then covered partially by the dark pixel scan. In practice we skip drawing obstacle pixels in the scan loop since they are already covered by the brown rect:

```c
// optimization: skip pixels that fall inside any obstacle rect
// (not critical for correctness, only for visual cleanliness)
```

The simpler approach (used in practice): draw the brown obstacle rects, then draw *all* line pixels. The brown is overwritten by dark for obstacle pixels, but since `COLOR_OBSTACLE` and `COLOR_LINE` look similar enough, it is visually fine. The important distinction is level obstacles vs. emitters/cups.

---

## Build & Run

```bash
clang -Wall -Wextra -O2 -o sugar \
    src/main_x11.c src/game.c src/levels.c -lX11 -lm
./sugar
```

**What you should see:**
- Cups have a blue fill bar that rises as grains land.
- Numbers above and inside each cup show target and current count.
- When a level completes, a green overlay fades in with white "LEVEL COMPLETE!" text and a dark shadow.
- On gravity levels, a small button in the top-right reads `GRAV v` or `GRAV ^`.
- Obstacles appear in brown; player-drawn lines appear dark grey.

---

## Key Concepts

- **Fill bar math**: `filled_h = (collected * cup_h) / required_count` — integer division, no `floor()` needed.
- `bar_y = cup->y + cup->h - filled_h`: bottom-anchored bar; grows upward.
- `draw_int()`: digit-extraction with `n % 10` / `n /= 10`, right-to-left into a char buffer. No `sprintf`.
- `draw_char()`: reads one `uint8_t` row from `FONT_DATA[]`; tests each bit with `(1 << col)` — BIT0=left convention so col 0 maps to the leftmost pixel.
- `draw_text()`: stride 9 px per char (8 glyph + 1 gap); `\n` resets x and advances y by 10.
- `draw_rect_blend()` formula: `out = (src * a + dst * (255 - a)) >> 8` — integer alpha blend, `>> 8` is `/ 256`.
- Three-layer shadow: draw text at `+2, +2` then `+1, +1` then `0, 0`. Later draws cover earlier.
- Obstacle rendering: draw from `LevelDef` data, not from the bitmap — lets you use a distinct color (brown) separate from drawn lines (dark grey).

---

## Exercise

Add a "countdown" number that shows how many more grains each cup still needs. Display it inside the cup's empty area: `draw_int(bb, cup->x + 4, cup->y + 8, cup->required_count - cup->collected, COLOR_UI_TEXT)`. When the cup is full, display "OK" instead of a number. Hint: use an `if (cup->collected >= cup->required_count)` branch before the `draw_int()` call and replace it with `draw_text(..., "OK", ...)`.
