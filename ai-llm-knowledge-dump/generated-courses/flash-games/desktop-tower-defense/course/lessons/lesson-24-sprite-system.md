# Lesson 24 — Sprite Atlas System

**Series**: Desktop Tower Defense in C  
**Prerequisite**: Lessons 1–23 (the game builds and runs without assets)  
**Goal**: Add a sprite/animation placeholder system that compiles to zero-cost
procedural drawing today and can swap in real pixel art with one `#define`
change tomorrow.

---

## Table of Contents

1. [What problem are we solving?](#1-what-problem-are-we-solving)
2. [Concept: Sprite atlas design](#2-concept-sprite-atlas-design)
3. [Concept: Placeholder pattern](#3-concept-placeholder-pattern)
4. [Concept: Animation frames](#4-concept-animation-frames)
5. [JavaScript analogy](#5-javascript-analogy)
6. [Implementation walkthrough](#6-implementation-walkthrough)
7. [Asset workflow](#7-asset-workflow)
8. [Checkpoints](#8-checkpoints)
9. [Exercises](#9-exercises)
10. [Further reading](#10-further-reading)

---

## 1. What problem are we solving?

Our game renders every entity with raw geometry: towers are gray rectangles,
creeps are colored circles, projectiles are 4×4 yellow dots. That is fine for
a prototype, but a finished game needs artwork.

The naive approach is to add `draw_sprite()` calls once you have PNG files.
The problem: you can't draw sprites you don't have yet, so development blocks
waiting for an artist. Professional teams solve this with a **placeholder
pattern**: write the full API first, wire it into the renderer, but have it
fall back to colored rectangles when no atlas is loaded. The artist can work in
parallel, and swapping in real art requires touching exactly one line.

This lesson builds that system for our tower-defense game.

---

## 2. Concept: Sprite atlas design

### 2.1 Why one texture, not many files?

Every unique PNG requires its own memory allocation, its own file-system read,
and (on the GPU) its own texture binding. In a typical game frame, the renderer
visits hundreds of entities. If each entity had its own texture:

```
draw tower[0]:  bind texture "pellet.png"    ← context switch
draw tower[1]:  bind texture "pellet.png"    ← same texture, but still checked
draw creep[0]:  bind texture "normal.png"    ← context switch
draw creep[1]:  bind texture "fast.png"      ← context switch
...
```

A **sprite atlas** (also called a *texture atlas* or *spritesheet*) packs all
frames into one image. All draws share one texture:

```
draw tower[0]:  src_rect = (0,0,16,16)       ← no texture switch
draw tower[1]:  src_rect = (0,0,16,16)       ← no texture switch
draw creep[0]:  src_rect = (0,16,16,16)      ← no texture switch
draw creep[1]:  src_rect = (16,16,16,16)     ← no texture switch
```

Even on a CPU software renderer like ours (we blit pixels directly into the
backbuffer), a single allocation is faster to load and easier to version.

### 2.2 Atlas layout for this game

We use a 256 × 256 atlas divided into 16 × 16 pixel cells:

```
Row 0 (y=0)   : Tower sprites       — PELLET SQUIRT DART SNAP SWARM FROST BASH
Row 1 (y=16)  : Creep sprites       — NORMAL FAST FLYING ARMOURED SPAWN BOSS(32×32)
Row 2 (y=32)  : Effects             — PROJECTILE EXPLOSION
Row 3 (y=48)  : Background tiles    — TILE_EVEN TILE_ODD
```

The `SpriteDef` struct records where each sprite lives:

```c
typedef struct {
    int src_x, src_y;           /* top-left pixel in the atlas */
    int src_w, src_h;           /* region size */
    uint32_t placeholder_color; /* color used when atlas is absent */
    const char *label;          /* text shown on the placeholder rect */
} SpriteDef;
```

### 2.3 Naming convention

Sprite IDs are `SPR_<CATEGORY>_<NAME>` and live in a `SpriteId` enum so the
compiler catches typos:

```c
typedef enum {
    SPR_TOWER_PELLET = 0,
    SPR_TOWER_SQUIRT,
    /* ... */
    SPR_COUNT,
} SpriteId;
```

`SPR_COUNT` acts as a sentinel; the `SPRITE_DEFS` array is exactly this long.

---

## 3. Concept: Placeholder pattern

### 3.1 API-first design

Write the full public API in the header before writing a single pixel of
artwork. The header becomes a *contract* between programmer and artist.

```c
/* Everything the rest of the game needs to know about sprites */
void sprites_init(const char *atlas_path);
void sprites_shutdown(void);
void draw_sprite(Backbuffer *bb, SpriteId id,
                 int dst_x, int dst_y, int dst_w, int dst_h);
void draw_sprite_frame(Backbuffer *bb, SpriteId id, int frame,
                       int dst_x, int dst_y, int dst_w, int dst_h);
```

The implementation file can change completely (from placeholder to real blits)
without touching any call site.

### 3.2 The USE_SPRITES compile-time switch

In `src/game.c` we define:

```c
#define USE_SPRITES 0
```

Then at every rendering site:

```c
#if USE_SPRITES
    draw_sprite(bb, s_tower_spr[t->type], px + 2, py + 2,
                CELL_SIZE - 4, CELL_SIZE - 4);
#else
    uint32_t body_col = (t->place_flash > 0.0f)
                      ? COLOR_WHITE : TOWER_DEFS[t->type].color;
    draw_rect(bb, px + 2, py + 2, CELL_SIZE - 4, CELL_SIZE - 4, body_col);
#endif
```

With `USE_SPRITES 0`, the compiler *erases* every sprite call, as if they were
never there. No runtime overhead at all. Flip to `1` and the sprite path is
compiled in instead.

This is better than a runtime `if (g_sprite_atlas.loaded)` check *at the call
site* because:
- It generates slightly less code in the common no-atlas case
- The intent is explicit — "this build uses sprites or it doesn't"
- It prevents accidental half-and-half states during development

`draw_sprite()` itself still has a runtime check (`!g_sprite_atlas.loaded`) so
it degrades gracefully when the atlas file is missing at runtime.

### 3.3 Placeholder rendering

```c
void draw_sprite(Backbuffer *bb, SpriteId id,
                 int dst_x, int dst_y, int dst_w, int dst_h)
{
    const SpriteDef *def = &SPRITE_DEFS[id];
    if (!g_sprite_atlas.loaded) {
        draw_rect(bb, dst_x, dst_y, dst_w, dst_h, def->placeholder_color);
        if (def->label && def->label[0] != '\0') {
            int tw = text_width(def->label, 1);
            int tx = dst_x + (dst_w - tw) / 2;
            int ty = dst_y + (dst_h - 8)  / 2;
            draw_text(bb, tx, ty, def->label,
                      GAME_RGB(0x00,0x00,0x00), 1);
        }
        return;
    }
    /* real blit here … */
}
```

The label (e.g. `"PLLT"` for PELLET tower, `"BOSS"` for boss creep) makes the
placeholder meaningful — you can screenshot the game and immediately know which
entity is which without cross-referencing the code.

### 3.4 Upgrading from placeholder to real blits

When you have a PNG:

1. Add `stb_image.h` to the project (single-header, MIT license):
   ```
   vendor/stb_image.h
   ```

2. In `sprites.c`, before any other includes, add:
   ```c
   #define STB_IMAGE_IMPLEMENTATION
   #include "../vendor/stb_image.h"
   ```

3. In `sprites_init`, replace the `(void)atlas_path` stub with:
   ```c
   if (atlas_path != NULL) {
       int w, h, channels;
       uint8_t *data = stbi_load(atlas_path, &w, &h, &channels, 4);
       if (data) {
           g_sprite_atlas.pixels = (uint32_t *)data;
           g_sprite_atlas.width  = w;
           g_sprite_atlas.height = h;
           g_sprite_atlas.loaded = 1;
       } else {
           fprintf(stderr, "sprites: failed to load '%s'\n", atlas_path);
       }
   }
   ```

4. In `sprites_shutdown`, free the memory:
   ```c
   if (g_sprite_atlas.pixels) {
       stbi_image_free(g_sprite_atlas.pixels);
   }
   memset(&g_sprite_atlas, 0, sizeof(g_sprite_atlas));
   ```

5. Replace the `/* real blit here */` comment in `draw_sprite` with a
   nearest-neighbour blit loop:
   ```c
   int stride = g_sprite_atlas.width;
   for (int dy = 0; dy < dst_h; dy++) {
       int sy = def->src_y + dy * def->src_h / dst_h;
       for (int dx = 0; dx < dst_w; dx++) {
           int sx = def->src_x + dx * def->src_w / dst_w;
           uint32_t pixel = g_sprite_atlas.pixels[sy * stride + sx];
           int bx = dst_x + dx;
           int by = dst_y + dy;
           if (bx >= 0 && bx < bb->width &&
               by >= 0 && by < bb->height) {
               bb->pixels[by * (bb->pitch / 4) + bx] = pixel;
           }
       }
   }
   ```

6. Call `sprites_init("assets/sprites.png")` and set `USE_SPRITES 1`.

---

## 4. Concept: Animation frames

### 4.1 Horizontal sprite strips

Animation stores multiple frames side-by-side in the atlas. For a creep with a
4-frame walk cycle, the strip looks like:

```
atlas column →
                frame 0  frame 1  frame 2  frame 3
                +------+ +------+ +------+ +------+
row 1 (y=16)   | NRM  | | NRM  | | NRM  | | NRM  |
                |  →   | |  ↓   | |  ←   | |  ↑   |
                +------+ +------+ +------+ +------+
                  16px     16px     16px     16px
```

`SpriteDef.src_w` stores the width of **one frame** (16 px). To select frame 2
you offset: `actual_src_x = def->src_x + frame * def->src_w`.

### 4.2 Timing

In `game_update()`, accumulate a per-creep animation timer:

```c
/* in Creep struct (game.h) */
float anim_timer;
int   anim_frame;

/* in game_update(), per-creep */
#define CREEP_ANIM_FPS 8.0f
c->anim_timer += dt;
if (c->anim_timer >= 1.0f / CREEP_ANIM_FPS) {
    c->anim_timer -= 1.0f / CREEP_ANIM_FPS;
    c->anim_frame  = (c->anim_frame + 1) % 4;
}
```

Then in `game_render()`:

```c
draw_sprite_frame(bb, s_creep_spr[c->type], c->anim_frame,
                  cx - r, cy - r, r * 2, r * 2);
```

### 4.3 Frame selection in draw_sprite_frame

```c
void draw_sprite_frame(Backbuffer *bb, SpriteId id, int frame,
                       int dst_x, int dst_y, int dst_w, int dst_h)
{
    if (!g_sprite_atlas.loaded) {
        /* Placeholder: ignore frame — all frames look the same */
        draw_sprite(bb, id, dst_x, dst_y, dst_w, dst_h);
        return;
    }
    const SpriteDef *def = &SPRITE_DEFS[id];
    /* Offset into the strip */
    int real_src_x = def->src_x + frame * def->src_w;
    /* … same blit loop as draw_sprite but use real_src_x … */
}
```

In placeholder mode the `frame` parameter is intentionally ignored because all
placeholder frames are identical rectangles.

### 4.4 Rotation (towers)

Towers rotate their barrel by tracking `t->angle`. Rather than pre-baking eight
rotation frames, we rotate pixels on the fly:

```c
/* Simple nearest-neighbour rotation blit (pseudocode) */
float cos_a = cosf(-t->angle);
float sin_a = sinf(-t->angle);
for (int dy = 0; dy < dst_h; dy++) {
    for (int dx = 0; dx < dst_w; dx++) {
        /* Rotate (dx,dy) relative to center back into sprite space */
        float fx = cos_a * (dx - dst_w/2) - sin_a * (dy - dst_h/2) + src_w/2;
        float fy = sin_a * (dx - dst_w/2) + cos_a * (dy - dst_h/2) + src_h/2;
        int sx = (int)fx + def->src_x;
        int sy = (int)fy + def->src_y;
        if (sx >= def->src_x && sx < def->src_x + def->src_w &&
            sy >= def->src_y && sy < def->src_y + def->src_h)
            bb->pixels[...] = g_sprite_atlas.pixels[sy * atlas_stride + sx];
    }
}
```

Alternatively, pre-bake 8 or 16 rotation frames in the atlas — a common trick
in classic sprite-based engines.

---

## 5. JavaScript analogy

If you come from web development, here is how the same idea maps:

### 5.1 CSS sprites

CSS sprites are the direct web equivalent of a texture atlas:

```css
.tower-pellet {
    background-image: url('sprites.png');
    background-position: 0px 0px;     /* src_x=0, src_y=0 */
    width: 16px;
    height: 16px;
}
.tower-squirt {
    background-image: url('sprites.png');
    background-position: -16px 0px;   /* src_x=16, src_y=0 */
    width: 16px;
    height: 16px;
}
```

Same single-file benefit; same src_rect indexing approach.

### 5.2 Canvas drawImage

The HTML5 Canvas API has a 9-parameter `drawImage` overload that is a
pixel-exact equivalent of our `draw_sprite`:

```js
// drawImage(image, sx, sy, sw, sh, dx, dy, dw, dh)
ctx.drawImage(
    atlas,                    // our g_sprite_atlas
    def.src_x, def.src_y,    // source top-left in atlas
    def.src_w, def.src_h,    // source size in atlas
    dst_x,    dst_y,         // destination on canvas
    dst_w,    dst_h          // destination size (scales if different)
);
```

Our C function signature mirrors this exactly; the only difference is that we
do the pixel copy in a loop rather than calling a browser built-in.

### 5.3 Placeholder in JavaScript

```js
function drawSprite(ctx, def, dx, dy, dw, dh) {
    if (!atlasLoaded) {
        // Placeholder: colored rect + label
        ctx.fillStyle = def.placeholderColor;
        ctx.fillRect(dx, dy, dw, dh);
        ctx.fillStyle = '#000';
        ctx.font = '8px monospace';
        ctx.fillText(def.label, dx + dw/2 - def.label.length*3, dy + dh/2);
        return;
    }
    ctx.drawImage(atlas, def.src_x, def.src_y, def.src_w, def.src_h,
                  dx, dy, dw, dh);
}
```

Identical structure to our C version. The pattern is language-agnostic.

---

## 6. Implementation walkthrough

### 6.1 File overview

| File | Role |
|---|---|
| `src/sprites.h` | Public API: `SpriteId` enum, `SpriteDef`, `SpriteAtlas`, function declarations |
| `src/sprites.c` | `SPRITE_DEFS[]`, `g_sprite_atlas`, `draw_sprite()`, `draw_sprite_frame()` |
| `src/game.c` | `#include "sprites.h"`, `#define USE_SPRITES 0`, conditional render sites |
| `build-dev.sh` | `src/sprites.c` added to `SHARED_SRCS` |
| `course/assets/README.md` | Asset creation workflow |

### 6.2 src/sprites.h

The header defines three things:

1. **`SpriteId` enum** — one value per visual entity, ending with `SPR_COUNT`.
2. **`SpriteDef` struct** — atlas position + placeholder data for one sprite.
3. **`SpriteAtlas` struct** — runtime atlas state (pixels pointer + loaded flag).

The API surface is intentionally tiny: `sprites_init`, `sprites_shutdown`,
`draw_sprite`, `draw_sprite_frame`. Real games often add `draw_sprite_rotated`,
`draw_sprite_tinted`, etc., but we keep it minimal.

### 6.3 src/sprites.c

`SPRITE_DEFS` is a designated-initializer array — we use `[SPR_TOWER_PELLET] =`
syntax so the indices are self-documenting and resistant to enum reordering:

```c
const SpriteDef SPRITE_DEFS[SPR_COUNT] = {
    [SPR_TOWER_PELLET] = { 0, 0, 16, 16, GAME_RGB(0x88,0x88,0x88), "PLLT" },
    [SPR_TOWER_SQUIRT] = {16, 0, 16, 16, GAME_RGB(0x44,0x88,0xCC), "SQRT" },
    /* … */
};
```

The placeholder colors mirror the `COLOR_*` constants from `game.h` exactly —
if you flip `USE_SPRITES` on in placeholder mode, the game looks the same as
the procedural version.

### 6.4 game.c render sites

We added three `#if USE_SPRITES` blocks:

**Towers** (replaces the tower body `draw_rect`):
```c
static const SpriteId s_tower_spr[TOWER_COUNT] = {
    SPR_TOWER_PELLET,   /* TOWER_NONE  — fallback */
    SPR_TOWER_PELLET,   /* TOWER_PELLET */
    SPR_TOWER_SQUIRT,
    SPR_TOWER_DART,
    SPR_TOWER_SNAP,
    SPR_TOWER_SWARM,
    SPR_TOWER_FROST,
    SPR_TOWER_BASH,
};
draw_sprite(bb, s_tower_spr[t->type], px+2, py+2, CELL_SIZE-4, CELL_SIZE-4);
```

**Creeps** (replaces `draw_circle`):
```c
static const SpriteId s_creep_spr[CREEP_COUNT] = { … };
draw_sprite(bb, s_creep_spr[c->type], cx-r, cy-r, r*2, r*2);
```

**Projectiles** (replaces the 4×4 `draw_rect`):
```c
draw_sprite(bb, SPR_PROJECTILE, (int)p->x - 4, (int)p->y - 4, 8, 8);
```

The barrel line and status tint circles are kept in both modes — they are
overlay effects, not the entity body.

### 6.5 Tower sprite mapping

`TowerType` starts at 1 (`TOWER_PELLET`), so index 0 (`TOWER_NONE`) maps to
`SPR_TOWER_PELLET` as a safe fallback. At runtime `t->type` for active towers
is never `TOWER_NONE`, so the fallback is never shown.

---

## 7. Asset workflow

### 7.1 Obtaining free assets

The easiest starting point is **Kenney.nl** (kenney.nl):

1. Download the "Tower Defense" pack (kenney.nl/assets/tower-defense-top-down)
2. All assets are CC0 — no attribution required, commercial use allowed
3. The pack includes: ground tiles, tower sprites, creep sprites, projectiles

For custom 16 × 16 pixel art:
- **Libresprite** (free, open-source fork of Aseprite)
- **Piskel** (browser-based, no install)
- **Aseprite** (paid, but the gold standard for pixel art)

### 7.2 Packing individual sprites into an atlas

```bash
# 1. Resize all raw sprites to 16×16 (ImageMagick)
mkdir -p sized
for f in raw/*.png; do
    convert "$f" -resize 16x16! "sized/$(basename "$f")"
done

# 2. Pack into a row (example: 7 tower sprites → 112×16 strip)
convert +append sized/pellet.png sized/squirt.png sized/dart.png \
                sized/snap.png   sized/swarm.png  sized/frost.png \
                sized/bash.png   row_towers.png

# 3. Combine rows vertically into the final atlas
convert -append row_towers.png row_creeps.png row_fx.png row_tiles.png \
        assets/sprites.png

# 4. Verify
identify assets/sprites.png
```

### 7.3 Verifying the atlas layout

Write a tiny test program (or use the game itself with debug output) to print
the coordinates that `SPRITE_DEFS` would blit:

```c
for (int i = 0; i < SPR_COUNT; i++) {
    const SpriteDef *d = &SPRITE_DEFS[i];
    printf("ID %2d: atlas (%d,%d) %dx%d  label='%s'\n",
           i, d->src_x, d->src_y, d->src_w, d->src_h, d->label);
}
```

Compare the printed coordinates against the atlas image (open in an editor with
pixel-grid overlay).

### 7.4 X11 pixel format note

Our backbuffer uses `0xAARRGGBB`. `stb_image` loads as `RGBA` (bytes: R, G, B,
A). On X11 the display expects `BGR`. When loading for the X11 backend, swap R
and B after calling `stbi_load`:

```c
uint8_t *data = stbi_load(path, &w, &h, &channels, 4);
/* Swap R↔B for X11 */
for (int i = 0; i < w * h; i++) {
    uint8_t *p = data + i * 4;
    uint8_t tmp = p[0]; p[0] = p[2]; p[2] = tmp;
}
```

The Raylib backend does its own swap when uploading to the GPU texture, so no
manual swap is needed there.

---

## 8. Checkpoints

Work through these in order. Each one should leave the game in a buildable,
runnable state.

### Checkpoint A — New files compile

```bash
cd course
./build-dev.sh --backend=raylib
```

Expected: **zero errors, zero warnings**. The game runs identically to before
(USE_SPRITES is 0 so no code path changed).

### Checkpoint B — Sprite drawing in placeholder mode

Edit `src/game.c`, change:
```c
#define USE_SPRITES 0
```
to:
```c
#define USE_SPRITES 1
```

Rebuild:
```bash
./build-dev.sh --backend=raylib
```

Run the game. You should see:
- Tower cells filled with colored rectangles + short labels (`PLLT`, `SQRT`, etc.)
- Creep circles replaced by colored squares with labels (`NRM`, `FST`, etc.)
- Projectiles replaced by slightly larger yellow squares with `PRJ` label

The labels confirm the sprite ID mapping is correct before any real art exists.

Revert to `USE_SPRITES 0` before committing.

### Checkpoint C — Atlas path passes through

Add a temporary `printf` inside `sprites_init`:
```c
printf("sprites_init: atlas_path='%s'\n",
       atlas_path ? atlas_path : "(null)");
```

Run the game — you should see `sprites_init: atlas_path='(null)'` in the
terminal. Remove the printf after confirming.

### Checkpoint D — (Optional, for when you have art)

1. Place `sprites.png` in `course/assets/`
2. Wire in `stb_image` as described in section 3.4
3. Call `sprites_init("assets/sprites.png")` in `game_init`
4. Set `USE_SPRITES 1`
5. Verify entities show pixel art instead of colored rects

---

## 9. Exercises

**Exercise 1 — Add a new sprite ID**

Add `SPR_TOWER_PELLET_UPGRADED` to `SpriteId` in `sprites.h`. Add a matching
entry to `SPRITE_DEFS` in `sprites.c` with a slightly different placeholder
color (e.g. brighter gray `0xBBBBBB`). Verify the build still succeeds.

**Exercise 2 — Explosion particle sprite**

When a creep dies, the game spawns cosmetic `Particle` structs. Currently they
draw as colored rectangles. Modify the particle renderer in `game.c` so that
when `USE_SPRITES 1`, it calls:
```c
draw_sprite(bb, SPR_EXPLOSION, (int)p->x - p->size/2, (int)p->y - p->size/2,
            p->size, p->size);
```
only for non-text particles.

**Exercise 3 — Animated creep**

Add an `anim_frame` and `anim_timer` field to the `Creep` struct in `game.h`.
In `game_update()`, advance `anim_timer` by `dt` and increment `anim_frame`
every `1.0f/8.0f` seconds (8 FPS animation). In `game_render()` with
`USE_SPRITES 1`, call `draw_sprite_frame()` instead of `draw_sprite()`.

Since the atlas has no real animation frames yet, the result should look
identical — but the timing machinery is in place for when real frames arrive.

**Exercise 4 — Background tile sprites**

The grid currently uses `draw_rect` for every cell. Add a `#if USE_SPRITES`
branch that calls:
```c
draw_sprite(bb, ((c+r)&1) ? SPR_BACKGROUND_TILE_ODD : SPR_BACKGROUND_TILE_EVEN,
            px, py, CELL_SIZE, CELL_SIZE);
```
Verify the grid looks identical in placeholder mode (same colors as before).

**Exercise 5 — Unit test for sprite IDs**

Write a small C program `tools/check_sprites.c` that:
1. Includes `sprites.h` and `sprites.c`
2. Calls `sprites_init(NULL)`
3. Asserts that every `SPRITE_DEFS[i].label` is non-NULL
4. Asserts that `SPRITE_DEFS[SPR_TOWER_PELLET].placeholder_color != 0`
5. Prints "All sprite defs OK" and exits 0

Add it to `build-dev.sh` as an optional `--check-sprites` flag.

---

## 10. Production upgrades (implemented in this lesson's update)

This section documents the improvements made to the sprite system beyond the
initial placeholder-only design.

### 10.1 SPR_MISSING — the "missing texture" placeholder

Whenever `draw_sprite()` receives an out-of-range ID, or when the atlas PNG
failed to load, it renders a **magenta checkerboard** — the industry-standard
"missing texture" indicator (popularized by Valve's Source Engine).

```c
/* src/sprites.h */
SPR_MISSING,   /* magenta drawn on invalid IDs or atlas load failure */
SPR_COUNT,
```

The pattern is drawn in `draw_missing_placeholder()` inside `sprites.c`:
- 4 × 4 pixel tiles alternating between magenta `0xFF00FF` and black `0x000000`
- `"???"` text centered in white

This means a misconfigured `SpriteId` is immediately visible at runtime rather
than silently drawing the wrong colored rectangle.

### 10.2 SpriteLoadState — explicit loading lifecycle

The original `SpriteAtlas.loaded` int (0 or 1) only expressed two states.
The production system adds four:

```c
typedef enum {
    SPRITE_LOAD_IDLE    = 0,  /* init not yet called              */
    SPRITE_LOAD_LOADING = 1,  /* background thread running        */
    SPRITE_LOAD_READY   = 2,  /* atlas pixels available           */
    SPRITE_LOAD_ERROR   = 3,  /* failed — error_msg has details   */
} SpriteLoadState;
```

This lets the renderer show a "Loading…" HUD, and distinguishes "we chose not
to load" (IDLE) from "we tried and failed" (ERROR).

### 10.3 Async loading with pthreads

`sprites_load_async(path)` spawns a background thread to load the PNG, keeping
the game fully interactive while the atlas loads:

```c
void sprites_load_async(const char *atlas_path)
{
    strncpy(g_async_path, atlas_path, sizeof(g_async_path) - 1);
    g_sprite_atlas.load_state = SPRITE_LOAD_LOADING;
    pthread_create(&g_load_thread, NULL, load_thread_fn, (void *)g_async_path);
    g_thread_started = 1;
}
```

The main loop can poll readiness each frame:
```c
if (!sprites_is_ready()) {
    /* still loading — draw a progress indicator */
} else if (g_sprite_atlas.load_state == SPRITE_LOAD_ERROR) {
    fprintf(stderr, "[SPRITES] %s\n", g_sprite_atlas.error_msg);
}
```

`sprites_shutdown()` calls `pthread_join()` before freeing memory, ensuring
the thread finishes before the atlas buffer is freed.

### 10.4 Error logging convention

Every load failure writes to `stderr`:

```
[SPRITES] ERROR: stbi_load("assets/sprites/atlas.png") failed: can't fopen
[SPRITES] ERROR: Out of memory allocating atlas (4096×4096)
[SPRITES] Loaded atlas: assets/sprites/atlas.png (256×256)
```

`g_sprite_atlas.error_msg` also holds the last error string so the game can
display it in a debug overlay.

### 10.5 STB_IMAGE_STATIC — avoiding Raylib symbol conflict

Raylib bundles its own copy of stb_image internally.  If we also define
`STB_IMAGE_IMPLEMENTATION`, the linker complains about multiple definitions of
`stbi_load`, `stbi__png_load`, etc.

The fix: use `STB_IMAGE_STATIC`, which prefixes every stb function with
`static`.  Now our copy is private to `sprites.c`'s translation unit and
Raylib's copy lives in `rtextures.o` — no collision:

```c
#define STBI_ONLY_PNG           /* only PNG support needed */
#define STB_IMAGE_STATIC        /* make all stbi_* functions static  */
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"
```

### 10.6 Asset folder structure

```
course/assets/sprites/
  atlas.png          ← main PNG atlas (place yours here)
  towers/            ← source PNGs for individual towers
  creeps/            ← source PNGs for individual creeps
  tiles/             ← terrain tiles (grass, water, mountain)
  effects/           ← projectile and explosion frames
  README.md          ← full workflow documentation
```

See `course/assets/sprites/README.md` for how to build the atlas from
individual PNGs using ImageMagick, plus links to free CC0 asset packs.

---

## 11. Further reading

- **stb_image** (nothings.org/stb) — single-header PNG/JPG/BMP loader, the
  simplest way to load an atlas in C without dependencies.

- **Texture atlases in practice** — "Optimizing 2D Rendering" on GDC Vault
  (2010, EA) explains GPU-side batch rendering benefits in detail.

- **Kenney.nl** — kenney.nl/assets — large collection of CC0 game art, all
  organized into consistent 16×16 or 32×32 grids.

- **libresprite.github.io** — free, open-source pixel-art editor; supports
  animation, onion-skin, sprite-sheet export.

- **ImageMagick documentation** — imagemagick.org/script/montage.php — the
  `montage` command used to assemble individual sprites into an atlas.

- **`course/assets/sprites/README.md`** — asset folder structure, free CC0
  asset sources, and ImageMagick atlas assembly workflow.

---

*End of Lesson 24.*
