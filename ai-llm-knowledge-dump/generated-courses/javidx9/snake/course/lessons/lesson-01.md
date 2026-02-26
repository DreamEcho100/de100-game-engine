# Lesson 01 — Open a Window with GLX/OpenGL

## By the end of this lesson you will have:

A blank black window titled "Snake" that opens at 1200×460 pixels, renders using OpenGL, reports its OpenGL version on startup, and closes cleanly with Q or the window-close button.

---

## Why OpenGL instead of plain X11?

The original snake course used `XCreateSimpleWindow` and drew directly with X11 drawing functions (`XFillRectangle`, `XDrawString`). This works but ties rendering to X11 — you can't reuse any drawing code on another platform.

The upgraded architecture renders everything into a CPU pixel buffer (`SnakeBackbuffer`). The platform's job is only to upload that buffer to the GPU and display it. OpenGL's `glTexImage2D` does that upload in a single call. This is the same approach used by emulators, game engines, and software renderers.

---

## Step 1 — X11 and GLX headers

```c
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
```

**New C concept — `#define _XOPEN_SOURCE 700`:**
Feature-test macros tell the C library to expose extended POSIX APIs. Without `_POSIX_C_SOURCE 200809L`, functions like `clock_gettime` aren't visible. They must appear before any `#include`.

**GLX** is the bridge between X11 and OpenGL. `glx.h` gives us:
- `glXChooseVisual` — find a visual that supports OpenGL
- `glXCreateContext` — create an OpenGL rendering context
- `glXMakeCurrent` — activate the context
- `glXSwapBuffers` — present the rendered frame (with VSync)

---

## Step 2 — Platform state struct

```c
typedef struct {
    Display    *display;
    Window      window;
    GLXContext  gl_context;
    GLuint      texture_id;   /* GPU texture for the backbuffer */
    int         screen;
    int         width;
    int         height;
    int         vsync_enabled;
} X11_State;

static int       g_is_running = 1;
static X11_State g_x11 = {0};
```

**New C concept — `static` globals:**
`static` at file scope means "only visible in this file." It's the C equivalent of a module-private variable. Other files can't accidentally access `g_x11`. The `= {0}` zero-initializes all fields.

---

## Step 3 — `platform_init`

```c
void platform_init(const char *title, int width, int height) {
    Bool supported;
    int visual_attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };

    g_x11.display = XOpenDisplay(NULL);
    if (!g_x11.display) {
        fprintf(stderr, "Cannot open X display. Is $DISPLAY set?\n");
        exit(1);
    }
```

`XOpenDisplay(NULL)` connects to the X server specified by `$DISPLAY` (usually `:0` on your local machine). Returns `NULL` on failure — always check.

```c
    /* Suppress synthetic key-repeat events */
    XkbSetDetectableAutoRepeat(g_x11.display, True, &supported);
    printf("✓ Auto-repeat detection: %s\n", supported ? "enabled" : "not supported");
```

X11 sends repeated `KeyPress` events when a key is held. `XkbSetDetectableAutoRepeat` suppresses these synthetics — we only see real hardware press/release transitions. This is essential for `GameButtonState` (Lesson 09) to work correctly.

```c
    XVisualInfo *visual = glXChooseVisual(g_x11.display, g_x11.screen, visual_attribs);
```

`GLX_RGBA` — we want 24-bit color. `GLX_DOUBLEBUFFER` — draw into a back buffer, then swap to front (prevents tearing). `None` terminates the attribute list.

```c
    XSetWindowAttributes attrs;
    attrs.colormap   = XCreateColormap(g_x11.display,
                           RootWindow(g_x11.display, g_x11.screen),
                           visual->visual, AllocNone);
    attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                       StructureNotifyMask;

    g_x11.window = XCreateWindow(
        g_x11.display, RootWindow(g_x11.display, g_x11.screen),
        100, 100, width, height, 0,
        visual->depth, InputOutput, visual->visual,
        CWColormap | CWEventMask, &attrs);
```

**`XCreateWindow` not `XCreateSimpleWindow`:**
When you specify a custom GLX visual, you must use `XCreateWindow` and provide a matching `Colormap`. `XCreateSimpleWindow` uses the root window's visual which won't match the GLX visual.

```c
    g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
    XFree(visual);  /* done with visual info — free it now */

    XStoreName(g_x11.display, g_x11.window, title);
    XMapWindow(g_x11.display, g_x11.window);
    XFlush(g_x11.display);

    glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);
```

`glXMakeCurrent` activates the OpenGL context for this window. All subsequent `gl*` calls affect this context.

```c
    /* 2D orthographic projection — (0,0) top-left, (w,h) bottom-right */
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    /* Create GPU texture slot for the backbuffer */
    glGenTextures(1, &g_x11.texture_id);
    glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}
```

`glOrtho(0, w, h, 0, -1, 1)` — note `bottom=h, top=0`. This flips Y so y=0 is the top of the window, matching our "rows grow downward" pixel convention.

`GL_NEAREST` — no interpolation. We want crisp pixels, not blurred upscaling.

---

## Step 4 — Minimal game loop (stub)

```c
int main(void) {
    platform_init("Snake", 1200, 460);

    while (g_is_running) {
        XEvent event;
        while (XPending(g_x11.display) > 0) {
            XNextEvent(g_x11.display, &event);
            if (event.type == ClientMessage) g_is_running = 0;
            if (event.type == KeyPress) {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_q || key == XK_Escape) g_is_running = 0;
            }
        }

        glClear(GL_COLOR_BUFFER_BIT);
        glXSwapBuffers(g_x11.display, g_x11.window);
    }

    /* Cleanup */
    glDeleteTextures(1, &g_x11.texture_id);
    glXMakeCurrent(g_x11.display, None, NULL);
    glXDestroyContext(g_x11.display, g_x11.gl_context);
    XDestroyWindow(g_x11.display, g_x11.window);
    XCloseDisplay(g_x11.display);
    return 0;
}
```

`XPending` returns the number of events waiting. The `while (XPending > 0)` drains all queued events each frame instead of blocking on `XNextEvent`.

`glXSwapBuffers` presents the back buffer. With VSync set up (Lesson 08), this call blocks until the monitor's refresh — naturally pacing the loop to 60fps.

---

## Build & Run

```sh
clang main_x11.c -o snake_x11 -lX11 -lxkbcommon -lGL -lGLX
./snake_x11
```

You should see:
```
✓ Auto-repeat detection: enabled
✓ OpenGL initialized: 4.6 ...
```
And a black window. Press Q to quit.

---

## Key Concepts

- `glXChooseVisual` — selects a visual with RGBA + double-buffer support
- `XCreateWindow` (not `XCreateSimpleWindow`) — required when using a custom GLX visual
- `XkbSetDetectableAutoRepeat` — suppress synthetic key-repeat, only see real transitions
- `glXMakeCurrent` — activate the OpenGL context
- `glOrtho(0, w, h, 0, ...)` — 2D projection with y=0 at top
- `GL_NEAREST` — no interpolation; crisp pixel display
- `XPending` + drain loop — non-blocking event processing

---

## Exercise

Add a `ConfigureNotify` handler that updates `glViewport` and `glOrtho` when the window is resized. Test by dragging the window corners. What happens if you skip the `glOrtho` update?
