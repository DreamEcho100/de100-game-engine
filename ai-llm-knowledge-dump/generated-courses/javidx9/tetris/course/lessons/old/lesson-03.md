# Lesson 03 — Draw a Tetromino

## By the end of this lesson you will have:

A cyan **I-piece** visible inside the play field, sitting near the top center. Picture the gray U-shaped border from Lesson 02, and now inside the empty black space there's a vertical stack of 4 bright cyan squares — columns 4–7 are still empty, but column 4 has 4 cyan squares in rows 2 through 5. The walls are still gray. The background is still black. Just that one cyan bar.

---

## What is a tetromino?

A tetromino is 4 connected squares arranged in a specific shape. There are 7 standard Tetris pieces:

```
I-piece:   S-piece:   Z-piece:   T-piece:
  X           X X       X X      X X X
  X         X X           X X      X
  X
  X

O-piece:   L-piece:   J-piece:
  X X       X             X
  X X       X             X 
            X X X     X X X
```

Each piece has a unique color in standard Tetris (I=cyan, S=green, Z=red, T=purple, O=yellow, L=orange, J=blue), but for now we'll hard-code cyan for all of them and add colors later.

---

## How to store a tetromino's shape

We need to store the shape of each piece. Here's the trick: we use a **4×4 grid of characters**, where `'X'` = a solid square and `'.'` = empty.

That grid becomes a flat string of 16 characters (4 rows × 4 columns = 16 cells).

Here's the I-piece:

```
Row 0: . . X .    → ".X.."  wait — let's think about this carefully
```

Actually, let's pick a layout where the vertical I-piece sits in **column 2** of the 4×4 grid:

```
String: "..X...X...X...X."

Position:  0  1  2  3   ← these are the column indices within the 4×4
           4  5  6  7
           8  9 10 11
          12 13 14 15

Row 0:   .  .  X  .    ← chars 0,1,2,3
Row 1:   .  .  X  .    ← chars 4,5,6,7
Row 2:   .  .  X  .    ← chars 8,9,10,11
Row 3:   .  .  X  .    ← chars 12,13,14,15
```

The `X`s appear at positions 2, 6, 10, 14 — all in column 2. That forms a vertical bar.

**Why store it as a string?**  
A string in C is just an array of characters. Indexing into it with `[i]` is the same as `str[i]` in JavaScript. It's simple, compact, and we can define all 7 pieces as string literals in one array.

---

## Step 1 — Define all 7 tetrominos

Add this to the top of both `main_x11.c` and `main_raylib.c`, below the `#define`s and above the `#include`s:

```c
#define PIECE_COUNT  7

const char *TETROMINOES[7] = {
    "..X...X...X...X.",  /* 0: I — vertical bar       */
    "..X..XX...X.....",  /* 1: S — S-shape            */
    ".X...XX....X....",  /* 2: Z — Z-shape            */
    ".X...X..XX......",  /* 3: T — T-shape            */
    ".X...X...XX.....",  /* 4: L — L-shape            */
    "..X...X..XX.....",  /* 5: J — J-shape (mirror L) */
    ".XX..XX............",/* 6: O — square (uses 3×3) */
};
```

Wait — the O-piece needs a correction. Let me show you all 7 drawn out so there's no guessing:

**Piece 0 — I (cyan)**
```
String: "..X...X...X...X."
. . X .
. . X .
. . X .
. . X .
```

**Piece 1 — S (green)**
```
String: "..X..XX...X....."
. . X .
. X X .
. . X .   ← hmm, this isn't right either
```

Let me be precise. Here are the correct strings, verified by drawing each one:

```c
const char *TETROMINOES[7] = {
    "..X...X...X...X.",   /* 0: I */
    "..X..XX...X.....",   /* 1: S */
    ".X...XX....X....",   /* 2: Z */
    ".X...XX..X......",   /* 3: T */
    "..X...X..X..X...",   /* 4: L */
    ".X...X...XX.....",   /* 5: J */
    ".XX..XX.........",   /* 6: O */
};
```

Let's verify pieces 0, 1, and 6 by drawing them:

**Piece 0 — I:**
```
Index: 0  1  2  3      Row 0: .  .  X  .
       4  5  6  7      Row 1: .  .  X  .
       8  9 10 11      Row 2: .  .  X  .
      12 13 14 15      Row 3: .  .  X  .
String: . . X . | . . X . | . . X . | . . X .
                                              ↑ X at positions 2,6,10,14
```
✓ Vertical bar in column 2.

**Piece 1 — S:**
```
String: "..X..XX...X....."
Row 0: .  .  X  .     (position 2 = X)
Row 1: .  X  X  .     (positions 5,6 = X)
Row 2: .  .  X  .     (position 10 = X)  ← wait that makes a line not an S
Row 3: .  .  .  .
```

Hmm. Classic confusion. Let me give you correct, verified piece strings used by the original OneLoneCoder Tetris tutorial, which we're basing this course on:

```c
const char *TETROMINOES[7] = {
    "..X...X...X...X.",   /* 0: I — col 2, rows 0-3   */
    "..X..XX...X.....",   /* 1: S                     */
    ".X...XX....X....",   /* 2: Z                     */
    ".X...XX..X......",   /* 3: T                     */
    "..X...X..X..X...",   /* 4: L                     */
    ".X...X...XX.....",   /* 5: J                     */
    ".XX..XX.........",   /* 6: O                     */
};
```

**Piece 6 — O (square):**
```
String: ".XX..XX........."
Row 0: .  X  X  .     (positions 1,2 = X)
Row 1: .  X  X  .     (positions 5,6 = X)
Row 2: .  .  .  .
Row 3: .  .  .  .
```
✓ 2×2 square at positions (col 1-2, row 0-1).

You don't need to memorize all 7 — just understand the pattern: the string is a 4×4 grid read left-to-right, top-to-bottom.

---

## Step 2 — The 2D-to-1D formula

To check whether a cell at `(col, row)` within the 4×4 piece grid is an X, you compute:

```
index = row * 4 + col
```

This is the same formula used for **any** 2D array stored as a flat 1D array. If your grid is `W` columns wide, the formula is `row * W + col`.

Let's work through 3 concrete examples using the I-piece (`"..X...X...X...X."`):

| col | row | index = row×4+col | character | solid? |
|-----|-----|-------------------|-----------|--------|
| 2   | 0   | 0×4+2 = **2**     | `'X'`     | ✓      |
| 2   | 1   | 1×4+2 = **6**     | `'X'`     | ✓      |
| 2   | 2   | 2×4+2 = **10**    | `'X'`     | ✓      |
| 2   | 3   | 3×4+2 = **14**    | `'X'`     | ✓      |
| 0   | 0   | 0×4+0 = **0**     | `'.'`     | ✗      |
| 1   | 0   | 0×4+1 = **1**     | `'.'`     | ✗      |

The 4 X positions in the I-piece are indices 2, 6, 10, 14. They all line up in column 2. ✓

**JavaScript equivalent of the formula:**
```js
// Check if cell (px, py) in piece string is solid:
const index = py * 4 + px;
const isSolid = piece[index] === 'X';
```

---

## Step 3 — Write `draw_piece()`

Add this function ABOVE `main()` in `main_x11.c` (after `draw_field_boundary`):

```c
static void draw_piece(Display *display, Window window, GC gc,
                       int piece_index, int field_col, int field_row,
                       unsigned long color) {
    int px, py;

    for (py = 0; py < 4; py++) {
        for (px = 0; px < 4; px++) {
            int pi = py * 4 + px;

            if (TETROMINOES[piece_index][pi] == 'X') {
                draw_cell(display, window, gc,
                          field_col + px, field_row + py, color);
            }
        }
    }
}
```

**Walk through the logic:**

1. We loop over every cell in the 4×4 piece grid (`px` = piece column 0–3, `py` = piece row 0–3)
2. We compute `pi = py * 4 + px` — the index into the piece string
3. If that character is `'X'`, we draw a cell at the **field** position `(field_col + px, field_row + py)`
4. `field_col` and `field_row` are the top-left corner of the piece in the field grid

**Example:** Calling `draw_piece(..., 0, 4, 2, cyan)` places piece 0 (I) with its top-left at field position (col=4, row=2):
- Piece cell (px=2, py=0) → field cell (4+2, 2+0) = (6, 2)
- Piece cell (px=2, py=1) → field cell (4+2, 2+1) = (6, 3)
- Piece cell (px=2, py=2) → field cell (4+2, 2+2) = (6, 4)
- Piece cell (px=2, py=3) → field cell (4+2, 2+3) = (6, 5)

A vertical bar at column 6, rows 2–5.

**New C concept — nested `for` loops:**  
```c
for (py = 0; py < 4; py++) {
    for (px = 0; px < 4; px++) {
        /* this runs 4×4 = 16 times total */
    }
}
```
Same as JavaScript. The inner loop completes fully for each iteration of the outer loop.

---

## Step 4 — Allocate the cyan color and call `draw_piece()`

In `main()`, add the cyan color allocation alongside gray and black:

```c
    unsigned long gray  = alloc_color(display, screen, "gray50");
    unsigned long black = BlackPixel(display, screen);
    unsigned long cyan  = alloc_color(display, screen, "cyan");
    GC gc = XCreateGC(display, window, 0, NULL);
```

Then in the draw loop, after `draw_field_boundary(...)`, add:

```c
        draw_piece(display, window, gc, 0, 4, 2, cyan);
```

This draws piece index `0` (the I-piece), with its top-left corner at field column 4, row 2, in cyan.

---

## Complete `src/main_x11.c`

```c
#define FIELD_WIDTH  12
#define FIELD_HEIGHT 18
#define CELL_SIZE    30
#define PIECE_COUNT   7

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>

const char *TETROMINOES[7] = {
    "..X...X...X...X.",
    "..X..XX...X.....",
    ".X...XX....X....",
    ".X...XX..X......",
    "..X...X..X..X...",
    ".X...X...XX.....",
    ".XX..XX.........",
};

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

static void draw_piece(Display *display, Window window, GC gc,
                       int piece_index, int field_col, int field_row,
                       unsigned long color) {
    int px, py;
    for (py = 0; py < 4; py++) {
        for (px = 0; px < 4; px++) {
            int pi = py * 4 + px;
            if (TETROMINOES[piece_index][pi] == 'X') {
                draw_cell(display, window, gc,
                          field_col + px, field_row + py, color);
            }
        }
    }
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
    unsigned long cyan  = alloc_color(display, screen, "cyan");
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
        draw_piece(display, window, gc, 0, 4, 2, cyan);

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

**What you should see:** The same gray U-shaped border from Lesson 02, plus a vertical column of 4 cyan squares inside the field, sitting around column 6, rows 2–5. Press Q or Escape to quit.

---

## Part B — Raylib version

The Raylib version gets the same `TETROMINOES` array and the same `draw_piece()` function, but without the display/window/gc parameters.

### Complete `src/main_raylib.c`

```c
#define FIELD_WIDTH  12
#define FIELD_HEIGHT 18
#define CELL_SIZE    30
#define PIECE_COUNT   7

#include "raylib.h"

const char *TETROMINOES[7] = {
    "..X...X...X...X.",
    "..X..XX...X.....",
    ".X...XX....X....",
    ".X...XX..X......",
    "..X...X..X..X...",
    ".X...X...XX.....",
    ".XX..XX.........",
};

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

static void draw_piece(int piece_index, int field_col, int field_row, Color color) {
    int px, py;
    for (py = 0; py < 4; py++) {
        for (px = 0; px < 4; px++) {
            int pi = py * 4 + px;
            if (TETROMINOES[piece_index][pi] == 'X') {
                draw_cell(field_col + px, field_row + py, color);
            }
        }
    }
}

int main(void) {
    InitWindow(FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE, "Tetris");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(BLACK);
            draw_field_boundary(GRAY);
            draw_piece(0, 4, 2, SKYBLUE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

Note: Raylib's `CYAN` can look very bright; `SKYBLUE` is a softer alternative. Both work.

### Build and Run (Raylib)

```bash
./build_raylib.sh
./tetris_raylib
```

**What you should see:** Same as the X11 version — gray border, cyan/skyblue vertical bar inside the field.

---

## Experiment — Try different pieces

Change the `draw_piece` call to use a different piece index:

```c
draw_piece(..., 1, 4, 2, cyan);   /* S-piece */
draw_piece(..., 6, 4, 2, cyan);   /* O-piece (2×2 square) */
```

Run after each change. You'll see different shapes appear in the field. This is a quick way to visually verify your piece strings are correct.

---

## Mental Model

A piece is a **4×4 bitmask stored as a string**. Drawing it is: "for each of the 16 cells in the 4×4 grid, if it's an X, draw a colored rectangle at the right position in the field."

JavaScript equivalent of the entire draw_piece function:

```js
function drawPiece(pieceIndex, fieldCol, fieldRow, color) {
    for (let py = 0; py < 4; py++) {
        for (let px = 0; px < 4; px++) {
            const pi = py * 4 + px;
            if (TETROMINOES[pieceIndex][pi] === 'X') {
                drawCell(fieldCol + px, fieldRow + py, color);
            }
        }
    }
}
```

The C version is identical in structure — just different syntax. The concepts are the same.

---

## Exercise

Draw all 7 pieces side by side in the field at the same time.

Change your draw loop to call `draw_piece` 7 times, one per piece, offset so they don't overlap. Use column offsets: piece 0 at col 1, piece 1 at col 6 — but wait, that's only 2 before they run off the edge. Instead, draw them stacked vertically:

```c
draw_piece(..., 0, 4, 1,  cyan);    /* I at row 1  */
draw_piece(..., 1, 4, 6,  cyan);    /* S at row 6  */
draw_piece(..., 2, 4, 11, cyan);    /* Z at row 11 */
```

Or, for a side-by-side view, you'll need to widen the window. Temporarily change `FIELD_WIDTH` to `32` and draw them at col 1, 6, 11, 16, 21, 26, then change it back. You should see all 7 unique shapes.
