# Lesson 02 — X11/GLX Window

## Overview

| Item | Value |
|------|-------|
| **Backend** | X11 only |
| **New concepts** | `XOpenDisplay` / `XCreateWindow`, `glXCreateContext`, `XSizeHints` (temporary) |
| **Observable outcome** | A second window appears from the X11 backend; identical visual to lesson 01 |
| **Files created** | `src/platforms/x11/base.h`, `src/platforms/x11/base.c`, `src/platforms/x11/main.c` |

---

## What you'll build

An X11/GLX window that opens, renders a dark background via OpenGL, and closes on Esc or the window's × button.

---

## Background

### Why X11 at all?

Raylib abstracts away the window system, which is great for rapid development but hides the plumbing.  The X11 backend teaches you exactly what Raylib is doing under the hood:

- `XOpenDisplay` → connect to the X server (the compositor / Wayland XWayland)
- `glXChooseVisual` → pick an OpenGL-capable framebuffer format
- `XCreateWindow` → create the OS window
- `glXCreateContext` → create an OpenGL context
- `glXMakeCurrent` → bind context to window
- `glXSwapBuffers` → swap front/back buffers (like `EndDrawing`)

### JS analogy

`XOpenDisplay` is like `fetch('http://localhost:6000')` — you're connecting to the X server process over a socket.  `XCreateWindow` is like `createElement('canvas')`.  `glXCreateContext` is like `canvas.getContext('webgl')`.

### The global `g_x11` struct

All X11 state lives in one struct (`X11State`) defined in `base.h` and allocated in `base.c`.  This keeps `main.c` clean and lets any future helper function access the display, window, and context via the global.

### `XSizeHints` — temporary fixed window size

In early lessons (03–07) we don't need letterbox math.  We use `XSizeHints` to lock the window to `GAME_W × GAME_H` so the canvas always matches the window.

**Lesson 08 removes this lock** and replaces it with real letterbox scaling.  The commented-out block in `main.c` shows where this lives.

---

## What to type

### `src/platforms/x11/base.h`

```c
#define _POSIX_C_SOURCE 200809L
#ifndef PLATFORMS_X11_BASE_H
#define PLATFORMS_X11_BASE_H

#include <stdbool.h>
#include <stdint.h>
#include <GL/glx.h>
#include <X11/Xlib.h>

/* Temporary constants — replaced by platform.h macros in Lesson 05 */
#define GAME_W    800
#define GAME_H    600

typedef struct {
    Display    *display;
    Window      window;
    GLXContext  gl_context;
    int         screen;
    int         window_w;
    int         window_h;
    Atom        wm_delete_window;
    bool        vsync_enabled;
    GLuint      texture_id;
} X11State;

extern X11State g_x11;
#endif
```

### `src/platforms/x11/base.c`

```c
#include "./base.h"
X11State g_x11 = {0};
```

### `src/platforms/x11/main.c` — init section (L02 delta)

The minimum X11 window:

```c
#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xutil.h>   /* XSizeHints — introduced L02, removed L08 */
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include "./base.h"
#include "../../platform.h"

int main(void) {
    g_x11.display = XOpenDisplay(NULL);
    if (!g_x11.display) { fprintf(stderr, "XOpenDisplay failed\n"); return 1; }

    Bool ok;
    XkbSetDetectableAutoRepeat(g_x11.display, True, &ok);
    if (!is_ok) {
        fprintf(stderr, "XkbSetDetectableAutoRepeat failed\n");
        return 1;
    }

    g_x11.screen   = DefaultScreen(g_x11.display);
    g_x11.window_w = GAME_W;
    g_x11.window_h = GAME_H;

    int attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    XVisualInfo *visual = glXChooseVisual(g_x11.display, g_x11.screen, attribs);
    if (!visual) {
        fprintf(stderr, "glXChooseVisual failed\n");
        return 1;
    }

    Colormap cmap = XCreateColormap(g_x11.display,
        RootWindow(g_x11.display, g_x11.screen), visual->visual, AllocNone);

    XSetWindowAttributes wa = {
        .colormap   = cmap,
        .event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                      StructureNotifyMask,
    };

    g_x11.window = XCreateWindow(g_x11.display,
        RootWindow(g_x11.display, g_x11.screen),
        100, 100, GAME_W, GAME_H, 0,
        visual->depth, InputOutput, visual->visual,
        CWColormap | CWEventMask, &wa);

    g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
    XFree(visual);

    XStoreName(g_x11.display, g_x11.window, TITLE);
    XMapWindow(g_x11.display, g_x11.window);

    g_x11.wm_delete_window = XInternAtom(g_x11.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_x11.display, g_x11.window, &g_x11.wm_delete_window, 1);

    /* LESSON 02 — XSizeHints: fix window size temporarily.
     * LESSON 08 — Remove these 5 lines and add letterbox math instead. */
    XSizeHints *hints = XAllocSizeHints();
    if (!hints) {
        fprintf(stderr, "XAllocSizeHints failed\n");
        return 1;
    }
    hints->flags      = PMinSize | PMaxSize;
    hints->min_width  = hints->max_width  = GAME_W;
    hints->min_height = hints->max_height = GAME_H;
    XSetWMNormalHints(g_x11.display, g_x11.window, hints);
    XFree(hints);

    glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &g_x11.texture_id);
    glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int is_running = 1;
    while (is_running) {
        XEvent ev;
        while (XPending(g_x11.display) > 0) {
            XNextEvent(g_x11.display, &ev);
            if (ev.type == ClientMessage &&
                (Atom)ev.xclient.data.l[0] == g_x11.wm_delete_window)
                is_running = 0;
        }
        glClearColor(20.0f/255, 20.0f/255, 30.0f/255, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glXSwapBuffers(g_x11.display, g_x11.window);
    }

    glXMakeCurrent(g_x11.display, None, NULL);
    glXDestroyContext(g_x11.display, g_x11.gl_context);
    XDestroyWindow(g_x11.display, g_x11.window);
    XCloseDisplay(g_x11.display);
    return 0;
}
```

---

## Explanation

| Concept | Detail |
|---------|--------|
| `XOpenDisplay(NULL)` | `NULL` reads the `$DISPLAY` env var (usually `:0` or `:1`) |
| `glXChooseVisual` | Finds a framebuffer config with depth=24 + double-buffering |
| `XCreateWindow` | Position (100,100), size GAME_W×GAME_H |
| `WM_DELETE_WINDOW` | Without this, the × button sends a `ClientMessage` that we intercept to quit cleanly |
| `XkbSetDetectableAutoRepeat` | Suppresses phantom key-repeat events so we get clean press/release pairs |
| `XSizeHints` | Tells the window manager not to let the user resize the window |

---

## Build and run

```sh
./build-dev.sh --backend=x11 -r
```

You should see the same dark window as lesson 01, this time from the X11 backend.

---

## Quiz

1. What does `XkbSetDetectableAutoRepeat` do and why is it important for games?
2. `g_x11` is a global struct.  What would be the trade-off of putting the same fields on the stack in `main()` instead?
3. `XSizeHints` is described as "temporary".  What problem does it solve for early lessons, and why isn't it the right solution for a resizable game window?
