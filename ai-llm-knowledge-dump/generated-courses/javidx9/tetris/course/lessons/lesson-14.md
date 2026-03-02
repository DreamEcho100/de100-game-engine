# Lesson 14 — Final Integration: VSync, Resize, Build Flags, and Polish

## By the end of this lesson you will have:

A production-ready build that:

- Uses VSync for smooth 60fps without busy-waiting
- Correctly resizes the backbuffer viewport when the window changes size
- Compiles with optimizations and warnings enabled
- Passes a Valgrind memory check showing zero leaks

---

## The Final Game Loop

```c
/* main() – pseudocode for both platforms */
platform_init("Tetris", WINDOW_WIDTH, WINDOW_HEIGHT);

GameState state;
GameInput input = {0};
game_init(&state);

TetrisBackbuffer backbuffer = {
    .pixels = malloc(WINDOW_WIDTH * WINDOW_HEIGHT * sizeof(uint32_t)),
    .width  = WINDOW_WIDTH,
    .height = WINDOW_HEIGHT,
};

double prev_time = platform_get_time();

while (1) {
    double curr_time = platform_get_time();
    float  delta_time = (float)(curr_time - prev_time);
    prev_time = curr_time;

    if (delta_time > MAX_DELTA_TIME) delta_time = MAX_DELTA_TIME;

    prepare_input_frame(&input);
    platform_get_input(&input);

    if (input.quit) break;
    if (input.restart && state.game_over) game_init(&state);

    tetris_update(&state, &input, delta_time);
    tetris_render(&state, &backbuffer);
    platform_display_backbuffer(&backbuffer);
}

free(backbuffer.pixels);
```

`MAX_DELTA_TIME = 0.1f` (100ms). Caps delta_time for debugger pauses or system hiccups that would otherwise launch pieces off-screen.

---

## VSync: Theory and X11 Implementation

**Without VSync**, the loop runs as fast as possible — 1000+ fps, burning CPU and causing screen tearing.

**With VSync**, `platform_display_backbuffer` blocks until the monitor's next refresh. This gives ~16.67ms per frame at 60Hz naturally, without a sleep.

### X11 VSync strategy (in `platform_init`/`platform_display_backbuffer`)

```c
static void setup_vsync(Display *display, GLXDrawable drawable) {
    const char *extensions = glXQueryExtensionsString(display,
                                DefaultScreen(display));

    /* Prefer EXT version first */
    if (strstr(extensions, "GLX_EXT_swap_control")) {
        PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT =
            (PFNGLXSWAPINTERVALEXTPROC)
                glXGetProcAddressARB((GLubyte*)"glXSwapIntervalEXT");
        if (glXSwapIntervalEXT)
            glXSwapIntervalEXT(display, drawable, 1);
        return;
    }
    /* Fall back to MESA version */
    if (strstr(extensions, "GLX_MESA_swap_control")) {
        PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA =
            (PFNGLXSWAPINTERVALMESAPROC)
                glXGetProcAddressARB((GLubyte*)"glXSwapIntervalMESA");
        if (glXSwapIntervalMESA)
            glXSwapIntervalMESA(1);
    }
}
```

**Why `glXGetProcAddressARB` instead of a direct call?**  
GLX extension functions aren't guaranteed to be in the linked library. `glXGetProcAddressARB` asks the driver at runtime for the function pointer. If the driver supports it, you get the pointer; if not, you get NULL. Always null-check.

**Fallback:** If neither extension is available, use `nanosleep` to target ~16ms per frame:

```c
struct timespec ts = {0, 16000000L};  /* 16 ms */
nanosleep(&ts, NULL);
```

### Raylib VSync

Raylib handles VSync with one call:

```c
SetTargetFPS(60);
```

This uses the OS scheduler. No manual extension queries needed.

---

## Window Resize Handling (X11)

```c
case ConfigureNotify: {
    int new_w = event.xconfigure.width;
    int new_h = event.xconfigure.height;

    if (new_w != backbuffer.width || new_h != backbuffer.height) {
        free(backbuffer.pixels);
        backbuffer.width  = new_w;
        backbuffer.height = new_h;
        backbuffer.pixels = malloc(new_w * new_h * sizeof(uint32_t));
    }

    glViewport(0, 0, new_w, new_h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, new_w, new_h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    break;
}
```

**`glViewport(0, 0, w, h)`**: tells OpenGL which portion of the window to render to. When the window resizes, the viewport must update or rendering clips to the old size.

**`glOrtho(0, w, h, 0, -1, 1)`**: sets a 2D orthographic projection where (0,0) is top-left and (w,h) is bottom-right. Recompute when window size changes — the projection depends on window dimensions.

**`glOrtho(0, w, h, 0, ...)` vs `glOrtho(0, w, 0, h, ...)`**: note that `bottom=h` and `top=0`. This flips Y so y=0 is the top of the window, matching our "rows grow downward" pixel convention.

---

## Build Flags

### Release build (`build_x11.sh`)

```sh
clang main_x11.c tetris.c \
    -o tetris \
    -O2 -Wall -Wextra -Wpedantic \
    -lX11 -lxkbcommon -lGL -lGLX
```

| Flag                | Purpose                                                          |
| ------------------- | ---------------------------------------------------------------- |
| `-O2`               | Optimize for speed without aggressive size changes               |
| `-Wall`             | Enable common warnings (unused variables, missing returns, etc.) |
| `-Wextra`           | Extra warnings (shadowed variables, sign comparisons)            |
| `-Wpedantic`        | Strict ISO C compliance warnings                                 |
| `-lX11 -lxkbcommon` | X11 and keyboard input libraries                                 |
| `-lGL -lGLX`        | OpenGL and GLX extension libraries                               |

### Debug build (add during development)

```sh
clang main_x11.c tetris.c \
    -o tetris_debug \
    -g -O0 -fsanitize=address,undefined \
    -lX11 -lxkbcommon -lGL -lGLX
```

| Flag                   | Purpose                                                         |
| ---------------------- | --------------------------------------------------------------- |
| `-g`                   | Include debug symbols (source line info in gdb/lldb)            |
| `-O0`                  | No optimization — predictable execution for debugging           |
| `-fsanitize=address`   | AddressSanitizer: detects heap overflows, use-after-free        |
| `-fsanitize=undefined` | UBSan: detects signed overflow, null pointer, misaligned access |

---

## Memory Check with Valgrind

```sh
valgrind --leak-check=full ./tetris
```

Expected output when exiting normally:

```
HEAP SUMMARY:
    in use at exit: 0 bytes in 0 blocks
  total heap usage: 3 allocs, 3 frees, ...

All heap blocks were freed -- no leaks are possible
```

**Two allocations in our game:**

1. `backbuffer.pixels` — `malloc(w * h * 4)` in `main()`, freed before exit
2. X11 internal allocations — freed by `XCloseDisplay`

If Valgrind reports leaks, check:

- Is `free(backbuffer.pixels)` called before the program exits?
- Is `XCloseDisplay(display)` called in the cleanup path?

**New C concept — `XDestroyWindow` and cleanup order:**

```c
/* Cleanup order matters for X11 */
glXMakeCurrent(display, None, NULL);  /* detach context before destroying */
glXDestroyContext(display, glx_ctx);
XDestroyWindow(display, window);
XCloseDisplay(display);
```

Destroying the context before the window is required. Reversing the order causes undefined behavior.

---

## `platform_get_time` Implementations

### X11 (using `clock_gettime`)

```c
double platform_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
```

`CLOCK_MONOTONIC` always increases (never jumps backward for NTP adjustments). Nanosecond resolution, returned as a `double` in seconds.

### Raylib

```c
double platform_get_time(void) {
    return GetTime();  /* Raylib built-in, seconds since window opened */
}
```

---

## What We Built: Architecture Recap

```
┌──────────────────────────────────────────────────────┐
│                   main_x11.c / main_raylib.c         │
│  platform_init → platform_get_input →                │
│  platform_display_backbuffer (VSync here)            │
└──────────────────────────┬───────────────────────────┘
                           │  TetrisBackbuffer *
                           ▼
┌──────────────────────────────────────────────────────┐
│                      tetris.c                        │
│  game_init, tetris_update, tetris_apply_input        │
│  tetris_render, draw_rect, draw_rect_blend           │
│  draw_char, draw_text, draw_cell, draw_piece         │
│  handle_action_with_repeat                           │
└──────────────────────────┬───────────────────────────┘
                           │  GameState, GameInput
                           ▼
┌──────────────────────────────────────────────────────┐
│                      tetris.h                        │
│  TetrisBackbuffer, GameState, GameInput              │
│  GAME_RGB/A, color constants                       │
│  Enums: TETROMINO_BY_IDX, TETROMINO_R_DIR,           │
│         TETRIS_FIELD_CELL, TETROMINO_ROTATE_X_VALUE  │
└──────────────────────────────────────────────────────┘
```

`tetris.c` and `tetris.h` are **platform-independent**. Adding a new platform (SDL3, WebAssembly, Win32) requires:

1. Implement `platform_init`, `platform_get_input`, `platform_get_time`, `platform_display_backbuffer`
2. Link with the new platform's libraries
3. No changes to `tetris.c` or `tetris.h`

---

## Where to Go From Here

| Feature              | Concepts                                                                  |
| -------------------- | ------------------------------------------------------------------------- |
| **Ghost piece**      | Simulate soft-drop to bottom; draw with low alpha using `draw_rect_blend` |
| **Hard drop**        | `Space` key, loop `tetromino_does_piece_fit` downward, lock immediately   |
| **SDL3 backend**     | New `main_sdl3.c`: `SDL_Texture` upload of `backbuffer.pixels`            |
| **WebAssembly**      | Emscripten + `emscripten_set_main_loop`; no blocking                      |
| **7-bag randomizer** | Generate all 7 pieces in a bag, shuffle, deal; refill on empty            |
| **Wall kicks**       | SRS (Super Rotation System): try offset positions when rotation fails     |
| **High score**       | `fopen`/`fwrite` a binary score file; load on startup                     |
| **Sound**            | PulseAudio or miniaudio.h; `game_audio_update` alongside `tetris_update`  |

Each of these is a self-contained addition that requires no changes to the core architecture — demonstrating the value of the platform-independent design you built.

---

## Final Build Test

```sh
cd course/
bash build_x11.sh
./tetris
```

Verify:

- [ ] Window opens at 560×540
- [ ] Piece spawns and falls with delta-time gravity
- [ ] Left/right/down move with DAS (hold then repeat)
- [ ] Z/X rotate left/right (single press, no repeat)
- [ ] Sidebar shows score, level, pieces, next piece preview
- [ ] Controls hint in dark gray at bottom
- [ ] Line clear dims the row, then removes it
- [ ] Game over shows overlay + red border + text
- [ ] R restarts the game; Q exits cleanly

```sh
valgrind --leak-check=full ./tetris
# Quit with Q → expect "0 bytes in use at exit"
```
