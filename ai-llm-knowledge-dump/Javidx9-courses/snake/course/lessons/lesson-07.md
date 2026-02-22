# Lesson 7 — Food, Eating, and Growing

**By the end of this lesson you will have:** A red food cell that appears in the arena; when the snake's head reaches it, the snake grows by 5 segments, food respawns elsewhere, and the score increments.

---

## What we're building

Food in Snake is dead simple: two integers (`food_x`, `food_y`) that mark a grid cell. Every move tick, we check whether the new head position matches those integers. If it does: score++, schedule growth, place new food.

The "grow" mechanic uses a counter (`grow_pending`). Instead of instantly stretching the snake, we skip the tail removal for `grow_pending` ticks. Each skipped removal makes the snake one cell longer. It's elegant and requires zero extra data structures.

---

## Step 1 — Add the food and score variables

In your variable declarations inside `main()`, add:

```c
int food_x = GRID_WIDTH / 2 - 10;
int food_y = GRID_HEIGHT / 2;
int score      = 0;
int grow_pending = 0;
```

The starting food position (`GRID_WIDTH/2 - 10`, `GRID_HEIGHT/2`) is just a placeholder. We'll replace it with a proper random spawn in Step 5.

---

## Step 2 — Draw the food cell

In your render section, after drawing the snake body, draw the food:

**X11:**
```c
/* Allocate red once near your other color setup */
XColor red_color;
XParseColor(display, DefaultColormap(display, screen), "red", &red_color);
XAllocColor(display, DefaultColormap(display, screen), &red_color);

/* In render loop: */
int fx = food_x * CELL_SIZE;
int fy = food_y * CELL_SIZE + HEADER_ROWS * CELL_SIZE;
XSetForeground(display, gc, red_color.pixel);
XFillRectangle(display, window, gc, fx + 1, fy + 1, CELL_SIZE - 2, CELL_SIZE - 2);
```

**Raylib:**
```c
int fx = food_x * CELL_SIZE;
int fy = food_y * CELL_SIZE + HEADER_ROWS * CELL_SIZE;
DrawRectangle(fx + 1, fy + 1, CELL_SIZE - 2, CELL_SIZE - 2, RED);
```

The `+ 1` / `- 2` inset gives a 1-pixel gap between cells so food doesn't visually bleed into adjacent cells.

**Build and run now.** You should see a red cell in the arena. It doesn't do anything yet — that's fine.

---

## Step 3 — The `spawn_food` function

Add this function above `main()` (or forward-declare it):

```c
void spawn_food(Segment *segments, int tail, int length,
                int *food_x, int *food_y) {
    int ok;
    do {
        *food_x = rand() % GRID_WIDTH;
        *food_y = rand() % GRID_HEIGHT;
        ok = 1;
        int idx = tail;
        int rem  = length;
        while (rem > 0) {
            if (segments[idx].x == *food_x &&
                segments[idx].y == *food_y) {
                ok = 0;
                break;
            }
            idx = (idx + 1) % MAX_SNAKE;
            rem--;
        }
    } while (!ok);
}
```

**What this does:**

1. Pick a random cell.
2. Walk the entire snake ring buffer to check whether that cell is occupied.
3. If occupied, pick again. Repeat until we find a free cell.

This is a "rejection sampling" loop — simple and correct. On a 60×20 grid with a small snake it almost always succeeds on the first try.

**Add `srand(time(NULL));` near the top of `main()`** so food doesn't always appear in the same spot on every run. You'll need `#include <time.h>`.

---

## Step 4 — Seed the initial food position

Replace the hardcoded initial `food_x`/`food_y` assignment with:

```c
spawn_food(segments, tail, length, &food_x, &food_y);
```

Call this after you've initialized the snake ring buffer (so `spawn_food` can avoid the snake's starting cells).

---

## Step 5 — Food collision detection

In the move logic, **after** writing the new head into the ring buffer (the head is now officially at `new_x`, `new_y`), add:

```c
if (new_x == food_x && new_y == food_y) {
    score++;
    grow_pending += 5;
    spawn_food(segments, tail, length, &food_x, &food_y);
}
```

**Ordering matters:** We check food collision after writing the head but before (potentially) advancing the tail. That way `length` already reflects the new head, which `spawn_food` needs to correctly avoid the full snake.

---

## Step 6 — The grow mechanic (modify tail advance)

Find the section that advances the tail. It currently looks like:

```c
tail = (tail + 1) % MAX_SNAKE;
length--;
```

Replace it with:

```c
if (grow_pending > 0) {
    grow_pending--;
    /* Don't advance tail — length was already incremented when we wrote head */
} else {
    tail = (tail + 1) % MAX_SNAKE;
    length--;
}
```

**Visualized:**

```
Normal move (grow_pending == 0):
  Before:  [T][.][.][.][H]        length = 5
  After:      [.][.][.][H][H']    length = 5  (tail advanced, head advanced)

Growing move (grow_pending > 0):
  Before:  [T][.][.][.][H]        length = 5
  After:   [T][.][.][.][H][H']    length = 6  (tail STAYS, head advanced)
```

The snake "grows" because we added a head cell without removing a tail cell. After 5 such moves the snake is 5 cells longer. `grow_pending` decrements each move until it hits 0, then tail removal resumes.

**The ring buffer length accounting:**

- When we write the new head: we increment `head` and `length` (we do this regardless).
- When we skip tail removal: we keep `tail` and don't decrement `length`. Net effect: length grows by 1.
- When tail advances normally: we increment `tail` and decrement `length`. Net effect: length stays the same.

---

## Step 7 — Verify the ring buffer length bookkeeping

At this point your head-advance code should look like:

```c
/* Advance head */
head = (head + 1) % MAX_SNAKE;
segments[head].x = new_x;
segments[head].y = new_y;
length++;

/* Grow or advance tail */
if (grow_pending > 0) {
    grow_pending--;
} else {
    tail = (tail + 1) % MAX_SNAKE;
    length--;
}
```

`length` always equals the actual number of live segments. Double-check your code matches this pattern.

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
- [ ] Red food cell is visible at game start
- [ ] Steer head onto the food cell → food disappears, reappears elsewhere
- [ ] Snake visibly grows (count cells before and after eating)
- [ ] After eating, the snake is longer by exactly 5 cells (after 5 more move ticks)
- [ ] Score variable increments (verify with a `printf("score=%d\n", score);` if you haven't added the header display yet)

---

## Mental Model

> **Food is just two integers. Eating is just comparing the new head position to those integers. Growing is just skipping the tail removal for a few ticks.**

```
Each move tick:
                                          ┌── grow_pending > 0?
new head written to ring buffer           │     YES → grow_pending--
         │                                │           (tail stays, length grows)
         ▼                                │     NO  → tail advances, length stays
  head == food? ── YES ──▶ score++        └──────────────────────────────────────
         │              grow_pending += 5
         │              spawn new food
         NO
         │
  (nothing extra)
```

The beauty of `grow_pending` as a counter: it's trivially adjustable. `+= 1` grows by 1. `+= 20` grows by 20. You're not storing 20 duplicate segments — you're just delaying tail removal 20 times.

---

## Exercise

Experiment with the grow amount:

1. Change `grow_pending += 5` to `grow_pending += 1`. Play. Does it feel too slow to grow?
2. Try `grow_pending += 10`. Does the snake feel satisfyingly chunky?
3. Try `grow_pending += 3`. This is what many Snake clones use.

There's no wrong answer — it's a game feel parameter. Leave it at whatever feels best to you. The important thing: you only need to change **one number** to change the entire growth behavior. That's good design.

**Bonus:** Add a food counter separate from score: `int food_eaten = 0;`. Increment it when food is eaten. After eating 5 food, spawn 2 food cells simultaneously (you'll need `food_x2`, `food_y2` and a second draw call). This previews the "multiple food items" extension from lesson 10.
