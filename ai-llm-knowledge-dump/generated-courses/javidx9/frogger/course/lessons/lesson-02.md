# Lesson 02 — `FroggerBackbuffer`: Platform-Independent Canvas

## By the end of this lesson you will have:

A `FroggerBackbuffer` struct holding a `uint32_t *` pixel array that `frogger_render` writes into, and platform code in `main_x11.c` / `main_raylib.c` that uploads it to the GPU each frame.

---

## The Core Idea

Every game frame:

```
frogger_render(&state, &bb)       ← writes to bb.pixels[]
platform_display_backbuffer(&bb)  ← uploads bb.pixels[] to GPU
```

`frogger_render` knows nothing about X11 or Raylib. It just fills a flat array of `uint32_t` values. This is the "backbuffer" — a CPU-side image that gets uploaded to the GPU once per frame.

---

## Step 1 — `FroggerBackbuffer` struct

```c
typedef struct {
    uint32_t *pixels;  /* flat array: pixels[y * width + x] */
    int       width;
    int       height;
} FroggerBackbuffer;
```

**Pixel format: bytes in memory are `[RR, GG, BB, AA]`**

The `FROGGER_RGBA` macro stores the uint32_t value as `0xAABBGGRR` — which on little-endian x86 places bytes in memory as:

| Memory offset | Byte | Meaning |
|--------------|------|---------|
| `+0` | `RR` | Red |
| `+1` | `GG` | Green |
| `+2` | `BB` | Blue |
| `+3` | `AA` | Alpha (always 255 = fully opaque) |

Example: bright red = `FROGGER_RGB(255,0,0)` → uint32 `0xFF0000FF`, bytes `[FF,00,00,FF]`. Cyan = `FROGGER_RGB(0,255,255)` → uint32 `0xFFFF0000`, bytes `[00,FF,FF,FF]`.

**New C concept — `uint32_t`:**
A 32-bit unsigned integer, defined in `<stdint.h>`. Guaranteed to be exactly 32 bits on every platform — unlike `int` which could be 16, 32, or 64 bits. Pixel storage requires exactly 4 bytes.

---

## Step 2 — Allocation in `main`

```c
bb.width  = SCREEN_PX_W;   /* 1024 */
bb.height = SCREEN_PX_H;   /* 640  */
bb.pixels = (uint32_t *)malloc((size_t)(bb.width * bb.height) * sizeof(uint32_t));
```

`1024 × 640 × 4 bytes = 2,621,440 bytes ≈ 2.5 MB`. One allocation, freed on exit:

```c
free(bb.pixels);
```

**Why `malloc` here and not in `frogger.c`?**
The backbuffer lifetime matches the process — allocated once in `main`, freed once on exit. It's not game data (doesn't reset between games). Keeping allocation in `main` makes `frogger_render` simpler: it just receives a pointer, no ownership questions.

---

## Step 3 — Upload to GPU (X11/OpenGL)

In `platform_display_backbuffer` (static, only in `main_x11.c`):

```c
glBindTexture(GL_TEXTURE_2D, g_texture);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
             bb->width, bb->height, 0,
             GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);
```

**Why `GL_RGBA` (not `GL_BGRA`)?**
Our pixel format stores bytes in memory as `[RR, GG, BB, AA]`. `GL_RGBA` tells OpenGL to read bytes left-to-right as R, G, B, A — which is exactly our layout. Using `GL_BGRA` would swap red and blue channels, producing wrong colours.

**Orthographic projection:**
```c
glOrtho(0, 1, 1, 0, -1, 1);
```
Maps NDC space so `x=0..1, y=0..1` (top-left to bottom-right) matches the texture. The quad covers the entire window.

---

## Step 4 — Upload to GPU (Raylib)

```c
/* Create texture once: */
Image img = { .data=bb.pixels, .width=bb.width, .height=bb.height,
              .mipmaps=1, .format=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
Texture2D tex = LoadTextureFromImage(img);

/* Each frame: */
UpdateTexture(tex, bb.pixels);
BeginDrawing();
    DrawTexture(tex, 0, 0, WHITE);
EndDrawing();
```

**Pixel format matches for Raylib:**
`PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` reads bytes from memory as R, G, B, A. Our pixel layout stores bytes as `[RR, GG, BB, AA]` — an exact match. No swapping needed.

---

## Step 5 — Writing pixels

Inside `frogger.c`, a single pixel write:

```c
bb->pixels[y * bb->width + x] = FROGGER_RGB(r, g, b);
```

`y * width + x` is the flat index for 2D coordinates. For `bb->width = 1024`, pixel at (col=10, row=5): `5 * 1024 + 10 = 5130`.

Clearing the entire buffer to black:

```c
/* Clear to black */
int i;
for (i = 0; i < bb->width * bb->height; i++)
    bb->pixels[i] = FROGGER_RGB(0, 0, 0);
```

Or more efficiently using `draw_rect`:

```c
draw_rect(bb, 0, 0, bb->width, bb->height, FROGGER_RGB(0, 0, 0));
```

---

## Key Concepts

- `FroggerBackbuffer { uint32_t *pixels; int width, height; }` — CPU pixel array
- Pixel bytes in memory: `[RR, GG, BB, AA]` — matches both `GL_RGBA` (X11) and `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` (Raylib)
- `malloc` in `main`, `free` before exit — one allocation, process lifetime
- X11: `glTexImage2D(GL_RGBA)` + fullscreen quad + `glXSwapBuffers`
- Raylib: `LoadTextureFromImage` + `UpdateTexture` + `DrawTexture`
- `pixels[y * width + x]` — 2D → 1D flat index

---

## Exercise

The `FROGGER_RGBA` macro stores bytes in memory as `[RR, GG, BB, AA]`. Write the uint32_t value for cyan (R=0, G=255, B=255, A=255) manually without using the macro. What is the hex value? Verify: on little-endian x86, bytes `[00, FF, FF, FF]` = uint32 `0xFFFF0000`. Confirm with `FROGGER_RGB(0, 255, 255)`: `a=0xFF, b=0xFF, g=0xFF, r=0x00` → `(0xFF<<24)|(0xFF<<16)|(0xFF<<8)|0x00 = 0xFFFFFF00`... wait, that's wrong — work it out step by step and verify against the macro definition.
