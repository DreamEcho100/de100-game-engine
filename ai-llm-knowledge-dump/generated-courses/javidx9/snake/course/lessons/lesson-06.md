# Lesson 06 — Drawing Primitives: `draw_rect` and `draw_cell`

## By the end of this lesson you will have:

A `draw_rect` function that fills a rectangle of pixels in the `SnakeBackbuffer`, a `draw_cell` function that renders one grid cell with a 1-pixel inset border, and a visible grid of test cells drawn into the buffer.

---

## Step 1 — `draw_rect`

All drawing in the game starts here. Every other draw function calls `draw_rect`.

```c
static void draw_rect(SnakeBackbuffer *bb, int x, int y, int w, int h,
                      uint32_t color) {
    /* Clip rectangle to backbuffer bounds */
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w > bb->width)  ? bb->width  : x + w;
    int y1 = (y + h > bb->height) ? bb->height : y + h;
    int px, py;
    for (py = y0; py < y1; py++)
        for (px = x0; px < x1; px++)
            bb->pixels[py * bb->width + px] = color;
}
```

**Parameters:**
- `x, y` — top-left corner in pixels (can be negative — clipping handles it)
- `w, h` — width and height in pixels
- `color` — packed `uint32_t` (`0xAARRGGBB`)

**Clipping:**
The clamp to `[0, bb->width)` / `[0, bb->height)` prevents writing outside the buffer. Without it, `py * bb->width + px` could point to a random memory location.

**New C concept — ternary operator:**
```c
int x0 = x < 0 ? 0 : x;
```
This is the C equivalent of `x < 0 ? 0 : x` in JS — same syntax.

**Pixel write:**
```c
bb->pixels[py * bb->width + px] = color;
```
`py * bb->width + px` is the flat index. For a 1200-pixel-wide buffer at row 5, column 10: `5 * 1200 + 10 = 6010`. This is the same formula from Lesson 02.

---

## Step 2 — `draw_cell`

A "cell" is one grid square in the play field. The play field starts at `y = HEADER_ROWS * CELL_SIZE` pixels (below the header).

```c
static void draw_cell(SnakeBackbuffer *bb, int col, int row, uint32_t color) {
    int field_y_offset = HEADER_ROWS * CELL_SIZE;
    draw_rect(bb,
              col * CELL_SIZE + 1,
              field_y_offset + row * CELL_SIZE + 1,
              CELL_SIZE - 2,
              CELL_SIZE - 2,
              color);
}
```

**Grid-to-pixel formula:**
```
pixel_x = col * CELL_SIZE + 1
pixel_y = HEADER_ROWS * CELL_SIZE + row * CELL_SIZE + 1
```

The `+ 1` inset and `- 2` size leave a 1-pixel dark gap between cells. This creates the grid-line visual effect without drawing actual lines — the background color (black) shows through the gaps.

**Example with `CELL_SIZE=20`, `HEADER_ROWS=3`:**
```
Cell (col=2, row=1):
  pixel_x = 2 * 20 + 1 = 41
  pixel_y = 3 * 20 + 1 * 20 + 1 = 81
  size: 18 × 18 pixels
```

---

## Step 3 — Visual test

Temporarily fill the buffer with a checkerboard to verify `draw_cell`:

```c
/* In snake_render — temporary test */
draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BLACK);  /* clear */

for (int row = 0; row < GRID_HEIGHT; row++) {
    for (int col = 0; col < GRID_WIDTH; col++) {
        uint32_t color = ((row + col) % 2 == 0) ? COLOR_DARK_GRAY : COLOR_GREEN;
        draw_cell(bb, col, row, color);
    }
}
```

You should see a 60×20 checkerboard grid with a 1-pixel dark gap between cells and a blank header band at the top.

---

## Step 4 — Performance: why a pixel loop is fast enough

`draw_rect` is a simple nested loop writing to a contiguous array. At 1200×460 pixels with 60fps, clearing the backbuffer writes ~55M pixels/second.

Modern CPUs can perform ~10 billion simple memory writes/second. Our game is nowhere near that. The overhead that would kill performance — `malloc`, system calls, OpenGL draw calls per rectangle — is absent. We make exactly one `glTexImage2D` call per frame.

**New C concept — `static` on a function:**
```c
static void draw_rect(...)
```
`static` on a function means "only callable from this file." It's like `private` in OOP — prevents other files from calling internal helpers. `snake_render` is public (declared in `snake.h`); `draw_rect` and `draw_cell` are private implementation details.

---

## Key Concepts

- `draw_rect(bb, x, y, w, h, color)` — fills a rectangle by writing to `bb->pixels`
- Clipping: clamp `x0/y0/x1/y1` to buffer bounds before the loop
- `pixels[py * width + px]` — flat pixel address for 2D coordinates
- `draw_cell(bb, col, row, color)` — grid position → pixel position + 1px inset
- `HEADER_ROWS * CELL_SIZE` — vertical offset to place grid below the header
- `+1`/`-2` inset: leaves 1px dark gap between adjacent cells
- `static` on internal functions — file-private, not callable from other files

---

## Exercise

Add a `draw_rect_outline` function that draws only the border of a rectangle (4 thin `draw_rect` calls) without filling the interior. Use it to draw a highlighted border around the food cell. How many `draw_rect` calls does it take? What dimensions should each edge rectangle have?
