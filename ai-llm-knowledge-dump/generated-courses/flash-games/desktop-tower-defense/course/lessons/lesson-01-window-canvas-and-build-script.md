# Lesson 01: Window, Canvas, and Build Script

## What we're building

A 760×520 window that opens, fills with a solid dark-grey background every frame,
and closes cleanly when you press Escape or the window's × button.  Nothing simulates
yet — this is pure proof-of-life.  By the end you will have the complete skeleton that
all future lessons build upon, running on **both** Raylib and X11.

## What you'll learn

- The **backbuffer pattern**: write pixels to a plain `uint32_t` array, then let the platform blit it to the screen
- The `GAME_RGB` / `GAME_RGBA` macros and the `0xAARRGGBB` pixel format
- How `platform_init` and a **delta-time game loop** work in C  
  (compare with `requestAnimationFrame` in JavaScript)
- How Raylib and X11 differ for window creation, frame timing, and pixel upload
- How to build and run with `build-dev.sh --backend=raylib|x11`

## Prerequisites

- C99 basics (structs, pointers, `#include`, `typedef`, `uint32_t`)
- Raylib installed (`brew install raylib` / `sudo apt install libraylib-dev`)
- Optional: X11 (`sudo apt install libx11-dev`)

---

## Step 1: `src/utils/backbuffer.h`

The backbuffer is the heart of the rendering pipeline.  The game writes pixels here;
the platform reads and uploads it every frame.

JS analogy: `Backbuffer.pixels` is exactly like `ImageData.data` behind a 2D
`<canvas>` — a flat, writable pixel array.  The game "draws" by writing integers;
the platform is the browser call that pushes it to the screen.

```c
/* src/utils/backbuffer.h  —  Desktop Tower Defense | Backbuffer
 *
 * Pixel format: 0xAARRGGBB  (little-endian: bytes [B, G, R, A] in memory).
 * X11: reads bytes natively as BGR — no swap needed.
 * Raylib: reads as RGBA — R↔B swap required before UpdateTexture.
 */
#ifndef DTD_BACKBUFFER_H
#define DTD_BACKBUFFER_H

#include <stdint.h>

/* Build a fully-opaque 32-bit color from 8-bit R, G, B components.
 * Alpha is fixed at 0xFF (opaque).
 *
 * Bit layout:  [31..24]=A  [23..16]=R  [15..8]=G  [7..0]=B
 *
 * JS analogy: like `(0xFF << 24) | (r << 16) | (g << 8) | b`
 * in a Canvas ImageData pixel; same bit-packing, same shift math. */
#define GAME_RGB(r, g, b) \
    (((uint32_t)0xFF << 24) | ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) <<  8) |  (uint32_t)(b))

/* GAME_RGBA: same but with explicit alpha (for semi-transparent overlays). */
#define GAME_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) <<  8) |  (uint32_t)(b))

/* The backbuffer: a flat array of ARGB pixels the game writes each frame.
 *
 * pitch = bytes per row.  For a packed buffer this equals width * 4.
 * Always use `pitch / 4` (not just `width`) when stepping between rows —
 * this stays correct if padding is ever added.
 *
 * Access pattern:  pixels[y * (pitch / 4) + x]
 *   or equivalently (for packed buffers): pixels[y * width + x] */
typedef struct {
    uint32_t *pixels; /* heap-allocated; size = height * pitch bytes */
    int       width;
    int       height;
    int       pitch;  /* bytes per row = width * sizeof(uint32_t) */
} Backbuffer;

#endif /* DTD_BACKBUFFER_H */
```

**What's happening:**

- `GAME_RGB(0x22, 0x22, 0x22)` packs three channel bytes into one `uint32_t`.
  The alpha channel is always 0xFF (fully opaque).
- `pixels[y * width + x]` — one flat array, no 2-D pointers.  Row `y` starts at
  index `y * width`.
- On a little-endian CPU `0xAARRGGBB` is stored as bytes `[B, G, R, A]`.  X11
  reads XImage data in that order and displays it correctly without any swap.

---

## Step 2: `src/platform.h` — the platform contract

```c
/* src/platform.h  —  Desktop Tower Defense | Platform Contract
 *
 * The four mandatory functions every backend (main_raylib.c, main_x11.c) must
 * implement.  game.c never calls OS or graphics APIs directly — it only uses
 * this contract.
 *
 * JS analogy: platform.h is a TypeScript interface; the backend files are the
 * concrete classes that `implements` it.
 */
#ifndef DTD_PLATFORM_H
#define DTD_PLATFORM_H

#include "game.h"

/* UPDATE_BUTTON: edge-detection helper for mouse buttons.
 * Sets .pressed (0→1 edge) and .released (1→0 edge) each frame.
 * Usage: UPDATE_BUTTON(mouse.left_pressed, mouse.left_released,
 *                      mouse.left_down, is_down_now); */
#define UPDATE_BUTTON(pressed, released, was_down, is_down_now) \
    do {                                                          \
        (pressed)  = (!(was_down)  &&  (is_down_now));           \
        (released) = ( (was_down)  && !(is_down_now));           \
        (was_down) = (is_down_now);                              \
    } while (0)

/* Open a window of the given pixel dimensions with the given title. */
void   platform_init(const char *title, int width, int height);

/* Return a monotonic timestamp in seconds (never jumps backward).
 * JS analogy: performance.now() / 1000 */
double platform_get_time(void);

/* Read OS input events; fill *mouse with the current mouse state.
 * Must be called once per frame, before game_update(). */
void   platform_get_input(MouseState *mouse);

/* Upload bb->pixels to the display and flip.
 * Raylib: performs R↔B swap before UpdateTexture.
 * X11: calls XPutImage directly (no swap needed). */
void   platform_display_backbuffer(const Backbuffer *bb);

/* Audio — stubbed in lesson 01; implemented later. */
void platform_audio_init(GameState *state, int samples_per_second);
void platform_audio_update(GameState *state);
void platform_audio_shutdown(void);

#endif /* DTD_PLATFORM_H */
```

---

## Step 3: `src/game.h` — minimal stub (Lesson 01 version)

For this first lesson `game.h` only needs the canvas constants, the color macros,
and a minimal `GameState` with `should_quit`.  It will grow substantially in later
lessons — this is just enough to make the backends compile.

```c
/* src/game.h  —  Desktop Tower Defense | Shared Types (Lesson 01 stub)
 *
 * DTD coordinate system: Y-DOWN, pixel units.
 *   pixel_x = col * CELL_SIZE,  pixel_y = row * CELL_SIZE
 *   Origin (0,0) = top-left of grid.  NO world-to-screen flip.
 *
 * ⚠️  Y-down exception: DTD is a grid game.  A Y-flip would add noise with
 * zero benefit, so Y-down is intentional throughout this course.
 */
#ifndef DTD_GAME_H
#define DTD_GAME_H

#include <stdint.h>
#include <string.h>    /* memset */
#include "utils/backbuffer.h"

/* -----------------------------------------------------------------------
 * CANVAS CONSTANTS
 * ----------------------------------------------------------------------- */
#define CANVAS_W  760   /* total canvas width  (600 grid + 160 side panel) */
#define CANVAS_H  520   /* total canvas height (400 grid + 120 HUD area)   */

/* Background color — dark grey */
#define COLOR_BG  GAME_RGB(0x22, 0x22, 0x22)

/* -----------------------------------------------------------------------
 * MINIMAL GAME STATE  (grows in later lessons)
 * ----------------------------------------------------------------------- */

/* MouseState stub so platform.h compiles — filled in Lesson 03. */
typedef struct { int x, y; int left_down, left_pressed, left_released; int right_pressed; } MouseState;

typedef struct {
    int should_quit;  /* set to 1 to exit the game loop */
} GameState;

/* -----------------------------------------------------------------------
 * GAME API (implemented in game.c)
 * ----------------------------------------------------------------------- */
void game_init(GameState *state);
void game_update(GameState *state, float dt);
void game_render(const GameState *state, Backbuffer *bb);

#endif /* DTD_GAME_H */
```

---

## Step 4: `src/game.c` — minimal stub

```c
/* src/game.c  —  Desktop Tower Defense | Game Logic (Lesson 01 stub) */
#include "game.h"

/* game_init: one-time setup.  Does nothing yet — expanded in Lesson 02. */
void game_init(GameState *state) {
    memset(state, 0, sizeof(*state));
}

/* game_update: advance simulation by dt seconds.  No-op for now. */
void game_update(GameState *state, float dt) {
    (void)dt;   /* suppress unused-parameter warning */
    (void)state;
}

/* game_render: draw everything to the backbuffer.
 * Right now we just fill every pixel with the background color.
 *
 * JS analogy: this is the function called inside requestAnimationFrame.
 * It gets a writable pixel buffer (bb) and paints the current frame. */
void game_render(const GameState *state, Backbuffer *bb) {
    (void)state;
    int total = bb->width * bb->height;
    for (int i = 0; i < total; i++)
        bb->pixels[i] = COLOR_BG;
}
```

---

## Step 5: `src/main_raylib.c` — Raylib backend

```c
/* src/main_raylib.c  —  Desktop Tower Defense | Raylib Platform Backend
 *
 * Responsibilities:
 *   - Open window (760×520, resizable)
 *   - Allocate pixel backbuffer on the heap
 *   - Run delta-time game loop
 *   - R↔B swap before uploading pixels to GPU texture
 *   - Letterbox-scale canvas to fill whatever the window size is
 */
#include "raylib.h"
#include <stdlib.h>   /* malloc, free */
#include <string.h>   /* memset       */
#include "platform.h"

/* GPU texture — updated from our pixel buffer every frame. */
static Texture2D g_texture;

/* Letterbox transform: canvas→window scale and centering offsets.
 * Computed each frame in platform_display_backbuffer.
 * Used in Lesson 03 to transform mouse coordinates back to canvas space. */
static float g_scale    = 1.0f;
static int   g_offset_x = 0;
static int   g_offset_y = 0;

void platform_init(const char *title, int width, int height) {
    SetTraceLogLevel(LOG_WARNING);        /* suppress Raylib log spam  */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(width, height, title);

    /* Create an empty CPU-side image so Raylib allocates the GPU texture.
     * PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 = 4 bytes per pixel (RGBA order).
     * We fill it from our own pixel array each frame via UpdateTexture. */
    Image img = {
        .data    = NULL,
        .width   = width,
        .height  = height,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    g_texture = LoadTextureFromImage(img);
    SetTargetFPS(60);
}

double platform_get_time(void) {
    /* GetTime() returns seconds since InitWindow — monotonic. */
    return GetTime();
}

void platform_get_input(MouseState *mouse) {
    (void)mouse; /* stubbed — mouse input added in Lesson 03 */
}

void platform_display_backbuffer(const Backbuffer *bb) {
    /* ----------------------------------------------------------------
     * R↔B channel swap — why is this needed?
     *
     * Our pixel format: 0xAARRGGBB
     * In memory (little-endian): bytes [B, G, R, A]
     *
     * Raylib's GPU texture format: RGBA
     * Raylib reads those same bytes as [R=B, G=G, B=R, A=A] → red and blue
     * are swapped.  We fix this before uploading and swap back afterward so
     * the game's pixel array is always in our canonical ARGB format.
     *
     * X11 reads bytes natively as BGR — no swap needed there.
     * ---------------------------------------------------------------- */
    uint32_t *px    = bb->pixels;
    int        total = bb->width * bb->height;
    for (int i = 0; i < total; i++) {
        uint32_t c = px[i];
        px[i] = (c & 0xFF00FF00u)           /* keep A and G in place */
              | ((c & 0x00FF0000u) >> 16)   /* R moves to B position */
              | ((c & 0x000000FFu) << 16);  /* B moves to R position */
    }
    UpdateTexture(g_texture, bb->pixels);
    for (int i = 0; i < total; i++) {       /* swap back */
        uint32_t c = px[i];
        px[i] = (c & 0xFF00FF00u)
              | ((c & 0x00FF0000u) >> 16)
              | ((c & 0x000000FFu) << 16);
    }

    /* Letterbox: scale canvas to window while keeping aspect ratio.
     * Black bars fill the unused space on either side or top/bottom. */
    int   win_w  = GetScreenWidth();
    int   win_h  = GetScreenHeight();
    float scaleX = (float)win_w / (float)CANVAS_W;
    float scaleY = (float)win_h / (float)CANVAS_H;
    g_scale    = (scaleX < scaleY) ? scaleX : scaleY;
    int dst_w  = (int)((float)CANVAS_W * g_scale);
    int dst_h  = (int)((float)CANVAS_H * g_scale);
    g_offset_x = (win_w - dst_w) / 2;
    g_offset_y = (win_h - dst_h) / 2;

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTextureEx(g_texture,
                  (Vector2){(float)g_offset_x, (float)g_offset_y},
                  0.0f, g_scale, WHITE);
    EndDrawing();
}

void platform_audio_init(GameState *s, int hz)  { (void)s; (void)hz; }
void platform_audio_update(GameState *s)         { (void)s; }
void platform_audio_shutdown(void)               {}

/* -----------------------------------------------------------------------
 * MAIN  —  delta-time game loop
 *
 * JS analogy:
 *   let prev = performance.now();
 *   function loop() {
 *     const dt = (performance.now() - prev) / 1000;
 *     prev = performance.now();
 *     gameUpdate(dt);
 *     gameRender(pixels);
 *     requestAnimationFrame(loop);
 *   }
 *   requestAnimationFrame(loop);
 *
 * In C we use a while loop with platform_get_time() for the same effect.
 * ----------------------------------------------------------------------- */
int main(void) {
    static GameState state;
    static Backbuffer bb;

    /* Allocate pixel buffer once — 760×520×4 = ~1.5 MB.
     * Declared static to avoid a large stack allocation, but pixels must be
     * heap-allocated so the platform can reference them across calls. */
    bb.pixels = (uint32_t *)malloc((size_t)(CANVAS_W * CANVAS_H) * sizeof(uint32_t));
    if (!bb.pixels) return 1;
    bb.width  = CANVAS_W;
    bb.height = CANVAS_H;
    bb.pitch  = CANVAS_W * 4;

    platform_init("Desktop Tower Defense", CANVAS_W, CANVAS_H);
    game_init(&state);

    double prev_time = platform_get_time();

    while (!WindowShouldClose() && !state.should_quit) {
        /* --- delta time ---
         * dt is the number of seconds since the last frame.
         * Capped at 0.1 s so a debugger pause doesn't give the simulation
         * a giant time step and teleport all creeps across the map. */
        double curr_time  = platform_get_time();
        float  dt         = (float)(curr_time - prev_time);
        if (dt > 0.1f) dt = 0.1f;
        prev_time = curr_time;

        platform_get_input(&state.mouse);
        game_update(&state, dt);
        game_render(&state, &bb);
        platform_display_backbuffer(&bb);
    }

    UnloadTexture(g_texture);
    CloseWindow();
    free(bb.pixels);
    return 0;
}
```

---

## Step 6: `src/main_x11.c` — X11 backend

X11 is more verbose but the structure is identical.  Key difference: X11 has no
built-in FPS limiter and no letterboxing — the window is fixed at 760×520.

```c
/* src/main_x11.c  —  Desktop Tower Defense | X11 Platform Backend */
#define _POSIX_C_SOURCE 199309L   /* expose clock_gettime, nanosleep */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <time.h>     /* clock_gettime, nanosleep */
#include <stdlib.h>   /* malloc, free */
#include <string.h>   /* memset */
#include "platform.h"

/* X11 globals — game.c never sees these. */
static Display  *g_display;
static Window    g_window;
static GC        g_gc;
static XImage   *g_ximage;
static int       g_screen;
static int       g_should_quit;

void platform_init(const char *title, int width, int height) {
    g_display = XOpenDisplay(NULL);
    if (!g_display) return;
    g_screen = DefaultScreen(g_display);

    g_window = XCreateSimpleWindow(
        g_display, RootWindow(g_display, g_screen),
        0, 0, (unsigned)width, (unsigned)height, 1,
        BlackPixel(g_display, g_screen),
        WhitePixel(g_display, g_screen));

    /* Lock window size — XPutImage has no built-in scaling. */
    XSizeHints *hints = XAllocSizeHints();
    if (hints) {
        hints->flags      = PMinSize | PMaxSize;
        hints->min_width  = hints->max_width  = width;
        hints->min_height = hints->max_height = height;
        XSetWMNormalHints(g_display, g_window, hints);
        XFree(hints);
    }

    XSelectInput(g_display, g_window,
                 ExposureMask | KeyPressMask | KeyReleaseMask |
                 ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | StructureNotifyMask);

    /* Wire the window-close (✕) button. */
    Atom wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &wm_delete, 1);

    XStoreName(g_display, g_window, title);
    XMapWindow(g_display, g_window);
    XFlush(g_display);

    g_gc = XCreateGC(g_display, g_window, 0, NULL);
}

double platform_get_time(void) {
    /* CLOCK_MONOTONIC never jumps backward — correct for delta-time. */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

void platform_get_input(MouseState *mouse) {
    (void)mouse; /* stubbed — mouse input added in Lesson 03 */
    Atom wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    while (XPending(g_display)) {
        XEvent ev;
        XNextEvent(g_display, &ev);
        if (ev.type == ClientMessage &&
            (Atom)ev.xclient.data.l[0] == wm_delete)
            g_should_quit = 1;
    }
}

void platform_display_backbuffer(const Backbuffer *bb) {
    if (!g_display) return;

    /* Create XImage once, pointing at our pixel buffer.
     * ZPixmap = packed rows, same stride as our buffer.
     * x11 reads bytes [B,G,R,A] natively — no R↔B swap needed. */
    if (!g_ximage) {
        g_ximage = XCreateImage(
            g_display, DefaultVisual(g_display, g_screen),
            (unsigned)DefaultDepth(g_display, g_screen),
            ZPixmap, 0,
            (char *)bb->pixels,
            (unsigned)bb->width, (unsigned)bb->height,
            32, bb->pitch);
    }

    XPutImage(g_display, g_window, g_gc, g_ximage,
              0, 0, 0, 0,
              (unsigned)bb->width, (unsigned)bb->height);
    XFlush(g_display);
}

void platform_audio_init(GameState *s, int hz)  { (void)s; (void)hz; }
void platform_audio_update(GameState *s)         { (void)s; }
void platform_audio_shutdown(void)               {}

int main(void) {
    static GameState state;
    static Backbuffer bb;

    bb.pixels = (uint32_t *)malloc((size_t)(CANVAS_W * CANVAS_H) * sizeof(uint32_t));
    if (!bb.pixels) return 1;
    bb.width  = CANVAS_W;
    bb.height = CANVAS_H;
    bb.pitch  = CANVAS_W * 4;

    platform_init("Desktop Tower Defense", CANVAS_W, CANVAS_H);
    game_init(&state);

    double prev_time = platform_get_time();

    while (!g_should_quit && !state.should_quit) {
        double curr_time  = platform_get_time();
        float  dt         = (float)(curr_time - prev_time);
        if (dt > 0.1f) dt = 0.1f;
        prev_time = curr_time;

        platform_get_input(&state.mouse);
        game_update(&state, dt);
        game_render(&state, &bb);
        platform_display_backbuffer(&bb);

        /* Manual 60 fps cap — X11 has no SetTargetFPS equivalent. */
        double elapsed = platform_get_time() - curr_time;
        double target  = 1.0 / 60.0;
        if (elapsed < target) {
            struct timespec ts;
            double rem  = target - elapsed;
            ts.tv_sec   = (time_t)rem;
            ts.tv_nsec  = (long)((rem - (double)ts.tv_sec) * 1e9);
            nanosleep(&ts, NULL);
        }
    }

    if (g_ximage) { g_ximage->data = NULL; XDestroyImage(g_ximage); }
    if (g_display) {
        XDestroyWindow(g_display, g_window);
        XCloseDisplay(g_display);
    }
    free(bb.pixels);
    return 0;
}
```

**X11 vs Raylib — key differences at a glance:**

| Concern         | Raylib                                 | X11                                    |
|-----------------|----------------------------------------|----------------------------------------|
| Window creation | `InitWindow(w, h, title)`              | `XOpenDisplay` + `XCreateSimpleWindow` |
| Frame timing    | `SetTargetFPS(60)`                     | Manual `nanosleep` at end of loop      |
| Pixel upload    | `UpdateTexture` → GPU draw             | `XPutImage` direct software blit       |
| Window close    | `WindowShouldClose()`                  | `WM_DELETE_WINDOW` ClientMessage       |
| Letterbox       | `DrawTextureEx` with `g_scale`         | Not done — window is fixed size        |
| R↔B swap        | Required (GPU RGBA vs our ARGB format) | Not required (X11 reads BGR natively)  |

---

## Step 7: `build-dev.sh`

```bash
#!/usr/bin/env bash
# build-dev.sh  —  Desktop Tower Defense | Developer Build Script
#
# Usage:
#   ./build-dev.sh [--backend=raylib|x11] [-r] [-d]
#
#   --backend=raylib  Build the Raylib backend (default)
#   --backend=x11     Build the X11 backend
#   -r / --run        Run the game after a successful build
#   -d / --debug-asan Enable AddressSanitizer + UBSan
#
# Examples:
#   ./build-dev.sh                        # Raylib, debug
#   ./build-dev.sh -r                     # Raylib, debug, then run
#   ./build-dev.sh --backend=x11 -r       # X11, debug, then run
#   ./build-dev.sh -d -r                  # Raylib + sanitizers, then run

set -e

BACKEND="raylib"
RUN_AFTER=0
DEBUG_ASAN=0

for arg in "$@"; do
    case "$arg" in
        --backend=x11)    BACKEND="x11"    ;;
        --backend=raylib) BACKEND="raylib" ;;
        -r|--run)         RUN_AFTER=1 ;;
        -d|--debug-asan)  DEBUG_ASAN=1 ;;
        *)
            echo "Unknown argument: $arg"
            echo "Usage: $0 [--backend=x11|raylib] [-r] [-d]"
            exit 1
        ;;
    esac
done

# Prefer clang; fall back to gcc
if command -v clang > /dev/null 2>&1; then CC="clang"
elif command -v gcc > /dev/null 2>&1;  then CC="gcc"
else echo "Error: neither clang nor gcc found."; exit 1; fi

# -O0 -g:          no optimisation, full debug symbols
# -DDEBUG:         enable ASSERT() macros
# -Wall -Wextra:   catch common mistakes
# -std=c99:        C99 (designated initializers, // comments, stdint.h)
if [ "$DEBUG_ASAN" = "1" ]; then
    DEBUG_FLAGS="-O0 -g -DDEBUG -fsanitize=address,undefined"
else
    DEBUG_FLAGS="-O0 -g -DDEBUG"
fi
COMMON_FLAGS="-Wall -Wextra -std=c99 $DEBUG_FLAGS"

mkdir -p build
OUT="build/game"

SHARED="src/game.c"

if [ "$BACKEND" = "x11" ]; then
    SRCS="src/main_x11.c $SHARED"
    LIBS="-lX11 -lm"
else
    SRCS="src/main_raylib.c $SHARED"
    LIBS="-lraylib -lm -lpthread -ldl"
fi

CMD="$CC $COMMON_FLAGS -o $OUT $SRCS $LIBS"
echo "Backend: $BACKEND | Output: ./$OUT"
echo "Running: $CMD"
$CMD
chmod +x "$OUT"
echo "Build successful → ./$OUT"

if [ "$RUN_AFTER" = "1" ]; then
    echo "Running ./$OUT ..."
    ./"$OUT"
fi
```

---

## Build and run

```bash
# Raylib backend
./build-dev.sh --backend=raylib -r

# X11 backend
./build-dev.sh --backend=x11 -r

# With AddressSanitizer (catches buffer overflows and use-after-free)
./build-dev.sh --backend=raylib -d -r
```

**Expected output:** A 760×520 window with a solid dark-grey (`#222222`) background.
Close it with Escape (Raylib) or the × button (both backends).

---

## Exercises

1. **Beginner:** Change `COLOR_BG` to `GAME_RGB(0x1A, 0x1A, 0x2E)` (deep navy blue).
   Rebuild with `./build-dev.sh -r` and confirm the color changes.

2. **Intermediate:** Add a `COLOR_GRID_EVEN GAME_RGB(0xDD, 0xDD, 0xDD)` constant to
   `game.h` and paint a 600×400 rectangle in `game_render` by writing to `bb->pixels`
   directly with a nested loop.  Confirm it appears in the top-left of the canvas.

3. **Challenge:** Why does X11 need `g_ximage->data = NULL` before `XDestroyImage`?
   Trace ownership: who allocated `bb->pixels`? Who would `XDestroyImage` try to free?
   What would happen if you omit the `= NULL`?

---

## What's next

In Lesson 02 we introduce the grid system: a flat `uint8_t` array of `CellState`
values, the `draw_rect` drawing primitive with clipping, and the math utility macros
(`MIN`, `MAX`, `CLAMP`, `ABS`).  By the end you'll see a 30×20 checkerboard grid
with a green entry cell and a red exit cell.
