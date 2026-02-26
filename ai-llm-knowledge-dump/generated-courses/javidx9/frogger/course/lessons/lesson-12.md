# Lesson 12 — Death Flash, Win State, Bitmap Font HUD + Final Integration

## By the end of this lesson you will have:

Death animations with frog flashing, a win overlay, a bitmap-font HUD showing homes reached, and a complete integrated build verified with `valgrind` — the final piece of the frogger implementation.

---

## Step 1 — Death state and timer

```c
/* in GameState */
int   dead;
float dead_timer;   /* counts down from DEAD_ANIM_TIME */
```

```c
#define DEAD_ANIM_TIME 0.4f

/* in frogger_tick */
if (state->dead) {
    state->dead_timer -= delta_time;
    if (state->dead_timer <= 0.0f) {
        /* Reset frog to start */
        state->dead = 0;
        state->frog_x = START_X;
        state->frog_y = START_Y;
    }
    return;  /* skip all other game logic while dead */
}
```

While `dead_timer > 0`, the game pauses — no movement, no input, no collision. The timer drains over 0.4 seconds, then the frog resets.

---

## Step 2 — Death flash in `frogger_render`

```c
int show_frog = 1;
if (state->dead) {
    int flash_step = (int)(state->dead_timer / 0.05f);
    show_frog = (flash_step % 2 == 0);
}
```

`DEAD_ANIM_TIME = 0.4f`, step size `0.05f`:
- `dead_timer = 0.40` → `flash_step = 8` → even → show
- `dead_timer = 0.35` → `flash_step = 7` → odd → hide
- `dead_timer = 0.30` → `flash_step = 6` → even → show
- ...5 complete blink cycles over 0.4 seconds.

The frog blinks 5 times before disappearing and resetting. The background lanes continue scrolling during the animation (only `frogger_tick` returns early — `frogger_render` always draws the full scene).

---

## Step 3 — DEAD! overlay

```c
if (state->dead) {
    /* Semi-transparent red overlay */
    draw_rect_blend(bb, 0, 0, bb->width, bb->height,
        FROGGER_RGBA(180, 30, 30, 100));

    /* "DEAD!" text centered */
    draw_text(bb,
        bb->width / 2 - 20,
        bb->height / 2 - 4,
        "DEAD!", 2, FROGGER_RGB(255, 255, 255));
}
```

`draw_rect_blend` does per-pixel alpha compositing:
```c
out_r = (src_r * src_a + dst_r * (255 - src_a)) / 255;
```
Alpha=100 (of 255) → 39% red overlay. The lane tiles still show through, giving a visual indication of where the frog died.

---

## Step 4 — Win state

```c
/* in GameState */
int won;
```

```c
/* in frogger_tick */
if (state->won) {
    /* Nothing — freeze everything until restart */
    return;
}
```

```c
/* in frogger_render */
if (state->won) {
    draw_rect_blend(bb, 0, 0, bb->width, bb->height,
        FROGGER_RGBA(30, 180, 30, 120));
    draw_text(bb,
        bb->width / 2 - 30,
        bb->height / 2 - 4,
        "YOU WIN!", 2, FROGGER_RGB(255, 255, 0));
}
```

Yellow "YOU WIN!" on a green overlay. The game freezes permanently — press Escape (`input.quit`) to exit.

---

## Step 5 — Bitmap font HUD

```c
/* In frogger_render, drawn last (on top of everything) */
char hud[32];
snprintf(hud, sizeof(hud), "HOMES: %d", state->homes_reached);
draw_text(bb, 8, 8, hud, 1, FROGGER_RGB(255, 255, 255));
```

`draw_text` uses the 5×7 glyph table defined in `frogger.c`:

```c
static const uint8_t FONT_GLYPHS[96][7] = { /* 0x20..0x7F */ };

static void draw_char(FroggerBackbuffer *bb, int x, int y,
                       char c, int scale, uint32_t color) {
    if (c < 0x20 || c > 0x7F) return;
    const uint8_t *glyph = FONT_GLYPHS[c - 0x20];
    for (int gy = 0; gy < 7; gy++) {
        for (int gx = 0; gx < 5; gx++) {
            if (glyph[gy] & (1 << (4 - gx))) {
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        if (x+gx*scale+sx >= 0 && x+gx*scale+sx < bb->width &&
                            y+gy*scale+sy >= 0 && y+gy*scale+sy < bb->height)
                            bb->pixels[(y+gy*scale+sy)*bb->width + x+gx*scale+sx] = color;
            }
        }
    }
}
```

Each glyph is 5 bits wide × 7 rows tall, stored as 7 bytes where each byte's high 5 bits encode one row. Scale=1 → 5×7px. Scale=2 → 10×14px.

`draw_text` calls `draw_char` for each character, advancing `x` by `(5+1) * scale` per character.

---

## Step 6 — Memory and final build

The only heap allocation in the entire program:
```c
bb.pixels = malloc(SCREEN_PX_W * SCREEN_PX_H * sizeof(uint32_t));
```

Everything else — `GameState`, `SpriteBank` buffers, `danger[]`, lane data arrays — are stack-allocated or static.

**Valgrind verification:**
```bash
valgrind --leak-check=full ./build/frogger_x11
# Expected: 0 bytes definitely lost
# One alloc from malloc (backbuffer), freed in cleanup
```

**Build flags summary:**
```
X11:    clang -std=c99 -Wall -Wextra -Wno-unused-parameter
        -DASSETS_DIR=\"assets\" -O2
        src/frogger.c src/main_x11.c
        -lX11 -lxkbcommon -lGL -lGLX -lm

Raylib: clang -std=c99 -Wall -Wextra -Wno-unused-parameter
        -DASSETS_DIR=\"assets\" -O2
        src/frogger.c src/main_raylib.c
        -lraylib -lm
```

---

## Full Rendering Order in `frogger_render`

```c
void frogger_render(const GameState *state, FroggerBackbuffer *bb) {
    /* 1. Clear */
    draw_rect(bb, 0, 0, bb->width, bb->height, FROGGER_RGB(0,0,0));

    /* 2. Lanes (all 10 rows, background to foreground) */
    for (int y = 0; y < NUM_LANES; y++) { /* draw_sprite_partial */ }

    /* 3. Frog (with death flash) */
    if (show_frog) { draw_sprite_partial(...); }

    /* 4. HUD (always on top) */
    draw_text(bb, 8, 8, hud, 1, white);

    /* 5. Overlays (DEAD! or YOU WIN!) */
    if (state->dead) { draw_rect_blend + draw_text; }
    if (state->won)  { draw_rect_blend + draw_text; }
}
```

**Why overlays last?**
The overlay must cover HUD text too (you don't want "HOMES: 3" showing through a death screen). Drawing overlays after HUD ensures full coverage.

---

## Key Concepts

- `dead_timer` counts down; `frogger_tick` returns early while dead (game frozen)
- `frogger_render` always runs — lanes scroll during death animation
- Flash: `(int)(dead_timer / 0.05f) % 2` — 10Hz blink, 5 cycles over 0.4 seconds
- `draw_rect_blend` → semi-transparent overlay using per-pixel alpha compositing
- Win state: `frogger_tick` returns early permanently; Escape to quit
- Bitmap font: 5×7 glyphs, 7 bytes each, scale parameter for 1× or 2× size
- Only one `malloc` (backbuffer pixels); zero leaks
- Render order: clear → lanes → frog → HUD → overlays

---

## Final Integration Checklist

- [ ] `frogger_init` initializes all `GameState` fields including `danger[]`
- [ ] `frogger_tick` order: process_hop → carry → off_screen → build_danger → collision → win → update_time
- [ ] `frogger_render` order: clear → lanes → frog → HUD → overlays
- [ ] `platform_display_backbuffer` uploads `bb->pixels` via `glTexImage2D` or `UpdateTexture`
- [ ] Both builds: zero warnings with `-Wall -Wextra`
- [ ] `valgrind`: zero leaks

## Exercise

Add a "lives" counter (`int lives = 3`). Each death decrements `lives`. When `lives == 0`, switch to a "GAME OVER" state instead of resetting the frog. Display `"LIVES: N"` in the HUD next to `"HOMES: N"`. What order should the HUD elements be drawn in? How does this interact with the existing `dead`/`won` state machine?
