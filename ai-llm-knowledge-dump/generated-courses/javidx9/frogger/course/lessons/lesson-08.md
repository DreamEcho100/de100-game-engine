# Lesson 08 — `frogger_render`: Sprites into the Backbuffer

## By the end of this lesson you will have:

A complete `frogger_render` function that draws all 10 lanes, the frog (with death flash), and HUD text into the `FroggerBackbuffer` — with no X11 or Raylib calls whatsoever.

---

## Architecture

```
frogger_render(&state, &bb)
    ↓
draw_rect(bb, ...)               ← clear + overlays
draw_sprite_partial(bb, ...)     ← tile sprites from SpriteBank
draw_text(bb, ...)               ← bitmap font HUD
    ↓ all write to
bb->pixels[y * bb->width + x]    ← uint32_t array
    ↑ uploaded by
platform_display_backbuffer      ← one glTexImage2D call
```

`frogger_render` knows nothing about X11 or Raylib. It fills pixels. The platform uploads them.

---

## Step 1 — `draw_sprite_partial`

The core primitive — draws a sub-region of a sprite sheet into the backbuffer:

```c
static void draw_sprite_partial(
    FroggerBackbuffer *bb,
    const SpriteBank *bank, int spr_id,
    int src_x, int src_y, int src_w, int src_h,
    int dest_px_x, int dest_px_y)
{
    int sheet_w = bank->widths[spr_id];
    int offset  = bank->offsets[spr_id];

    for (sy = 0; sy < src_h; sy++) {
        for (sx = 0; sx < src_w; sx++) {
            int idx    = offset + (src_y + sy) * sheet_w + (src_x + sx);
            int16_t c  = bank->colors[idx];
            int16_t gl = bank->glyphs[idx];
            if (gl == 0x0020) continue;   /* transparent */

            int ci = c & 0x0F;
            uint32_t color = FROGGER_RGB(
                CONSOLE_PALETTE[ci][0],
                CONSOLE_PALETTE[ci][1],
                CONSOLE_PALETTE[ci][2]);

            for (py = 0; py < CELL_PX; py++) {
                for (px = 0; px < CELL_PX; px++) {
                    int bx = dest_px_x + sx * CELL_PX + px;
                    int by = dest_px_y + sy * CELL_PX + py;
                    if (bx >= 0 && bx < bb->width &&
                        by >= 0 && by < bb->height)
                        bb->pixels[by * bb->width + bx] = color;
                }
            }
        }
    }
}
```

**vs. the old X11 version:**
```c
/* OLD: one system call per sprite cell */
XSetForeground(g_display, g_gc, pixel);
XFillRectangle(g_display, g_backbuffer, g_gc,
    dest_px_x + sx * CELL_PX, dest_px_y + sy * CELL_PX,
    CELL_PX, CELL_PX);

/* NEW: direct pixel writes, no system calls */
bb->pixels[by * bb->width + bx] = color;
```

Each sprite cell is `CELL_PX × CELL_PX = 8 × 8 = 64` pixels. For a full 8×8-cell sprite: `64 cells × 64 pixels = 4096` pixel writes per sprite draw call — all to contiguous memory.

**Bounds check:**
`if (bx >= 0 && bx < bb->width && by >= 0 && by < bb->height)` — tiles at the left edge have negative `dest_px_x` (partially off-screen). Without the check, we'd write outside `bb->pixels`.

---

## Step 2 — Tile-to-sprite mapping

```c
static int tile_to_sprite(char c, int *src_x) {
    *src_x = 0;
    switch (c) {
        case 'j': *src_x =  0; return SPR_LOG;  /* log start  */
        case 'l': *src_x =  8; return SPR_LOG;  /* log middle */
        case 'k': *src_x = 16; return SPR_LOG;  /* log end    */
        case 'a': *src_x =  0; return SPR_BUS;  /* bus seg 1  */
        case 's': *src_x =  8; return SPR_BUS;  /* bus seg 2  */
        case 'd': *src_x = 16; return SPR_BUS;  /* bus seg 3  */
        case 'f': *src_x = 24; return SPR_BUS;  /* bus seg 4  */
        /* ... */
        case '.': return -1;  /* road stays black */
    }
}
```

Multi-cell sprites (log=24 cells wide, bus=32 cells wide) are stored as full sheets in the `SpriteBank`. When rendering, we select which 8-cell-wide segment to draw using `src_x`. Log tile `'l'` (middle) → `src_x=8`, draws columns 8..15 of the log sheet.

---

## Step 3 — Rendering order

```c
void frogger_render(const GameState *state, FroggerBackbuffer *bb) {
    /* 1. Clear to black */
    draw_rect(bb, 0, 0, bb->width, bb->height, FROGGER_RGB(0, 0, 0));

    /* 2. Draw all lanes (background first, row by row) */
    for (y = 0; y < NUM_LANES; y++) {
        int tile_start, px_offset;
        lane_scroll(state->time, lane_speeds[y], &tile_start, &px_offset);
        int dest_py = y * TILE_PX;  /* pixel Y for this row */

        for (i = 0; i < LANE_WIDTH; i++) {
            char c  = lane_patterns[y][(tile_start + i) % LANE_PATTERN_LEN];
            int src_x = 0;
            int spr   = tile_to_sprite(c, &src_x);
            int dest_px = (-1 + i) * TILE_PX - px_offset;

            if (spr >= 0)
                draw_sprite_partial(bb, &state->sprites, spr,
                    src_x, 0, TILE_CELLS, TILE_CELLS, dest_px, dest_py);
        }
    }

    /* 3. Draw frog (on top of lanes) */
    /* 4. HUD (on top of everything) */
}
```

Painter's algorithm: clear first → lanes → frog → HUD. Each layer can overwrite the one below.

---

## Step 4 — Frog rendering with death flash

```c
int show_frog = 1;
if (state->dead) {
    int flash = (int)(state->dead_timer / 0.05f);
    show_frog = (flash % 2 == 0);
}
if (show_frog) {
    int frog_px = (int)(state->frog_x * (float)TILE_PX);
    int frog_py = (int)(state->frog_y * (float)TILE_PX);
    draw_sprite_partial(bb, &state->sprites, SPR_FROG,
        0, 0,
        state->sprites.widths [SPR_FROG],
        state->sprites.heights[SPR_FROG],
        frog_px, frog_py);
}
```

**Flash formula:** `flash = (int)(dead_timer / 0.05f)`. The dead timer counts down from 0.4s. Every 50ms, `flash` increments. `flash % 2`: alternates 0/1 at 10Hz → frog blinks 5 times over 0.4 seconds.

**Sub-pixel frog position:**
```c
int frog_px = (int)(state->frog_x * (float)TILE_PX);
```
`frog_x` is a float (e.g., `8.73` tiles). Multiplying by `TILE_PX=64` gives pixel position `559`. No intermediate rounding to tile grid — the river carries the frog in smooth fractional increments.

---

## Key Concepts

- `draw_sprite_partial` writes to `bb->pixels` directly — no OS/GPU calls
- Bounds check per pixel: tiles at left edge have negative `dest_px_x`
- `tile_to_sprite(c, &src_x)` maps tile char to sprite ID + horizontal segment offset
- `src_x` selects which 8-column segment of a multi-cell sprite to draw
- `'.'` (road) returns `-1` from `tile_to_sprite` → skip → stays black from clear
- Flash: `(int)(dead_timer / 0.05f) % 2` — toggles at 10Hz
- Frog sub-pixel position: `frog_x * TILE_PX` gives exact pixel, not tile-snapped

---

## Exercise

The road tile `'.'` is drawn as black (background). The pavement tile `'p'` is drawn as a sprite. What's the visual difference? Can you change `tile_to_sprite` to make road show the pavement texture too? Would this affect gameplay? (Hint: `tile_is_safe` checks `c == '.'` as safe, so changing the visual won't affect collision.)
