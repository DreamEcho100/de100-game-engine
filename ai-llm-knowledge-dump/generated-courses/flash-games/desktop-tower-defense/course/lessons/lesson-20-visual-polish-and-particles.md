# Lesson 20: Visual Polish and Particles

## What we're building

The game goes from functional to *fun-feeling*.  Creep deaths pop with radial
particles.  Boss kills explode with 15 flying fragments.  Bash-stunned creeps
spin three small yellow stars around their bodies.  Frost towers emit an
expanding ice-ring pulse when they fire.  Tower placements flash briefly.
Game-over and victory screens use semi-transparent dark overlays so the
battlefield remains visible underneath the result text.

## What you'll learn

- `spawn_explosion()`: radial particle burst using a per-function RNG and
  `cosf` / `sinf` directions
- How to drive a stun-star animation with only a `stun_timer` field already
  present on the `Creep` struct
- How `draw_rect_blend()` composites an alpha value over the existing canvas
  pixels without a separate alpha channel on the backbuffer
- How to attach a temporary ring animation to a tower without adding permanent
  struct fields

## Prerequisites

- Lessons 01–19 complete: the particle pool (`Particle particles[MAX_PARTICLES]`,
  `spawn_particle()`, `update_particles()`, `draw_particles()`) is fully
  implemented
- `Creep.stun_timer` and `Tower.frost_ring_timer` exist in the structs
- `draw_rect`, `draw_text`, `draw_rect_outline`, and `draw_circle_outline`
  all exist in `render.c`
- `<math.h>` is included in `render.c` and `creeps.c`

---

## Step 1: `spawn_explosion()`

```c
/* src/creeps.c — spawn_explosion(): radial particle burst */
void spawn_explosion(GameState *state, float x, float y,
                     uint32_t color, int count) {
    /* File-scope RNG — keeps the function re-entrant without globals leaking */
    static unsigned int rng = 7654321u;

    for (int i = 0; i < count; i++) {
        /* Advance RNG twice: once for angle, once for speed */
        rng = rng * 1664525u + 1013904223u;
        float angle = ((float)(rng & 0xFFFFu) / 65536.0f) * 6.283185f; /* 2π */

        rng = rng * 1664525u + 1013904223u;
        float speed = 30.0f + ((float)((rng >> 16) & 0xFFu) / 255.0f) * 40.0f;

        float vx = cosf(angle) * speed;
        float vy = sinf(angle) * speed;

        spawn_particle(state, x, y, vx, vy,
                       0.4f,              /* lifetime: 0.4 s     */
                       color,
                       PARTICLE_DOT,      /* rendered as 3×3 px  */
                       "");
    }
}
```

**What's happening:**

- `static unsigned int rng` is local to the function — it persists across
  calls but is invisible to other modules.  Each call advances the LCG, so
  successive explosions look different without seeding overhead.
- `rng & 0xFFFF` gives a 16-bit value for angle — enough for a visually
  uniform circle.  The top bits `(rng >> 16) & 0xFF` provide speed variation.
- `PARTICLE_DOT` type means the draw loop renders a 3×3 filled rectangle at
  the particle's current `(x, y)` — cheap and readable.

---

## Step 2: Wire `spawn_explosion` into `kill_creep()`

```c
/* src/creeps.c — kill_creep(): add particle bursts */
void kill_creep(GameState *state, int idx) {
    Creep *c = &state->creeps[idx];

    /* ... existing gold award and spawn-child logic ... */

    /* Particle burst on death */
    int count = (c->type == CREEP_BOSS) ? 15 : 5;
    spawn_explosion(state, c->x, c->y, CREEP_DEFS[c->type].color, count);

    c->active = 0;
}
```

**What's happening:**

- Normal creeps produce 5 particles — enough to read as a pop without
  overwhelming 20 simultaneous kills.
- Boss death produces 15 particles in the boss body colour (dark red), which
  creates a visually satisfying large explosion.
- If `MAX_PARTICLES` is exhausted, `spawn_particle()` returns without crashing
  (it scans for an inactive slot and early-returns on failure).

---

## Step 3: Stun stars for Bash-stunned creeps

Bash towers (from `TOWER_BASH`) set `creep->stun_timer` on hit.  While it
is positive, the render loop draws three small yellow ★ shapes rotating
around the creep body.

```c
/* src/render.c — inside draw_creep(), after drawing the body */
if (c->stun_timer > 0.0f) {
    /* Star base angle increases over time to create a spin */
    float base_angle = c->stun_timer * 10.0f; /* radians, increases each frame */
    int   star_r     = (int)c->size / 2 + 5;  /* orbit radius                 */

    for (int s = 0; s < 3; s++) {
        float a  = base_angle + (float)s * 2.094395f; /* 2π/3 per star */
        int   sx = (int)(c->x + cosf(a) * (float)star_r);
        int   sy = (int)(c->y + sinf(a) * (float)star_r);

        /* Draw a simple 5×5 yellow dot as a star stand-in */
        draw_rect(bb, sx - 2, sy - 2, 5, 5, GAME_RGB(0xFF, 0xFF, 0x00));
        /* Bright centre pixel */
        draw_rect(bb, sx - 1, sy - 1, 3, 3, GAME_RGB(0xFF, 0xFF, 0xCC));
    }
}
```

**What's happening:**

- `stun_timer` is already decremented in `game_update()` by `dt`.  The render
  code just reads it — the same field drives both logic and visuals.
- `2.094395f` is `2π / 3` — three evenly spaced stars.
- The spin speed is `10 radians per second of stun time remaining`.  Because
  `stun_timer` decreases, the spin slows and stops naturally as the stun
  expires.

---

## Step 4: Frost ring pulse

When a Frost tower fires, set `tower->frost_ring_timer = 0.25f`.  Each frame
the ring grows and fades.

```c
/* src/towers.c — frost tower fire callback */
if (t->type == TOWER_FROST && t->fire_cooldown <= 0.0f) {
    /* ... normal hit and slow logic ... */
    t->frost_ring_timer = 0.25f;   /* start ring animation */
}

/* src/game.c — game_update(): decrement frost_ring_timer */
for (int i = 0; i < MAX_TOWERS; i++) {
    Tower *t = &state->towers[i];
    if (!t->active) continue;
    if (t->frost_ring_timer > 0.0f)
        t->frost_ring_timer -= dt;
}
```

Render the ring in `draw_towers()`:

```c
/* src/render.c — inside draw_towers(), after drawing the tower body */
if (t->type == TOWER_FROST && t->frost_ring_timer > 0.0f) {
    /* Ring expands outward as timer counts down */
    float progress = 1.0f - (t->frost_ring_timer / 0.25f); /* 0 → 1 */
    int   radius   = (int)(progress * (float)TOWER_DEFS[TOWER_FROST].range);

    /* Brightness fades as the ring expands */
    int   bright = (int)(255.0f * (1.0f - progress));
    uint32_t ring_color = GAME_RGBA(bright, bright, 0xFF, 0xFF);

    draw_circle_outline(bb,
                        t->cx, t->cy,   /* tower centre pixels */
                        radius,
                        ring_color);
}
```

**What's happening:**

- `progress` goes from 0 (just fired) to 1 (animation complete).
- The ring radius linearly approaches the tower's actual range, giving the
  player a visual confirmation of exactly what got frozen.
- Colour goes from `RGB(255, 255, 255)` (white) to `RGB(0, 0, 255)` (blue)
  as it fades — achieved by decrementing the R and G channels while keeping B
  at 255.

---

## Step 5: `draw_rect_blend()` — alpha overlay without a real alpha channel

The backbuffer stores opaque `ARGB` pixels.  To draw a semi-transparent
overlay, we blend each pixel manually:

```c
/* src/render.c — draw_rect_blend(): alpha composite over existing pixels */
void draw_rect_blend(GameBackbuffer *bb,
                     int x, int y, int w, int h,
                     uint32_t color) {
    /* Extract overlay alpha [0..255] */
    int a = (int)((color >> 24) & 0xFF);
    if (a == 0)   return;
    if (a == 255) { draw_rect(bb, x, y, w, h, color); return; }

    int oa = 255 - a;   /* one-minus-alpha weight for destination */

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > bb->width  ? bb->width  : (x + w);
    int y1 = (y + h) > bb->height ? bb->height : (y + h);

    int src_r = (int)((color >> 16) & 0xFF);
    int src_g = (int)((color >>  8) & 0xFF);
    int src_b = (int)( color        & 0xFF);

    for (int py = y0; py < y1; py++) {
        uint32_t *row = bb->pixels + py * bb->width;
        for (int px = x0; px < x1; px++) {
            uint32_t dst = row[px];
            int dr = (int)((dst >> 16) & 0xFF);
            int dg = (int)((dst >>  8) & 0xFF);
            int db = (int)( dst        & 0xFF);

            int nr = (src_r * a + dr * oa) >> 8;
            int ng = (src_g * a + dg * oa) >> 8;
            int nb = (src_b * a + db * oa) >> 8;

            row[px] = GAME_RGB(nr, ng, nb);
        }
    }
}
```

**What's happening:**

- `>> 8` is equivalent to dividing by 256, approximating the /255 blend.  The
  maximum rounding error is 1 LSB — invisible.
- Early-exit for `a == 255` keeps the common case (solid overlays) at the
  speed of `draw_rect`.
- We read `color >> 24` for alpha, matching the `GAME_RGBA(r,g,b,a)` macro
  which places alpha in the highest byte.

---

## Step 6: Phase overlay screens

```c
/* src/render.c — draw_phase_overlay(): called from game_render() */
void draw_phase_overlay(GameBackbuffer *bb, const GameState *state) {
    switch (state->phase) {

    case GAME_PHASE_TITLE:
        draw_rect_blend(bb, 0, 0, CANVAS_W, CANVAS_H, GAME_RGBA(0, 0, 0, 180));
        draw_text_centered(bb, CANVAS_H / 2 - 20, "Desktop Tower Defense",
                           COLOR_WHITE, FONT_LARGE);
        draw_text_centered(bb, CANVAS_H / 2 + 4,  "Click to start",
                           GAME_RGB(0xAA, 0xAA, 0xAA), FONT_NORMAL);
        break;

    case GAME_PHASE_GAME_OVER:
        draw_rect_blend(bb, 0, 0, CANVAS_W, CANVAS_H, GAME_RGBA(0, 0, 0, 160));
        draw_text_centered(bb, CANVAS_H / 2 - 20, "GAME OVER",
                           GAME_RGB(0xFF, 0x33, 0x33), FONT_LARGE);
        {
            char buf[48];
            snprintf(buf, sizeof(buf), "Reached wave %d", state->current_wave);
            draw_text_centered(bb, CANVAS_H / 2 + 6, buf,
                               COLOR_WHITE, FONT_NORMAL);
        }
        draw_text_centered(bb, CANVAS_H / 2 + 22, "Press R to restart",
                           GAME_RGB(0xAA, 0xAA, 0xAA), FONT_NORMAL);
        break;

    case GAME_PHASE_VICTORY:
        draw_rect_blend(bb, 0, 0, CANVAS_W, CANVAS_H, GAME_RGBA(0, 0, 40, 180));
        draw_text_centered(bb, CANVAS_H / 2 - 30, "YOU WIN!",
                           GAME_RGB(0xFF, 0xDD, 0x00), FONT_LARGE);
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "Gold remaining: %d", state->player_gold);
            draw_text_centered(bb, CANVAS_H / 2 - 8, buf,
                               COLOR_GOLD_TEXT, FONT_NORMAL);
            snprintf(buf, sizeof(buf), "Lives remaining: %d", state->player_lives);
            draw_text_centered(bb, CANVAS_H / 2 + 8, buf,
                               GAME_RGB(0xFF, 0x66, 0x66), FONT_NORMAL);
        }
        draw_text_centered(bb, CANVAS_H / 2 + 26, "Press R to play again",
                           GAME_RGB(0xAA, 0xAA, 0xAA), FONT_NORMAL);
        break;

    default:
        break;
    }
}
```

---

## Step 7: Tower-placement flash

Store a `place_flash_timer` on the tower (0.15 s):

```c
/* src/game.c — place_tower(): set flash timer */
t->place_flash_timer = 0.15f;

/* src/game.c — game_update(): decrement */
if (t->place_flash_timer > 0.0f) t->place_flash_timer -= dt;

/* src/render.c — draw_towers(): bright outline during flash */
if (t->place_flash_timer > 0.0f) {
    draw_rect_outline(bb, tx - 2, ty - 2, t_size + 4, t_size + 4,
                      GAME_RGB(0xFF, 0xFF, 0xFF));
}
```

---

## Build and run

```bash
mkdir -p build

# Raylib
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG \
      -o build/dtd \
      src/main_raylib.c src/game.c src/render.c \
      src/creeps.c src/towers.c src/levels.c src/bfs.c \
      -lraylib -lm
./build/dtd

# X11
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG \
      -o build/dtd \
      src/main_x11.c src/game.c src/render.c \
      src/creeps.c src/towers.c src/levels.c src/bfs.c \
      -lX11 -lm
./build/dtd
```

**Expected:** Creep deaths produce small coloured particle pops.  Boss death
generates a large 15-particle explosion.  Bash-stunned creeps display three
spinning yellow dots.  Frost tower shots produce an expanding blue-white ring.
Freshly placed towers flash white for 0.15 seconds.  Game-over and victory
screens show semi-transparent overlays over the visible battlefield.

---

## Exercises

1. **Beginner** — Increase the normal creep explosion to 8 particles and the
   boss to 20.  Observe the visual difference at a busy moment in wave 10.

2. **Intermediate** — The frost ring currently uses `draw_circle_outline`.
   Replace it with four `draw_rect_outline` calls at +45° rotations to create
   a diamond-shaped pulse effect instead of a circle.

3. **Challenge** — Implement screen shake: when a Boss dies, store a
   `shake_timer = 0.4f` and `shake_magnitude = 6` in `GameState`.  In the
   render function, offset every `draw_rect` call by
   `(int)(cosf(shake_timer * 50.0f) * shake_magnitude)` pixels horizontally.
   Decrease `shake_magnitude` linearly to 0 as `shake_timer` expires.

---

## What's next

In Lesson 21, the final lesson, we add a complete **audio system** with a PCM
phase-accumulator synthesiser, per-tower fire sounds, creep death / life-lost /
wave-complete events, ambient background music, and platform integration for
both Raylib and X11/ALSA.
