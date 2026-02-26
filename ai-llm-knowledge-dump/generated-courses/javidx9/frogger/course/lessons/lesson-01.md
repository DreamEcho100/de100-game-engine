# Lesson 01 — Window + GLX/OpenGL Context

## By the end of this lesson you will have:

A 1024×640 black window using X11/GLX with an OpenGL context, VSync, and auto-repeat suppression — or the equivalent Raylib window. Nothing is drawn yet except black.

---

## Architecture Overview

```
main_x11.c / main_raylib.c
    ↓ calls
platform_init("Frogger", 1024, 640)
    ↓ opens
Window + OpenGL context
    ↓ ready for
delta-time game loop
```

This lesson focuses on **platform_init only**. The game loop, input, and rendering are built in later lessons.

---

## Step 1 — Why OpenGL instead of XPixmap?

The old frogger backend used an X11 `Pixmap` for double-buffering:

```c
g_backbuffer = XCreatePixmap(g_display, g_window, width, height, depth);
/* ... draw many rectangles ... */
XCopyArea(g_display, g_backbuffer, g_window, g_gc, 0, 0, w, h, 0, 0);
```

The problem: every sprite cell required its own `XSetForeground` + `XFillRectangle` system call. A full frame with ~180 tiles × 64 cells = ~11,520 X11 drawing calls per frame. Each one crosses the process boundary to the X server.

The new approach:
1. `frogger_render` writes all pixels to a `uint32_t *` array (no X11 calls)
2. `platform_display_backbuffer` uploads the entire array in ONE `glTexImage2D` call
3. OpenGL draws a fullscreen quad

One GPU upload per frame instead of 11,520 system calls.

---

## Step 2 — GLX context creation

X11 doesn't know about OpenGL — `GLX` is the bridge between them.

```c
/* Choose a framebuffer config that supports RGBA + double-buffering */
static const int fb_attribs[] = {
    GLX_RENDER_TYPE,   GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_DOUBLEBUFFER,  True,
    GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_ALPHA_SIZE, 8,
    None
};
GLXFBConfig *fbc = glXChooseFBConfig(g_display, screen, fb_attribs, &n_configs);
XVisualInfo *vi  = glXGetVisualFromFBConfig(g_display, fbc[0]);
```

**New C concept — `None` as terminator:**
`None` (value 0) terminates the attribute list. This is the same pattern as `NULL`-terminated strings, but for integer arrays. Many X11/OpenGL functions use this convention.

**Why `XCreateWindow` not `XCreateSimpleWindow`?**
`XCreateSimpleWindow` uses the parent's visual. To use the GLX visual (`vi->visual`), we must create the window with `XCreateWindow` and explicitly pass `vi->visual` and a matching colormap.

```c
swa.colormap = XCreateColormap(g_display, root, vi->visual, AllocNone);
g_window = XCreateWindow(g_display, root,
    0, 0, width, height, 0,
    vi->depth, InputOutput, vi->visual,
    CWColormap | CWEventMask, &swa);
```

---

## Step 3 — VSync

Without VSync, `glXSwapBuffers` returns immediately and the loop runs at thousands of FPS, burning 100% CPU.

```c
typedef void (*PFNGLXSWAPINTERVALEXT)(Display *, GLXDrawable, int);
PFNGLXSWAPINTERVALEXT glXSwapIntervalEXT =
    (PFNGLXSWAPINTERVALEXT)(void *)glXGetProcAddressARB(
        (const GLubyte *)"glXSwapIntervalEXT");
if (glXSwapIntervalEXT)
    glXSwapIntervalEXT(g_display, glXGetCurrentDrawable(), 1);
```

With `interval=1`, `glXSwapBuffers` blocks until the next monitor vertical sync (typically 16.67ms at 60Hz). This gives ~60fps with zero spin-wait.

**New C concept — function pointer cast:**
`(PFNGLXSWAPINTERVALEXT)(void *)glXGetProcAddressARB(...)` — `glXGetProcAddressARB` returns a generic function pointer (`__GLXextFuncPtr`). We cast it to the specific function type we need. The `(void *)` intermediate cast avoids a strict-aliasing warning.

---

## Step 4 — Auto-repeat suppression

When a key is held, X11 auto-repeat generates fake `KeyRelease`+`KeyPress` pairs. Without suppression, the frog hops continuously while a key is held — not what we want (one hop per tap).

```c
Bool supported;
XkbSetDetectableAutoRepeat(g_display, True, &supported);
```

After this call, X11 suppresses the fake events. A held key sends only `KeyPress` once and no further events until the key is actually released.

**New C concept — `Bool` (X11 type):**
`Bool` is defined in `<X11/Xlib.h>` as `int`. It's just a convention — X11 functions use it where `int` would do.

---

## Step 5 — Raylib (simpler)

```c
void platform_init(const char *title, int width, int height) {
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(width, height, title);
    SetTargetFPS(0);  /* uncapped — VSync via EndDrawing */
}
```

`SetTargetFPS(0)` removes the frame cap. `EndDrawing()` already waits for VSync internally via the OS swap interval. We don't need `SetTargetFPS(60)` because we control timing with `delta_time`.

---

## Build & Run

```sh
cd course/
./build_x11.sh && ./build/frogger_x11
```

You should see:
```
✓ Auto-repeat detection: enabled
✓ VSync enabled (GLX_EXT_swap_control)
✓ OpenGL initialized: 4.x ...
```

A black 1024×640 window opens. Press Escape to close.

---

## Key Concepts

- GLX bridges X11 and OpenGL — `glXChooseFBConfig` + `XCreateWindow` + `glXCreateContext`
- `XCreateWindow` (not `XCreateSimpleWindow`) — required to specify a custom GLX visual
- `glXSwapIntervalEXT(..., 1)` — enables VSync; `glXSwapBuffers` blocks until next frame
- `XkbSetDetectableAutoRepeat` — suppresses fake auto-repeat events from X11
- `SetTargetFPS(0)` in Raylib — uncapped loop, VSync handled by `EndDrawing`

---

## Exercise

Comment out `XkbSetDetectableAutoRepeat`. Build and run. Hold the Up arrow and watch what happens in the terminal if you add `printf("up pressed\n");` to the KeyPress handler. Restore it and observe the difference.
