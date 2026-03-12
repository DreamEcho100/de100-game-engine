
#define _POSIX_C_SOURCE 200809L

#include "../../platform.h"
#include "./base.h"
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h> /* XSizeHints — introduced L02, removed L08 */
#include <X11/keysym.h>
#include <stdio.h>

int main(void) {
  g_x11.display = XOpenDisplay(NULL);
  if (!g_x11.display) {
    fprintf(stderr, "XOpenDisplay failed\n");
    return 1;
  }

  Bool is_ok = false;
  XkbSetDetectableAutoRepeat(g_x11.display, true, &is_ok);
  if (!is_ok) {
    fprintf(stderr, "XkbSetDetectableAutoRepeat failed\n");
    return 1;
  }

  g_x11.screen = DefaultScreen(g_x11.display);
  g_x11.window_w = GAME_W;
  g_x11.window_h = GAME_H;

  int attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
  XVisualInfo *visual = glXChooseVisual(g_x11.display, g_x11.screen, attribs);
  if (!visual) {
    fprintf(stderr, "glXChooseVisual failed\n");
    return 1;
  }

  Window root_window = RootWindow(g_x11.display, g_x11.screen);

  XSetWindowAttributes wa = {
      .colormap = XCreateColormap(g_x11.display, root_window, visual->visual,
                                  AllocNone),
      .event_mask =
          ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask,
  };

  g_x11.window = XCreateWindow(       //
      g_x11.display,                  // connection
      root_window,                    // parent window
      0, 0,                           // x, y
      g_x11.window_w, g_x11.window_h, // width, height
      0,                              // border width
      visual->depth,                  // depth
      InputOutput,                    // what kind of window
      visual->visual,                 // visual
      CWColormap | CWEventMask,       // mask
      &wa);

  g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
  XFree(visual);

  XStoreName(g_x11.display, g_x11.window, TITLE);
  XMapWindow(g_x11.display, g_x11.window);

  g_x11.wm_delete_window =
      XInternAtom(g_x11.display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(g_x11.display, g_x11.window, &g_x11.wm_delete_window, 1);

  /* LESSON 02 — XSizeHints: fix window size temporarily.
   * LESSON 08 — Remove these 5 lines and add letterbox math instead. */
  XSizeHints *hints = XAllocSizeHints();
  if (!hints) {
    fprintf(stderr, "XAllocSizeHints failed\n");
    return 1;
  }
  hints->flags = PMinSize | PMaxSize;
  hints->min_width = hints->max_width = g_x11.window_w;
  hints->min_height = hints->max_height = g_x11.window_h;
  XSetWMNormalHints(g_x11.display, g_x11.window, hints);
  XFree(hints);

  glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

  glEnable(GL_TEXTURE_2D);
  glGenTextures(1, &g_x11.texture_id);
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  int is_runing = 1;

  while (is_runing) {
    XEvent event;
    while (XPending(g_x11.display)) {
      XNextEvent(g_x11.display, &event);
      switch (event.type) {
      case ClientMessage:
        if ((Atom)(event.xclient.data.l[0]) == g_x11.wm_delete_window) {
          is_runing = 0;
        }
        break;
      }
    }

    glClearColor(20.0f / 255, 20.0f / 255, 30.0f / 255, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glXSwapBuffers(g_x11.display, g_x11.window);
  }

  glXMakeCurrent(g_x11.display, None, None);
  glXDestroyContext(g_x11.display, g_x11.gl_context);
  XDestroyWindow(g_x11.display, g_x11.window);
  XCloseDisplay(g_x11.display);

  return 0;
}
