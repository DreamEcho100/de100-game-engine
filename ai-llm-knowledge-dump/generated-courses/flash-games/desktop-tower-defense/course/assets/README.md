# course/assets — Sprite Assets Guide

This directory is where you place sprite atlases (spritesheet PNG files) once
you have real pixel art. Until then, the game runs in **placeholder mode**:
every entity is drawn as a colored rectangle with a short label.

---

## What is a sprite atlas?

A sprite atlas (or spritesheet) is a single large PNG that contains every
texture frame packed side-by-side. Instead of loading 50 individual files,
you load one image and index into it with (x, y, width, height) rectangles.

Benefits:
- One GPU texture bind per frame (faster)
- One file to ship / version control
- Easier to share / resize consistently

---

## Expected atlas layout (256 × 256, 16 × 16 grid)

Each cell is **16 × 16 pixels**. The table below matches the `src_x / src_y`
values in `SPRITE_DEFS[]` (`src/sprites.c`):

```
     col 0    col 1    col 2    col 3    col 4    col 5    col 6
row 0 [PELLET] [SQUIRT] [DART]  [SNAP]  [SWARM]  [FROST]  [BASH]
row 1 [NORM]   [FAST]   [FLY]   [ARM]   [SPAWN]  [BOSS*]
row 2 [PRJ]    [EXP]
row 3 [TILE_E] [TILE_O]

* BOSS occupies a 32×32 block (cols 5-6, rows 1-2)
```

ASCII diagram (each square = 16 × 16 px):

```
+-------+-------+-------+-------+-------+-------+-------+- - -
| PLLT  | SQRT  | DART  | SNAP  | SWRM  | FRST  | BASH  |  row 0  (towers)
+-------+-------+-------+-------+-------+-------+-------+
| NRM   | FST   | FLY   | ARM   | SPN   | BOSS          |  row 1  (creeps)
+-------+-------+-------+-------+-------+               +
|                                       | (boss 32×32)  |  row 2
+-------+-------+- - - - - - - - - - - - - - - - - - - -+
| PRJ   | EXP   |                                        |  row 2  (fx)
+-------+-------+
| TILE_E| TILE_O|                                        |  row 3  (bg tiles)
+-------+-------+- - - - - - - - - - - - - - - - - - - -+
```

---

## Recommended free asset sources

| Source | License | Notes |
|---|---|---|
| [OpenGameArt.org](https://opengameart.org) | CC0 / CC-BY | Filter by "CC0" for zero-attribution assets |
| [Kenney.nl](https://kenney.nl/assets) | CC0 | Huge packs, consistent style |
| [itch.io free assets](https://itch.io/game-assets/free) | Varies | Check each listing's license |

Recommended searches:
- "tower defense sprite sheet"
- "16x16 tileset CC0"
- "top-down shooter sprites"

---

## Creating your own atlas with ImageMagick

If you have individual PNG files (one per entity), assemble them into one
atlas with `montage` (part of ImageMagick):

```bash
# Install ImageMagick
sudo apt install imagemagick   # Debian/Ubuntu
brew install imagemagick       # macOS

# Resize all individual sprites to 16×16
for f in raw/*.png; do
    convert "$f" -resize 16x16! "sized/$(basename $f)"
done

# Arrange into a 16-column grid, no gaps, black background
montage sized/pellet.png sized/squirt.png sized/dart.png \
        sized/snap.png sized/swarm.png sized/frost.png sized/bash.png \
        -tile 16x -geometry 16x16+0+0 -background none \
        assets/sprites.png

# Verify the output dimensions
identify assets/sprites.png
# Should report: sprites.png PNG 256x256 ...
```

If you want to pack frames for animation (e.g. a 4-frame walk cycle), place
all frames for one sprite in a horizontal strip and update `src_w` in
`SPRITE_DEFS` to the total strip width (e.g. `src_w = 64` for 4 × 16px
frames). `draw_sprite_frame(bb, SPR_CREEP_NORMAL, frame, ...)` will select
the correct column.

---

## Wiring it up in the game

1. Drop your `sprites.png` into this directory (`course/assets/sprites.png`).

2. In `src/game.c`, change the `sprites_init` call inside `game_init`:

   ```c
   /* Before (placeholder mode) */
   sprites_init(NULL);

   /* After (real atlas) */
   sprites_init("assets/sprites.png");
   ```

3. Set `USE_SPRITES 1` at the top of `src/game.c`:

   ```c
   #define USE_SPRITES 1
   ```

4. Rebuild:

   ```bash
   ./build-dev.sh --backend=raylib
   ```

5. If the atlas loaded successfully, `g_sprite_atlas.loaded` will be `1` and
   `draw_sprite()` will blit from pixels instead of drawing rectangles.

> **Note**: `stb_image` integration (step 2 inside `sprites_init`) is marked
> `TODO` in `src/sprites.c`. See Lesson 24 for the exact code to drop in.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Colored rectangles still appear | `g_sprite_atlas.loaded == 0` | Check the `stbi_load` return value and print the path |
| Distorted sprites | Wrong `src_w / src_h` in SPRITE_DEFS | Recount pixels in your atlas |
| Build error: `stbi_load` undeclared | stb_image not included | Add `#define STB_IMAGE_IMPLEMENTATION` + `#include "stb_image.h"` before the call in sprites.c |
| X11 backend has wrong colors | Pixel format mismatch | The atlas should be RGBA; X11 reads BGR — add a swap loop after loading |
