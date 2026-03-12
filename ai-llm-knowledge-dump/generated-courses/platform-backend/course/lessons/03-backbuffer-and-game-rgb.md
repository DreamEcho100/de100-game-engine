# Lesson 03 — Backbuffer and GAME_RGB

## Overview

| Item | Value |
|------|-------|
| **Backend** | Both (Raylib + X11) |
| **New concepts** | `Backbuffer` struct + pitch, `GAME_RGB`/`GAME_RGBA` macro, GPU texture upload |
| **Observable outcome** | Canvas fills with a solid color; colored rectangle visible |
| **Files created** | `src/utils/backbuffer.h`, `src/game/demo.c`, `src/game/demo.h` |
| **Files modified** | `src/platforms/raylib/main.c`, `src/platforms/x11/main.c` |

---

## What you'll build

A CPU pixel array (`Backbuffer`) that both backends upload to the GPU each frame.  The canvas renders a solid background color.

---

## Background

### JS analogy

A `Backbuffer` is like a `new Uint8ClampedArray(width * height * 4)` — raw RGBA pixels in a flat array, exactly what `new ImageData(...)` uses.  Instead of `ctx.putImageData(...)`, we call `glTexImage2D` (X11) or `UpdateTexture` (Raylib).

### The `Backbuffer` struct

```c
typedef struct {
    uint32_t *pixels;      // Flat pixel array
    int       width;       // Canvas width (GAME_W)
    int       height;      // Canvas height (GAME_H)
    int       pitch;       // Bytes per row = width * bytes_per_pixel
    int       bytes_per_pixel;
} Backbuffer;
```

`pitch` (bytes per row) is the safe way to advance to the next row.  On some platforms the driver may pad rows to 4- or 16-byte boundaries; using `pitch` instead of `width * 4` is future-proof.

### `GAME_RGB` / `GAME_RGBA` — 0xAARRGGBB

```c
#define GAME_RGBA(r, g, b, a) \
    ( ((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
      ((uint32_t)(g) <<  8) |  (uint32_t)(r) )
#define GAME_RGB(r, g, b) GAME_RGBA(r, g, b, 255)
```

On a little-endian CPU, the bytes in memory are `[R, G, B, A]` from lowest to highest address.  The integer value is `0xAARRGGBB`.

### Pixel format — why `GL_RGBA` on both platforms

| Backend | Format constant | In-memory read order |
|---------|----------------|----------------------|
| X11/GLX | `GL_RGBA` | Reads `[R,G,B,A]` bytes — matches our memory layout exactly |
| Raylib | `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` | Reads `[R,G,B,A]` bytes — same layout |

**Both backends use the same format — no swap needed.**

Our `GAME_RGB(r,g,b)` stores bytes in memory as `[R][G][B][A]` (R at the lowest address).  `GL_RGBA` tells the driver "byte[0] = Red, byte[1] = Green, byte[2] = Blue, byte[3] = Alpha" — which is exactly right.

`GL_BGRA` would tell the driver "byte[0] = Blue, byte[1] = Green, byte[2] = Red" — swapping R↔B, so `COLOR_RED = GAME_RGB(220,50,50)` would display as blue.  This is a common mistake: **do not use `GL_BGRA` with our layout.**

### `game/demo.c` — shared demo rendering

During the platform course (lessons 03–13), a `demo_render()` function in `game/demo.c` produces the visible output.  Both backends call this single function so rendering logic isn't written twice.

**Lesson 14** strips this file and leaves blank stubs where game courses add `game_render()`.

---

## What to type

### `src/utils/backbuffer.h` (new)

```c
#ifndef UTILS_BACKBUFFER_H
#define UTILS_BACKBUFFER_H
#include <stdint.h>

typedef struct {
    uint32_t *pixels;
    int       width;
    int       height;
    int       pitch;
    int       bytes_per_pixel;
} Backbuffer;

#define GAME_RGBA(r, g, b, a) \
    ( ((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
      ((uint32_t)(g) <<  8) |  (uint32_t)(r) )
#define GAME_RGB(r, g, b) GAME_RGBA(r, g, b, 255)

#define COLOR_BG    GAME_RGB( 20,  20,  30)
#define COLOR_WHITE GAME_RGB(255, 255, 255)
#define COLOR_RED   GAME_RGB(220,  50,  50)
#define COLOR_GREEN GAME_RGB( 50, 205,  50)
/* add more colors as needed */
#endif
```

### `src/game/demo.c` (new, L03 state)

```c
#include "utils/backbuffer.h"
#include "utils/draw-shapes.h"
#include "game/base.h"
#include <stdio.h>

void demo_render(Backbuffer *bb, GameInput *input, int fps) {
    /* LESSON 03 — fill background */
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BG);

    /* LESSON 04 — rectangles added here */
    /* LESSON 06 — text label added here */
    /* LESSON 08 — FPS counter added here */
}
```

### Raylib `main.c` — texture creation (add in `main()` after `InitWindow`)

```c
// Allocate backbuffer
Backbuffer bb = { .width=GAME_W, .height=GAME_H,
                  .bytes_per_pixel=4, .pitch=GAME_W*4 };
bb.pixels = malloc(GAME_W * GAME_H * 4);
memset(bb.pixels, 0, GAME_W * GAME_H * 4);

// Create GPU texture — PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 matches our [R,G,B,A] layout
Image img = { .data=bb.pixels, .width=GAME_W, .height=GAME_H,
              .mipmaps=1, .format=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
Texture2D texture = LoadTextureFromImage(img);
SetTextureFilter(texture, TEXTURE_FILTER_POINT);
```

Inside the loop, replace `ClearBackground(...)` with:

```c
demo_render(&bb, curr_input, GetFPS());

UpdateTexture(texture, bb.pixels);
BeginDrawing();
ClearBackground(BLACK);
DrawTexture(texture, 0, 0, WHITE);   // L08: replaced by DrawTexturePro
EndDrawing();
```

### X11 `main.c` — texture setup (add to `init_gl`, after glTexParameteri calls)

```c
// Allocate the GPU texture once at startup (NULL = no pixel data yet).
// GL_RGBA: our GAME_RGB stores bytes in memory as [R,G,B,A].
// GL_RGBA tells the driver to read [R,G,B,A] — matches exactly.
// IMPORTANT: do NOT use GL_BGRA here; that would swap R↔B channels.
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
             GAME_W, GAME_H, 0,
             GL_RGBA, GL_UNSIGNED_BYTE,
             NULL);
```

### X11 `main.c` — texture upload (add to `display_backbuffer`)

```c
static void display_backbuffer(Backbuffer *bb) {
    glClear(GL_COLOR_BUFFER_BIT);
    glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
    // glTexSubImage2D: updates pixels in the already-allocated texture (no re-alloc).
    // Use glTexSubImage2D every frame (not glTexImage2D) for correct performance.
    // GL_RGBA: matches our [R,G,B,A] in-memory byte order.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    bb->width, bb->height,
                    GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);
    // Full-screen quad (L08: replaced by letterbox quad)
    glBegin(GL_QUADS);
        glTexCoord2f(0,0); glVertex2f(0,         0);
        glTexCoord2f(1,0); glVertex2f(GAME_W,    0);
        glTexCoord2f(1,1); glVertex2f(GAME_W, GAME_H);
        glTexCoord2f(0,1); glVertex2f(0,      GAME_H);
    glEnd();
    glXSwapBuffers(g_x11.display, g_x11.window);
}
```

---

## Explanation

| Concept | Detail |
|---------|--------|
| `pitch = width * 4` | Bytes per row; use in row-advance arithmetic: `pixels + row * (pitch/4) + col` |
| `GL_RGBA` (X11) | Correct format: reads bytes as [R,G,B,A] — matches our `GAME_RGB` memory layout |
| `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` (Raylib) | Reads bytes as [R,G,B,A] — same layout as GL_RGBA; both backends use the same format |
| `glTexImage2D` (allocate) | Call once in `init_gl` with `NULL` pixels to reserve GPU memory for `GAME_W×GAME_H` |
| `glTexSubImage2D` (update) | Call every frame in `display_backbuffer`; updates pixels in the already-allocated texture without re-allocating GPU memory |
| `UpdateTexture(texture, pixels)` | Raylib's equivalent of `glTexSubImage2D` |

---

## Build and run

```sh
./build-dev.sh --backend=raylib -r
./build-dev.sh --backend=x11 -r
```

Both windows should show a solid dark-blue canvas.

---

## Quiz

1. What is `pitch` and why is it `width * bytes_per_pixel` in our case?  When might it differ?
2. Both `GL_RGBA` and `GL_BGRA` exist.  With our `GAME_RGB` layout, which is correct and why?  What would you see if you accidentally used the wrong one?
3. `LoadTextureFromImage` + `UpdateTexture` vs `glTexImage2D`: which re-allocates GPU memory every frame?  Which is preferred in the inner loop?
