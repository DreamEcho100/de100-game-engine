# Lesson 01 — Open a Window with OpenGL/GLX Context

## By the end of this lesson you will have:

Two working programs — `tetris_x11` and `tetris_raylib` — each opens a **black window**, titled **"Tetris"**, and stays open until you press **Q** or close the window.

The X11 version uses **OpenGL via GLX** to create a hardware-accelerated rendering context — a prerequisite for the backbuffer display pipeline we will build in later lessons. The Raylib version uses `InitWindow` as before.

---

## Why OpenGL for X11?

In the original lesson, we drew directly with `XFillRectangle` and `XDrawString`. Those calls work fine for simple drawing, but our new architecture uses a **platform-independent pixel buffer** (a `uint32_t *` array). To display that buffer efficiently we need to:

1. Upload the pixel array to the GPU as a texture
2. Draw a full-screen quad textured with it
3. Call `glXSwapBuffers` to show it (and optionally sync to the monitor refresh)

That requires an **OpenGL context created via GLX**. This lesson sets it up.

---

## Step 0 — Directory structure

```bash
mkdir -p src build
touch src/main_x11.c src/main_raylib.c src/tetris.h src/tetris.c src/platform.h
touch build_x11.sh build_raylib.sh
chmod +x build_x11.sh build_raylib.sh
```

---

## Part A — The X11 + OpenGL version

### Step 1 — Create `build_x11.sh`

```bash
#!/bin/bash
set -e
mkdir -p build
clang src/tetris.c src/main_x11.c -o build/tetris_x11 \
    -Wall -Wextra -g \
    -lX11 -lxkbcommon -lGL -lGLX
echo "Build OK -> ./build/tetris_x11"
```

**New flags compared to the original:**

| Flag | Reason |
|------|--------|
| `clang` | Replaced `gcc`. Clang gives better error messages |
| `-lGL` | OpenGL functions (`glGenTextures`, `glTexImage2D`, etc.) |
| `-lGLX` | GLX context creation and `glXSwapBuffers` |
| `-lxkbcommon` | `XKBlib.h` for correct keyboard symbol lookup |
| `mkdir -p build` | Put the binary inside `build/` |

Install the required dev headers if needed:
```bash
sudo apt install libx11-dev libgl-dev libxkbcommon-dev
```

---

### Step 2 — Includes

Open `src/main_x11.c` and write the includes:

```c
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
```

**New C concept — feature-test macros (`_XOPEN_SOURCE`):**  
`#define _XOPEN_SOURCE 700` before any system include unlocks POSIX and X/Open extensions in glibc headers (e.g., `nanosleep`, `gettimeofday`). It must appear before any `#include`.

---

### Step 3 — Platform state struct

Replace the scattered global variables with a single struct:

```c
typedef struct {
    Display    *display;
    Window      window;
    GLXContext  gl_context;
    GLuint      texture_id;
    int         screen;
    int         width;
    int         height;
    bool        vsync_enabled;
} X11_State;

static bool        g_is_running         = true;
static X11_State   g_x11                = {0};
static double      g_start_time         = 0.0;
static double      g_last_frame_time    = 0.0;
static const double g_target_frame_time = 1.0 / 60.0;
```

**What each field is for:**

| Field | Purpose |
|-------|---------|
| `display` | Handle to the X server connection |
| `window` | The window ID |
| `gl_context` | The OpenGL rendering context |
| `texture_id` | GPU texture ID for uploading the backbuffer |
| `screen` | Index of the default screen |
| `width/height` | Current window dimensions |
| `vsync_enabled` | Whether VSync is active (affects frame pacing) |

The `static` keyword on global variables means they are private to this translation unit (this `.c` file). Other files cannot access them directly.

---

### Step 4 — Timing helper

```c
static double get_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double now = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
    if (g_start_time == 0.0)
        g_start_time = now;
    return now - g_start_time;
}

static void sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
```

`get_time()` returns seconds elapsed since the program started. On first call it records `g_start_time`; all subsequent calls subtract that baseline. This avoids floating-point precision loss that would occur with absolute Unix timestamps.

---

### Step 5 — VSync setup

```c
typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display *, GLXDrawable, int);

static void setup_vsync(void) {
    const char *extensions =
        glXQueryExtensionsString(g_x11.display, g_x11.screen);

    if (extensions && strstr(extensions, "GLX_EXT_swap_control")) {
        PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT =
            (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddressARB(
                (const GLubyte *)"glXSwapIntervalEXT");
        if (glXSwapIntervalEXT) {
            glXSwapIntervalEXT(g_x11.display, g_x11.window, 1);
            g_x11.vsync_enabled = true;
            printf("✓ VSync enabled (GLX_EXT_swap_control)\n");
            return;
        }
    }

    if (extensions && strstr(extensions, "GLX_MESA_swap_control")) {
        PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA =
            (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddressARB(
                (const GLubyte *)"glXSwapIntervalMESA");
        if (glXSwapIntervalMESA) {
            glXSwapIntervalMESA(1);
            g_x11.vsync_enabled = true;
            printf("✓ VSync enabled (GLX_MESA_swap_control)\n");
            return;
        }
    }

    g_x11.vsync_enabled = false;
    printf("⚠ VSync not available\n");
}
```

**How VSync works:**  
Without VSync, `glXSwapBuffers` returns immediately and we sleep manually to target 60 FPS. With VSync, `glXSwapBuffers` blocks until the next monitor refresh — no sleep needed. We try two extension paths (EXT, then MESA) with fallback.

`PFNGLXSWAPINTERVALEXTPROC` is a function pointer type. GLX extensions are loaded at runtime via `glXGetProcAddressARB` because the extension may not exist on all GPUs.

---

### Step 6 — `platform_init`: GLX context creation

```c
int platform_init(const char *title, int width, int height) {
    g_x11.display = XOpenDisplay(NULL);
    if (!g_x11.display) {
        fprintf(stderr, "Failed to open display\n");
        return 1;
    }

    Bool supported;
    XkbSetDetectableAutoRepeat(g_x11.display, True, &supported);
    printf("✓ Auto-repeat detection: %s\n",
           supported ? "enabled" : "not supported");

    g_x11.screen = DefaultScreen(g_x11.display);
    g_x11.width  = width;
    g_x11.height = height;

    /* Find a framebuffer config that supports RGBA + double buffering */
    int visual_attribs[] = {
        GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None
    };
    XVisualInfo *visual =
        glXChooseVisual(g_x11.display, g_x11.screen, visual_attribs);
    if (!visual) {
        fprintf(stderr, "Failed to choose GLX visual\n");
        return 1;
    }

    Colormap colormap =
        XCreateColormap(g_x11.display,
                        RootWindow(g_x11.display, g_x11.screen),
                        visual->visual, AllocNone);

    XSetWindowAttributes attrs;
    attrs.colormap   = colormap;
    attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask
                     | StructureNotifyMask;

    /* XCreateWindow instead of XCreateSimpleWindow — needed to pass visual */
    g_x11.window = XCreateWindow(
        g_x11.display,
        RootWindow(g_x11.display, g_x11.screen),
        100, 100, width, height, 0,
        visual->depth, InputOutput,
        visual->visual, CWColormap | CWEventMask, &attrs);

    g_x11.gl_context =
        glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
    if (!g_x11.gl_context) {
        fprintf(stderr, "Failed to create GLX context\n");
        XFree(visual);
        return 1;
    }

    XFree(visual);
    XStoreName(g_x11.display, g_x11.window, title);
    XMapWindow(g_x11.display, g_x11.window);
    XFlush(g_x11.display);

    glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

    /* Configure OpenGL for 2D rendering */
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);  /* top-left origin, y-down */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    /* Allocate GPU texture for the backbuffer */
    glGenTextures(1, &g_x11.texture_id);
    glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    setup_vsync();

    printf("✓ OpenGL initialized: %s\n", glGetString(GL_VERSION));
    return 0;
}
```

**Key differences from `XCreateSimpleWindow` in the original:**

| Original | New |
|----------|-----|
| `XCreateSimpleWindow` | `XCreateWindow` (must use visual from GLX) |
| No OpenGL | GLX context + `glXMakeCurrent` |
| No texture | `glGenTextures` allocates GPU texture |
| No VSync | `setup_vsync()` attempts VSync |

`glOrtho(0, width, height, 0, -1, 1)` sets up a 2D projection where pixel (0,0) is top-left and Y increases downward — matching our backbuffer coordinate system.

---

### Step 7 — `platform_shutdown`

```c
void platform_shutdown(void) {
    if (g_x11.texture_id)
        glDeleteTextures(1, &g_x11.texture_id);
    if (g_x11.gl_context) {
        glXMakeCurrent(g_x11.display, None, NULL);
        glXDestroyContext(g_x11.display, g_x11.gl_context);
    }
    if (g_x11.window)
        XDestroyWindow(g_x11.display, g_x11.window);
    if (g_x11.display)
        XCloseDisplay(g_x11.display);
}
```

Always free GPU resources before CPU resources: texture → GL context → window → display.

---

### Step 8 — Minimal `main()` for this lesson

```c
int main(void) {
    int screen_width  = 560;  /* placeholder: field + sidebar */
    int screen_height = 540;

    if (platform_init("Tetris", screen_width, screen_height) != 0)
        return 1;

    g_last_frame_time = get_time();

    while (g_is_running) {
        double current_time = get_time();
        float  delta_time   = (float)(current_time - g_last_frame_time);
        g_last_frame_time   = current_time;

        /* TODO: input handling */
        /* TODO: game update */
        /* TODO: render */

        /* Frame pacing: sleep if no VSync */
        if (!g_x11.vsync_enabled) {
            double frame_time = get_time() - current_time;
            if (frame_time < g_target_frame_time)
                sleep_ms((int)((g_target_frame_time - frame_time) * 1000.0));
        }

        /* Swap buffers (blocks on VSync if enabled) */
        glXSwapBuffers(g_x11.display, g_x11.window);
    }

    platform_shutdown();
    return 0;
}
```

Build and run:
```bash
./build_x11.sh
./build/tetris_x11
```

Expected terminal output:
```
✓ Auto-repeat detection: enabled
✓ VSync enabled (GLX_EXT_swap_control)
✓ OpenGL initialized: 4.6.0 ...
```

A black window opens. No input handling yet — close it with Ctrl+C for now.

---

## Part B — The Raylib version

### `build_raylib.sh` (unchanged)

```bash
#!/bin/bash
set -e
mkdir -p build
clang src/tetris.c src/main_raylib.c -o build/tetris_raylib \
    -Wall -Wextra -g \
    -lraylib -lm -lpthread -ldl
echo "Build OK -> ./build/tetris_raylib"
```

### Minimal `src/main_raylib.c`

```c
#include <raylib.h>

int main(void) {
    InitWindow(560, 540, "Tetris");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

Raylib handles VSync internally (`SetTargetFPS`). The backbuffer texture pipeline is added in Lesson 02.

---

## Mental Model: The Display Pipeline

```
game writes uint32_t pixels
      ↓
glTexImage2D uploads to GPU
      ↓
full-screen textured quad drawn
      ↓
glXSwapBuffers → monitor refresh
```

The game never calls X11 drawing functions. It writes pixel values into a CPU array. The GPU uploads that array each frame. This is the same pipeline used by emulators, software renderers, and retro-style games across all platforms.

---

## Key Concepts

- `glXChooseVisual` — find a framebuffer config with double buffering
- `XCreateWindow` (not `XCreateSimpleWindow`) — needed when specifying a GLX visual
- `glXCreateContext` / `glXMakeCurrent` — bind an OpenGL context to the window
- `glOrtho` with `y=0` at top — pixel (0,0) is top-left, matching the backbuffer
- VSync via `GLX_EXT_swap_control` or `GLX_MESA_swap_control` with fallback
- `static` globals — private to the file, not accessible from other translation units
