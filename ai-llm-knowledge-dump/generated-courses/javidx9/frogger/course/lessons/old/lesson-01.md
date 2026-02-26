# Lesson 1 — Open a Window

**By the end of this lesson you will have:**
A 1024×640 black window that opens, stays open, and closes cleanly when you press
Escape or click ✕. Nothing else — just the window. This is your "Hello, World".

---

## Why start here?

Every game is a loop: open window → update → draw → repeat → close window.
Before we can draw sprites or move a frog, we need that loop running.

In JavaScript you'd open a canvas:
```js
const canvas = document.getElementById('game');
const ctx = canvas.getContext('2d');
```

In C with our setup, we separate "how to open a window" (platform code) from
"what the game does" (game code). This lesson wires up that separation.

---

## Step 1 — Understand the file structure

Open a terminal in the `course/` directory. You'll see:

```
course/
├── src/
│   ├── platform.h       ← the contract: 6 functions both backends must provide
│   ├── frogger.h        ← game types and constants
│   ├── frogger.c        ← game logic (calls platform_* functions)
│   ├── main_x11.c       ← X11 backend: implements platform functions + main()
│   └── main_raylib.c    ← Raylib backend: same contract, different code
├── assets/              ← sprite files
├── build_x11.sh
└── build_raylib.sh
```

**New C concept — #include:**
`#include "file.h"` is like `import` in JS — it pastes the contents of that file
here. The `.h` files declare *what* exists; the `.c` files define *how* it works.

---

## Step 2 — Read `platform.h`

Open `src/platform.h`. It declares 6 functions:

```c
void platform_init(int width, int height, const char *title);
void platform_get_input(InputState *input);
void platform_render(const GameState *state);
void platform_sleep_ms(int ms);
int  platform_should_quit(void);
void platform_shutdown(void);
```

**New C concept — function declarations vs definitions:**
This file only *declares* (says these functions exist). The definitions (actual
code) live in `main_x11.c` or `main_raylib.c`. When you compile, you pick one.

JS analogy: like a TypeScript interface — you declare the shape, then implement it.

**New C concept — `const char *title`:**
A pointer to a read-only string. Like `string` in JS, but C strings are just arrays
of characters ending in a `\0` (null byte). `const` means we promise not to modify it.

---

## Step 3 — Read the game loop in `frogger.c`

Open `src/frogger.c` and find `frogger_run()`:

```c
void frogger_run(const char *assets_dir) {
    GameState state;
    frogger_init(&state, assets_dir);    /* set up game data    */

    platform_init(SCREEN_PX_W, SCREEN_PX_H, "Frogger");  /* open window */

    InputState input = {0};             /* zero all fields     */
    struct timespec prev, now;
    clock_gettime(CLOCK_MONOTONIC, &prev);

    while (!platform_should_quit()) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        float dt = (float)(now.tv_sec  - prev.tv_sec) +
                   (float)(now.tv_nsec - prev.tv_nsec) * 1e-9f;
        prev = now;

        platform_get_input(&input);         /* what keys are pressed? */
        frogger_tick(&state, &input, dt);   /* update game logic      */
        platform_render(&state);            /* draw everything        */
        platform_sleep_ms(16);              /* ~60 FPS cap            */
    }

    platform_shutdown();
}
```

**New C concept — `struct timespec` and `clock_gettime`:**
This gives us high-precision time (nanoseconds). `dt` is delta-time — seconds
since the last frame. We use it to make movement frame-rate independent.
JS analogy: `performance.now()` gives milliseconds; this gives seconds directly.

**New C concept — `while (!condition)`:**
The game loop. Runs until `platform_should_quit()` returns non-zero.
In Raylib: `WindowShouldClose()` returns 1 when you press Escape or ✕.
In X11: we track a `g_quit` flag and set it on `WM_DELETE_WINDOW` events.

**New C concept — `float dt`:**
`float` is a 32-bit floating-point number. Like JS `number` but explicitly 32-bit.
We want dt to be a fraction of a second (like 0.016 for 60 FPS), so `float` is fine.

---

## Step 4 — Look at how Raylib opens a window

Open `src/main_raylib.c` and find `platform_init`:

```c
void platform_init(int width, int height, const char *title) {
    InitWindow(width, height, title);
    SetTargetFPS(60);
}
```

That's it. Raylib hides all the complexity.

**Compare to X11** (`src/main_x11.c`):

```c
void platform_init(int width, int height, const char *title) {
    g_display = XOpenDisplay(NULL);           /* connect to X server  */
    int screen = DefaultScreen(g_display);
    Window root = RootWindow(g_display, screen);
    g_window = XCreateSimpleWindow(           /* create the window    */
        g_display, root, 0, 0, width, height, 0, 0, 0);
    XSelectInput(g_display, g_window,         /* subscribe to events  */
        KeyPressMask | KeyReleaseMask | ExposureMask);
    g_wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &g_wm_delete, 1);
    XStoreName(g_display, g_window, title);
    XMapWindow(g_display, g_window);          /* make it visible      */
    g_gc = XCreateGC(g_display, g_window, 0, NULL);
    XFlush(g_display);
}
```

Raylib does all of this internally. The X11 version teaches you what's really
happening: connecting to a display server, registering for events, creating a
graphics context.

**New C concept — `static` at file scope:**
`static Display *g_display = NULL;` means `g_display` is only visible inside
`main_x11.c`. It's like a module-level `let` in JS that can't be imported.
The `g_` prefix is a naming convention meaning "global to this file".

---

## Build & Run

**Raylib backend:**
```sh
cd course/
./build_raylib.sh
./frogger_raylib
```

**X11 backend:**
```sh
cd course/
./build_x11.sh
./frogger_x11
```

**What you should see:**
A black 1024×640 window titled "Frogger". Press Escape or click ✕ to close.
Nothing else is drawn yet — we haven't written the render code.

**If you see a compile error:**
- `cannot open sprite` — make sure you're running from the `course/` directory,
  not from inside `src/`
- `cannot open X display` — you need a running X server (`echo $DISPLAY`)
- Raylib not found — install with `sudo apt install libraylib-dev`

---

## Mental Model

The game loop is like `setInterval(() => { update(); draw(); }, 16)` in JS,
except we measure real time (dt) instead of trusting the interval to be exact.
The platform functions are the C equivalent of the browser APIs — they hide OS
details so game code stays the same regardless of whether we use X11 or Raylib.

---

## Exercise

In `main_x11.c` (or `main_raylib.c`), change the window title from `"Frogger"`
to your name. Rebuild and confirm the title bar shows your change.

Then: add `printf("Frame!\n");` inside the while loop in `frogger_run()` and
watch the terminal flood with "Frame!" at ~60 FPS. Remove it after — you've just
verified the loop is running.
