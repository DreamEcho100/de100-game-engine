# Lesson 4 — Sprite Rendering

**By the end of this lesson you will have:**
The frog sprite visible in the center of the window — a small green-and-white
pixel-art frog drawn from the real `.spr` file data. Each sprite "pixel" renders
as an 8×8 block of screen pixels.

---

## How sprite rendering works

The sprite stores a grid of (color, glyph) pairs. For each cell:
1. Look up the glyph — is it a full block (`0x2588`) or space (`0x0020`)?
2. If space: skip (transparent — leave black background)
3. Otherwise: get the foreground color index (low nibble of color attribute)
4. Look up the RGB values in `CONSOLE_PALETTE[color_index]`
5. Fill a `CELL_PX × CELL_PX` rectangle with that color

---

## Step 1 — Read `draw_sprite_partial` in the platform files

Open `src/main_raylib.c` (or `main_x11.c`). Find `draw_sprite_partial`:

```c
static void draw_sprite_partial(
    const SpriteBank *bank, int spr_id,
    int src_x, int src_y, int src_w, int src_h,
    int dest_px_x, int dest_px_y)
{
    int sheet_w = bank->widths[spr_id];   /* total width of this sprite    */
    int offset  = bank->offsets[spr_id]; /* where it starts in the pool   */

    for (int sy = 0; sy < src_h; sy++) {       /* row within the region   */
        for (int sx = 0; sx < src_w; sx++) {   /* col within the region   */
            /* Index into the flat pool:
               index = offset              (start of this sprite in pool)
                     + (src_y + sy) * sheet_w  (row offset)
                     + (src_x + sx)            (column offset)            */
            int idx    = offset + (src_y + sy) * sheet_w + (src_x + sx);
            int16_t c  = bank->colors[idx];
            int16_t g  = bank->glyphs[idx];

            if (g == 0x0020) continue;   /* space = transparent           */

            int ci = c & 0x0F;           /* foreground color index 0-15   */
            /* Look up RGB from the Windows console palette               */
            Color col = {
                CONSOLE_PALETTE[ci][0],
                CONSOLE_PALETTE[ci][1],
                CONSOLE_PALETTE[ci][2],
                255
            };

            /* Fill one CELL_PX × CELL_PX block at the correct screen pos */
            DrawRectangle(
                dest_px_x + sx * CELL_PX,
                dest_px_y + sy * CELL_PX,
                CELL_PX, CELL_PX, col);
        }
    }
}
```

**New C concept — 2D array as 1D index:**
The sprite pool is a 1D array, but sprites are 2D. The formula is:

```
index = row * width + col

Example: frog is 8 wide. Cell at row=2, col=3:
index = 2 * 8 + 3 = 19

Like: frog[2][3] would be frog[2*8+3] = frog[19] in a flat array
```

This is a fundamental C pattern. No 2D arrays needed — just math.

**New C concept — `continue`:**
Skips the rest of the current loop iteration and jumps to the next `sx`.
JS analogy: same — `continue` in JS works identically.

**New C concept — `static` on a function:**
`static void draw_sprite_partial(...)` means this function is only visible inside
this `.c` file. It's a helper — the platform contract doesn't expose it.
JS analogy: like a non-exported function in a module.

---

## Step 2 — Understand `CONSOLE_PALETTE`

From `frogger.h`:

```c
static const uint8_t CONSOLE_PALETTE[16][3] = {
    {  0,   0,   0},  /* 0  Black        */
    {  0,   0, 128},  /* 1  Dark Blue    */
    {  0, 128,   0},  /* 2  Dark Green   */
    ...
    {255, 255, 255},  /* 15 White        */
};
```

**New C concept — 2D array literal:**
`uint8_t CONSOLE_PALETTE[16][3]` = 16 rows of 3 bytes each.
Access: `CONSOLE_PALETTE[2][0]` = Red channel of color 2 (Dark Green) = 0.
JS analogy: `const palette = [[0,0,0], [0,0,128], [0,128,0], ...]`

**New C concept — `uint8_t`:**
Unsigned 8-bit integer. Values 0-255. Exactly right for an RGB channel.
From `<stdint.h>`. JS analogy: like a value in a `Uint8ClampedArray`.

**New C concept — `static const`:**
`static` = file-scoped (only visible in frogger.h's translation unit).
`const` = read-only after initialization.
Together: a compile-time constant table. Zero runtime cost.

---

## Step 3 — Trace a frog pixel by hand

Frog sprite, row 1 (y=1), col 0 (x=0):
- `colors[0 + 1*8 + 0]` = colors[8] = `0x000F` → fg = `0x0F` = 15 (White)
- `glyphs[0 + 1*8 + 0]` = glyphs[8] = `0x2588` (full block)
- ci = 15 → `CONSOLE_PALETTE[15]` = `{255, 255, 255}` (bright white)
- Draw a white 8×8 block at screen position:
  `(dest_px_x + 0*8, dest_px_y + 1*8)` = `(dest_px_x, dest_px_y + 8)`

That's the frog's left eye — a white pixel on row 1, col 0.

---

## Step 4 — How platform_render calls draw_sprite_partial

Look at the end of `platform_render` in `main_raylib.c`:

```c
/* Draw frog — pixel-accurate position (no intermediate TILE_CELLS cast) */
int frog_px = (int)(state->frog_x * (float)TILE_PX);
int frog_py = (int)(state->frog_y * (float)TILE_PX);
draw_sprite_partial(&state->sprites, SPR_FROG,
    0, 0,                                 /* src: start at top-left of sprite */
    state->sprites.widths[SPR_FROG],      /* src_w = full width (8) */
    state->sprites.heights[SPR_FROG],     /* src_h = full height (8) */
    frog_px, frog_py);                    /* dest pixel position */
```

Converting frog tile position (8, 9) to pixels:
```
frog_px = (int)(8.0f × 64) = (int)(512.0) = 512
frog_py = (int)(9.0f × 64) = (int)(576.0) = 576
```

`TILE_PX = TILE_CELLS * CELL_PX = 8 * 8 = 64`. We multiply all the way to pixels
before truncating with `(int)`. This gives 1-pixel precision — important later when
the frog drifts at fractional tile positions while riding a log.

---

## Build & Run

```sh
./build_x11.sh && ./frogger_x11
# or
./build_raylib.sh && ./frogger_raylib
```

**What you should see:**
A small colorful frog sprite in the lower-center area of the black window.
The frog is made of green and white pixel blocks.

If the frog looks wrong or garbled:
- Check that the sprite file exists: `ls assets/frog.spr`
- Check that you're running from `course/` (not from `course/src/`)
- Verify file size: `wc -c assets/frog.spr` should be 264

---

## Mental Model

Sprite rendering = nested loop over a 2D grid, converting each cell to a
rectangle on screen:

```
For each sprite cell (sx, sy):
  1. flat_index = offset + sy * width + sx
  2. color = colors[flat_index]
  3. glyph = glyphs[flat_index]
  4. if glyph == space: skip
  5. RGB = PALETTE[color & 0x0F]
  6. DrawRect(dest_x + sx*CELL_PX, dest_y + sy*CELL_PX, CELL_PX, CELL_PX, RGB)
```

Each sprite "pixel" = 1 cell = 8×8 screen pixels. The palette is just a lookup
table — like a CSS color variable. No complex math.

---

## Exercise

Draw a second sprite — the `SPR_WATER` sprite — somewhere on screen.
Add a second `draw_sprite_partial` call in `platform_render` with:
- `spr_id = SPR_WATER`
- `dest_px_x = 0, dest_px_y = 0` (top-left corner)

After you see it, remove the test call.

Then: look up what `CONSOLE_PALETTE[3]` is (Dark Cyan). Is that the color you
see in the water tile?
