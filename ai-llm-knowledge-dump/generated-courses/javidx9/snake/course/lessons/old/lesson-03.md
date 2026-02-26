# Lesson 03 — Draw a Static Snake

**By the end of this lesson you will have:**
A 10-segment green snake visible in the arena, made of colored rectangles. It doesn't move yet — it's drawn in a fixed horizontal position.

---

## What We're Building

After this lesson your window looks like this:

```
┌──────────────────────────────────────────────────────────────────┐
│░░░░░░░░░░░░░░ gray bar ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
│  SCORE: 0                                                        │
│░░░░░░░░░░░░░░ gray bar ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
│                                                                  │
│          [][][][][][][][][]██  ← 9 green + 1 bright head        │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

Each `[]` is a 13×13 pixel filled rectangle (14px cell minus 1px gap on each side).

---

## Step 1 — What Is the Snake?

The snake is a **sequence of grid cells**. Each cell has an (x, y) coordinate in the grid — not in pixels, but in columns and rows.

For example, a snake whose head is at column 34, row 10, and stretches left:

```
Grid coords:   (25,10) (26,10) (27,10) ... (33,10) (34,10)
               [tail]  [    ]  [    ]  ...  [   ]   [head]
```

To draw cell (col, row) on screen:
```
pixel_x = col * CELL_SIZE          = 34 * 14 = 476
pixel_y = (row + HEADER_ROWS) * CELL_SIZE  = (10 + 3) * 14 = 182
```

We add `HEADER_ROWS` because the arena sits below the header.

---

## Step 2 — Define the Segment Type

Add this **above `main()`**, right after your `#define` lines:

```c
typedef struct {
    int x;
    int y;
} Segment;
```

**What `typedef struct` means:**

In C, you define custom types with `struct`. The `typedef` keyword gives it a short name so you can write `Segment` instead of `struct Segment` everywhere.

> **JS equivalent:**
> ```typescript
> type Segment = { x: number; y: number };
> ```
> It's the same idea — a named shape with two integer fields.

Without `typedef` you'd have to write `struct Segment seg;` every time. With it, you write `Segment seg;`. `typedef` is just a convenience alias.

---

## Step 3 — The Ring Buffer — Why and How

We need a data structure that supports two operations cheaply every game tick:
1. **Add a new cell to the front** (the new head position)
2. **Remove the old cell from the back** (the old tail)

In a plain array, adding to the front means shifting every element right — expensive. **A ring buffer solves this with two moving indices and no copying.**

Here's how it works visually:

```
Array slots:   [  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  ]
                        ↑ tail                        ↑ head

Each move tick:
  1. Write new head cell at (head + 1) % MAX_SNAKE
  2. Advance head index by 1
  3. Advance tail index by 1  (this "removes" the old tail — we just stop reading it)

If snake is GROWING (just ate food):
  - Do step 1 and 2 but skip step 3 (tail stays put = one extra cell retained)
```

Concrete example with a 5-slot snake, MAX_SNAKE=8:
```
Before move:
  slots:  [ . | B | B | B | B | H | . | . ]
                ↑ tail=1                ↑ head=5
  (B=body, H=head, .=unused)

After move (no food):
  slots:  [ . | . | B | B | B | H | N | . ]
                    ↑ tail=2            ↑ head=6  (N=new head)
```

The old tail at index 1 is just "forgotten" — we never read it again. **No copying, no shifting.** We just moved two numbers.

---

## Step 4 — Define MAX_SNAKE

Add this `#define` with your other constants:

```c
#define MAX_SNAKE  (GRID_WIDTH * GRID_HEIGHT)
```

**Why this value?**
- `GRID_WIDTH * GRID_HEIGHT = 60 * 20 = 1200`
- The snake can never be longer than the entire grid — it would have to eat every food pellet without dying to reach this. So 1200 slots is the safe maximum.

This evaluates at compile time. `#define` values can reference other `#define` values.

---

## Step 5 — Declare the Snake Data in `main()`

Add this at the top of `main()`, after the window setup code but before any drawing:

```c
    Segment segments[MAX_SNAKE];
    int head   = 9;   /* index of the head segment */
    int tail   = 0;   /* index of the tail segment */
    int length = 10;  /* number of active segments */

    /* Initialize 10 segments in a horizontal line at the center */
    int i;
    for (i = 0; i < 10; i++) {
        segments[i].x = GRID_WIDTH / 2 - 5 + i;   /* columns 25..34 */
        segments[i].y = GRID_HEIGHT / 2;            /* row 10         */
    }
```

**Breaking down the initialization math:**

```
GRID_WIDTH / 2        = 60 / 2 = 30    (center column)
GRID_WIDTH / 2 - 5    = 30 - 5 = 25    (start 5 left of center)

i=0: x = 25 + 0 = 25   (tail)
i=1: x = 25 + 1 = 26
...
i=9: x = 25 + 9 = 34   (head)

GRID_HEIGHT / 2 = 20 / 2 = 10   (center row)
```

So the snake occupies columns 25–34 on row 10. `head = 9` because `segments[9]` is at column 34 (the head). `tail = 0` because `segments[0]` is at column 25.

> **`int i` declared before `for`:** In C89 (older C standard), you must declare variables at the top of a block before any code. Many compilers still warn if you declare inside a `for` loop. If you're compiling with `-std=c99` or later you can write `for (int i = 0; ...)` — but declaring at the top is always safe.

---

## Step 6 — Write the `draw_cell` Helper

Add this function **above `main()`** (C requires functions to be declared before they're called):

```c
void draw_cell(Display *display, Window window, GC gc,
               int col, int row, unsigned long color)
{
    /* Convert grid coordinates to pixel coordinates */
    int px = col * CELL_SIZE + 1;
    int py = (row + HEADER_ROWS) * CELL_SIZE + 1;
    int size = CELL_SIZE - 2;

    XSetForeground(display, gc, color);
    XFillRectangle(display, window, gc, px, py, size, size);
}
```

**The pixel math explained:**

```
col = 5, CELL_SIZE = 14, HEADER_ROWS = 3

px   = 5 * 14 + 1 = 71
py   = (row + 3) * 14 + 1

size = 14 - 2 = 12
```

The `+ 1` and `- 2` create a **1-pixel gap** between adjacent cells. Without it, all cells would be flush against each other and look like a solid blob. The gap gives you the grid-line effect:

```
With gap (size=12, offset=1):      Without gap (size=14, offset=0):
┌──────────┐ ┌──────────┐          ┌────────────────────────┐
│          │ │          │          │                        │
│  cell A  │ │  cell B  │    vs    │  cell A  ││  cell B    │
│          │ │          │          │          ││            │
└──────────┘ └──────────┘          └────────────────────────┘
     ↑ 1px gap                          ↑ no gap = looks merged
```

---

## Step 7 — Allocate Snake Colors

Add color allocation in `main()`, with your other color setup:

```c
    XColor body_col, head_col, exact2;
    XAllocNamedColor(display, cmap, "lime green",  &body_col, &exact2);
    XAllocNamedColor(display, cmap, "green",       &head_col, &exact2);
```

`"lime green"` is a bright green (RGB ~50,205,50). `"green"` is a darker pure green (RGB 0,128,0). The head uses the brighter color to stand out.

---

## Step 8 — Draw the Snake

Add this drawing code in `main()`, after the header bars and score text, before `XFlush`:

```c
    /* Draw snake body (tail to head, excluding head) */
    int idx;
    for (idx = tail; idx != head; idx = (idx + 1) % MAX_SNAKE) {
        draw_cell(display, window, gc, segments[idx].x, segments[idx].y, body_col.pixel);
    }
    /* Draw head in brighter green */
    draw_cell(display, window, gc, segments[head].x, segments[head].y, head_col.pixel);
```

**How the ring buffer loop works:**

```
tail = 0, head = 9, MAX_SNAKE = 1200

idx starts at 0 (tail)
loop body: draw segments[0]  (x=25)
idx = (0 + 1) % 1200 = 1
loop body: draw segments[1]  (x=26)
...
idx = (8 + 1) % 1200 = 9
9 == head → stop loop

Then draw segments[9] as the head (x=34)
```

The loop draws every segment from tail up to (but not including) head, then we draw head separately in a different color.

> **Why `idx != head` and not `idx <= head`?** Because the ring buffer wraps around. If head < tail (the indices have wrapped), a `<=` check would fail. Using `!=` with modular increment works correctly in all cases.

---

## Step 9 — Full `src/main_x11.c` So Far

```c
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <string.h>

#define GRID_WIDTH   60
#define GRID_HEIGHT  20
#define CELL_SIZE    14
#define HEADER_ROWS   3
#define MAX_SNAKE    (GRID_WIDTH * GRID_HEIGHT)

typedef struct {
    int x;
    int y;
} Segment;

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
    if (!display) {
        fprintf(stderr, "Error: cannot open X display\n");
        return 1;
    }

    int screen = DefaultScreen(display);
    int win_w  = GRID_WIDTH  * CELL_SIZE;
    int win_h  = (GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE;

    Window window = XCreateSimpleWindow(
        display,
        RootWindow(display, screen),
        0, 0,
        win_w, win_h,
        1,
        BlackPixel(display, screen),
        BlackPixel(display, screen)
    );

    XSelectInput(display, window, KeyPressMask);
    XStoreName(display, window, "Snake");
    XMapWindow(display, window);
    XFlush(display);

    /* Allocate colors */
    Colormap cmap = DefaultColormap(display, screen);
    XColor wall_col, body_col, head_col, exact;
    XAllocNamedColor(display, cmap, "gray40",    &wall_col, &exact);
    XAllocNamedColor(display, cmap, "lime green",&body_col, &exact);
    XAllocNamedColor(display, cmap, "green",     &head_col, &exact);
    GC gc = XCreateGC(display, window, 0, NULL);

    /* Declare snake data */
    Segment segments[MAX_SNAKE];
    int head_idx = 9;
    int tail_idx = 0;
    int i;
    for (i = 0; i < 10; i++) {
        segments[i].x = GRID_WIDTH / 2 - 5 + i;
        segments[i].y = GRID_HEIGHT / 2;
    }

    /* --- Draw everything --- */

    /* Header bars */
    XSetForeground(display, gc, wall_col.pixel);
    XFillRectangle(display, window, gc, 0, 0, win_w, CELL_SIZE);
    XFillRectangle(display, window, gc, 0, 2 * CELL_SIZE, win_w, CELL_SIZE);

    /* Score label */
    char score_buf[64];
    snprintf(score_buf, sizeof(score_buf), "SCORE: 0");
    XSetForeground(display, gc, WhitePixel(display, screen));
    XDrawString(display, window, gc,
        8, CELL_SIZE + 11,
        score_buf, (int)strlen(score_buf));

    /* Snake body */
    int idx;
    for (idx = tail_idx; idx != head_idx; idx = (idx + 1) % MAX_SNAKE) {
        draw_cell(display, window, gc, segments[idx].x, segments[idx].y, body_col.pixel);
    }
    draw_cell(display, window, gc, segments[head_idx].x, segments[head_idx].y, head_col.pixel);

    XFlush(display);

    /* Event loop */
    XEvent event;
    while (1) {
        XNextEvent(display, &event);
        if (event.type == KeyPress) {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_q || key == XK_Q || key == XK_Escape) {
                break;
            }
        }
    }

    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
```

> Note: we renamed `head` and `tail` to `head_idx` and `tail_idx` to avoid shadowing any potential future variables.

---

## Step 10 — Raylib Version

Update `src/main_raylib.c`:

```c
#include "raylib.h"
#include <stdio.h>

#define GRID_WIDTH   60
#define GRID_HEIGHT  20
#define CELL_SIZE    14
#define HEADER_ROWS   3
#define MAX_SNAKE    (GRID_WIDTH * GRID_HEIGHT)

typedef struct {
    int x;
    int y;
} Segment;

int main(void) {
    int win_w = GRID_WIDTH  * CELL_SIZE;
    int win_h = (GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE;

    InitWindow(win_w, win_h, "Snake");
    SetTargetFPS(60);

    /* Snake data */
    Segment segments[MAX_SNAKE];
    int head_idx = 9;
    int tail_idx = 0;
    int i;
    for (i = 0; i < 10; i++) {
        segments[i].x = GRID_WIDTH / 2 - 5 + i;
        segments[i].y = GRID_HEIGHT / 2;
    }

    Color wall_color = {102, 102, 102, 255};
    Color body_color = { 50, 205,  50, 255};  /* lime green */
    Color head_color = {  0, 128,   0, 255};  /* green */

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(BLACK);

            /* Header */
            DrawRectangle(0, 0,             win_w, CELL_SIZE, wall_color);
            DrawRectangle(0, 2 * CELL_SIZE, win_w, CELL_SIZE, wall_color);
            DrawText("SCORE: 0", 8, CELL_SIZE + 2, 12, WHITE);

            /* Snake body */
            int idx;
            for (idx = tail_idx; idx != head_idx; idx = (idx + 1) % MAX_SNAKE) {
                int px = segments[idx].x * CELL_SIZE + 1;
                int py = (segments[idx].y + HEADER_ROWS) * CELL_SIZE + 1;
                DrawRectangle(px, py, CELL_SIZE - 2, CELL_SIZE - 2, body_color);
            }
            /* Head */
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

**What to see:** The header with two gray bars and "SCORE: 0", then in the middle of the dark play area: a horizontal line of 9 green rectangles (body) with a brighter green rectangle at the right end (head). Press Q or Escape to quit.

---

## Mental Model

The ring buffer is just an array with two moving pointers. `head_idx` marks where the newest cell is written. `tail_idx` marks the oldest cell still alive. Each move tick, both pointers advance by one (modulo MAX_SNAKE). No data is ever copied or shifted — only the two index numbers change. When `head_idx` or `tail_idx` reaches 1199, the next increment wraps back to 0.

> JS analogy: Imagine a circular conveyor belt with 1200 slots. New items go in at `head`. Old items fall off at `tail`. The belt never moves — only the two "markers" walk forward.

---

## Exercise

**Change the starting snake to be vertical instead of horizontal.**

Replace the initialization loop with:

```c
    for (i = 0; i < 10; i++) {
        segments[i].x = GRID_WIDTH  / 2;          /* center column */
        segments[i].y = GRID_HEIGHT / 2 - 5 + i;  /* rows 5..14   */
    }
```

Build and run. You should see a vertical line of 10 green rectangles in the center of the arena. The head (`segments[9]`) is now at row 14 (lower) and the tail (`segments[0]`) is at row 5 (upper).

Try also changing `head_idx` and `tail_idx` and the initial length, and see how the drawing changes.
