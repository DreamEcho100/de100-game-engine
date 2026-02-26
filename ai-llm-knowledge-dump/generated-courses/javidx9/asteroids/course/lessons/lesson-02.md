# Lesson 02 — AsteroidsBackbuffer + draw_pixel_w

## By the end of this lesson you will have:
The game renders its first colored pixel. You will see a single white dot appear at position (100, 200) on the black screen. More importantly, you will understand *exactly* what `uint32_t` color values mean in memory and how the pixel array maps to screen positions.

---

## Step 1 — The pixel array: what it is and why

A framebuffer is just a flat array of integers — one integer per pixel. For an 800×600 screen that is 480,000 integers. Each `uint32_t` stores one pixel's color as four bytes packed together.

```c
typedef struct {
    uint32_t *pixels; /* flat array: pixels[y * width + x] */
    int       width;
    int       height;
} AsteroidsBackbuffer;
```

**JS analogy:** This is exactly like a `Uint32Array` backing a `canvas` 2D context's `ImageData`. If you have ever done:

```js
const imageData = ctx.getImageData(0, 0, 800, 600);
const data = new Uint32Array(imageData.data.buffer);
data[y * 800 + x] = 0xFF0000FF; // set pixel
```

...you were doing the same thing. `AsteroidsBackbuffer` is that `Uint32Array`.

**Why a flat 1D array instead of a 2D array?** Hardware and cache lines. A 2D array `uint32_t pixels[600][800]` would work, but passing it around requires knowing both dimensions at compile time. A flat array `pixels[y * width + x]` is more flexible and maps directly to how the GPU expects data in `glTexImage2D`.

---

## Step 2 — Why `malloc` in `main`, not in game code

In `main_x11.c`, the backbuffer is allocated like this:

```c
bb.width  = SCREEN_W;
bb.height = SCREEN_H;
bb.pixels = (uint32_t *)malloc((size_t)(bb.width * bb.height) * sizeof(uint32_t));
if (!bb.pixels) { fprintf(stderr, "FATAL: out of memory\n"); return 1; }
```

The game code (`asteroids.c`, `asteroids_render`) receives a *pointer* to the already-allocated backbuffer. It never calls `malloc` or `free`.

**Why?** Separation of concerns. The game logic does not know (and should not care) how memory was obtained — whether from `malloc`, from a stack allocation, from a custom arena, or from a memory-mapped file. The platform (`main_x11.c`, `main_raylib.c`) owns the memory. The game just writes into it.

This is the same reason you don't call `document.createElement('canvas')` inside a React component's render function — you pass the ref down.

**The `(size_t)` cast matters here:**  
`bb.width * bb.height` is `int * int = int` = 800 × 600 = 480,000. That fits in an `int`, but to be safe against wider screens, we cast to `size_t` before multiplying by `sizeof(uint32_t)` (4 bytes), giving 1,920,000 bytes (~1.83 MB).

---

## Step 3 — How GL_RGBA reads the bytes in memory

This is the trickiest part. We use this macro to build colors:

```c
#define ASTEROIDS_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
     ((uint32_t)(g) <<  8) |  (uint32_t)(r))
```

Let's work through a concrete example. `COLOR_WHITE` is `ASTEROIDS_RGB(255, 255, 255)` which expands to `ASTEROIDS_RGBA(255, 255, 255, 0xFF)`:

```
r=255, g=255, b=255, a=255

uint32_t value = (255 << 24) | (255 << 16) | (255 << 8) | 255
               = 0xFF000000  | 0x00FF0000  | 0x0000FF00 | 0x000000FF
               = 0xFFFFFFFF
```

On a little-endian x86 CPU (every modern desktop), that `uint32_t` is stored in memory with the **least significant byte first**:

```
Address:  [base+0]  [base+1]  [base+2]  [base+3]
Byte:        0xFF     0xFF      0xFF      0xFF
Meaning:      R        G         B         A
```

OpenGL's `GL_RGBA` format reads those four bytes in order as R, G, B, A — which is exactly what we stored. No byte-swapping needed.

**Another example — `COLOR_YELLOW = ASTEROIDS_RGB(255, 255, 0)`:**

```
r=255, g=255, b=0, a=255

uint32_t = (255 << 24) | (0 << 16) | (255 << 8) | 255
         = 0xFF00FFFF

Memory bytes: [0xFF] [0xFF] [0x00] [0xFF]
               R=255  G=255  B=0    A=255   → yellow ✓
```

**JS analogy:** The canvas `ImageData` stores pixels as `[R, G, B, A]` bytes in a `Uint8ClampedArray`. This backbuffer does the same thing, just packed into one `uint32_t` per pixel.

---

## Step 4 — draw_pixel_w: writing one pixel with toroidal wrapping

```c
static void draw_pixel_w(AsteroidsBackbuffer *bb, int x, int y, uint32_t color) {
    x = ((x % bb->width)  + bb->width)  % bb->width;
    y = ((y % bb->height) + bb->height) % bb->height;
    bb->pixels[y * bb->width + x] = color;
}
```

**The index formula:**  
`pixels[y * width + x]` — multiply the row number by the row length to skip past earlier rows, then add the column.

Example: writing a pixel at (100, 200) in an 800-wide buffer:
```
index = 200 * 800 + 100
      = 160,000 + 100
      = 160,100
```
So `bb->pixels[160100]` gets the color value.

**The wrapping formula — why two modulos?**  
C's `%` operator on negative numbers returns a negative result (unlike JavaScript's `%`). If `x = -3` and `width = 800`:

```
Step 1:  -3 % 800   = -3           (still negative in C!)
Step 2:  -3 + 800   = 797
Step 3:  797 % 800  = 797          ✓ correct wrap
```

If `x = 805` and `width = 800`:
```
Step 1:  805 % 800  = 5
Step 2:  5 + 800    = 805
Step 3:  805 % 800  = 5            ✓ correct wrap
```

The formula `((x % w) + w) % w` handles both negative and out-of-range-positive in one shot.

**Why toroidal?** Asteroids is a wraparound game — objects that fly off the right edge should reappear on the left. By making every single pixel write wrap, even *lines* crossing an edge render correctly without any special-case edge detection code. (We'll see this in the next lesson.)

---

## Step 5 — Uploading the backbuffer to the GPU each frame

In `platform_display_backbuffer` (inside `main_x11.c`):

```c
static void platform_display_backbuffer(const AsteroidsBackbuffer *bb) {
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 bb->width, bb->height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);

    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(0, 0);
        glTexCoord2f(1, 0); glVertex2f(1, 0);
        glTexCoord2f(1, 1); glVertex2f(1, 1);
        glTexCoord2f(0, 1); glVertex2f(0, 1);
    glEnd();

    glXSwapBuffers(g_display, g_window);
}
```

Every frame: CPU writes pixels into `bb->pixels` → `glTexImage2D` uploads the whole array to the GPU → a fullscreen quad textured with it fills the screen → `glXSwapBuffers` flips front/back buffers and waits for VSync.

**Why `glTexImage2D` every frame instead of `glTexSubImage2D`?** For this course, the difference is negligible (≈0.5 ms on a modern GPU for a 800×600 texture). `glTexImage2D` is simpler to explain and always correct.

---

## Build & Run

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11 -lX11 -lxkbcommon -lGL -lGLX -lm
./build/asteroids_x11
```

**What you should see:** The window opens with the Asteroids game running (two yellow wireframe asteroids, a white ship in the center). The game is already fully connected — the backbuffer system we described here is what makes everything visible.

---

## Key Concepts

- `AsteroidsBackbuffer` — a flat `uint32_t` array accessed as `pixels[y * width + x]`.
- `malloc` in `main`, not in game code — the platform owns memory, the game reads/writes it.
- `ASTEROIDS_RGBA(r,g,b,a)` packs bytes in little-endian order so `GL_RGBA` reads them correctly as R, G, B, A.
- `draw_pixel_w` uses `((x % w) + w) % w` to wrap both negative and oversized coordinates.
- Toroidal wrapping at the pixel level means lines crossing screen edges just work, no special casing needed.
- `glTexImage2D` + fullscreen quad: the simplest way to display a CPU pixel array via OpenGL.

## Exercise

Add a named constant `COLOR_CYAN` (already defined in `asteroids.h` as `ASTEROIDS_RGB(0, 255, 255)`) and manually compute what `uint32_t` value it produces. Then verify it by checking the macro definition: the result should be `0xFF00FFFF` when written as a little-endian `uint32_t`.
