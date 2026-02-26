# Lesson 02 — SnakeBackbuffer: Platform-Independent Canvas

## By the end of this lesson you will have:

A `uint32_t` pixel buffer allocated on the heap, cleared to solid green each frame, and displayed fullscreen in the window via an OpenGL texture upload — the complete backbuffer pipeline running end-to-end.

---

## The Problem with Direct Drawing

In the old snake course, `platform_render` called X11 functions (`XFillRectangle`) or Raylib functions (`DrawRectangle`) directly. That means:
- Game code depends on the platform
- Every new backend must reimplement the entire draw loop
- You can't add a terminal backend, WASM backend, or screenshot feature

The backbuffer approach inverts this: **the game draws into a plain `uint32_t` array; the platform just copies that array to the screen.** Adding a new backend requires zero changes to game code.

---

## Step 1 — `SnakeBackbuffer` struct

In `snake.h`:

```c
typedef struct {
    uint32_t *pixels; /* 0xAARRGGBB packed pixel data */
    int       width;
    int       height;
} SnakeBackbuffer;
```

`uint32_t` — a 32-bit unsigned integer. Each pixel is one `uint32_t`, encoding alpha, red, green, blue in one 4-byte value. We'll cover the exact packing in Lesson 03.

**New C concept — `uint32_t`:**
`int` is platform-size (could be 16, 32, or 64 bits). `uint32_t` from `<stdint.h>` is always exactly 32 bits. For pixel math, exact sizes matter.

```js
// JS equivalent: typed arrays are the closest analogy
const pixels = new Uint32Array(width * height);
```

---

## Step 2 — Allocate in `main()`

```c
SnakeBackbuffer backbuffer;
backbuffer.width  = WINDOW_WIDTH;
backbuffer.height = WINDOW_HEIGHT;
backbuffer.pixels = (uint32_t *)malloc(
    WINDOW_WIDTH * WINDOW_HEIGHT * sizeof(uint32_t));

if (!backbuffer.pixels) {
    fprintf(stderr, "Failed to allocate backbuffer\n");
    return 1;
}
```

**New C concept — `malloc`:**
`malloc(n)` allocates `n` bytes on the heap and returns a pointer. Like `new` in JS, but no garbage collector — you must call `free()` yourself. Always check for `NULL` return (allocation failed).

`sizeof(uint32_t)` = 4 bytes. Total: `1200 × 460 × 4 = 2,208,000 bytes` (~2.1 MB).

At the end of `main()`:
```c
free(backbuffer.pixels);
```

---

## Step 3 — Upload to GPU via `platform_display_backbuffer`

The X11 backend uploads the backbuffer pixels to a GPU texture each frame:

```c
static void platform_display_backbuffer(const SnakeBackbuffer *bb) {
    glClear(GL_COLOR_BUFFER_BIT);

    /* Upload CPU pixels → GPU texture */
    glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 bb->width, bb->height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);

    /* Draw fullscreen quad with texture */
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0,         0);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(bb->width, 0);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(bb->width, bb->height);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0,         bb->height);
    glEnd();

    glXSwapBuffers(g_x11.display, g_x11.window);
}
```

**`glTexImage2D`** — uploads `bb->pixels` from CPU memory to the GPU texture slot created in Lesson 01. Each call replaces the entire texture with the new frame's pixels.

**`GL_QUADS`** — a fullscreen quad is two triangles that together cover the entire window. Four vertices, each paired with a texture coordinate:
```
(0,0) top-left  → texcoord (0,0) = first pixel in buffer
(w,0) top-right → texcoord (1,0) = last pixel in first row
(w,h) bot-right → texcoord (1,1) = last pixel in buffer
(0,h) bot-left  → texcoord (0,1) = first pixel in last row
```

### Raylib equivalent

```c
UpdateTexture(texture, backbuffer.pixels);  /* upload */
BeginDrawing();
ClearBackground(BLACK);
DrawTexture(texture, 0, 0, WHITE);          /* display */
EndDrawing();
```

Raylib wraps the same `glTexImage2D` + draw call under a simpler API.

---

## Step 4 — Fill test: clear to green

Before calling `snake_render`, fill the buffer manually to verify the pipeline:

```c
/* Temporary: fill entire backbuffer with green */
for (int i = 0; i < backbuffer.width * backbuffer.height; i++)
    backbuffer.pixels[i] = 0xFF00FF00;  /* 0xAARRGGBB: green, fully opaque */
```

If you see a solid green window, the pipeline works:
CPU pixels → GPU texture → fullscreen quad → screen.

---

## Step 5 — Pixel addressing

The backbuffer is a flat 1D array representing a 2D grid.

```
pixel at (col, row) = pixels[row * width + col]
```

Example: window is 10 pixels wide.
```
row=0: pixels[0], pixels[1], ..., pixels[9]   (top row)
row=1: pixels[10], pixels[11], ..., pixels[19]
row=2: pixels[20], ...
```

For (col=3, row=2, width=10): `2 * 10 + 3 = 23`.

**New C concept — 2D array as flat 1D:**
C doesn't have native 2D dynamic arrays. A 2D grid of size W×H is stored as W×H elements in a single 1D array. The "row * width + col" formula converts 2D coordinates to a flat index. This is the pattern behind every image, texture, and game grid you'll encounter.

---

## Key Concepts

- `SnakeBackbuffer`: `uint32_t *pixels`, `width`, `height` — the game's canvas
- `malloc(w * h * sizeof(uint32_t))` — allocate on heap; `free()` when done
- `glTexImage2D` — upload CPU pixels to GPU each frame
- `GL_QUADS` fullscreen quad — maps texture to window
- `pixel[row * width + col]` — flat index for 2D coordinates
- Backbuffer pipeline: game writes pixels → platform uploads → screen shows them

---

## Exercise

The backbuffer is currently one flat array. What if you wanted to support a "scanline effect" — drawing every other row slightly darker? Write a loop that, after filling the backbuffer with a solid color, darkens every even row by halving each pixel's RGB values. (Hint: you'll need to extract R, G, B from the `uint32_t` — the next lesson shows how.)
