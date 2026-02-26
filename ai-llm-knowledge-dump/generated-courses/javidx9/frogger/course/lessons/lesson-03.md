# Lesson 03 — Color System: `FROGGER_RGB` + `CONSOLE_PALETTE`

## By the end of this lesson you will have:

The `FROGGER_RGB` / `FROGGER_RGBA` macros for creating colors, and an understanding of how the 16-color Windows console palette from the `.spr` files maps to 32-bit pixel values for the backbuffer.

---

## Step 1 — `FROGGER_RGB` and `FROGGER_RGBA`

```c
/* Pack (r,g,b,a) so that bytes in memory are [RR, GG, BB, AA] on
 * little-endian x86 — i.e. the uint32_t value is 0xAABBGGRR.
 * This matches PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 (Raylib) which
 * reads bytes in memory order as R, G, B, A, and GL_RGBA (X11/OpenGL)
 * which does the same.  Both backends see correct colours.          */
#define FROGGER_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
     ((uint32_t)(g) <<  8) |  (uint32_t)(r))

#define FROGGER_RGB(r, g, b) FROGGER_RGBA(r, g, b, 0xFF)
```

`FROGGER_RGB(255, 0, 0)` → `0xFF0000FF` (bright red, fully opaque).

The same pattern as in the snake/tetris courses — consistent across all three games. Any code that writes pixels uses these macros.

**Why `0xAABBGGRR` (not `0xAARRGGBB`)?**

The uint32_t value is stored in memory on little-endian x86 with the least-significant byte first:

```
uint32_t value = 0xAABBGGRR
Memory bytes:   [RR] [GG] [BB] [AA]   (address+0 .. address+3)
```

Both upload paths read bytes from memory in order:
- `GL_RGBA` (X11/OpenGL) reads byte[0]=R, byte[1]=G, byte[2]=B, byte[3]=A ✓
- `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` (Raylib) reads the same order ✓

**Why NOT `0xAARRGGBB`?**

`0xAARRGGBB` stores bytes as `[BB, GG, RR, AA]` in memory. OpenGL's `GL_BGRA` can read this correctly (it expects byte[0]=B), but Raylib's `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` would read byte[0] as R, getting B instead → red and blue channels swapped → wrong colours (e.g. yellow buses appear cyan).

**Bit layout of the uint32_t value itself:**

```
Bit 31..24  = A (alpha)    ← 0xFF = fully opaque
Bit 23..16  = B (blue)
Bit 15..8   = G (green)
Bit  7..0   = R (red)
```

The bit layout looks "reversed", but the byte layout in memory is `[RR, GG, BB, AA]` — which is what both renderers expect.

---

## Step 2 — `CONSOLE_PALETTE`

The `.spr` sprite files use Windows console color attributes. Each cell has:
- A `colors` field (16-bit): `FG_color = colors & 0x0F` (low nibble = foreground)
- A `glyphs` field (16-bit): `0x2588` = filled block (draw it), `0x0020` = space (transparent)

The 16 foreground colors map to RGB values via `CONSOLE_PALETTE`:

```c
static const uint8_t CONSOLE_PALETTE[16][3] = {
    {  0,   0,   0},  /* 0  Black        */
    {  0,   0, 128},  /* 1  Dark Blue    */
    {  0, 128,   0},  /* 2  Dark Green   */
    {  0, 128, 128},  /* 3  Dark Cyan    */
    {128,   0,   0},  /* 4  Dark Red     */
    {128,   0, 128},  /* 5  Dark Magenta */
    {128, 128,   0},  /* 6  Dark Yellow  */
    {192, 192, 192},  /* 7  Gray         */
    {128, 128, 128},  /* 8  Dark Gray    */
    {  0,   0, 255},  /* 9  Blue         */
    {  0, 255,   0},  /* 10 Bright Green */
    {  0, 255, 255},  /* 11 Cyan         */
    {255,   0,   0},  /* 12 Red          */
    {255,   0, 255},  /* 13 Magenta      */
    {255, 255,   0},  /* 14 Yellow       */
    {255, 255, 255},  /* 15 White        */
};
```

**New C concept — 2D array `[16][3]`:**
`CONSOLE_PALETTE[ci]` gives a pointer to a 3-byte sub-array `{R, G, B}`. `CONSOLE_PALETTE[ci][0]` = R, `[ci][1]` = G, `[ci][2]` = B. This is a 16×3 matrix stored as 48 contiguous bytes.

**Usage in `draw_sprite_partial`:**
```c
int ci = bank->colors[idx] & 0x0F;  /* extract FG color index */
uint32_t color = FROGGER_RGB(
    CONSOLE_PALETTE[ci][0],
    CONSOLE_PALETTE[ci][1],
    CONSOLE_PALETTE[ci][2]);
/* write CELL_PX × CELL_PX pixels at this color */
```

---

## Step 3 — Transparent cells

```c
if (bank->glyphs[idx] == 0x0020) continue;  /* space = transparent, skip */
```

A glyph value of `0x0020` (Unicode space) means "show nothing here." The frog sprite has transparent cells around its edges — the tile behind it shows through. If we wrote black pixels for transparent cells, the frog would have a visible black bounding box.

The `continue` skips to the next sprite cell, leaving whatever was already in the backbuffer (the tile drawn below) untouched.

---

## Step 4 — Why 16 colors?

The original Javidx9 code ran in the Windows console. The `olcConsoleGameEngine` drew colored Unicode block characters (`█` = `0x2588`). The console only had 16 colors.

Our port preserves the same visual. `CONSOLE_PALETTE[2]` (Dark Green = `{0, 128, 0}`) for water tiles, `CONSOLE_PALETTE[10]` (Bright Green = `{0, 255, 0}`) for log tiles, etc. The game looks the same as the original on any display.

This also means the `.spr` files ship as-is from the original — no data conversion needed.

---

## Key Concepts

- `FROGGER_RGB(r,g,b)` → `0xAABBGGRR` uint32, bytes in memory `[RR,GG,BB,AA]` — both GL_RGBA and Raylib R8G8B8A8 read this correctly
- `FROGGER_RGBA(r,g,b,a)` → same layout with explicit alpha (for overlays)
- **Do NOT use `0xAARRGGBB`** — that would require `GL_BGRA` in X11 but Raylib would swap R and B
- `CONSOLE_PALETTE[16][3]` — maps `.spr` 4-bit FG color index to `{R, G, B}` bytes
- `colors[idx] & 0x0F` — extract foreground color index from sprite cell attribute
- `glyphs[idx] == 0x0020` → transparent cell, skip (`continue`) without writing pixels

---

## Exercise

Which `CONSOLE_PALETTE` index is the frog most likely to use for its green color? Look at the palette table — there are two greens: index 2 (Dark Green) and index 10 (Bright Green). Open `assets/frog.spr` in a hex editor (or write a small test program that calls `sprite_load` and prints the unique color indices). Which one does the frog use?
