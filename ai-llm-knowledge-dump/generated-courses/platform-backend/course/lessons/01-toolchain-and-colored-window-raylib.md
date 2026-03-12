# Lesson 01 — Toolchain and Colored Window (Raylib)

## Overview

| Item | Value |
|------|-------|
| **Backend** | Raylib only |
| **New concepts** | build script, `InitWindow` / `CloseWindow`, `SetTargetFPS` |
| **Observable outcome** | A 800×600 window appears filled with a solid color; closes on Esc |
| **Files created** | `build-dev.sh`, `src/platforms/raylib/main.c` |

---

## What you'll build

A blank 800×600 window that opens, renders a solid dark-blue background every frame, and closes cleanly when you press Esc or the close button.

---

## Background

### JS analogy

Think of `InitWindow` like `document.createElement('canvas')` + `document.body.appendChild(canvas)` + setting up a `requestAnimationFrame` loop — but in one call.

Raylib owns the OS window, the OpenGL context, and the game loop timing.  You just call `EndDrawing()` and it handles VSync sleep internally.

### The build script

The full course always has two backends (Raylib and X11/ALSA).  Rather than a `Makefile`, we use a small shell script:

```sh
./build-dev.sh --backend=raylib        # release build
./build-dev.sh --backend=raylib -r     # build + run immediately
./build-dev.sh --backend=raylib -d     # debug: -O0 -g -DDEBUG -fsanitize=address,undefined
```

The `SOURCES` variable lists every shared `.c` file.  Backend files are appended in the `case "$BACKEND"` block.

---

## What to type

### `build-dev.sh` (full file)

Create this file at the root of `course/`:

```sh
#!/bin/bash
# LESSON 01 — SOURCES variable, --backend flag, -r/-d flags introduced here.
set -e
mkdir -p build

BACKEND="raylib"
RUN_AFTER_BUILD=false
DEBUG=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*) BACKEND="${1#*=}" ;;
        -r|--run)    RUN_AFTER_BUILD=true ;;
        -d|--debug)  DEBUG=true ;;
        *) echo "Unknown: $1" >&2; exit 1 ;;
    esac
    shift
done

# Shared source files (both backends compile these)
# NOTE: Start empty — add each .c file as you create it in later lessons:
#   L03: add src/game/demo.c
#   L04: add src/utils/draw-shapes.c
#   L06: add src/utils/draw-text.c
#   L09/L10: add src/game/audio_demo.c
SOURCES=""

BASE_FLAGS="-Wall -Wextra -DBACKEND=$BACKEND"
if [[ "$DEBUG" == true ]]; then
    FLAGS="$BASE_FLAGS -O0 -g -DDEBUG -fsanitize=address,undefined"
else
    FLAGS="$BASE_FLAGS -O2"
fi

case "$BACKEND" in
    raylib)
        BACKEND_LIBS="-lm -lraylib -lpthread -ldl"
        SOURCES="$SOURCES src/platforms/raylib/main.c"
        ;;
    x11)
        BACKEND_LIBS="-lm -lX11 -lxkbcommon -lasound -lGL -lGLX"
        SOURCES="$SOURCES src/platforms/x11/base.c src/platforms/x11/audio.c src/platforms/x11/main.c"
        ;;
    *) echo "Unknown backend '$BACKEND'" >&2; exit 1 ;;
esac

clang $FLAGS -Isrc -o ./build/game $SOURCES $BACKEND_LIBS
echo "✓ Build complete: ./build/game"

if [[ "$RUN_AFTER_BUILD" == true ]]; then exec ./build/game; fi
```

Run `chmod +x build-dev.sh` to make it executable.

### `src/platforms/raylib/main.c` (L01 state)

```c
/* L01 state: minimal Raylib window.  Headers added progressively each lesson.
 * L03 adds backbuffer.h; L05 replaces the #defines with platform.h;
 * L07 adds game/base.h; L09 adds game/audio_demo.h.                        */
#include <raylib.h>

/* Temporary constants — replaced by platform.h macros in Lesson 05 */
#define GAME_W    800
#define GAME_H    600
#define TARGET_FPS 60

int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(GAME_W, GAME_H, "Platform Foundation");
    SetTargetFPS(TARGET_FPS);
    SetWindowState(FLAG_WINDOW_RESIZABLE);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){20, 20, 30, 255});  /* #14141E dark blue */
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
```

> **Note:** The final `main.c` in the repo is the complete version (L01 through L14 applied).  Each lesson shows the *delta* — what changes or is added.  For L01, use the minimal snippet above which compiles without any files from future lessons.

---

## Explanation

| Line | What it does |
|------|-------------|
| `SetTraceLogLevel(LOG_WARNING)` | Suppresses Raylib's verbose INFO logs |
| `InitWindow(GAME_W, GAME_H, TITLE)` | Opens an 800×600 window titled "Platform Foundation v1.0" |
| `SetTargetFPS(60)` | Tells Raylib to sleep inside `EndDrawing()` to hit 60 FPS |
| `SetWindowState(FLAG_WINDOW_RESIZABLE)` | Allows resize; letterbox (L08) handles the scaling |
| `WindowShouldClose()` | Returns true when Esc is pressed or the × button is clicked |
| `ClearBackground(...)` | Fills the framebuffer with a solid color |

`GAME_W`, `GAME_H`, `TARGET_FPS`, and `TITLE` are defined in `src/platform.h` (lesson 05).  For now you can hard-code them or `#define` them at the top of `main.c`.

---

## Build and run

```sh
./build-dev.sh --backend=raylib -r
```

You should see a 800×600 dark window.  Press Esc to exit.

---

## Quiz

1. Why does `SetTargetFPS(60)` belong here and not in the game logic layer?
2. What happens if you remove `SetWindowState(FLAG_WINDOW_RESIZABLE)`?  What will lesson 08 need to change?
3. The `SOURCES` variable lists shared `.c` files.  Why are the backend-specific files appended separately in the `case "$BACKEND"` block?
