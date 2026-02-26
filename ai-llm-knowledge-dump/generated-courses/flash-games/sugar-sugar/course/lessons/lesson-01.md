# Lesson 01 — Window + Backbuffer Pipeline

**By the end of this lesson you will have:**
A 640×480 window with a warm off-white background that stays open until you press Escape or click the close button. Nothing moves. The window just sits there, ready to be drawn on.

---

## Step 1 — Why We Split Platform from Game

In React you separate business logic from UI components. Same idea here.

The **platform layer** (`main_x11.c`) owns the OS window, the event loop, and the clock. The **game layer** (`game.c`) owns all logic and rendering. They talk through four functions defined in `platform.h`.

This means you can swap the platform layer for Windows or macOS later without touching one line of game code.

Create `course/src/platform.h`:

```c
#ifndef PLATFORM_H
#define PLATFORM_H

#include "game.h"

void   platform_init(const char *title, int width, int height);
double platform_get_time(void);
void   platform_get_input(GameInput *input);
void   platform_display_backbuffer(const GameBackbuffer *backbuffer);
void   platform_play_sound(int sound_id);
void   platform_play_music(int music_id);
void   platform_stop_music(void);

#endif
```

`GameInput` and `GameBackbuffer` are forward-declared in `game.h`, which we write next.

---

## Step 2 — Color Encoding: 0xAARRGGBB

Every pixel is a single `uint32_t`. The 32 bits are split into four 8-bit channels:

```
Bit layout of 0xAARRGGBB:
  bits 31-24 → Alpha  (0xFF = fully opaque)
  bits 23-16 → Red
  bits 15-8  → Green
  bits  7-0  → Blue

Example: pure red = 0xFFFF0000
  FF → opaque
  FF → red = 255
  00 → green = 0
  00 → blue = 0
```

The macro builds this from three numbers:

```c
#define GAME_RGB(r,g,b)  \
    (((uint32_t)0xFF << 24) | \
     ((uint32_t)(r)  << 16) | \
     ((uint32_t)(g)  <<  8) | \
      (uint32_t)(b))
```

Concrete example — `GAME_RGB(245, 242, 235)`:

```
0xFF  shifted left 24 = 0xFF000000
0xF5  shifted left 16 = 0x00F50000   (245 decimal = 0xF5)
0xF2  shifted left  8 = 0x0000F200   (242 decimal = 0xF2)
0xEB  shifted left  0 = 0x000000EB   (235 decimal = 0xEB)
OR all together      = 0xFFF5F2EB   ← warm off-white
```

---

## Step 3 — game.h: All Types in One Place

Create `course/src/game.h`. We write all structs, enums, constants, and function declarations here. Every `.c` file includes this one header.

```c
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stddef.h>

/* ── Canvas size ──────────────────────────────────────── */
#define CANVAS_W 640
#define CANVAS_H 480

/* ── Color helpers ────────────────────────────────────── */
#define GAME_RGB(r,g,b) \
    (((uint32_t)0xFF<<24)|((uint32_t)(r)<<16)|((uint32_t)(g)<<8)|(uint32_t)(b))

#define COLOR_BG   GAME_RGB(245, 242, 235)   /* warm off-white */
#define COLOR_LINE GAME_RGB(50,  50,  50)    /* near-black     */

/* ── Backbuffer ───────────────────────────────────────── */
/*
 * Like a 2D canvas in JS, but flat:
 *   canvas[y][x]  in JS
 *   pixels[y*width + x]  in C
 *
 * pitch = bytes per row = width * sizeof(uint32_t)
 */
typedef struct {
    uint32_t *pixels;
    int       width;
    int       height;
    int       pitch;   /* bytes per row */
} GameBackbuffer;

/* ── Button state ─────────────────────────────────────── */
/*
 * half_transition_count: how many times the key changed state this frame.
 * ended_down: 1 if the key is currently held, 0 if released.
 *
 * A key tapped once in a frame: half_transition_count=2, ended_down=0
 * A key held:                   half_transition_count=1, ended_down=1
 */
typedef struct {
    int half_transition_count;
    int ended_down;
} GameButtonState;

#define UPDATE_BUTTON(button, is_down)                          \
    do {                                                        \
        if ((button).ended_down != (is_down)) {                 \
            (button).half_transition_count++;                   \
            (button).ended_down = (is_down);                    \
        }                                                       \
    } while(0)

#define BUTTON_PRESSED(b)   ((b).half_transition_count > 0 && (b).ended_down)
#define BUTTON_RELEASED(b)  ((b).half_transition_count > 0 && !(b).ended_down)

/* ── Mouse ────────────────────────────────────────────── */
typedef struct {
    int x, y;
    int prev_x, prev_y;
    GameButtonState left;
    GameButtonState right;
} MouseInput;

/* ── Full input frame ─────────────────────────────────── */
typedef struct {
    MouseInput      mouse;
    GameButtonState escape;
    GameButtonState reset;
    GameButtonState gravity;
} GameInput;

/* ── Grain colors ─────────────────────────────────────── */
typedef enum {
    GRAIN_WHITE  = 0,
    GRAIN_RED    = 1,
    GRAIN_GREEN  = 2,
    GRAIN_ORANGE = 3,
    GRAIN_COLOR_COUNT
} GRAIN_COLOR;

/* ── Grain pool (Struct of Arrays) ────────────────────── */
#define MAX_GRAINS 4096
typedef struct {
    float   x[MAX_GRAINS];
    float   y[MAX_GRAINS];
    float   vx[MAX_GRAINS];
    float   vy[MAX_GRAINS];
    uint8_t color[MAX_GRAINS];
    uint8_t active[MAX_GRAINS];
    uint8_t tpcd[MAX_GRAINS];   /* teleport cooldown */
    int     count;              /* high-watermark */
} GrainPool;

/* ── Level objects ────────────────────────────────────── */
typedef struct {
    int   x, y;
    int   grains_per_second;
    float spawn_timer;
} Emitter;

typedef struct {
    int        x, y, w, h;
    GRAIN_COLOR required_color;
    int        required_count;
    int        collected;
} Cup;

typedef struct {
    int         x, y, w, h;
    GRAIN_COLOR output_color;
} ColorFilter;

typedef struct {
    int ax, ay;   /* portal A center */
    int bx, by;   /* portal B center */
    int radius;
} Teleporter;

typedef struct {
    int x, y, w, h;
} Obstacle;

/* ── Level definition ─────────────────────────────────── */
typedef struct {
    int index;

    Emitter    emitters[2];    int emitter_count;
    Cup        cups[8];        int cup_count;
    ColorFilter filters[4];   int filter_count;
    Teleporter teleporters[2]; int teleporter_count;
    Obstacle   obstacles[12]; int obstacle_count;

    int has_gravity_switch;
    int is_cyclic;
} LevelDef;

/* ── Line bitmap ──────────────────────────────────────── */
/* 1 byte per pixel: 0 = empty, 1 = has line */
typedef struct {
    uint8_t pixels[CANVAS_W * CANVAS_H];
} LineBitmap;

/* ── Game phase FSM ───────────────────────────────────── */
typedef enum {
    PHASE_TITLE,
    PHASE_PLAYING,
    PHASE_LEVEL_COMPLETE,
    PHASE_FREEPLAY,
    PHASE_COUNT
} GAME_PHASE;

/* ── Master game state ────────────────────────────────── */
typedef struct {
    GAME_PHASE phase;
    float      phase_timer;

    int        current_level;
    int        unlocked_count;

    LevelDef   level;
    GrainPool  grains;
    LineBitmap lines;

    int gravity_sign;   /* +1 = down, -1 = up */

    int title_hover;
    int reset_hover;
    int gravity_hover;

    int should_quit;
} GameState;

/* ── Public API ───────────────────────────────────────── */
void game_init(GameState *state, GameBackbuffer *backbuffer);
void prepare_input_frame(GameInput *input);
void game_update(GameState *state, GameInput *input, float delta_time);
void game_render(const GameState *state, GameBackbuffer *backbuffer);

extern LevelDef g_levels[30];

#endif
```

---

## Step 4 — game.c: Init and a Blank Render

Create `course/src/game.c`. For now it only fills the screen with the background color.

```c
#include <string.h>
#include <stdlib.h>
#include "game.h"

/* ── Drawing helpers ──────────────────────────────────── */

static void draw_pixel(GameBackbuffer *bb, int x, int y, uint32_t color) {
    if (x < 0 || x >= bb->width || y < 0 || y >= bb->height) return;
    bb->pixels[y * bb->width + x] = color;
    /*          ^^^^^^^^^^^^^^^^^^^^^^
     * The flat-array index formula.
     * Row y starts at byte offset (y * width).
     * Column x adds x more slots.
     *
     * Example: pixel at (10, 3) in a 640-wide buffer:
     *   index = 3 * 640 + 10 = 1930
     */
}

static void draw_rect(GameBackbuffer *bb,
                      int x, int y, int w, int h,
                      uint32_t color) {
    /* Clamp to screen bounds so we never write out of range */
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > bb->width  ? bb->width  : (x + w);
    int y1 = (y + h) > bb->height ? bb->height : (y + h);

    for (int row = y0; row < y1; row++) {
        for (int col = x0; col < x1; col++) {
            bb->pixels[row * bb->width + col] = color;
        }
    }
}

/* ── Public API ───────────────────────────────────────── */

/*
 * memset(&state, 0, ...) is safe here because:
 *   - All int fields: 0 is a valid "not set" value
 *   - All float fields: 0.0f in IEEE 754 is all-zero bits
 *   - All pointer fields: NULL is all-zero bits on every platform we target
 *   - Enum fields: 0 maps to PHASE_TITLE, GRAIN_WHITE — both correct defaults
 *
 * In JS you'd write: const state = { phase: 'title', grains: [], ... }
 * In C, zeroing the struct gives us the same "blank slate".
 */
void game_init(GameState *state, GameBackbuffer *backbuffer) {
    (void)backbuffer;   /* unused in lesson 01 */
    memset(state, 0, sizeof(*state));
    state->gravity_sign   = 1;       /* gravity pulls down */
    state->unlocked_count = 1;       /* first level unlocked */
}

void prepare_input_frame(GameInput *input) {
    /* Reset transition counts at the start of every frame.
     * We keep ended_down — it remembers if the key is still held.
     * We zero half_transition_count — it's fresh per-frame event data. */
    input->mouse.left.half_transition_count  = 0;
    input->mouse.right.half_transition_count = 0;
    input->escape.half_transition_count      = 0;
    input->reset.half_transition_count       = 0;
    input->gravity.half_transition_count     = 0;
    input->mouse.prev_x = input->mouse.x;
    input->mouse.prev_y = input->mouse.y;
}

void game_update(GameState *state, GameInput *input, float delta_time) {
    (void)delta_time;  /* unused in lesson 01 */

    if (BUTTON_PRESSED(input->escape)) {
        state->should_quit = 1;
    }
}

void game_render(const GameState *state, GameBackbuffer *backbuffer) {
    (void)state;   /* unused in lesson 01 */

    /* Fill every pixel with the background color */
    draw_rect(backbuffer, 0, 0, backbuffer->width, backbuffer->height, COLOR_BG);
}
```

---

## Step 5 — levels.c: Stub for Now

Create `course/src/levels.c`. It must exist so the linker is happy:

```c
#include "game.h"

LevelDef g_levels[30] = { 0 };
```

---

## Step 6 — main_x11.c: The Platform Layer

Create `course/src/main_x11.c`. Read it section by section.

```c
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "platform.h"

/* ── X11 globals ──────────────────────────────────────── */
static Display *g_display;   /* connection to the X server  */
static Window   g_window;    /* our OS window handle        */
static GC       g_gc;        /* graphics context (pen)      */
static XImage  *g_ximage;    /* wraps our pixel buffer      */
static int      g_width;
static int      g_height;
static int      g_should_quit = 0;

/*
 * X11 in one sentence:
 *   XOpenDisplay   → open a socket to the display server
 *   XCreateSimpleWindow → ask the server to make a window
 *   XSelectInput   → say which events we want
 *   XMapWindow     → make it visible
 *   XPending       → non-blocking: any events waiting?
 *   XNextEvent     → pull one event off the queue
 *   XPutImage      → blit our pixel buffer to the window
 */

/* ── platform_init ────────────────────────────────────── */
void platform_init(const char *title, int width, int height) {
    g_width  = width;
    g_height = height;

    g_display = XOpenDisplay(NULL);
    if (!g_display) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    int screen = DefaultScreen(g_display);

    g_window = XCreateSimpleWindow(
        g_display,
        RootWindow(g_display, screen),
        0, 0,              /* x, y position (window manager may ignore) */
        (unsigned)width,
        (unsigned)height,
        0,                 /* border width */
        BlackPixel(g_display, screen),
        BlackPixel(g_display, screen)
    );

    XStoreName(g_display, g_window, title);

    /* Ask for these event types */
    XSelectInput(g_display, g_window,
        ExposureMask       |   /* window uncovered / needs redraw */
        KeyPressMask       |
        KeyReleaseMask     |
        ButtonPressMask    |
        ButtonReleaseMask  |
        PointerMotionMask
    );

    /* Listen for the WM_DELETE_WINDOW message (close button) */
    Atom wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &wm_delete, 1);

    g_gc = XCreateGC(g_display, g_window, 0, NULL);
    XMapWindow(g_display, g_window);   /* make visible */
    XFlush(g_display);
}

/* ── platform_get_time ────────────────────────────────── */
double platform_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
    /*
     * CLOCK_MONOTONIC never jumps backwards.
     * Date.now() in JS can jump (NTP, DST).
     * This is the safe equivalent for frame timing.
     */
}

/* ── platform_get_input ───────────────────────────────── */
/*
 * X11 events come from a queue.
 * XPending() checks if there are any — non-blocking, returns count.
 * XNextEvent() pulls one event and blocks if queue is empty.
 * We drain the queue each frame with the while(XPending) loop.
 */
void platform_get_input(GameInput *input) {
    static Atom wm_delete = 0;
    if (!wm_delete)
        wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);

    while (XPending(g_display)) {
        XEvent ev;
        XNextEvent(g_display, &ev);

        switch (ev.type) {
        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == wm_delete)
                g_should_quit = 1;
            break;

        case KeyPress: {
            KeySym sym = XLookupKeysym(&ev.xkey, 0);
            if (sym == XK_Escape) UPDATE_BUTTON(input->escape,  1);
            if (sym == XK_r)      UPDATE_BUTTON(input->reset,   1);
            if (sym == XK_g)      UPDATE_BUTTON(input->gravity, 1);
            break;
        }
        case KeyRelease: {
            KeySym sym = XLookupKeysym(&ev.xkey, 0);
            if (sym == XK_Escape) UPDATE_BUTTON(input->escape,  0);
            if (sym == XK_r)      UPDATE_BUTTON(input->reset,   0);
            if (sym == XK_g)      UPDATE_BUTTON(input->gravity, 0);
            break;
        }
        case ButtonPress:
            if (ev.xbutton.button == Button1)
                UPDATE_BUTTON(input->mouse.left,  1);
            if (ev.xbutton.button == Button3)
                UPDATE_BUTTON(input->mouse.right, 1);
            break;
        case ButtonRelease:
            if (ev.xbutton.button == Button1)
                UPDATE_BUTTON(input->mouse.left,  0);
            if (ev.xbutton.button == Button3)
                UPDATE_BUTTON(input->mouse.right, 0);
            break;
        case MotionNotify:
            input->mouse.x = ev.xmotion.x;
            input->mouse.y = ev.xmotion.y;
            break;
        }
    }
}

/* ── platform_display_backbuffer ──────────────────────── */
/*
 * XImage wraps our pixel array without copying it.
 * XPutImage blits the XImage to the window via the X server.
 *
 * In JS terms: ctx.putImageData(imageData, 0, 0)
 */
void platform_display_backbuffer(const GameBackbuffer *bb) {
    if (!g_ximage) {
        g_ximage = XCreateImage(
            g_display,
            DefaultVisual(g_display, DefaultScreen(g_display)),
            (unsigned)DefaultDepth(g_display, DefaultScreen(g_display)),
            ZPixmap,    /* pixel format */
            0,          /* offset */
            (char *)bb->pixels,
            (unsigned)bb->width,
            (unsigned)bb->height,
            32,         /* bitmap_pad: 32-bit aligned rows */
            bb->pitch   /* bytes_per_line */
        );
    } else {
        g_ximage->data = (char *)bb->pixels;
    }

    XPutImage(g_display, g_window, g_gc, g_ximage,
              0, 0, 0, 0,
              (unsigned)bb->width,
              (unsigned)bb->height);
    XFlush(g_display);
}

/* ── Stub sound functions ─────────────────────────────── */
void platform_play_sound(int id)  { (void)id; }
void platform_play_music(int id)  { (void)id; }
void platform_stop_music(void)    {}

/* ── main ─────────────────────────────────────────────── */
int main(void) {
    /* 1. Allocate the pixel buffer on the heap.
     *    640 * 480 * 4 bytes = 1,228,800 bytes ≈ 1.2 MB.
     *    The stack is typically 8 MB — fine, but heap is cleaner for large buffers. */
    uint32_t *pixels = calloc((size_t)(CANVAS_W * CANVAS_H), sizeof(uint32_t));
    if (!pixels) { fprintf(stderr, "Out of memory\n"); return 1; }

    GameBackbuffer bb = {
        .pixels = pixels,
        .width  = CANVAS_W,
        .height = CANVAS_H,
        .pitch  = CANVAS_W * (int)sizeof(uint32_t)
    };

    /* 2. Open window */
    platform_init("Sugar, Sugar", CANVAS_W, CANVAS_H);

    /* 3. Init game state */
    GameState state;
    GameInput input;
    memset(&input, 0, sizeof(input));
    game_init(&state, &bb);

    /* 4. Delta-time game loop
     *
     *    prev_time ─────────────────────────── curr_time
     *    |← dt (e.g. 0.016 s at 60 fps) ────→|
     *
     *    dt is the real elapsed seconds since last frame.
     *    We cap it at 0.1 s so a debugger breakpoint doesn't
     *    send grains flying through walls.
     */
    double prev_time = platform_get_time();

    while (!g_should_quit && !state.should_quit) {
        double curr_time = platform_get_time();
        float  dt        = (float)(curr_time - prev_time);
        prev_time        = curr_time;
        if (dt > 0.1f) dt = 0.1f;   /* clamp: max 100 ms per frame */

        prepare_input_frame(&input);
        platform_get_input(&input);
        game_update(&state, &input, dt);
        game_render(&state, &bb);
        platform_display_backbuffer(&bb);
    }

    free(pixels);
    return 0;
}
```

---

## Build & Run

```bash
cd course
clang -Wall -Wextra -O2 -o sugar \
    src/main_x11.c src/game.c src/levels.c \
    -lX11 -lm
./sugar
```

**What you should see:**
A 640×480 window titled "Sugar, Sugar" filled with a warm off-white color. Press Escape or click the window's close button to quit.

---

## Key Concepts

- `uint32_t`: an unsigned 32-bit integer — same as `number` in JS but with a known, exact size.
- `pixels[y * width + x]`: the canonical formula to address a 2D grid stored flat. At `(10, 3)` in a 640-wide buffer the index is `3*640+10 = 1930`.
- `GameBackbuffer`: a struct holding a pointer to the pixel array. The struct is small (16 bytes); the actual pixels live in heap memory allocated with `calloc`.
- `memset(ptr, 0, size)`: writes `size` zero bytes starting at `ptr`. Zeroing a struct is safe when all-zero bits represent a valid initial state (no pointer fields set to garbage, no floats that must start non-zero).
- `delta_time (dt)`: the real number of seconds since the last frame. Multiplying velocity by `dt` makes movement frame-rate-independent. At 60 fps `dt ≈ 0.0167`. At 30 fps `dt ≈ 0.0333`.
- `XPending` + `XNextEvent`: drain the OS event queue without blocking. This is the C equivalent of `addEventListener` in JS — but you pull events rather than having them pushed to callbacks.
- `(void)param`: tells the compiler "I know I'm not using this parameter; don't warn me." Equivalent to `_param` prefix in TypeScript.

---

## Exercise

Change `COLOR_BG` to `GAME_RGB(30, 30, 50)` (dark navy) and rebuild. The window should now open with a dark background. Revert when done — lessons ahead assume the warm off-white background.
