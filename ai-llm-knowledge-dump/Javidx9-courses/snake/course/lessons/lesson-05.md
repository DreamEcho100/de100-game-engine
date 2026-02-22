# Lesson 05 — Steer the Snake

**By the end of this lesson you will have:**
Arrow keys turn the snake clockwise (right arrow) and counter-clockwise (left arrow). The snake changes direction immediately when you press a key. You can steer it around the arena.

---

## What We're Building

After this lesson you can actually play — steer the snake around with the left and right arrow keys. The game still has no food or collision (we add those next), but you can drive the snake anywhere in the arena.

---

## Step 1 — How Snake Turning Works

In classic Snake, you don't set absolute directions (press UP → go up). Instead you **rotate relative to where you're currently facing**:

- **Right arrow** = rotate clockwise (turn right from your current heading)
- **Left arrow** = rotate counter-clockwise (turn left from your current heading)

This matches how driving a car works: you don't say "go north" — you say "turn right" or "turn left" from wherever you're pointed.

```
Going RIGHT and press right arrow → now going DOWN
Going RIGHT and press left  arrow → now going UP
Going DOWN  and press right arrow → now going LEFT
Going DOWN  and press left  arrow → now going RIGHT
```

---

## Step 2 — The Direction Circle

Our direction numbers form a circle. Visualize them like compass points on a clock:

```
           0 (UP)
            │
3 (LEFT) ───┼─── 1 (RIGHT)
            │
           2 (DOWN)
```

Turning **clockwise** (right arrow) moves one step forward around the circle:
```
0 → 1 → 2 → 3 → 0 → ...
UP → RIGHT → DOWN → LEFT → UP → ...
```

Turning **counter-clockwise** (left arrow) moves one step backward:
```
0 → 3 → 2 → 1 → 0 → ...
UP → LEFT → DOWN → RIGHT → UP → ...
```

---

## Step 3 — The Rotation Formulas

**Turn clockwise (right arrow):**
```c
new_direction = (direction + 1) % 4
```

Concrete examples:
```
direction=0 (UP):    (0+1) % 4 = 1  → RIGHT  ✓
direction=1 (RIGHT): (1+1) % 4 = 2  → DOWN   ✓
direction=2 (DOWN):  (2+1) % 4 = 3  → LEFT   ✓
direction=3 (LEFT):  (3+1) % 4 = 0  → UP     ✓ (wraps!)
```

**Turn counter-clockwise (left arrow):**
```c
new_direction = (direction + 3) % 4
```

Why `+ 3` instead of `- 1`? Because C's `%` operator on negative numbers gives a negative result (`(-1) % 4 == -1`), which would be a wrong array index. Adding 4 first makes it always positive, and adding 3 is the same as subtracting 1 on a circle of 4:

```
(-1 + 4) % 4 = 3 % 4 = 3   → same as going "back one"
```

Concrete examples with `(direction + 3) % 4`:
```
direction=0 (UP):    (0+3) % 4 = 3  → LEFT   ✓
direction=1 (RIGHT): (1+3) % 4 = 0  → UP     ✓
direction=2 (DOWN):  (2+3) % 4 = 1  → RIGHT  ✓
direction=3 (LEFT):  (3+3) % 4 = 2  → DOWN   ✓
```

> **JS equivalent:** `direction = ((direction - 1) % 4 + 4) % 4` — the double-mod trick in JS to handle negative modulo. In C we just use `(direction + 3) % 4`.

---

## Step 4 — Preventing 180° Reversals

If the snake is going RIGHT (direction=1) and you somehow set direction to LEFT (direction=3), it would instantly reverse and eat its own neck. We must prevent this.

**The rule:** If `|new_direction - old_direction| == 2`, it's a reversal — ignore the input.

Check with all reversal pairs:
```
Going RIGHT (1), trying LEFT (3):   |1 - 3| = |-2| = 2  → BLOCK ✓
Going UP    (0), trying DOWN (2):   |0 - 2| = |-2| = 2  → BLOCK ✓
Going DOWN  (2), trying UP   (0):   |2 - 0| = | 2| = 2  → BLOCK ✓
Going LEFT  (3), trying RIGHT(1):   |3 - 1| = | 2| = 2  → BLOCK ✓
```

Non-reversals (valid turns):
```
Going RIGHT (1), turning to UP  (0): |1 - 0| = 1  → ALLOW ✓
Going RIGHT (1), turning to DOWN(2): |1 - 2| = 1  → ALLOW ✓
```

The reversal check in C:
```c
int diff = new_dir - direction;
if (diff < 0) diff = -diff;    /* absolute value */
if (diff != 2) direction = new_dir;
```

Or more concisely using the standard library's `abs()`:
```c
#include <stdlib.h>
if (abs(new_dir - direction) != 2) direction = new_dir;
```

---

## Step 5 — Input Buffering

**The problem:** What if the player presses right arrow twice very quickly between two move ticks? With naive code, the second press would read `direction` after it was already updated by the first press — so both presses modify `direction` and they compound. But they happened between the same two move ticks, so visually only one of them should count.

**The solution:** buffer the next direction in a separate variable `next_direction`. Input writes to `next_direction`. The move tick reads from it and applies it to `direction`.

```
Frame timeline:

Tick 1: [sleep 150ms] → [read input → set next_direction=RIGHT]
Tick 2: [sleep 150ms] → [read input → nothing]
        direction = next_direction  → move RIGHT
Tick 3: [sleep 150ms] → [read input → set next_direction=DOWN] 
        direction = next_direction  → move DOWN
```

Even if the player presses two keys between ticks, only the last one takes effect per tick. This is the correct behavior.

Add to your game variables in `main()`:

```c
    int next_direction = direction;
```

---

## Step 6 — Implement Input in X11

Add `#include <stdlib.h>` to your includes (for `abs()`).

Inside the `while (XPending(display))` input-draining loop, **replace** the existing key handling with:

```c
        while (XPending(display)) {
            XNextEvent(display, &event);
            if (event.type == KeyPress) {
                KeySym key = XLookupKeysym(&event.xkey, 0);

                if (key == XK_q || key == XK_Q || key == XK_Escape) {
                    running = 0;

                } else if (key == XK_Right) {
                    int turned = (direction + 1) % 4;
                    if (abs(turned - direction) != 2)
                        next_direction = turned;

                } else if (key == XK_Left) {
                    int turned = (direction + 3) % 4;
                    if (abs(turned - direction) != 2)
                        next_direction = turned;
                }
            }
        }
```

> **`XK_Right` and `XK_Left`** are named keysym constants from `<X11/keysym.h>`. They match the arrow keys on your keyboard.

Then, just before computing `new_x` and `new_y` in the move logic, apply the buffered direction:

```c
        /* Apply buffered turn */
        direction = next_direction;

        /* Move */
        int new_x = segments[head_idx].x + DX[direction];
        int new_y = segments[head_idx].y + DY[direction];
        ...
```

---

## Step 7 — Walk Through a Turn

Let's trace a right-arrow keypress when the snake is going RIGHT:

```
State: direction = 1 (RIGHT), next_direction = 1

Player presses Right arrow:
  key == XK_Right
  turned = (1 + 1) % 4 = 2
  abs(2 - 1) = 1 ≠ 2  → allowed
  next_direction = 2

... (sleep 150ms, wait for move tick)

Move tick fires:
  direction = next_direction = 2 (DOWN)
  new_x = segments[head_idx].x + DX[2] = x + 0 = x  (same column)
  new_y = segments[head_idx].y + DY[2] = y + 1       (one row down)

Result: snake turns and starts moving DOWN.
```

Now let's trace an attempted reversal (RIGHT trying to go LEFT):

```
State: direction = 1 (RIGHT), next_direction = 1

(Somehow next_direction was set to 3 by bad code)
  turned = 3
  abs(3 - 1) = 2 == 2  → BLOCKED
  next_direction unchanged = 1 (stays RIGHT)
```

---

## Step 8 — Full `src/main_x11.c`

```c
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define GRID_WIDTH   60
#define GRID_HEIGHT  20
#define CELL_SIZE    14
#define HEADER_ROWS   3
#define MAX_SNAKE    (GRID_WIDTH * GRID_HEIGHT)

typedef struct { int x; int y; } Segment;

static const int DX[4] = { 0,  1,  0, -1 };
static const int DY[4] = {-1,  0,  1,  0 };

void platform_sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void draw_cell(Display *display, Window window, GC gc,
               int col, int row, unsigned long color)
{
    int px   = col * CELL_SIZE + 1;
    int py   = (row + HEADER_ROWS) * CELL_SIZE + 1;
    int size = CELL_SIZE - 2;
    XSetForeground(display, gc, color);
    XFillRectangle(display, window, gc, px, py, size, size);
}

int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (!display) { fprintf(stderr, "Error: no X display\n"); return 1; }

    int screen = DefaultScreen(display);
    int win_w  = GRID_WIDTH  * CELL_SIZE;
    int win_h  = (GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE;

    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
        0, 0, win_w, win_h, 1,
        BlackPixel(display, screen), BlackPixel(display, screen));

    XSelectInput(display, window, KeyPressMask);
    XStoreName(display, window, "Snake");
    XMapWindow(display, window);
    XFlush(display);

    Colormap cmap = DefaultColormap(display, screen);
    XColor wall_col, body_col, head_col, exact;
    XAllocNamedColor(display, cmap, "gray40",     &wall_col, &exact);
    XAllocNamedColor(display, cmap, "lime green", &body_col, &exact);
    XAllocNamedColor(display, cmap, "green",      &head_col, &exact);
    GC gc = XCreateGC(display, window, 0, NULL);

    /* Snake state */
    Segment segments[MAX_SNAKE];
    int head_idx = 9, tail_idx = 0;
    int direction = 1, next_direction = 1;   /* start RIGHT */
    int speed = 8, tick_count = 0;
    int i;
    for (i = 0; i < 10; i++) {
        segments[i].x = GRID_WIDTH / 2 - 5 + i;
        segments[i].y = GRID_HEIGHT / 2;
    }

    int running = 1;
    XEvent event;

    while (running) {
        platform_sleep_ms(150);

        /* Drain input events */
        while (XPending(display)) {
            XNextEvent(display, &event);
            if (event.type == KeyPress) {
                KeySym key = XLookupKeysym(&event.xkey, 0);

                if (key == XK_q || key == XK_Q || key == XK_Escape) {
                    running = 0;

                } else if (key == XK_Right) {
                    int turned = (direction + 1) % 4;
                    if (abs(turned - direction) != 2)
                        next_direction = turned;

                } else if (key == XK_Left) {
                    int turned = (direction + 3) % 4;
                    if (abs(turned - direction) != 2)
                        next_direction = turned;
                }
            }
        }
        if (!running) break;

        /* Tick */
        tick_count++;
        if (tick_count < speed) continue;
        tick_count = 0;

        /* Apply buffered direction */
        direction = next_direction;

        /* Move */
        int new_x = segments[head_idx].x + DX[direction];
        int new_y = segments[head_idx].y + DY[direction];
        head_idx = (head_idx + 1) % MAX_SNAKE;
        segments[head_idx].x = new_x;
        segments[head_idx].y = new_y;
        tail_idx = (tail_idx + 1) % MAX_SNAKE;

        /* Redraw */
        XSetForeground(display, gc, BlackPixel(display, screen));
        XFillRectangle(display, window, gc, 0, HEADER_ROWS * CELL_SIZE,
                       win_w, GRID_HEIGHT * CELL_SIZE);

        XSetForeground(display, gc, wall_col.pixel);
        XFillRectangle(display, window, gc, 0, 0, win_w, CELL_SIZE);
        XFillRectangle(display, window, gc, 0, 2 * CELL_SIZE, win_w, CELL_SIZE);

        XSetForeground(display, gc, WhitePixel(display, screen));
        XDrawString(display, window, gc, 8, CELL_SIZE + 11, "SCORE: 0", 8);

        int idx;
        for (idx = tail_idx; idx != head_idx; idx = (idx + 1) % MAX_SNAKE)
            draw_cell(display, window, gc,
                      segments[idx].x, segments[idx].y, body_col.pixel);
        draw_cell(display, window, gc,
                  segments[head_idx].x, segments[head_idx].y, head_col.pixel);

        XFlush(display);
    }

    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
```

---

## Step 9 — Raylib Version

Update `src/main_raylib.c`:

```c
#include "raylib.h"
#include <stdlib.h>

#define GRID_WIDTH   60
#define GRID_HEIGHT  20
#define CELL_SIZE    14
#define HEADER_ROWS   3
#define MAX_SNAKE    (GRID_WIDTH * GRID_HEIGHT)

typedef struct { int x; int y; } Segment;

static const int DX[4] = { 0,  1,  0, -1 };
static const int DY[4] = {-1,  0,  1,  0 };

int main(void) {
    int win_w = GRID_WIDTH  * CELL_SIZE;
    int win_h = (GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE;

    InitWindow(win_w, win_h, "Snake");
    SetTargetFPS(60);

    Color wall_color = {102, 102, 102, 255};
    Color body_color = { 50, 205,  50, 255};
    Color head_color = {  0, 128,   0, 255};

    Segment segments[MAX_SNAKE];
    int head_idx = 9, tail_idx = 0;
    int direction = 1, next_direction = 1;
    int speed = 8, tick_count = 0;
    int i;
    for (i = 0; i < 10; i++) {
        segments[i].x = GRID_WIDTH / 2 - 5 + i;
        segments[i].y = GRID_HEIGHT / 2;
    }

    float accum = 0.0f;

    while (!WindowShouldClose()) {
        accum += GetFrameTime() * 1000.0f;

        /* Input */
        if (IsKeyPressed(KEY_RIGHT)) {
            int turned = (direction + 1) % 4;
            if (abs(turned - direction) != 2)
                next_direction = turned;
        }
        if (IsKeyPressed(KEY_LEFT)) {
            int turned = (direction + 3) % 4;
            if (abs(turned - direction) != 2)
                next_direction = turned;
        }

        /* Tick */
        if (accum >= 150.0f) {
            accum -= 150.0f;
            tick_count++;
            if (tick_count >= speed) {
                tick_count = 0;
                direction = next_direction;

                int new_x = segments[head_idx].x + DX[direction];
                int new_y = segments[head_idx].y + DY[direction];
                head_idx = (head_idx + 1) % MAX_SNAKE;
                segments[head_idx].x = new_x;
                segments[head_idx].y = new_y;
                tail_idx = (tail_idx + 1) % MAX_SNAKE;
            }
        }

        BeginDrawing();
            ClearBackground(BLACK);

            DrawRectangle(0, 0,             win_w, CELL_SIZE, wall_color);
            DrawRectangle(0, 2 * CELL_SIZE, win_w, CELL_SIZE, wall_color);
            DrawText("SCORE: 0", 8, CELL_SIZE + 2, 12, WHITE);

            int idx;
            for (idx = tail_idx; idx != head_idx; idx = (idx + 1) % MAX_SNAKE) {
                int px = segments[idx].x * CELL_SIZE + 1;
                int py = (segments[idx].y + HEADER_ROWS) * CELL_SIZE + 1;
                DrawRectangle(px, py, CELL_SIZE - 2, CELL_SIZE - 2, body_color);
            }
            {
                int px = segments[head_idx].x * CELL_SIZE + 1;
                int py = (segments[head_idx].y + HEADER_ROWS) * CELL_SIZE + 1;
                DrawRectangle(px, py, CELL_SIZE - 2, CELL_SIZE - 2, head_color);
            }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

**X11 vs Raylib input comparison:**

| Task | X11 | Raylib |
|------|-----|--------|
| Check for available events | `XPending(display)` | *(automatic)* |
| Get next event | `XNextEvent(display, &event)` | *(automatic)* |
| Check key pressed | `XLookupKeysym(&event.xkey, 0) == XK_Right` | `IsKeyPressed(KEY_RIGHT)` |

Raylib handles event polling internally. You just call `IsKeyPressed()` each frame — much simpler.

---

## Build & Run

**X11:**
```bash
./build_x11.sh
./snake_x11
```

**Raylib:**
```bash
./build_raylib.sh
./snake_raylib
```

**What to see:** The snake moves automatically to the right. Press the **right arrow** to turn clockwise (snake turns down). Press **left arrow** to turn counter-clockwise (snake turns up). You can steer it all around the arena. It still vanishes off the edges — collision comes in Lesson 06.

Try to steer it in a big circle without letting it leave the arena. How far can you go?

---

## Mental Model

Turning is not "set direction to LEFT." It's "rotate clockwise or counter-clockwise from wherever I'm facing." Direction is a number 0–3 on a circle. `(direction + 1) % 4` = one step clockwise. `(direction + 3) % 4` = one step counter-clockwise. The modulo wraps around so 3+1=0 (LEFT→UP) works automatically.

> JS analogy: `direction = (direction + 1) % 4` is like a `currentIndex` in a carousel component that loops: `currentIndex = (currentIndex + 1) % items.length`. Same pattern.

---

## Exercise

**Add direct UP/DOWN/LEFT/RIGHT control as an alternative scheme.**

When `XK_Up` is pressed, set `next_direction = 0`. When `XK_Down` is pressed, set `next_direction = 2`. But still check for reversals first:

```c
} else if (key == XK_Up) {
    if (abs(0 - direction) != 2)
        next_direction = 0;

} else if (key == XK_Down) {
    if (abs(2 - direction) != 2)
        next_direction = 2;
}
```

In Raylib:
```c
if (IsKeyPressed(KEY_UP)   && abs(0 - direction) != 2) next_direction = 0;
if (IsKeyPressed(KEY_DOWN) && abs(2 - direction) != 2) next_direction = 2;
```

Build and run. Notice the feel is different: pressing UP always means "go up," not "turn left relative to my heading." Which control scheme feels more natural for Snake? (The original game uses absolute directions, but many modern remakes use relative turning.)
