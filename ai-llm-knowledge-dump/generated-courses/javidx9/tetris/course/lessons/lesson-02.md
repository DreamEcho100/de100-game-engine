# Lesson 02 — TetrisBackbuffer: The Platform-Independent Canvas

## By the end of this lesson you will have:

A `TetrisBackbuffer` struct allocated in `main()`, filled with a solid color by a tiny loop, and displayed on screen. The game layer owns the *drawing*; the platform layer owns the *buffer lifetime* and *display*.

---

## The Core Idea: Separate Drawing from Display

In the original architecture, each platform backend drew directly:
- X11 called `XFillRectangle`, `XSetForeground`
- Raylib called `DrawRectangle`, `DrawText`

If you wanted to add a new platform (SDL, WebAssembly, Windows), you'd have to duplicate all the drawing logic.

The new architecture inverts this:

```
OLD: platform draws game   →  two different drawing implementations
NEW: game draws to buffer  →  platforms just "blit" the buffer
```

The `TetrisBackbuffer` is the shared canvas. It's just a plain array of pixels. Any platform can display an array of pixels.

---

## Step 1 — Add `TetrisBackbuffer` to `tetris.h`

Open `src/tetris.h` and add this near the top (after the `#include` guards):

```c
#ifndef TETRIS_H
#define TETRIS_H

#include <stdint.h>

/* ── Backbuffer ──────────────────────────────────────────────────── */

typedef struct {
    uint32_t *pixels;  /* RGBA pixel data, 0xAARRGGBB format */
    int       width;
    int       height;
    int       pitch;   /* bytes per row — usually width * 4 */
} TetrisBackbuffer;

#endif
```

**New C concept — `uint32_t`:**  
`uint32_t` is an unsigned integer guaranteed to be exactly 32 bits wide, defined in `<stdint.h>`. A pixel has four 8-bit channels: Alpha, Red, Green, Blue. Packed into one 32-bit integer: `0xAARRGGBB`. Using `uint32_t` means one write per pixel instead of four separate byte writes.

**Why `pitch`?**  
`pitch` is bytes per row. For a tightly-packed buffer `pitch == width * 4`. Some image libraries (and older GPUs) may pad rows to alignment boundaries — having `pitch` in the struct lets you handle both cases correctly without changing calling code.

**Ownership model:**
- `main()` **allocates** and **frees** the pixel array — it owns the memory
- `tetris_render()` **writes** into it — it borrows the buffer
- The platform's display function **reads** it — it also borrows

---

## Step 2 — Allocate the backbuffer in `main_x11.c`

In `main()`, after `platform_init`:

```c
int main(void) {
    int screen_width  = 560;
    int screen_height = 540;

    if (platform_init("Tetris", screen_width, screen_height) != 0)
        return 1;

    /* ── Allocate backbuffer ── */
    TetrisBackbuffer backbuffer;
    backbuffer.width  = screen_width;
    backbuffer.height = screen_height;
    backbuffer.pitch  = screen_width * sizeof(uint32_t);
    backbuffer.pixels =
        (uint32_t *)malloc(screen_width * screen_height * sizeof(uint32_t));

    if (!backbuffer.pixels) {
        fprintf(stderr, "Failed to allocate backbuffer\n");
        platform_shutdown();
        return 1;
    }
    /* ... game loop ... */

    free(backbuffer.pixels);
    platform_shutdown();
    return 0;
}
```

**New C concept — `malloc` and `free`:**  
`malloc(n)` allocates `n` bytes from the heap and returns a pointer. `free(ptr)` releases it. They must be balanced — one `free` per `malloc`. The check `if (!backbuffer.pixels)` handles the rare case where the system runs out of memory.

`(uint32_t *)` is a **cast** — it tells the compiler to treat the `void *` that `malloc` returns as a `uint32_t *`.

The size calculation: `screen_width * screen_height * sizeof(uint32_t)` = 560 × 540 × 4 = **1,209,600 bytes** (~1.2 MB). Tiny compared to modern RAM.

---

## Step 3 — Write a test fill

Before wiring up the display, prove the buffer works by filling it:

```c
/* Fill the entire backbuffer with a solid color (dark blue) */
uint32_t test_color = 0xFF001040;  /* 0xAARRGGBB: opaque, R=0, G=16, B=64 */
for (int i = 0; i < backbuffer.width * backbuffer.height; i++) {
    backbuffer.pixels[i] = test_color;
}
```

**Pixel format breakdown:**

```
0x FF  00  10  40
   ┃   ┃   ┃   └── Blue  = 0x40 = 64
   ┃   ┃   └────── Green = 0x10 = 16
   ┃   └────────── Red   = 0x00 = 0
   └────────────── Alpha = 0xFF = 255 (fully opaque)
```

---

## Step 4 — Display the backbuffer via OpenGL

Add a static display helper in `main_x11.c`:

```c
static void platform_display_backbuffer(TetrisBackbuffer *bb) {
    glClear(GL_COLOR_BUFFER_BIT);

    /* Upload backbuffer pixels to the GPU texture */
    glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 bb->width, bb->height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);

    /* Draw a full-screen quad with the texture */
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0,          0);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(bb->width,  0);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(bb->width,  bb->height);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0,          bb->height);
    glEnd();

    glXSwapBuffers(g_x11.display, g_x11.window);
}
```

**How this maps pixels to the screen:**
1. `glTexImage2D` uploads `bb->pixels` as RGBA bytes to the GPU texture
2. A quad covers the full window (`0,0` to `width,height`)
3. Texture coordinates `(0,0)...(1,1)` map the entire texture onto the quad
4. `glXSwapBuffers` flips the front and back buffers (double buffering)

`GL_UNSIGNED_BYTE` matches our `uint32_t` layout: four bytes RGBA in order. `GL_NEAREST` filtering (set in `platform_init`) means no interpolation — pixels are crisp.

---

## Step 5 — Wire it into the game loop

```c
while (g_is_running) {
    double current_time = get_time();
    float  delta_time   = (float)(current_time - g_last_frame_time);
    g_last_frame_time   = current_time;

    /* Test fill — replace with tetris_render() later */
    for (int i = 0; i < backbuffer.width * backbuffer.height; i++)
        backbuffer.pixels[i] = 0xFF001040;

    platform_display_backbuffer(&backbuffer);

    if (!g_x11.vsync_enabled) {
        double elapsed = get_time() - current_time;
        if (elapsed < g_target_frame_time)
            sleep_ms((int)((g_target_frame_time - elapsed) * 1000.0));
    }
}
```

Build and run — you should see a dark blue window at 60 FPS.

---

## Step 6 — Same pattern in Raylib

```c
int main(void) {
    int screen_width  = 560;
    int screen_height = 540;

    InitWindow(screen_width, screen_height, "Tetris");
    SetTargetFPS(60);

    /* Allocate backbuffer */
    TetrisBackbuffer backbuffer;
    backbuffer.width  = screen_width;
    backbuffer.height = screen_height;
    backbuffer.pitch  = screen_width * sizeof(uint32_t);
    backbuffer.pixels =
        (uint32_t *)malloc(screen_width * screen_height * sizeof(uint32_t));

    /* Create Raylib texture from the backbuffer */
    Image img = {
        .data    = backbuffer.pixels,
        .width   = screen_width,
        .height  = screen_height,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    Texture2D texture = LoadTextureFromImage(img);

    while (!WindowShouldClose()) {
        /* Test fill */
        for (int i = 0; i < backbuffer.width * backbuffer.height; i++)
            backbuffer.pixels[i] = 0xFF001040;

        /* Upload and display */
        UpdateTexture(texture, backbuffer.pixels);
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexture(texture, 0, 0, WHITE);
        EndDrawing();
    }

    UnloadTexture(texture);
    free(backbuffer.pixels);
    CloseWindow();
    return 0;
}
```

**Raylib pixel format:** `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` matches our `0xAARRGGBB` layout — Raylib reads the same bytes in the same order. The `WHITE` tint in `DrawTexture` means "no tint" (multiplied by 1.0 per channel).

---

## Mental Model

```
CPU:  backbuffer.pixels = malloc(w * h * 4)
         ↓ write pixels
      pixels[y * w + x] = 0xFFRRGGBB

GPU:  glTexImage2D → texture
         ↓
      textured quad → framebuffer
         ↓
      glXSwapBuffers → screen
```

The key insight: **writing a pixel is just writing one integer into an array**. No API call, no color lookup, no context. Just:

```c
backbuffer.pixels[y * backbuffer.width + x] = color;
```

This is exactly how `draw_rect` works in Lesson 08.

---

## Key Concepts

- `TetrisBackbuffer` is a plain struct: a pointer + dimensions
- `malloc` in `main()` owns the memory; `free` before exit
- One `uint32_t` per pixel — four bytes: alpha, red, green, blue
- `pitch = width * 4` — bytes per row, not pixels per row
- X11 path: `glTexImage2D` uploads, quad draws, `glXSwapBuffers` presents
- Raylib path: `UpdateTexture`, then `DrawTexture` with `WHITE` tint

---

## Exercise

Modify the test fill to draw a horizontal gradient: column 0 is black (`0xFF000000`) and column `width-1` is red (`0xFF0000FF` in ABGR... wait — check the byte order). What value makes a pure red pixel in `0xAARRGGBB` format?

Answer: `0xFFFF0000` — Alpha=255, R=255, G=0, B=0.
