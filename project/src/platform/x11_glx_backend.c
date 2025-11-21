#define _POSIX_C_SOURCE 199309L
#include "x11_glx_backend.h"
#include "../game/game.h"
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint32_t rgba_format(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return (a << 24) | (r << 16) | (g << 8) | b;
}

int platform_main() {
  int width = 800, height = 600;

  Display *d = XOpenDisplay(NULL);
  if (!d) {
    fprintf(stderr, "Failed to open display\n");
    return 1;
  }

  // Check for GLX extension
  int glx_major, glx_minor;
  if (!glXQueryVersion(d, &glx_major, &glx_minor)) {
    fprintf(stderr, "GLX extension not available\n");
    XCloseDisplay(d);
    return 1;
  }

  // GLX framebuffer config attributes
  static int visual_attribs[] = {GLX_X_RENDERABLE,
                                 True,
                                 GLX_DRAWABLE_TYPE,
                                 GLX_WINDOW_BIT,
                                 GLX_RENDER_TYPE,
                                 GLX_RGBA_BIT,
                                 GLX_X_VISUAL_TYPE,
                                 GLX_TRUE_COLOR,
                                 GLX_RED_SIZE,
                                 8,
                                 GLX_GREEN_SIZE,
                                 8,
                                 GLX_BLUE_SIZE,
                                 8,
                                 GLX_ALPHA_SIZE,
                                 8,
                                 GLX_DEPTH_SIZE,
                                 24,
                                 GLX_DOUBLEBUFFER,
                                 True,
                                 None};

  int fbcount;
  GLXFBConfig *fbc =
      glXChooseFBConfig(d, DefaultScreen(d), visual_attribs, &fbcount);
  if (!fbc) {
    fprintf(stderr, "Failed to retrieve framebuffer config\n");
    XCloseDisplay(d);
    return 1;
  }

  GLXFBConfig bestFbc = fbc[0];
  XFree(fbc);

  XVisualInfo *vi = glXGetVisualFromFBConfig(d, bestFbc);

  XSetWindowAttributes swa;
  swa.colormap =
      XCreateColormap(d, RootWindow(d, vi->screen), vi->visual, AllocNone);
  swa.background_pixmap = None;
  swa.border_pixel = 0;
  swa.event_mask = KeyPressMask | ExposureMask;

  Window win = XCreateWindow(d, RootWindow(d, vi->screen), 0, 0, width, height,
                             0, vi->depth, InputOutput, vi->visual,
                             CWBorderPixel | CWColormap | CWEventMask, &swa);

  XMapWindow(d, win);
  XStoreName(d, win, "X11 GLX Backend");

  // Create OpenGL context
  GLXContext ctx = glXCreateNewContext(d, bestFbc, GLX_RGBA_TYPE, NULL, True);
  if (!ctx) {
    fprintf(stderr, "Failed to create OpenGL context\n");
    XDestroyWindow(d, win);
    XCloseDisplay(d);
    return 1;
  }

  glXMakeCurrent(d, win, ctx);

  // Setup OpenGL
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Create texture for pixel buffer
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  glEnable(GL_TEXTURE_2D);

  uint32_t *pixels = malloc(width * height * sizeof(uint32_t));
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

    game_update(&state, pixels, width, height, rgba_format);

    // Upload texture to GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels);

    // Render textured quad
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(0, 0);
    glTexCoord2f(1, 0);
    glVertex2f(width, 0);
    glTexCoord2f(1, 1);
    glVertex2f(width, height);
    glTexCoord2f(0, 1);
    glVertex2f(0, height);
    glEnd();

    glXSwapBuffers(d, win); // V-sync if enabled

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
  free(pixels);
  glDeleteTextures(1, &texture);
  glXMakeCurrent(d, None, NULL);
  glXDestroyContext(d, ctx);
  XDestroyWindow(d, win);
  XFree(vi);
  XCloseDisplay(d);
  return 0;
}
