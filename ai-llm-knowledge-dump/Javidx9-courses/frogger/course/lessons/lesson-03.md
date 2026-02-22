# Lesson 3 — Binary .spr File Format

**By the end of this lesson you will have:**
A small standalone C program that opens `assets/frog.spr`, reads its dimensions,
and prints them to the terminal. You'll understand exactly how binary file I/O
works in C, and what data is packed into each sprite file.

---

## Why binary files?

The `.spr` files were created by `olcConsoleGameEngine` for Windows. They're
binary — raw bytes, not text. Reading them teaches us:
- How `fopen`/`fread` work in C
- What "little-endian" means
- How structs map to raw memory

JS analogy: like `Buffer.readInt32LE(offset)` in Node.js, but in C you just cast
memory directly.

---

## Step 1 — What's inside a .spr file?

Let's look at `assets/frog.spr` as raw hex. Run:

```sh
xxd assets/frog.spr | head -5
```

You should see:
```
00000000: 0800 0000 0800 0000 0000 0200 0200 0000 ...
```

Breaking it down byte by byte:
```
Byte 0-3:  08 00 00 00  → int32, little-endian → value = 8  (width)
Byte 4-7:  08 00 00 00  → int32, little-endian → value = 8  (height)
Byte 8-135: 64 int16 values → color attributes
Byte 136-263: 64 int16 values → Unicode glyph codes
```

**New C concept — little-endian:**
On x86/x64 (your Linux PC), multi-byte integers are stored LSB-first.
So `08 00 00 00` means the 8 is in the first byte, 0s fill the rest.
As a decimal: 8 × 1 + 0 × 256 + 0 × 65536 + 0 × 16777216 = **8**.

**New C concept — `int32_t`:**
A signed 32-bit integer. Unlike `int` (whose size varies), `int32_t` is always
exactly 4 bytes. You must `#include <stdint.h>` to use it.
JS analogy: like explicitly choosing `Int32Array` instead of plain `number`.

---

## Step 2 — The .spr layout as a C struct

Here's the format as a C struct (from `frogger.h`):

```c
/* .spr file layout — NOT how we store it, but what we read from disk:
   int32_t  width;                           4 bytes
   int32_t  height;                          4 bytes
   int16_t  colors[width * height];          2 bytes × w×h
   int16_t  glyphs[width * height];          2 bytes × w×h

   Total for frog.spr (8×8):
   4 + 4 + (64×2) + (64×2) = 8 + 128 + 128 = 264 bytes ✓
*/
```

Verify:
```sh
wc -c assets/frog.spr   # should print 264
wc -c assets/bus.spr    # 32×8 sprite: 8 + (256×2) + (256×2) = 1032 bytes
```

**Color attributes:**
Each `int16_t` color = Windows console color flags.
- Bits 0-3: foreground color index (0-15)
- Bits 4-7: background color index (0-15)

The 16 color indices map to RGB via `CONSOLE_PALETTE[16][3]` in `frogger.h`.

**Glyph codes:**
Each `int16_t` glyph = Unicode character code.
- `0x2588` = █ (FULL BLOCK) → fill this cell with the foreground color
- `0x0020` = ` ` (space)    → transparent, don't draw this cell

---

## Step 3 — Write a sprite reader (experiment program)

Create a small test program to verify your understanding. Make a file
`src/test_sprite.c`:

```c
/* test_sprite.c — reads a .spr file and prints its info */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>

int main(void) {
    FILE *f = fopen("assets/frog.spr", "rb");
    if (!f) {
        fprintf(stderr, "Cannot open frog.spr\n");
        return 1;
    }

    /* Read width and height (int32_t = 4 bytes each) */
    int32_t w, h;
    fread(&w, sizeof(int32_t), 1, f);
    fread(&h, sizeof(int32_t), 1, f);
    printf("frog.spr: %d x %d = %d cells\n", w, h, w * h);

    /* Read the 64 color attributes */
    printf("First 8 color attributes (row 0):\n");
    for (int i = 0; i < 8; i++) {
        int16_t color;
        fread(&color, sizeof(int16_t), 1, f);
        int fg = color & 0x0F;         /* low nibble  = FG color */
        int bg = (color >> 4) & 0x0F;  /* high nibble = BG color */
        printf("  cell[%d]: fg=%d  bg=%d  (raw=0x%04X)\n", i, fg, bg, (unsigned)color);
    }

    fclose(f);
    return 0;
}
```

Compile and run:
```sh
gcc -Wall -g -o test_sprite src/test_sprite.c && ./test_sprite
```

Expected output:
```
frog.spr: 8 x 8 = 64 cells
First 8 color attributes (row 0):
  cell[0]: fg=0  bg=0  (raw=0x0000)   ← black / transparent
  cell[1]: fg=2  bg=0  (raw=0x0002)   ← dark green
  cell[2]: fg=2  bg=0  (raw=0x0002)   ← dark green
  cell[3]: fg=0  bg=0  (raw=0x0000)
  ...
```

Row 0 of the frog is: `space, GREEN, GREEN, space, space, GREEN, GREEN, space`
That's the frog's eye-spots! Green circles near the top of the sprite.

**New C concept — `fread(&var, size, count, file)`:**
Reads `count` items of `size` bytes into the memory at address `&var`.
`&var` is the *address* of `var` — where it lives in memory.
JS analogy: like `buffer.copy(dest, offset)` but you point at a variable directly.

**New C concept — `>>` (right shift) and `& 0x0F` (bitwise AND):**
- `color & 0x0F`: keep only the lower 4 bits. Example: `0x2F & 0x0F = 0x0F = 15`
- `color >> 4`: shift right by 4 bits (divide by 16). Gets the high nibble.
JS analogy: same operators, same meaning — JS just uses them less often.

---

## Step 4 — Verify the SpriteBank layout in `frogger.h`

The actual game uses `SpriteBank` — a flat pool of all sprite data:

```c
typedef struct {
    int16_t colors[SPR_POOL_CELLS]; /* all colors, sprites packed end-to-end */
    int16_t glyphs[SPR_POOL_CELLS]; /* all glyphs, same packing              */
    int     widths [NUM_SPRITES];   /* sprite width  per ID                  */
    int     heights[NUM_SPRITES];   /* sprite height per ID                  */
    int     offsets[NUM_SPRITES];   /* start index into colors[]/glyphs[]    */
} SpriteBank;
```

**DOD benefit explained:**
Imagine looking up color cell (3, 2) of the frog sprite (ID=0):
```
index = offsets[SPR_FROG] + 2 * widths[SPR_FROG] + 3
      = 0 + 2 * 8 + 3
      = 19

colors[19] = color for frog cell (col=3, row=2)
glyphs[19] = glyph for frog cell (col=3, row=2)
```

All 9 sprites' colors are in ONE contiguous array. When you render all tiles in
a lane (touching many different sprites), the CPU prefetches ahead in that array
— no jumping around between separate `Sprite` objects.

---

## Build & Run

```sh
gcc -Wall -g -o test_sprite src/test_sprite.c
./test_sprite
```

Clean up:
```sh
rm test_sprite src/test_sprite.c
```

---

## Mental Model

A binary file is just bytes. `fread` copies them straight into your C variables.
Because C variables have a fixed byte layout (int32_t = 4 bytes, int16_t = 2 bytes)
and Linux x86 is also little-endian, the bytes from the Windows-written .spr file
map directly to C integers with no conversion needed.

```
File bytes:  08 00 00 00
             └─┬─────┘
              int32_t = 8  (little-endian: least significant byte first)
```

---

## Exercise

Check the `bus.spr` file:
1. How many cells is it? (`bus.spr` is the bus sprite — look at the draw code
   to figure out how many "tiles" a bus needs)
2. Print its dimensions with your test program (change `"frog.spr"` to `"bus.spr"`)
3. Verify: expected size = 8 + (w × h × 2) + (w × h × 2) bytes
