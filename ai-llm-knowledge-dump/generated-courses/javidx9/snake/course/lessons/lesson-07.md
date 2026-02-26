# Lesson 07 — `snake_render`: The Complete Drawing Function

## By the end of this lesson you will have:

A fully working `snake_render` function in `snake.c` that draws the header background, walls, food, snake body, and snake head into the `SnakeBackbuffer` — with no platform API calls whatsoever.

---

## Architecture Recap

```
main_x11.c / main_raylib.c
    ↓ calls
snake_render(&state, &backbuffer)   ← in snake.c
    ↓ calls
draw_rect / draw_cell               ← internal helpers in snake.c
    ↓ writes to
backbuffer.pixels[]                 ← plain uint32_t array
    ↑ later uploaded by
platform_display_backbuffer         ← platform-specific
```

`snake_render` knows nothing about X11 or Raylib. It just fills a `uint32_t` array.

---

## Step 1 — Clear and header

```c
void snake_render(const GameState *s, SnakeBackbuffer *bb) {
    /* 1. Clear entire window to black */
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BLACK);

    /* 2. Header band (top HEADER_ROWS rows) */
    draw_rect(bb, 0, 0, bb->width, HEADER_ROWS * CELL_SIZE, COLOR_DARK_GRAY);

    /* 3. Separator line at bottom of header */
    draw_rect(bb, 0, (HEADER_ROWS - 1) * CELL_SIZE, bb->width, 2, COLOR_GREEN);
```

Drawing order matters: later draws overwrite earlier ones. Clear first (black), then header band (gray), then separator line on top of the gray.

**Header height:** `HEADER_ROWS * CELL_SIZE = 3 * 20 = 60px`. This is where score text will go (Lesson 12).

---

## Step 2 — Border walls

The play field has walls on the left, right, and bottom. No top wall — the header acts as the visual boundary above.

```c
    int col, row;
    for (row = 0; row < GRID_HEIGHT; row++) {
        draw_cell(bb, 0,              row, COLOR_GREEN);  /* left wall  */
        draw_cell(bb, GRID_WIDTH - 1, row, COLOR_GREEN);  /* right wall */
    }
    for (col = 0; col < GRID_WIDTH; col++) {
        draw_cell(bb, col, GRID_HEIGHT - 1, COLOR_GREEN); /* bottom wall */
    }
```

**Why loop over rows for side walls?**
The grid is `GRID_HEIGHT=20` rows tall. We draw one cell per row on column 0 (left) and column `GRID_WIDTH-1` (right). The bottom wall iterates all 60 columns on the last row.

---

## Step 3 — Food

```c
    draw_cell(bb, s->food_x, s->food_y, COLOR_RED);
```

Food is drawn before the snake so the snake overlaps food (not visible in normal gameplay, but important ordering if they happen to be at the same position during a frame).

---

## Step 4 — Snake body and head

```c
    int idx, rem;
    uint32_t body_color = s->game_over ? COLOR_DARK_RED : COLOR_YELLOW;
    uint32_t head_color = s->game_over ? COLOR_DARK_RED : COLOR_WHITE;

    /* Draw body: tail to second-to-last segment */
    idx = s->tail;
    rem = s->length - 1;  /* all except head */
    while (rem > 0) {
        draw_cell(bb, s->segments[idx].x, s->segments[idx].y, body_color);
        idx = (idx + 1) % MAX_SNAKE;
        rem--;
    }

    /* Draw head separately (different color when alive) */
    draw_cell(bb, s->segments[s->head].x, s->segments[s->head].y, head_color);
```

**Why draw head last?**
The head is always visible on top. Body segments might overlap each other (though in practice they don't). Drawing head last guarantees it's always rendered.

**Dead snake:**
When `game_over=1`, both head and body become `COLOR_DARK_RED`. The color distinction disappears — the whole snake turns dark red, giving a clear visual "dead" state.

---

## Step 5 — Rendering order summary

```
1. Clear to black              (entire window)
2. Header background           (gray band, top 60px)
3. Header separator            (green line at y=40)
4. Left wall                   (column 0, all rows, green)
5. Right wall                  (column 59, all rows, green)
6. Bottom wall                 (row 19, all columns, green)
7. Food                        (red cell)
8. Snake body segments         (yellow or dark red)
9. Snake head                  (white or dark red)
10. Game over overlay          (added in Lesson 12)
```

Each layer can safely overwrite what's below it. This is the "painter's algorithm" — draw back-to-front.

---

## Build & Run

```sh
./build_x11.sh && ./build/snake_x11
```

You should see:
- Black window with a dark gray header band
- Green walls on left, right, and bottom
- A red food cell somewhere inside the walls
- A white head + yellow body snake in the middle, pointing right
- The snake doesn't move yet (movement in Lesson 08)

---

## Key Concepts

- `snake_render` is platform-independent — no X11 or Raylib calls
- Painter's algorithm: draw in order back-to-front; later calls overwrite earlier
- `draw_rect(bb, 0, 0, w, h, COLOR_BLACK)` — clear entire buffer
- `draw_cell(bb, col, row, color)` — grid-coordinate cell draw with 1px inset
- Ring buffer traversal: `idx=tail, rem=length-1, idx=(idx+1)%MAX_SNAKE`
- Head drawn separately (after body) so it always renders on top

---

## Exercise

The snake renders tail-to-head-minus-one for the body, then head separately. What if you drew the head as part of the body loop (not separately), using `rem = s->length` instead of `rem = s->length - 1`? Would the visual result be different? What would you lose?
