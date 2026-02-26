# Lesson 02 — Draw the Arena

**By the end of this lesson you will have:**
The window shows a Snake arena: two horizontal gray bars forming a header border, a "SCORE: 0" label in the header area, and a dark grid play area below.

---

## What We're Building

After this lesson your window looks like this (ASCII approximation):

```
┌──────────────────────────────────────────────────────────────────┐
│░░░░░░░░░░░░░░░ gray bar (row 0) ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
│  SCORE: 0                                                        │
│░░░░░░░░░░░░░░░ gray bar (row 2) ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
│                                                                  │
│                    (dark play area)                              │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

Row 0 = top gray bar. Row 1 = score text row. Row 2 = bottom gray bar (the "ceiling" of the arena). Rows 3–22 = the 20-row play area.

---

## Step 1 — Add the Constants

Open `src/main_x11.c` (from Lesson 01). Right after the `#include` lines and before `main()`, add:

```c
#define GRID_WIDTH   60
#define GRID_HEIGHT  20
#define CELL_SIZE    14
#define HEADER_ROWS   3
```

**What each constant means:**

| Constant | Value | Meaning |
|----------|-------|---------|
| `GRID_WIDTH` | 60 | The arena is 60 cells wide |
| `GRID_HEIGHT` | 20 | The arena is 20 cells tall |
| `CELL_SIZE` | 14 | Each cell is 14×14 pixels |
| `HEADER_ROWS` | 3 | The header occupies 3 rows above the arena |

> **`#define` is like `const` in JS — but simpler.** When you write `#define CELL_SIZE 14`, the compiler does a text find-and-replace before compiling: every place you wrote `CELL_SIZE` becomes `14`. There's no variable, no memory — it's purely a compile-time substitution.
>
> JS equivalent: `const CELL_SIZE = 14;`

---

## Step 2 — Calculate the Window Size

Replace the hardcoded `840, 322` in `XCreateSimpleWindow` with calculated values. First, let's understand the math:

```
Window width  = GRID_WIDTH  × CELL_SIZE
              = 60          × 14
              = 840 pixels

Window height = (GRID_HEIGHT + HEADER_ROWS) × CELL_SIZE
              = (20          + 3)            × 14
              = 23 × 14
              = 322 pixels
```

The header rows are **above** the arena. Think of it like CSS: the header takes up 3 × 14 = 42 pixels at the top, and the play area takes up 20 × 14 = 280 pixels below it. Total = 322px.

Now compute these at the top of `main()`, before `XCreateSimpleWindow`:

```c
    int win_w = GRID_WIDTH  * CELL_SIZE;
    int win_h = (GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE;
```

And use them in `XCreateSimpleWindow`:

```c
    Window window = XCreateSimpleWindow(
        display,
        RootWindow(display, screen),
        0, 0,
        win_w, win_h,           /* <-- use variables instead of hardcoded 840, 322 */
        1,
        BlackPixel(display, screen),
        BlackPixel(display, screen)
    );
```

Build and run. The window should look exactly the same — we just made the size formula visible.

---

## Step 3 — Allocate Colors for X11 Drawing

Before we can draw colored rectangles in X11, we need to **allocate colors**. Add this code inside `main()`, after `XMapWindow(display, window)` and before the event loop:

```c
    Colormap cmap = DefaultColormap(display, screen);
    XColor wall_col, exact;
    XAllocNamedColor(display, cmap, "gray40", &wall_col, &exact);
```

**Why we need this:**
X11 was designed for systems with limited color palettes. Before using a color, you request a "slot" in the colormap. `XAllocNamedColor` looks up `"gray40"` (a named X11 color — medium-dark gray) and fills the `wall_col` struct with its pixel value.

> Think of it like: `const wallColor = getComputedStyle(el).getPropertyValue('--gray40')`. You're resolving a name to an actual color value.

You'll also need a `GC` (Graphics Context) — an X11 object that holds drawing settings like foreground color:

```c
    GC gc = XCreateGC(display, window, 0, NULL);
```

> A `GC` is like a Canvas 2D context in the browser: `const ctx = canvas.getContext('2d')`. You set properties on it (color, line width) and then draw.

---

## Step 4 — Draw the Two Header Bars

Now draw the gray bars. Add this drawing code **inside the event loop, before `XNextEvent`** — we'll restructure to a proper render loop in a moment. Actually, let's draw once before the event loop, then flush.

Add this block right before `while (1) {`:

```c
    /* Draw header bars */
    XSetForeground(display, gc, wall_col.pixel);

    /* Top bar: row 0 */
    XFillRectangle(display, window, gc,
        0,               /* x pixel position */
        0,               /* y pixel position */
        win_w,           /* width  = full window width */
        CELL_SIZE        /* height = one cell tall */
    );

    /* Bottom-of-header bar: row 2 */
    XFillRectangle(display, window, gc,
        0,                        /* x pixel position */
        2 * CELL_SIZE,            /* y = row 2 × 14 = 28 pixels from top */
        win_w,                    /* width = full window width */
        CELL_SIZE                 /* height = one cell tall */
    );

    XFlush(display);
```

**Coordinate diagram:**

```
y=0  ┌─────────────────────────────────┐
     │  Gray bar (CELL_SIZE=14px tall) │  ← row 0
y=14 ├─────────────────────────────────┤
     │  SCORE: 0   (blank row for text)│  ← row 1
y=28 ├─────────────────────────────────┤
     │  Gray bar (CELL_SIZE=14px tall) │  ← row 2
y=42 ├─────────────────────────────────┤
     │                                 │
     │         Play area               │  ← rows 3..22
     │                                 │
y=322└─────────────────────────────────┘
```

`XFillRectangle` takes: display, drawable, gc, x, y, width, height. The x and y are pixel positions from the top-left corner.

---

## Step 5 — Draw the Score Text

Add this right after the two `XFillRectangle` calls:

```c
    char score_buf[64];
    snprintf(score_buf, sizeof(score_buf), "SCORE: 0");

    XSetForeground(display, gc, WhitePixel(display, screen));
    XDrawString(display, window, gc,
        8,              /* x: 8 pixels from left edge */
        CELL_SIZE + 11, /* y: row 1 baseline — 14px + 11px padding */
        score_buf,
        (int)strlen(score_buf)
    );
```

You'll need `<string.h>` for `strlen`. Add it to your includes at the top:

```c
#include <string.h>
```

**`snprintf` vs `sprintf`:**

| Function | Risk | Use |
|----------|------|-----|
| `sprintf(buf, fmt, ...)` | Can overflow `buf` if the result is too long — classic C security bug | **Never use** |
| `snprintf(buf, sizeof(buf), fmt, ...)` | Second arg limits how many bytes are written — safe | **Always use** |

> `snprintf` is like JS template literals but with a size limit: `` `SCORE: ${score}` `` but it won't blow up the buffer if `score` is gigantic.

**Why `CELL_SIZE + 11` for the y coordinate?**
`XDrawString` draws text with y as the **baseline** (bottom of the letters), not the top. Row 1 starts at y=14. The text is about 11px tall, so baseline at 14+11=25 puts it nicely centered in the row.

---

## Step 6 — Full `src/main_x11.c` So Far

```c
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <string.h>

#define GRID_WIDTH   60
#define GRID_HEIGHT  20
#define CELL_SIZE    14
#define HEADER_ROWS   3

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

    /* Allocate colors and GC */
    Colormap cmap = DefaultColormap(display, screen);
    XColor wall_col, exact;
    XAllocNamedColor(display, cmap, "gray40", &wall_col, &exact);
    GC gc = XCreateGC(display, window, 0, NULL);

    /* Draw header bars */
    XSetForeground(display, gc, wall_col.pixel);
    XFillRectangle(display, window, gc, 0, 0, win_w, CELL_SIZE);
    XFillRectangle(display, window, gc, 0, 2 * CELL_SIZE, win_w, CELL_SIZE);

    /* Draw score label */
    char score_buf[64];
    snprintf(score_buf, sizeof(score_buf), "SCORE: 0");
    XSetForeground(display, gc, WhitePixel(display, screen));
    XDrawString(display, window, gc,
        8, CELL_SIZE + 11,
        score_buf, (int)strlen(score_buf)
    );

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

---

## Step 7 — Do the Same for Raylib

Update `src/main_raylib.c`:

```c
#include "raylib.h"
#include <stdio.h>

#define GRID_WIDTH   60
#define GRID_HEIGHT  20
#define CELL_SIZE    14
#define HEADER_ROWS   3

int main(void) {
    int win_w = GRID_WIDTH  * CELL_SIZE;
    int win_h = (GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE;

    InitWindow(win_w, win_h, "Snake");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(BLACK);

            /* Header bars */
            Color wall_color = {102, 102, 102, 255};  /* ~gray40 */
            DrawRectangle(0, 0,              win_w, CELL_SIZE, wall_color);
            DrawRectangle(0, 2 * CELL_SIZE,  win_w, CELL_SIZE, wall_color);

            /* Score label */
            DrawText("SCORE: 0", 8, CELL_SIZE + 2, 12, WHITE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

**Raylib drawing functions:**

| Function | Parameters | Purpose |
|----------|-----------|---------|
| `DrawRectangle` | x, y, width, height, color | Filled rectangle |
| `DrawText` | text, x, y, fontSize, color | Text at pixel position |

**Color `{102, 102, 102, 255}`** — R=102, G=102, B=102 is medium gray. The 4th value (255) is alpha = fully opaque. `gray40` in X11 is roughly RGB(102, 102, 102).

Note the y offset for `DrawText`: in Raylib, y is the **top** of the text (not the baseline like X11), so `CELL_SIZE + 2` (= 16) places it just inside row 1.

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

**What to see:** A window with two horizontal gray bars at the top, "SCORE: 0" text between them, and a solid black play area below. Press Q or Escape to quit.

---

## Mental Model

The window is just a blank canvas — a rectangle of pixels. Drawing is just placing colored rectangles and text at specific pixel positions. There's nothing special about the "header" — it's regular rows at the top of the window. The constants `GRID_WIDTH`, `GRID_HEIGHT`, `CELL_SIZE`, and `HEADER_ROWS` are just numbers we use to convert between "grid coordinates" (columns and rows) and "pixel coordinates" (x and y on screen).

> JS analogy: It's like drawing on a `<canvas>` element with `ctx.fillRect(x, y, w, h)` — that's exactly what `XFillRectangle` and `DrawRectangle` are.

---

## Exercise

**Add a "SPEED: 0" label on the right side of the header.**

In X11, after the `SCORE: 0` drawing code, add:

```c
    char speed_buf[64];
    snprintf(speed_buf, sizeof(speed_buf), "SPEED: 0");
    XSetForeground(display, gc, WhitePixel(display, screen));
    XDrawString(display, window, gc,
        win_w - 80,       /* hint: position from the right edge */
        CELL_SIZE + 11,
        speed_buf, (int)strlen(speed_buf)
    );
```

In Raylib, add:
```c
DrawText("SPEED: 0", win_w - 80, CELL_SIZE + 2, 12, WHITE);
```

Build and run. You should see "SCORE: 0" on the left of the header and "SPEED: 0" on the right. Try adjusting the x position (`win_w - 80`, `win_w - 100`, etc.) until it looks right.
