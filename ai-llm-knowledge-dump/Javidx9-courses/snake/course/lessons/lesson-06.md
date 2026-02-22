# Lesson 6 — Collision Detection: Walls and Self

**By the end of this lesson you will have:** A snake that dies (freezes with a "GAME OVER" overlay) when it hits a wall or its own body.

---

## What we're building

Right now the snake can pass through walls and itself — it just keeps going. That makes for a terrible game. In this lesson we add the two classic Snake death conditions:

1. **Wall collision** — the head goes outside the grid bounds
2. **Self collision** — the head enters a cell already occupied by the snake's body

Both checks happen in the same place: **after computing `new_x`/`new_y`, before writing the new head into the ring buffer.**

---

## Step 1 — Add the `game_over` flag

In your variable declarations inside `main()`, add:

```c
int game_over = 0;
```

This is a simple integer used as a boolean. `0` = alive, `1` = dead. C has no `bool` type by default; using `int` as 0/1 is idiomatic C.

---

## Step 2 — Wall collision check

Find the section in your move logic where you compute `new_x` and `new_y`:

```c
int new_x = segments[head].x + dx[direction];
int new_y = segments[head].y + dy[direction];
```

Immediately after those two lines — before you do anything else with `new_x`/`new_y` — add:

```c
if (new_x < 0 || new_x >= GRID_WIDTH ||
    new_y < 0 || new_y >= GRID_HEIGHT) {
    game_over = 1;
}
```

**Why this works:** The valid grid cells are `x = 0 .. GRID_WIDTH-1` and `y = 0 .. GRID_HEIGHT-1`. If `new_x` is `-1` or `GRID_WIDTH`, the head has stepped off the edge. Note `>= GRID_WIDTH`, not `> GRID_WIDTH` — arrays are zero-indexed.

---

## Step 3 — Self collision check

Add this immediately after the wall check (still before writing the ring buffer):

```c
int idx = tail;
int remaining = length;
while (remaining > 0) {
    if (segments[idx].x == new_x && segments[idx].y == new_y) {
        game_over = 1;
        break;
    }
    idx = (idx + 1) % MAX_SNAKE;
    remaining--;
}
```

**How the loop works:**

- `tail` is the index of the oldest (rearmost) segment in the ring buffer.
- We walk forward through the ring buffer — `(idx + 1) % MAX_SNAKE` wraps around the array end back to 0.
- We check exactly `length` segments — that's the entire living snake.
- If any segment matches `new_x`/`new_y`, the head is about to enter an occupied cell → death.

**Why start at `tail`, not `head`?** It doesn't matter — we check all `length` segments regardless. Starting at `tail` is conventional: oldest to newest.

---

## Step 4 — Freeze the snake when dead

Wrap the entire movement block in a guard:

```c
if (!game_over) {
    /* tick counter */
    /* compute new_x / new_y */
    /* wall check */
    /* self check */
    /* write new head into ring buffer */
    /* advance tail */
}
```

When `game_over` is `1`, we skip all movement. The snake stops exactly where it was. The window keeps running (you can still close it), but the snake is frozen.

---

## Step 5 — Game Over overlay in X11

In the render section, after drawing the snake, add:

```c
if (game_over) {
    /* Dark background box */
    int box_w = 200, box_h = 80;
    int box_x = (GRID_WIDTH  * CELL_SIZE - box_w) / 2;
    int box_y = (GRID_HEIGHT * CELL_SIZE - box_h) / 2 + HEADER_ROWS * CELL_SIZE;

    XSetForeground(display, gc, 0x222222);
    XFillRectangle(display, window, gc, box_x, box_y, box_w, box_h);

    /* "GAME OVER" text */
    XSetForeground(display, gc, 0xFF3333);
    XDrawString(display, window, gc,
                box_x + 50, box_y + 30,
                "GAME OVER", 9);

    /* Instruction */
    XSetForeground(display, gc, 0xCCCCCC);
    XDrawString(display, window, gc,
                box_x + 30, box_y + 55,
                "Restart coming in lesson 8", 26);
}
```

**Coordinate math explained:**

- `GRID_WIDTH * CELL_SIZE` = total pixel width of the play area (60 × 14 = 840 px).
- `(total_width - box_w) / 2` = left edge to center the box horizontally.
- `+ HEADER_ROWS * CELL_SIZE` = push the box down past the header (3 × 14 = 42 px).

---

## Step 5 (Raylib version) — Game Over overlay

```c
if (game_over) {
    int box_w = 220, box_h = 90;
    int box_x = (GRID_WIDTH  * CELL_SIZE - box_w) / 2;
    int box_y = (GRID_HEIGHT * CELL_SIZE - box_h) / 2 + HEADER_ROWS * CELL_SIZE;

    DrawRectangle(box_x, box_y, box_w, box_h, (Color){30, 30, 30, 220});
    DrawText("GAME OVER",
             box_x + 45, box_y + 18, 22, RED);
    DrawText("Restart coming in lesson 8",
             box_x + 14, box_y + 55, 12, LIGHTGRAY);
}
```

Raylib's `Color` struct takes `{r, g, b, a}` where `a = 220` gives a semi-transparent dark background.

---

## Build & Run

**X11:**
```sh
gcc -o snake_x11 src/main_x11.c -lX11 && ./snake_x11
```

**Raylib:**
```sh
gcc -o snake_raylib src/main_raylib.c -lraylib -lm && ./snake_raylib
```

**Test checklist:**
- [ ] Snake moves normally at start
- [ ] Steer into the top wall → snake freezes, "GAME OVER" box appears
- [ ] Steer into the left or right wall → same
- [ ] Let the snake grow (cheat: temporarily set `grow_pending = 20` at start), then steer into the body → same
- [ ] Window stays open and responsive after game over

---

## Mental Model

> **Collision detection is just asking: "Is the next position already taken?"**

```
Compute new_x, new_y
        │
        ▼
  [Wall check]  ── out of bounds? ──▶  game_over = 1
        │
        ▼
  [Self check]  ── in ring buffer? ──▶  game_over = 1
        │
        ▼
  game_over == 0?
     │         │
    YES        NO
     │         └──▶  do nothing (snake frozen)
     ▼
  Advance ring buffer
```

The snake never actually moves if `game_over` is set. We compute where it *would* go, check if that's legal, and only then commit the move.

**Why check the ring buffer and not a 2D grid array?**

You could maintain a separate `int occupied[GRID_HEIGHT][GRID_WIDTH]` and check it in O(1) — that's actually better for large snakes. The ring buffer scan is O(n) in snake length. For a 60×20 grid the max snake is 1200 segments — still very fast — but the 2D approach is the "correct" data structure. The exercise below asks you to build it.

---

## Exercise

Add a 2D occupancy array for O(1) self-collision:

1. Declare at the top of `main()`:
   ```c
   int occupied[GRID_HEIGHT][GRID_WIDTH];
   memset(occupied, 0, sizeof(occupied));
   ```

2. When advancing the head (adding a new segment at `new_x`, `new_y`):
   ```c
   occupied[new_y][new_x] = 1;
   ```

3. When advancing the tail (removing the rearmost segment):
   ```c
   occupied[segments[tail].y][segments[tail].x] = 0;
   tail = (tail + 1) % MAX_SNAKE;
   ```

4. Replace the self-collision loop with:
   ```c
   if (occupied[new_y][new_x]) {
       game_over = 1;
   }
   ```

5. Also initialize `occupied` to match the starting snake position (mark each initial segment as `1`).

Notice the index order: `occupied[y][x]`, not `occupied[x][y]`. Rows first, columns second — the same way 2D arrays work in memory in C (row-major order).
