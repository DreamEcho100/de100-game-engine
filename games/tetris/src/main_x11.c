#include "base.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned long alloc_color(Display *display, int screen,
                                 const char *name) {
  XColor color, exact;
  Colormap cmap = DefaultColormap(display, screen);
  XAllocNamedColor(display, cmap, name, &color, &exact);
  return color.pixel;
}

static void draw_cell(Display *display, Window window, GC gc, int col, int row,
                      unsigned long color) {
  int x = col * CELL_SIZE + 1;
  int y = row * CELL_SIZE + 1;
  int w = CELL_SIZE - 1;
  int h = CELL_SIZE - 1;

  XSetForeground(display, gc, color);
  XFillRectangle(display, window, gc, x, y, w, h);
}

static void draw_field_boundary(Display *display, Window window, GC gc,
                                unsigned long wall_color) {
  int col, row;

  /* Left wall: column 0, all rows */
  for (row = 0; row < FIELD_HEIGHT; ++row) {
    draw_cell(display, window, gc, 0, row, wall_color);
  }

  /* Right wall: column 11, all rows */
  for (row = 0; row < FIELD_HEIGHT; ++row) {
    draw_cell(display, window, gc, FIELD_WIDTH - 1, row, wall_color);
  }

  /* Floor: row 17, all columns */
  for (col = 0; col < FIELD_WIDTH; ++col) {
    draw_cell(display, window, gc, col, FIELD_HEIGHT - 1, wall_color);
  }
}

static void draw_piece(Display *display, Window window, GC gc, int piece_index,
                       int field_col, int field_row, unsigned long color) {
  int px, py;

  for (py = 0; py < TETROMINO_LAYER_COUNT; py++) {
    for (px = 0; px < TETROMINO_LAYER_COUNT; px++) {
      int pi = py * TETROMINO_LAYER_COUNT + px;

      if (TETROMINOES[piece_index][pi] == TETROMINO_BLOCK) {
        draw_cell(display, window, gc, field_col + px, field_row + py, color);
      }
    }
  }
}

int main(void) {
  Display *display = XOpenDisplay(NULL);
  if (!display) {
    fprintf(stderr, "Failed to open display\n");
    return 1;
  }

  int screen = DefaultScreen(display);

  // Allocate dark blue color
  unsigned long dark_blue_clr = alloc_color(display, screen, "darkblue");

  Window window = XCreateSimpleWindow(
      display,                     // display
      RootWindow(display, screen), // parent window
      100,
      100,                                               // x, y
      FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE, // w, h
      1,                                                 // border width
      BlackPixel(display, screen),                       // border color
      BlackPixel(display, screen) // background color (dark blue)
  );

  unsigned long gray = alloc_color(display, screen, "gray50");
  unsigned long black = BlackPixel(display, screen);
  unsigned long cyan = alloc_color(display, screen, "cyan");

  GC gc = XCreateGC(display, window, 0, NULL);

  XStoreName(display, window, "Tetris");
  XSelectInput(display, window, ExposureMask | KeyPressMask);
  XMapWindow(display, window); // Show the window
  XFlush(display); // Force all the buffered commands to be sent to the X server

  XEvent event; // Events are sent to the application as XEvents, we get them
                // using XNextEvent on the game loop.

  bool is_game_running = true;

  printf("Game is running! Press Escape to exit.\n");

  while (is_game_running) {
    XNextEvent(display, &event);
    XSetForeground(display, gc, black);
    XFillRectangle(display, window, gc, 0, 0, FIELD_WIDTH * CELL_SIZE,
                   FIELD_HEIGHT * CELL_SIZE);

    switch (event.type) {
    case KeyPress: {
      KeySym key = XLookupKeysym(
          &event.xkey, 0); // `0` is the index for the first keysym in the list
                           // (for non-modified keys), and `1` would be for the
                           // second keysym (for Shift-modified keys), etc.
      switch (key) {
      case XK_q:
      case XK_Q:
      case XK_Escape: {
        is_game_running = false;
        break;
      }
      }
    }
    }

    draw_field_boundary(display, window, gc, gray);
    // TETROMINO_I_IDX,
    draw_piece(display, window, gc, TETROMINO_I_IDX, 0, 0, cyan);
    // TETROMINO_J_IDX,
    draw_piece(display, window, gc, TETROMINO_J_IDX, 1, 2, cyan);
    // TETROMINO_L_IDX,
    draw_piece(display, window, gc, TETROMINO_L_IDX, 3, 0, cyan);
    // TETROMINO_O_IDX,
    draw_piece(display, window, gc, TETROMINO_O_IDX, 3, 2, cyan);
    // TETROMINO_S_IDX,
    draw_piece(display, window, gc, TETROMINO_S_IDX, 4, 0, cyan);
    // TETROMINO_T_IDX,
    draw_piece(display, window, gc, TETROMINO_T_IDX, 5, 2, cyan);
    // TETROMINO_Z_IDX,
    draw_piece(display, window, gc, TETROMINO_Z_IDX, 7, 0, cyan);

    XFlush(display);
  }

  XFreeGC(display, gc);
  XDestroyWindow(display, window);
  XCloseDisplay(display);
  return 0;
}