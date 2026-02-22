# Lesson 04 — Make the Snake Move

**By the end of this lesson you will have:**
The snake glides across the arena on its own — one cell per tick. When it hits the right wall it vanishes (no collision yet). You can watch the ring buffer in action.

---

## What We're Building

The snake now moves automatically to the right at a steady pace. No controls yet — just automatic rightward movement. When the head goes past column 59 (the right edge), it wraps to negative territory and vanishes off the left edge too. That's fine — we'll add wall collision in Lesson 06.

---

## Step 1 — What "Movement" Means

Movement is just two operations every game tick:
1. **Add a new head cell** one step ahead in the current direction
2. **Remove the old tail cell** (advance the tail index)

The snake appears to slide forward. The middle segments don't move at all — they're just forgotten from the tail end and revealed at the head end.

```
Before move (direction = RIGHT):
  [25,10] [26,10] [27,10] [28,10] ... [33,10] [34,10]
   tail=0                                       head=9

After move:
  [25,10] [26,10] [27,10] [28,10] ... [33,10] [34,10] [35,10]
           tail=1                                       head=10
   (slot 0 is "gone" — tail advanced past it)
```

`[25,10]` is still in the array, but we won't read it because `tail_idx` has moved past it. The ring buffer makes this a zero-copy operation — we just changed two integers.

---

## Step 2 — Add the Direction System

Add this **above `main()`**, with your constants:

```c
/* direction: 0=UP, 1=RIGHT, 2=DOWN, 3=LEFT */
static const int DX[4] = { 0,  1,  0, -1 };
static const int DY[4] = {-1,  0,  1,  0 };
```

**What these arrays do:**

| `direction` | Name | `DX[direction]` | `DY[direction]` | Meaning |
|-------------|------|-----------------|-----------------|---------|
| 0 | UP    |  0 | -1 | x stays same, y decreases (up in screen coords) |
| 1 | RIGHT |  1 |  0 | x increases, y stays same |
| 2 | DOWN  |  0 |  1 | x stays same, y increases |
| 3 | LEFT  | -1 |  0 | x decreases, y stays same |

**Why y decreases when going UP:** Screen coordinates in X11 (and most graphics systems) have y=0 at the **top** and increasing downward. So "up" on screen = smaller y value.

```
y=0  ── top of window
y=14 ── row 1
y=28 ── row 2
         ↓ y increases downward
y=322 ── bottom of window
```

> In JS canvas: `ctx.fillRect(0, 0, 10, 10)` draws at the **top-left** — same thing.

---

## Step 3 — Add `platform_sleep_ms`

Our game loop needs to sleep between frames. Add this function **above `main()`**:

```c
#include <time.h>

void platform_sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
```

**Understanding `struct timespec`:**

`nanosleep` takes time split into two parts:
- `tv_sec` — whole seconds
- `tv_nsec` — the remaining nanoseconds (billionths of a second)

**Concrete examples:**

```
50ms:
  ms = 50
  tv_sec  = 50 / 1000 = 0       (zero whole seconds)
  tv_nsec = (50 % 1000) * 1000000 = 50 * 1000000 = 50,000,000 nanoseconds

150ms:
  ms = 150
  tv_sec  = 150 / 1000 = 0
  tv_nsec = (150 % 1000) * 1000000 = 150 * 1000000 = 150,000,000 nanoseconds

1500ms (1.5 seconds):
  ms = 1500
  tv_sec  = 1500 / 1000 = 1     (one whole second)
  tv_nsec = (1500 % 1000) * 1000000 = 500 * 1000000 = 500,000,000 nanoseconds
```

> `1000000L` — the `L` suffix means "long integer literal". It prevents integer overflow on 32-bit systems where `1000 * 1000000` could overflow a plain `int`.

Add `#include <time.h>` to your includes at the top of the file.

---

## Step 4 — Add the Speed Counter

Add these variables in `main()`, near your other game variables:

```c
    int direction   = 1;    /* start moving RIGHT */
    int speed       = 8;    /* move every 8 ticks */
    int tick_count  = 0;
```

**How the speed system works:**

We sleep 150ms per tick (`BASE_TICK_MS = 150`). We only move the snake every `speed` ticks.

```
speed=8:  8 ticks × 150ms = 1200ms = 1.2 seconds per step  (very slow)
speed=4:  4 ticks × 150ms =  600ms = 0.6 seconds per step
speed=2:  2 ticks × 150ms =  300ms = 0.3 seconds per step
speed=1:  1 tick  × 150ms =  150ms = 0.15 seconds per step  (fast)
```

We'll use `speed` to control difficulty later — as the score increases, `speed` decreases.

**Tick timeline (speed=8):**

```
Tick:   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15  16
Move?:  ·   ·   ·   ·   ·   ·   ·   ✓   ·   ·   ·   ·   ·   ·   ·   ✓
        └─────────────────────────┘   └─────────────────────────┘
               7 idle ticks              move on tick 8!
```

---

## Step 5 — Restructure the Game Loop

Replace the simple event loop from Lesson 01 with a **proper game loop** that:
1. Sleeps 150ms
2. Processes any pending input events (without blocking)
3. Advances the tick counter
4. Moves the snake every `speed` ticks
5. Redraws everything

Replace the `while (1) { XNextEvent... }` block with:

```c
    int running = 1;
    XEvent event;

    while (running) {
        /* 1. Sleep for one tick */
        platform_sleep_ms(150);

        /* 2. Process all pending input (non-blocking) */
        while (XPending(display)) {
            XNextEvent(display, &event);
            if (event.type == KeyPress) {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_q || key == XK_Q || key == XK_Escape) {
                    running = 0;
                }
            }
        }
        if (!running) break;

        /* 3. Advance tick counter */
        tick_count++;
        if (tick_count < speed) continue;  /* not time to move yet */
        tick_count = 0;

        /* 4. Move the snake */
        int new_x = segments[head_idx].x + DX[direction];
        int new_y = segments[head_idx].y + DY[direction];

        /* Advance head */
        head_idx = (head_idx + 1) % MAX_SNAKE;
        segments[head_idx].x = new_x;
        segments[head_idx].y = new_y;

        /* Advance tail (remove old tail) */
        tail_idx = (tail_idx + 1) % MAX_SNAKE;

        /* 5. Redraw */
        XSetForeground(display, gc, BlackPixel(display, screen));
        XFillRectangle(display, window, gc, 0, HEADER_ROWS * CELL_SIZE,
                       win_w, GRID_HEIGHT * CELL_SIZE);

        /* Header bars */
        XSetForeground(display, gc, wall_col.pixel);
        XFillRectangle(display, window, gc, 0, 0, win_w, CELL_SIZE);
        XFillRectangle(display, window, gc, 0, 2 * CELL_SIZE, win_w, CELL_SIZE);

        /* Score */
        XSetForeground(display, gc, WhitePixel(display, screen));
        XDrawString(display, window, gc, 8, CELL_SIZE + 11,
                    "SCORE: 0", 8);

        /* Snake body */
        int idx;
        for (idx = tail_idx; idx != head_idx; idx = (idx + 1) % MAX_SNAKE) {
            draw_cell(display, window, gc, segments[idx].x, segments[idx].y,
                      body_col.pixel);
        }
        draw_cell(display, window, gc, segments[head_idx].x, segments[head_idx].y,
                  head_col.pixel);

        XFlush(display);
    }
```

**Key difference from before — `XPending` vs `XNextEvent`:**

| Before | Now |
|--------|-----|
| `XNextEvent` **blocks** — waits forever for input | `XPending` checks if events are available without blocking |
| Loop could only advance on a keypress | Loop advances on a timer (`platform_sleep_ms`) |

```
Old loop:  [wait for keypress] → [process key] → [wait for keypress] → ...
New loop:  [sleep 150ms] → [drain any pending keys] → [maybe move] → [redraw] → [sleep 150ms] → ...
```

> JS equivalent: `XNextEvent` was like `await fetch(url)` (blocks). `XPending` + `XNextEvent` is like checking `if (eventQueue.length > 0) processNext()` — non-blocking.

---

## Step 6 — Walk Through the Move Logic

Let's trace one complete move step with real numbers:

```
State before move:
  direction = 1 (RIGHT)
  DX[1] = 1, DY[1] = 0
  head_idx = 9,  segments[9]  = {x:34, y:10}
  tail_idx = 0,  segments[0]  = {x:25, y:10}

Step A: compute new head position
  new_x = segments[9].x + DX[1] = 34 + 1 = 35
  new_y = segments[9].y + DY[1] = 10 + 0 = 10

Step B: advance head index
  head_idx = (9 + 1) % 1200 = 10
  segments[10] = {x:35, y:10}   ← new head written

Step C: advance tail index
  tail_idx = (0 + 1) % 1200 = 1
  segments[0] is now "forgotten"

State after move:
  head_idx = 10, segments[10] = {x:35, y:10}
  tail_idx = 1,  segments[1]  = {x:26, y:10}
  Length is still 10 (we wrote 1 and removed 1)
```

Visually:
```
Before: [25,10][26,10][27,10]...[33,10][34,10]
              ↑tail=0                   ↑head=9

After:  [25,10][26,10][27,10]...[33,10][34,10][35,10]
                ↑tail=1                         ↑head=10
         (slot 0 ignored)
```

---

## Step 7 — Full `src/main_x11.c`

```c
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <string.h>
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
    int direction = 1;   /* RIGHT */
    int speed = 8, tick_count = 0;
    int i;
    for (i = 0; i < 10; i++) {
        segments[i].x = GRID_WIDTH / 2 - 5 + i;
        segments[i].y = GRID_HEIGHT / 2;
    }

    /* Main game loop */
    int running = 1;
    XEvent event;
    while (running) {
        platform_sleep_ms(150);

        /* Drain input */
        while (XPending(display)) {
            XNextEvent(display, &event);
            if (event.type == KeyPress) {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_q || key == XK_Q || key == XK_Escape)
                    running = 0;
            }
        }
        if (!running) break;

        /* Tick */
        tick_count++;
        if (tick_count < speed) continue;
        tick_count = 0;

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

## Step 8 — Raylib Version

Update `src/main_raylib.c`:

```c
#include "raylib.h"
#include <stdio.h>

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
    int direction = 1;
    int speed = 8, tick_count = 0;
    int i;
    for (i = 0; i < 10; i++) {
        segments[i].x = GRID_WIDTH / 2 - 5 + i;
        segments[i].y = GRID_HEIGHT / 2;
    }

    float accum = 0.0f;
    const float TICK_MS = 150.0f;

    while (!WindowShouldClose()) {
        accum += GetFrameTime() * 1000.0f;

        BeginDrawing();
            ClearBackground(BLACK);

            DrawRectangle(0, 0,             win_w, CELL_SIZE, wall_color);
            DrawRectangle(0, 2 * CELL_SIZE, win_w, CELL_SIZE, wall_color);
            DrawText("SCORE: 0", 8, CELL_SIZE + 2, 12, WHITE);

            /* Move if enough time has passed */
            if (accum >= TICK_MS) {
                accum -= TICK_MS;
                tick_count++;
                if (tick_count >= speed) {
                    tick_count = 0;
                    int new_x = segments[head_idx].x + DX[direction];
                    int new_y = segments[head_idx].y + DY[direction];
                    head_idx = (head_idx + 1) % MAX_SNAKE;
                    segments[head_idx].x = new_x;
                    segments[head_idx].y = new_y;
                    tail_idx = (tail_idx + 1) % MAX_SNAKE;
                }
            }

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

> **Why `accum` in Raylib?** Raylib runs at 60 FPS — one frame every ~16ms. We want to move the snake every 150ms. The accumulator adds up elapsed time each frame and fires a tick when it exceeds 150ms. This is a standard "fixed timestep" technique.

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

**What to see:** The snake slides to the right, one cell at a time. It moves roughly once per second (8 ticks × 150ms). When the head goes past column 59, it vanishes because we draw it outside the window boundaries. Press Q or Escape to quit.

---

## Mental Model

The snake "moves" by adding one cell at the front and discarding one from the back each tick. The middle stays the same — it's never touched. The ring buffer makes this a zero-copy operation: only two integer indices (`head_idx` and `tail_idx`) change. If this were a regular array, we'd have to shift all 10 elements left every tick — the ring buffer does the same logical thing by just moving two pointers.

> JS analogy: Imagine a circular conveyor belt. `head_idx` is where you put new items. `tail_idx` is where items fall off. The belt itself never moves — only the two position markers advance.

---

## Exercise

**Change `direction = 2` (DOWN) at startup and observe.** Then try `direction = 0` (UP). In each case, trace through the math manually to predict which direction the snake will move before you run the program:

```
direction = 2 (DOWN):
  DX[2] = 0, DY[2] = 1
  Each tick: x stays same, y increases → snake moves DOWN
  Starting head at (34, 10) → next head at (34, 11) → (34, 12) → ...

direction = 0 (UP):
  DX[0] = 0, DY[0] = -1
  Each tick: x stays same, y decreases → snake moves UP
  Starting head at (34, 10) → next head at (34, 9) → (34, 8) → ...
```

Build and run each. Confirm your prediction matches what you see. This validates your mental model of the direction system.
