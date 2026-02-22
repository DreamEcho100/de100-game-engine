# Lesson 02 — Draw the Play Field

## By the end of this lesson you will have:

Both programs now draw the Tetris play field on screen. Picture this: a black background with a **gray rectangle border** — vertical gray bars on the left and right, a horizontal gray bar across the bottom. The inside is empty black space. The gray cells are solid 28×28 pixel squares with a 1-pixel dark gap between them, giving a subtle grid feel.

That's what we're building. No pieces yet — just the walls.

---

## What is the play field?

The Tetris play field is a grid of cells, 12 columns wide and 18 rows tall.

```
Col:  0  1  2  3  4  5  6  7  8  9 10 11
      ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐
Row 0 │██│  │  │  │  │  │  │  │  │  │  │██│
Row 1 │██│  │  │  │  │  │  │  │  │  │  │██│
Row 2 │██│  │  │  │  │  │  │  │  │  │  │██│
  ... │  │                              │  │
Row16 │██│  │  │  │  │  │  │  │  │  │  │██│
Row17 │██│██│██│██│██│██│██│██│██│██│██│██│
      └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘
```

- **Col 0** = left wall (gray)
- **Col 11** = right wall (gray)
- **Row 17** = floor (gray, all columns)
- Everything else = empty playing area

The field has **12 columns × 18 rows = 216 cells**. Each cell is `CELL_SIZE` pixels square.

---

## Step 1 — Define constants

At the very top of `src/main_x11.c`, ABOVE the `#include` lines, add:

```c
#define FIELD_WIDTH  12
#define FIELD_HEIGHT 18
#define CELL_SIZE    30
```

**New C concept — `#define`:**  
`#define` is like `const` in JavaScript, but simpler. It's a text substitution — before compiling, the preprocessor replaces every occurrence of `FIELD_WIDTH` in your code with `12`. There's no type, no memory, no variable — just find-and-replace. Convention: all-caps names for defines.

```js
// JavaScript equivalent:
const FIELD_WIDTH  = 12;
const FIELD_HEIGHT = 18;
const CELL_SIZE    = 30;
```

The window size should now match the field. Verify:
- Width: `FIELD_WIDTH * CELL_SIZE` = `12 * 30` = `360` ✓
- Height: `FIELD_HEIGHT * CELL_SIZE` = `18 * 30` = `540` ✓

Your `main_x11.c` top now looks like:

```c
#define FIELD_WIDTH  12
#define FIELD_HEIGHT 18
#define CELL_SIZE    30

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>
```

---

## Step 2 — Understand the coordinate system

Before writing any drawing code, let's be precise about how columns and rows map to pixels.

```
Cell (col=0, row=0):
  pixel x = col * CELL_SIZE = 0 * 30 = 0
  pixel y = row * CELL_SIZE = 0 * 30 = 0

Cell (col=3, row=2):
  pixel x = 3 * 30 = 90
  pixel y = 2 * 30 = 60

Cell (col=11, row=17):  ← bottom-right corner
  pixel x = 11 * 30 = 330
  pixel y = 17 * 30 = 510
```

The formula is always: `x = col * CELL_SIZE`, `y = row * CELL_SIZE`. Simple multiplication.

---

## Step 3 — The `alloc_color` helper (X11 only)

X11 colors aren't just hex codes — you have to ask the X server to allocate a color by name, and it gives you back an ID. Let's write a helper function for this.

Add this function ABOVE `main()` in `main_x11.c`:

```c
static unsigned long alloc_color(Display *display, int screen, const char *name) {
    XColor color, exact;
    Colormap cmap = DefaultColormap(display, screen);
    XAllocNamedColor(display, cmap, name, &color, &exact);
    return color.pixel;
}
```

**What's happening here:**
- `XColor color, exact` — X11 returns two versions of the color: the exact requested color and the closest the hardware can actually display. We use `color.pixel` (the hardware version).
- `DefaultColormap` — gets the default color table for the screen. Think of a colormap as a palette.
- `XAllocNamedColor` — looks up a color by name ("gray50", "cyan", "red") and fills in the `XColor` struct.
- The function returns `color.pixel` — an integer ID that you pass to drawing functions.

**New C concept — `static`:**  
`static` on a function means "this function is only visible in this file". Like a private function in a JS module. It's good practice for helper functions.

**New C concept — `const char *name`:**  
A string in C is a pointer to a sequence of characters ending with a null byte (`\0`). `const char *` means "a pointer to characters that I won't modify". In JavaScript this is just `string`.

---

## Step 4 — Declare colors and a Graphics Context

At the start of `main()`, after `int screen = DefaultScreen(display);`, add:

```c
    unsigned long gray   = alloc_color(display, screen, "gray50");
    unsigned long black  = BlackPixel(display, screen);

    GC gc = XCreateGC(display, window, 0, NULL);
```

**What is a `GC`?**  
GC = Graphics Context. It's a bundle of drawing settings — current color, line width, fill style, etc. Every X11 drawing call needs one. Think of it like a canvas `ctx` in browser JavaScript: you set properties on it, then call draw functions. `XCreateGC` creates a fresh default one.

Your `main()` top now looks like:

```c
int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    int screen = DefaultScreen(display);

    Window window = XCreateSimpleWindow(
        display,
        RootWindow(display, screen),
        100, 100,
        FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE,
        1,
        BlackPixel(display, screen),
        BlackPixel(display, screen)
    );

    unsigned long gray  = alloc_color(display, screen, "gray50");
    unsigned long black = BlackPixel(display, screen);
    GC gc = XCreateGC(display, window, 0, NULL);

    XSelectInput(display, window, KeyPressMask);
    XStoreName(display, window, "Tetris");
    XMapWindow(display, window);
    XFlush(display);
```

Note we also updated `XCreateSimpleWindow` to use `FIELD_WIDTH * CELL_SIZE` and `FIELD_HEIGHT * CELL_SIZE` instead of hardcoded numbers.

---

## Step 5 — Write `draw_cell()`

Add this function ABOVE `main()` (but below `alloc_color`):

```c
static void draw_cell(Display *display, Window window, GC gc,
                      int col, int row, unsigned long color) {
    int x = col * CELL_SIZE + 1;
    int y = row * CELL_SIZE + 1;
    int w = CELL_SIZE - 2;
    int h = CELL_SIZE - 2;

    XSetForeground(display, gc, color);
    XFillRectangle(display, window, gc, x, y, w, h);
}
```

**The `+1` / `-2` trick:**  
Without the offset, cells would touch each other with no gap. By starting 1 pixel in (`+1`) and making the size 2 pixels smaller (`-2`), we leave a 1-pixel black gap on all sides. This creates a subtle grid line effect.

```
Without gap:          With gap:
┌────────────────┐    ┌────────────────┐
│cell │cell │cell│    │ cell  cell  cell│
│     │     │    │    │ [1px gap between]
└────────────────┘    └────────────────┘
```

**`XSetForeground`:** Sets the drawing color on the GC. Like `ctx.fillStyle = color` in canvas.

**`XFillRectangle(display, window, gc, x, y, width, height)`:** Draws a filled rectangle. The `window` here is the drawable target.

---

## Step 6 — Write `draw_field_boundary()`

Add this function ABOVE `main()`:

```c
static void draw_field_boundary(Display *display, Window window, GC gc,
                                unsigned long wall_color) {
    int col, row;

    /* Left wall: column 0, all rows */
    for (row = 0; row < FIELD_HEIGHT; row++) {
        draw_cell(display, window, gc, 0, row, wall_color);
    }

    /* Right wall: column 11, all rows */
    for (row = 0; row < FIELD_HEIGHT; row++) {
        draw_cell(display, window, gc, FIELD_WIDTH - 1, row, wall_color);
    }

    /* Floor: row 17, all columns */
    for (col = 0; col < FIELD_WIDTH; col++) {
        draw_cell(display, window, gc, col, FIELD_HEIGHT - 1, wall_color);
    }
}
```

**New C concept — `for` loops:**  
```c
for (row = 0; row < FIELD_HEIGHT; row++)
```
This is identical to JavaScript's `for` loop:
```js
for (let row = 0; row < FIELD_HEIGHT; row++)
```
Three parts: initialize (`row = 0`), condition (`row < FIELD_HEIGHT`), increment (`row++`).

Note: In C, you should declare loop variables before the loop body (in older C standards). We declared `int col, row;` at the top of the function. This is standard C89/C90 style.

---

## Step 7 — Call it from the event loop

Update the event loop to clear the background and draw the field on every iteration:

```c
    XEvent event;
    while (1) {
        /* --- Draw frame --- */
        XSetForeground(display, gc, black);
        XFillRectangle(display, window, gc, 0, 0,
                       FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE);

        draw_field_boundary(display, window, gc, gray);

        XFlush(display);

        /* --- Handle events --- */
        while (XPending(display)) {
            XNextEvent(display, &event);

            if (event.type == KeyPress) {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_q || key == XK_Q || key == XK_Escape) {
                    goto cleanup;
                }
            }
        }
    }

    cleanup:
```

**Key changes:**
1. We clear the whole window to black first (paint over everything)
2. We draw the field boundary
3. We call `XFlush` to push the frame to the screen
4. We use `XPending` to check if there are events waiting — this way the loop keeps drawing even if no events come in. Without `XPending`, `XNextEvent` would block until a key is pressed, freezing the drawing.

**New C concept — `goto`:**  
`goto label` jumps to a point in code marked with `label:`. It's generally avoided in modern code, but it's idiomatic in C for jumping out of nested loops to a cleanup section. Think of it like a `break` that can reach past multiple loop levels.

Add the cleanup label and code after the loop:

```c
    cleanup:
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
```

---

## Complete `src/main_x11.c`

```c
#define FIELD_WIDTH  12
#define FIELD_HEIGHT 18
#define CELL_SIZE    30

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>

static unsigned long alloc_color(Display *display, int screen, const char *name) {
    XColor color, exact;
    Colormap cmap = DefaultColormap(display, screen);
    XAllocNamedColor(display, cmap, name, &color, &exact);
    return color.pixel;
}

static void draw_cell(Display *display, Window window, GC gc,
                      int col, int row, unsigned long color) {
    int x = col * CELL_SIZE + 1;
    int y = row * CELL_SIZE + 1;
    int w = CELL_SIZE - 2;
    int h = CELL_SIZE - 2;
    XSetForeground(display, gc, color);
    XFillRectangle(display, window, gc, x, y, w, h);
}

static void draw_field_boundary(Display *display, Window window, GC gc,
                                unsigned long wall_color) {
    int col, row;

    for (row = 0; row < FIELD_HEIGHT; row++)
        draw_cell(display, window, gc, 0, row, wall_color);

    for (row = 0; row < FIELD_HEIGHT; row++)
        draw_cell(display, window, gc, FIELD_WIDTH - 1, row, wall_color);

    for (col = 0; col < FIELD_WIDTH; col++)
        draw_cell(display, window, gc, col, FIELD_HEIGHT - 1, wall_color);
}

int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    int screen = DefaultScreen(display);

    Window window = XCreateSimpleWindow(
        display,
        RootWindow(display, screen),
        100, 100,
        FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE,
        1,
        BlackPixel(display, screen),
        BlackPixel(display, screen)
    );

    unsigned long gray  = alloc_color(display, screen, "gray50");
    unsigned long black = BlackPixel(display, screen);
    GC gc = XCreateGC(display, window, 0, NULL);

    XSelectInput(display, window, KeyPressMask);
    XStoreName(display, window, "Tetris");
    XMapWindow(display, window);
    XFlush(display);

    XEvent event;
    while (1) {
        XSetForeground(display, gc, black);
        XFillRectangle(display, window, gc, 0, 0,
                       FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE);

        draw_field_boundary(display, window, gc, gray);

        XFlush(display);

        while (XPending(display)) {
            XNextEvent(display, &event);
            if (event.type == KeyPress) {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_q || key == XK_Q || key == XK_Escape) {
                    goto cleanup;
                }
            }
        }
    }

    cleanup:
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
```

### Build and Run (X11)

```bash
./build_x11.sh
./tetris_x11
```

**What you should see:** A black window with gray cells forming a U-shape: left column, right column, and bottom row all filled gray. The inside is black. Each cell has a thin dark gap around it giving a tiled look.

---

## Part B — Raylib version

Open `src/main_raylib.c`. The Raylib changes are parallel — same logic, different drawing API.

### Step 1 — Add constants

```c
#define FIELD_WIDTH  12
#define FIELD_HEIGHT 18
#define CELL_SIZE    30

#include "raylib.h"
```

### Step 2 — Write `draw_cell()`

```c
static void draw_cell(int col, int row, Color color) {
    int x = col * CELL_SIZE + 1;
    int y = row * CELL_SIZE + 1;
    int w = CELL_SIZE - 2;
    int h = CELL_SIZE - 2;
    DrawRectangle(x, y, w, h, color);
}
```

Raylib's `DrawRectangle(x, y, width, height, color)` draws a filled rectangle. The `Color` type is a struct with `r, g, b, a` fields. Raylib defines constants like `GRAY`, `BLACK`, `CYAN`, etc.

### Step 3 — Write `draw_field_boundary()`

```c
static void draw_field_boundary(Color wall_color) {
    int col, row;

    for (row = 0; row < FIELD_HEIGHT; row++)
        draw_cell(0, row, wall_color);

    for (row = 0; row < FIELD_HEIGHT; row++)
        draw_cell(FIELD_WIDTH - 1, row, wall_color);

    for (col = 0; col < FIELD_WIDTH; col++)
        draw_cell(col, FIELD_HEIGHT - 1, wall_color);
}
```

### Step 4 — Update `main()`

```c
int main(void) {
    InitWindow(FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE, "Tetris");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(BLACK);
            draw_field_boundary(GRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

### Complete `src/main_raylib.c`

```c
#define FIELD_WIDTH  12
#define FIELD_HEIGHT 18
#define CELL_SIZE    30

#include "raylib.h"

static void draw_cell(int col, int row, Color color) {
    int x = col * CELL_SIZE + 1;
    int y = row * CELL_SIZE + 1;
    int w = CELL_SIZE - 2;
    int h = CELL_SIZE - 2;
    DrawRectangle(x, y, w, h, color);
}

static void draw_field_boundary(Color wall_color) {
    int col, row;

    for (row = 0; row < FIELD_HEIGHT; row++)
        draw_cell(0, row, wall_color);

    for (row = 0; row < FIELD_HEIGHT; row++)
        draw_cell(FIELD_WIDTH - 1, row, wall_color);

    for (col = 0; col < FIELD_WIDTH; col++)
        draw_cell(col, FIELD_HEIGHT - 1, wall_color);
}

int main(void) {
    InitWindow(FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE, "Tetris");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(BLACK);
            draw_field_boundary(GRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

### Build and Run (Raylib)

```bash
./build_raylib.sh
./tetris_raylib
```

**What you should see:** Same result as the X11 version — black background, gray U-shaped border.

---

## Common Confusion: Why doesn't the window redraw itself?

If you're used to the browser, you might expect the window to just... update automatically.

It doesn't.

In X11 (and in most game loops), YOU are responsible for redrawing the entire screen on every frame. Nothing is "retained" from the previous frame. Each time through the loop, you paint a black rectangle over everything, then draw the walls on top. If you didn't clear to black first, old frames would pile up.

This is called **immediate mode rendering** (as opposed to the browser's DOM, which is **retained mode** — the browser remembers what you drew and redraws it for you).

The Raylib version is the same — `ClearBackground(BLACK)` wipes the slate clean at the start of every frame.

---

## Mental Model

A cell is just a rectangle at position `(col * CELL_SIZE, row * CELL_SIZE)`. That's it. The whole game field — 216 cells — is just 216 small rectangles drawn in the right places.

JavaScript equivalent of the draw logic:

```js
function drawCell(col, row, color) {
    const x = col * CELL_SIZE + 1;
    const y = row * CELL_SIZE + 1;
    ctx.fillStyle = color;
    ctx.fillRect(x, y, CELL_SIZE - 2, CELL_SIZE - 2);
}
```

The C code is doing exactly this. The only difference is the API name (`XFillRectangle` vs `ctx.fillRect`) and that in C you pass the drawing context explicitly.

---

## Exercise

Change `CELL_SIZE` from `30` to `20`:

```c
#define CELL_SIZE    20
```

Rebuild both versions. The window should shrink to 240×360 pixels and the cells should appear smaller but the U-shape border should still be correct.

Then change it back to `30`.

This confirms that everything is driven by the constant — no hardcoded pixel positions anywhere in the drawing code.
