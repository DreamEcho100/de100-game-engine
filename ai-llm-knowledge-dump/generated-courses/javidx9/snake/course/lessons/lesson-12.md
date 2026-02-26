# Lesson 12 — Bitmap Font, HUD, Game Over Overlay + Final Integration

## By the end of this lesson you will have:

A 5×7 bitmap font for rendering score text in the header, a `draw_rect_blend` function for the semi-transparent game-over overlay, and a complete `snake_render` that shows score, best score, speed tier, and a "PRESS R TO RESTART" message — with both builds compiling cleanly.

---

## Step 1 — The 5×7 Bitmap Font

Each character is stored as a 5-column × 7-row bit pattern, packed into 5 bytes (one per column, 7 bits used):

```c
/* font_glyphs[ASCII_index - 32][col] */
static const uint8_t font_glyphs[96][5] = {
    /* ' ' */  { 0x00, 0x00, 0x00, 0x00, 0x00 },
    /* '!' */  { 0x00, 0x00, 0x5F, 0x00, 0x00 },
    /* '"' */  { 0x00, 0x03, 0x00, 0x03, 0x00 },
    /* ... 96 printable ASCII characters ... */
    /* '0' */  { 0x3E, 0x51, 0x49, 0x45, 0x3E },
    /* '1' */  { 0x00, 0x42, 0x7F, 0x40, 0x00 },
    /* ... */
};
```

The array index is `character - 32`. Space (ASCII 32) → index 0. `'A'` (ASCII 65) → index 33.

**New C concept — `uint8_t`:**
`uint8_t` is an 8-bit unsigned integer (0–255), defined in `<stdint.h>`. Guarantees exactly 8 bits on every platform. Used here because each glyph column is a 7-bit mask.

---

## Step 2 — `draw_char` and `draw_text`

```c
static void draw_char(SnakeBackbuffer *bb, int x, int y, char c,
                      int scale, uint32_t color) {
    if (c < 32 || c > 127) return;
    const uint8_t *glyph = font_glyphs[(int)c - 32];
    int col, row;
    for (col = 0; col < 5; col++) {
        for (row = 0; row < 7; row++) {
            if (glyph[col] & (1 << row))
                draw_rect(bb, x + col * scale, y + row * scale,
                          scale, scale, color);
        }
    }
}
```

`glyph[col] & (1 << row)` extracts bit `row` from column `col`. If set, draw a `scale × scale` filled rect at that glyph pixel.

```c
static void draw_text(SnakeBackbuffer *bb, int x, int y, const char *text,
                      int scale, uint32_t color) {
    while (*text) {
        draw_char(bb, x, y, *text, scale, color);
        x += (5 + 1) * scale;   /* 5 columns + 1 gap */
        text++;
    }
}
```

`(5 + 1) * scale` advances by 5 glyph columns plus 1 column of spacing.

---

## Step 3 — `draw_rect_blend` (semi-transparent overlay)

For the game-over overlay, we blend a dark semi-transparent rectangle over the existing pixels:

```c
static void draw_rect_blend(SnakeBackbuffer *bb, int x, int y, int w, int h,
                             uint32_t color) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w > bb->width)  ? bb->width  : x + w;
    int y1 = (y + h > bb->height) ? bb->height : y + h;

    uint8_t sa = (color >> 24) & 0xFF;
    uint8_t sr = (color >> 16) & 0xFF;
    uint8_t sg = (color >>  8) & 0xFF;
    uint8_t sb =  color        & 0xFF;

    int px, py;
    for (py = y0; py < y1; py++) {
        for (px = x0; px < x1; px++) {
            uint32_t dst = bb->pixels[py * bb->width + px];
            uint8_t dr = (dst >> 16) & 0xFF;
            uint8_t dg = (dst >>  8) & 0xFF;
            uint8_t db =  dst        & 0xFF;

            /* Alpha blend: out = src * alpha + dst * (1 - alpha) */
            uint8_t or_ = (uint8_t)((sr * sa + dr * (255 - sa)) / 255);
            uint8_t og  = (uint8_t)((sg * sa + dg * (255 - sa)) / 255);
            uint8_t ob  = (uint8_t)((sb * sa + db * (255 - sa)) / 255);

            bb->pixels[py * bb->width + px] = SNAKE_RGB(or_, og, ob);
        }
    }
}
```

**Alpha math:**
`out = src * alpha/255 + dst * (1 - alpha/255)`.

For `color = SNAKE_RGBA(0, 0, 0, 180)` (black, ~70% opaque):
- A pixel that was `COLOR_GREEN` becomes: `0 * 180/255 + 0x00,0xCC,0x00 * 75/255` → dark green tint
- The field is still faintly visible through the overlay

**`SNAKE_RGBA` macro:**
```c
#define SNAKE_RGBA(r, g, b, a) \
    ((uint32_t)(a) << 24 | (uint32_t)(r) << 16 | (uint32_t)(g) << 8 | (uint32_t)(b))
```

---

## Step 4 — Complete `snake_render`

```c
void snake_render(const GameState *s, SnakeBackbuffer *bb) {
    char buf[64];

    /* 1. Clear */
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BLACK);

    /* 2. Header */
    draw_rect(bb, 0, 0, bb->width, HEADER_ROWS * CELL_SIZE, COLOR_DARK_GRAY);
    draw_rect(bb, 0, (HEADER_ROWS - 1) * CELL_SIZE, bb->width, 2, COLOR_GREEN);

    /* 3. HUD: score and best score */
    snprintf(buf, sizeof(buf), "SCORE:%d", s->score);
    draw_text(bb, 8, 8, buf, 2, COLOR_WHITE);

    snprintf(buf, sizeof(buf), "BEST:%d", s->best_score);
    draw_text(bb, 200, 8, buf, 2, COLOR_YELLOW);

    /* 4. Walls */
    int col, row;
    for (row = 0; row < GRID_HEIGHT; row++) {
        draw_cell(bb, 0,              row, COLOR_GREEN);
        draw_cell(bb, GRID_WIDTH - 1, row, COLOR_GREEN);
    }
    for (col = 0; col < GRID_WIDTH; col++)
        draw_cell(bb, col, GRID_HEIGHT - 1, COLOR_GREEN);

    /* 5. Food */
    draw_cell(bb, s->food_x, s->food_y, COLOR_RED);

    /* 6. Snake body */
    uint32_t body_color = s->game_over ? COLOR_DARK_RED : COLOR_YELLOW;
    uint32_t head_color = s->game_over ? COLOR_DARK_RED : COLOR_WHITE;
    int idx = s->tail;
    int rem = s->length - 1;
    while (rem > 0) {
        draw_cell(bb, s->segments[idx].x, s->segments[idx].y, body_color);
        idx = (idx + 1) % MAX_SNAKE;
        rem--;
    }
    draw_cell(bb, s->segments[s->head].x, s->segments[s->head].y, head_color);

    /* 7. Game over overlay */
    if (s->game_over) {
        draw_rect_blend(bb, 0, 0, bb->width, bb->height,
                        SNAKE_RGBA(0, 0, 0, 180));

        const char *msg  = "GAME OVER";
        const char *msg2 = "PRESS R TO RESTART";
        int cx = bb->width / 2;
        int scale = 3;
        int char_w = (5 + 1) * scale;

        draw_text(bb, cx - (int)strlen(msg)  * char_w / 2,
                  bb->height / 2 - 30, msg,  scale, COLOR_WHITE);
        draw_text(bb, cx - (int)strlen(msg2) * char_w / 2,
                  bb->height / 2 + 10, msg2, 2,     COLOR_YELLOW);
    }
}
```

**Text centering:**
```c
x = center_x - (int)strlen(text) * char_w / 2
```

`char_w = (5 + 1) * scale`. For "GAME OVER" (9 chars) at scale=3: `char_w=18`, `total_width=162`, `x = 600 - 81 = 519`.

---

## Step 5 — Final build verification

```sh
cd ai-llm-knowledge-dump/Javidx9-courses/snake/course
./build_x11.sh   && ./build/snake_x11
./build_raylib.sh && ./build/snake_raylib
```

Expected output:
```
Building snake_x11...
Done! Run with: ./build/snake_x11
✓ Auto-repeat detection: enabled
✓ VSync enabled (GLX_EXT_swap_control)
✓ OpenGL initialized: 4.x ...
```

Both builds should compile with **zero warnings** and produce a playable snake game with:
- Score and best score in the header
- Smooth movement at 150ms/step, speeding up every 3 points
- Green walls, white head, yellow body, red food
- Semi-transparent "GAME OVER" overlay on death
- Press R to restart (best score persists)

---

## Step 6 — Memory and cleanup

`SnakeBackbuffer.pixels` is the only heap allocation:

```c
/* In main: */
backbuffer.pixels = malloc(width * height * sizeof(uint32_t));
/* ... game loop ... */
free(backbuffer.pixels);
```

`GameState` uses a fixed `segments[MAX_SNAKE]` array on the stack — no allocation needed.

Run under Valgrind to verify:
```sh
valgrind --leak-check=full ./build/snake_x11
```

Expected: `0 bytes lost in 0 blocks` after closing the window.

---

## Build flags summary

| Flag | Purpose |
|------|---------|
| `-Wall -Wextra -Wpedantic` | Catch all warnings |
| `-std=c99` | C99 features (snprintf, `//` comments, designated initializers) |
| `-lX11 -lxkbcommon` | X11 display + keyboard |
| `-lGL -lGLX` | OpenGL + GLX context creation |
| `-lraylib -lm` | Raylib + math library |
| `-fsanitize=address` | Optional: detect buffer overflows during development |

---

## Key Concepts

- `font_glyphs[c - 32][col]` — 5×7 bitmap, column-major, bit `row` = `glyph[col] & (1<<row)`
- `draw_char`: 5×7 loop, each set bit → `draw_rect` at glyph pixel × scale
- `draw_text`: advance `x += (5+1)*scale` per character
- `draw_rect_blend`: alpha compositing — `out = src*a + dst*(255-a)`, all divided by 255
- `SNAKE_RGBA(r,g,b,a)` — packs alpha into top byte
- Game-over overlay: `draw_rect_blend` over entire buffer, then centered text on top
- Text centering: `x = cx - strlen(text) * char_w / 2`
- Only one heap allocation: `backbuffer.pixels` — `malloc` in main, `free` before exit

---

## Exercise

The game-over overlay blends over the entire window including the header. Modify `snake_render` to blend only over the play field (from `y = HEADER_ROWS * CELL_SIZE` downward). How does this change the visual? Which looks better: full-window overlay or play-field-only overlay?
