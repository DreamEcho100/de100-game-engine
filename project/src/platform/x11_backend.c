#define _POSIX_C_SOURCE 199309L
#include "x11_backend.h"
#include "../game/game.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>

static uint32_t bgra_format(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return (a << 24) | (b << 16) | (g << 8) | r;
}

int platform_main() {
  int width = 800, height = 600;

  Display *d = XOpenDisplay(NULL);
  if (!d) {
    fprintf(stderr, "Failed to open display\n");
    return 1;
  }

  // Check if MIT-SHM extension is available
  int shm_major, shm_minor;
  Bool pixmaps;
  if (!XShmQueryVersion(d, &shm_major, &shm_minor, &pixmaps)) {
    fprintf(stderr, "MIT-SHM extension not available\n");
    XCloseDisplay(d);
    return 1;
  }

  Window root = DefaultRootWindow(d);
  Window win = XCreateSimpleWindow(d, root, 0, 0, width, height, 0,
                                   BlackPixel(d, 0), WhitePixel(d, 0));

  XSelectInput(d, win, ExposureMask | KeyPressMask);
  XMapWindow(d, win);

  GC gc = XCreateGC(d, win, 0, NULL);

  // Create shared memory segment
  XShmSegmentInfo shminfo;
  Visual *visual = DefaultVisual(d, 0);
  XImage *img = XShmCreateImage(d, visual, DefaultDepth(d, 0), ZPixmap, NULL,
                                &shminfo, width, height);

  if (!img) {
    fprintf(stderr, "Failed to create shared memory image\n");
    XCloseDisplay(d);
    return 1;
  }

  // Allocate shared memory
  shminfo.shmid =
      shmget(IPC_PRIVATE, img->bytes_per_line * img->height, IPC_CREAT | 0777);
  if (shminfo.shmid < 0) {
    fprintf(stderr, "Failed to allocate shared memory\n");
    XDestroyImage(img);
    XCloseDisplay(d);
    return 1;
  }

  shminfo.shmaddr = img->data = (char *)shmat(shminfo.shmid, 0, 0);
  shminfo.readOnly = False;

  // Attach to X server
  if (!XShmAttach(d, &shminfo)) {
    fprintf(stderr, "Failed to attach shared memory to X server\n");
    shmdt(shminfo.shmaddr);
    shmctl(shminfo.shmid, IPC_RMID, 0);
    XDestroyImage(img);
    XCloseDisplay(d);
    return 1;
  }

  // Mark segment for deletion after last detach
  shmctl(shminfo.shmid, IPC_RMID, 0);

  uint32_t *buffer = (uint32_t *)shminfo.shmaddr;
  GameState state = {0};

  struct timespec target_frame_time = {0, 16666667}; // 60 FPS
  struct timespec frame_start, frame_end, sleep_time;

  while (1) {
    clock_gettime(CLOCK_MONOTONIC, &frame_start);

    while (XPending(d)) {
      XEvent e;
      XNextEvent(d, &e);

      if (e.type == KeyPress)
        goto cleanup;
    }

    game_update(&state, buffer, width, height, bgra_format);

    // Fast shared memory put - no data copy!
    XShmPutImage(d, win, gc, img, 0, 0, 0, 0, width, height, False);
    XFlush(d);

    // Frame timing
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

cleanup:
  XShmDetach(d, &shminfo);
  XDestroyImage(img);
  shmdt(shminfo.shmaddr);
  XCloseDisplay(d);
  return 0;
}
