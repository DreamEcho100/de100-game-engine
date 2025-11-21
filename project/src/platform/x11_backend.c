#define _POSIX_C_SOURCE 199309L
#include "x11_backend.h"
#include "../game/game.h"
#include <X11/Xlib.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint32_t bgra_format(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  // X11 typically uses BGR byte order in memory (BGRA with alpha at high bits)
  return (a << 24) | (b << 16) | (g << 8) | r;
}

int platform_main() {
  int width = 800, height = 600;

  Display *d = XOpenDisplay(NULL);
  Window root = DefaultRootWindow(d);

  Window win = XCreateSimpleWindow(d, root, 0, 0, width, height, 0,
                                   BlackPixel(d, 0), WhitePixel(d, 0));

  XSelectInput(d, win, ExposureMask | KeyPressMask);
  XMapWindow(d, win);

  GC gc = XCreateGC(d, win, 0, NULL);

  uint32_t *buffer = malloc(width * height * sizeof(uint32_t));
  Visual *visual = DefaultVisual(d, 0);
  XImage *img = XCreateImage(d, visual, DefaultDepth(d, 0), ZPixmap, 0,
                             (char *)buffer, width, height, 32, 0);

  // Set byte order for RGBA format
  img->byte_order = LSBFirst;

  GameState state = {0};

  struct timespec target_frame_time = {0, 16666667}; // 60 FPS (16.666667ms)
  struct timespec frame_start, frame_end, sleep_time;

  while (1) {
    clock_gettime(CLOCK_MONOTONIC, &frame_start);

    while (XPending(d)) {
      XEvent e;
      XNextEvent(d, &e);

      if (e.type == KeyPress)
        return 0;
    }

    game_update(&state, buffer, width, height, bgra_format);

    XPutImage(d, win, gc, img, 0, 0, 0, 0, width, height);
    XFlush(d); // Ensure the image is sent to the server

    // Precise frame timing
    clock_gettime(CLOCK_MONOTONIC, &frame_end);
    long elapsed_ns = (frame_end.tv_sec - frame_start.tv_sec) * 1000000000L +
                      (frame_end.tv_nsec - frame_start.tv_nsec);
    long sleep_ns = target_frame_time.tv_nsec - elapsed_ns;

    if (sleep_ns > 0) {
      sleep_time.tv_sec = 0;
      sleep_time.tv_nsec = sleep_ns;
      nanosleep(&sleep_time, NULL);
    }
  }

  return 0;
}
