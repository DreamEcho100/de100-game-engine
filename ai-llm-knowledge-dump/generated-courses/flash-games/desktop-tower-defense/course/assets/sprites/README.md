# DTD Sprite Assets

## Folder structure

```
assets/sprites/
  atlas.png          ← main sprite atlas (single PNG, all sprites packed here)
  towers/            ← source art for individual tower sprites (optional)
  creeps/            ← source art for individual creep sprites
  tiles/             ← terrain tiles (grass, water, mountain, path)
  effects/           ← projectile and explosion frames
  README.md          ← this file
```

## Enabling real sprites

1. Create or download a sprite atlas PNG (see "Free asset sources" below).
2. Edit `SPRITE_DEFS[]` in `src/sprites.c` — update `src_x`, `src_y`, `src_w`, `src_h`
   to match where each sprite lives in your atlas.
3. Change the call in `game_init()` from:
   ```c
   sprites_init(NULL);           // placeholder mode
   ```
   to:
   ```c
   sprites_load_async("assets/sprites/atlas.png");  // async PNG load
   ```
   Or use the synchronous version if you want loading to block startup:
   ```c
   sprites_init("assets/sprites/atlas.png");
   ```
4. Build normally — the game falls back to colored placeholders if the file is missing.

## Atlas layout convention

The default `SPRITE_DEFS[]` assumes a 256×256 atlas with 16×16 tiles:

| Row | Contents                                  |
|-----|-------------------------------------------|
| 0   | Towers (7 × 16×16 tiles, cols 0–6)        |
| 1   | Creeps (6 × 16×16 tiles + 1 × 32×32 boss)|
| 2   | Projectiles / explosion frames            |
| 3   | Background tiles (20×20 px each)          |

You can change this layout freely — just update `src_x / src_y / src_w / src_h` in the table.

## Adding a new sprite

1. Add a new `SPR_MY_SPRITE` entry to the `SpriteId` enum in `src/sprites.h` **before** `SPR_MISSING`.
2. Add a matching row to `SPRITE_DEFS[]` in `src/sprites.c`.
3. Call `draw_sprite(bb, SPR_MY_SPRITE, x, y, w, h)` wherever needed.

If the atlas file is missing or the path is wrong, `draw_sprite()` automatically
renders a **magenta checkerboard** with `"???"` text, logs to `stderr`, and
sets `g_sprite_atlas.error_msg` — so you always know exactly which file failed.

## Error log format

```
[SPRITES] ERROR: stbi_load("assets/sprites/atlas.png") failed: can't fopen
[SPRITES] ERROR: stbi_load("bad.png") failed: unknown image type
[SPRITES] Loaded atlas: assets/sprites/atlas.png (256×256)
```

Check `stderr` (your terminal) for these messages.

## Free asset sources

| Source | License | Notes |
|--------|---------|-------|
| [Kenney.nl](https://kenney.nl/assets) | CC0 | Tower defense pack, top-down tiles |
| [OpenGameArt.org](https://opengameart.org) | Various | Search "tower defense" |
| [itch.io free assets](https://itch.io/game-assets/free) | Various | Many CC0 packs |
| [craftpix.net free](https://craftpix.net/freebies/) | Free license | Good TD sprites |

## Building a single-atlas PNG with ImageMagick

```bash
# Combine individual PNGs into one atlas (tiles at fixed positions):
convert \
  -size 256x256 xc:transparent \
  \( towers/pellet.png -geometry +0+0   \) -composite \
  \( towers/squirt.png -geometry +16+0  \) -composite \
  \( creeps/normal.png -geometry +0+16  \) -composite \
  atlas.png
```

Adjust offsets to match your `SPRITE_DEFS[]` `src_x / src_y` values.
