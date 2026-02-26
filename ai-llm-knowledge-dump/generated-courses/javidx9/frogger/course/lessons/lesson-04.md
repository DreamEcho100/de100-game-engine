# Lesson 04 — `SpriteBank`: `.spr` Binary Format + Pool Loader

## By the end of this lesson you will have:

A `SpriteBank` struct that holds all 9 sprite sheets in a single flat pool, and a `sprite_load` function that reads the binary `.spr` format from disk into the pool.

---

## Step 1 — The `.spr` Binary Format

The `.spr` files were created by the original Javidx9 Windows console engine. Binary layout:

```
int32_t  width;                   ← 4 bytes, sprite width in cells
int32_t  height;                  ← 4 bytes, sprite height in cells
int16_t  colors[width * height];  ← 2 bytes per cell, FG color in low nibble
int16_t  glyphs[width * height];  ← 2 bytes per cell, Unicode char
```

Total bytes per sprite: `8 + (width * height * 4)`.

For the frog sprite (8×8 cells): `8 + (64 × 4) = 264 bytes`.

**New C concept — `int32_t` / `int16_t`:**
Fixed-width integer types from `<stdint.h>`. `int32_t` is exactly 4 bytes; `int16_t` is exactly 2 bytes. We must use these because `int` width varies by platform. The `.spr` file was written on a 32-bit Windows system where `int = 4 bytes` — that happens to match, but using `int32_t` makes the intent explicit and cross-platform correct.

---

## Step 2 — `sprite_load`

```c
static int sprite_load(SpriteBank *bank, int spr_id, const char *path) {
    FILE *f = fopen(path, "rb");  /* "rb" = read binary */
    if (!f) {
        fprintf(stderr, "FATAL: cannot open sprite '%s'\n", path);
        return 0;
    }
    int32_t w, h;
    fread(&w, sizeof(int32_t), 1, f);
    fread(&h, sizeof(int32_t), 1, f);

    int offset = bank->offsets[spr_id];
    int count  = w * h;

    if (offset + count > SPR_POOL_CELLS) {
        fprintf(stderr, "FATAL: sprite pool overflow for '%s'\n", path);
        fclose(f);
        return 0;
    }

    fread(bank->colors + offset, sizeof(int16_t), count, f);
    fread(bank->glyphs + offset, sizeof(int16_t), count, f);

    bank->widths [spr_id] = (int)w;
    bank->heights[spr_id] = (int)h;
    fclose(f);
    return 1;
}
```

**New C concept — `fread`:**
`fread(ptr, size, count, stream)` reads `count` items of `size` bytes each from `stream` into `ptr`. Returns the number of items actually read. Here:
- `fread(&w, sizeof(int32_t), 1, f)` — read 1 item of 4 bytes into `w`
- `fread(bank->colors + offset, sizeof(int16_t), count, f)` — read `count` 2-byte shorts into the pool

`"rb"` mode: `r` = read, `b` = binary (important on Windows where text mode would misinterpret byte `0x0A` as newline).

---

## Step 3 — `SpriteBank` pool design

All sprite data lives in two flat arrays inside `GameState`:

```c
typedef struct {
    int16_t colors[SPR_POOL_CELLS];   /* all colors, all sprites packed */
    int16_t glyphs[SPR_POOL_CELLS];   /* all glyphs, all sprites packed */
    int     widths [NUM_SPRITES];
    int     heights[NUM_SPRITES];
    int     offsets[NUM_SPRITES];     /* where each sprite starts in pool */
} SpriteBank;
```

`SPR_POOL_CELLS = 9 * 32 * 8 = 2304`. The largest sprite (bus = 32×8) defines the pool size per sprite; with 9 sprites that's 2304 cells max.

**Offsets are computed in `frogger_init`:**

```c
static const int spr_cells[NUM_SPRITES] = {
    /* frog */ 8*8,  /* water */ 8*8,  /* pavement */ 8*8,
    /* wall */ 8*8,  /* home  */ 8*8,  /* log  */ 24*8,
    /* car1 */ 16*8, /* car2  */ 16*8, /* bus  */ 32*8,
};
int offset = 0;
for (i = 0; i < NUM_SPRITES; i++) {
    bank->offsets[i] = offset;
    offset += spr_cells[i];
}
```

`SPR_LOG` starts at `offsets[5] = (5 × 64) = 320`. Accessing cell (2, 3) of the log: `index = offsets[SPR_LOG] + 3 * widths[SPR_LOG] + 2 = 320 + 3*24 + 2 = 394`.

---

## Step 4 — Why a pool instead of separate arrays?

**Alternative:** `int16_t log_colors[24*8]; int16_t bus_colors[32*8]; ...`

**Problem:** 18 separate arrays (colors + glyphs × 9 sprites), all needing explicit sizes. No way to loop over sprites generically.

**Pool approach:** One array, offsets table. `draw_sprite_partial` accesses any sprite with the same code:

```c
int offset  = bank->offsets[spr_id];
int sheet_w = bank->widths[spr_id];
/* cell (sx, sy): */
int idx = offset + sy * sheet_w + sx;
int16_t c  = bank->colors[idx];
int16_t gl = bank->glyphs[idx];
```

No branching on sprite ID. No heap allocation. The entire bank is inside `GameState` — one `memset` clears it all.

---

## Step 5 — Sprite dimensions

| Sprite | Cells | Notes |
|--------|-------|-------|
| frog | 8×8 | Player character |
| water | 8×8 | River fill |
| pavement | 8×8 | Safe ground |
| wall | 8×8 | Home-row wall (deadly) |
| home | 8×8 | Goal tile |
| log | 24×8 | 3-tile wide (j/l/k segments drawn at `src_x=0/8/16`) |
| car1 | 16×8 | 2-tile vehicle |
| car2 | 16×8 | 2-tile vehicle |
| bus | 32×8 | 4-tile vehicle (a/s/d/f segments at `src_x=0/8/16/24`) |

Multi-tile sprites are drawn one TILE_CELLS-wide segment at a time. `src_x` selects which segment.

---

## Key Concepts

- `.spr` layout: `int32_t width, height` then `int16_t colors[w*h]` then `int16_t glyphs[w*h]`
- `fread(&w, sizeof(int32_t), 1, f)` — read exactly 4 bytes from binary file
- `int16_t / int32_t` — fixed-width types required for cross-platform binary file reading
- `SpriteBank` pool: all sprite data in two flat arrays; `offsets[]` tracks where each starts
- `offsets[spr_id] + sy * widths[spr_id] + sx` — flat index into sprite sheet

---

## Exercise

The `.spr` format stores colors and glyphs as two separate arrays (all colors, then all glyphs). Why not interleave them as `{color, glyph}` pairs per cell? Think about `fread`: how many `fread` calls would you need for interleaved vs. separate format? Which is faster to load? Which is faster to iterate during rendering?
