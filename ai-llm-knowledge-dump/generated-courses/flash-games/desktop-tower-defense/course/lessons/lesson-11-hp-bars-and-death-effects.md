# Lesson 11: HP Bars and Death Effects

## What we're building

Green→yellow→red HP bars rendered above every creep, a brief shrinking death
animation when a creep's HP reaches zero, and a floating "+N gold" text particle
that rises from the kill position.  By the end of this lesson every creep kill
has visible feedback and the player can quickly read battlefield health at a
glance.

## What you'll learn

- How to derive a color from a ratio using a simple threshold (CLAMP-based color)
- How to drive a death animation with a `death_timer` field instead of immediately
  deactivating the creep
- The **object pool** pattern applied to a `Particle` array — the same technique
  AAA engines use to avoid per-frame allocation
- How to render alpha-blended text and colored rectangles from a particle pool

## Prerequisites

- Lessons 01–10 complete: creeps move along the path, take damage from
  projectiles, and award gold on kill
- `draw_rect` and `draw_text` helpers exist in `render.c`
- `GAME_RGBA(r,g,b,a)` macro produces a 0xAARRGGBB color

---

## Step 1: HP bar constants and the draw helper

Add these constants to `game.h`:

```c
/* src/game.h — HP bar colors */
#define COLOR_HP_FULL   GAME_RGB(0x22, 0xDD, 0x44)   /* green  */
#define COLOR_HP_MID    GAME_RGB(0xFF, 0xCC, 0x00)   /* yellow */
#define COLOR_HP_LOW    GAME_RGB(0xEE, 0x22, 0x11)   /* red    */
#define COLOR_HP_BG     GAME_RGB(0x33, 0x33, 0x33)   /* dark bg bar */
```

In `render.c`, add `draw_hp_bar()`:

```c
/* render.c — draw an HP bar above a creep cell */
void draw_hp_bar(GameBackbuffer *bb, int cx, int cy, float hp_ratio) {
    /* cx, cy = top-left pixel of the creep cell */
    int bar_x = cx + 2;
    int bar_y = cy - 6;
    int bar_max_w = CELL_SIZE - 4;
    int bar_h = 3;

    /* dark background */
    draw_rect(bb, bar_x, bar_y, bar_max_w, bar_h, COLOR_HP_BG);

    /* colored fill */
    int bar_w = (int)((float)bar_max_w * hp_ratio);
    if (bar_w < 1) bar_w = 1;

    uint32_t color;
    if (hp_ratio > 0.60f)      color = COLOR_HP_FULL;
    else if (hp_ratio > 0.33f) color = COLOR_HP_MID;
    else                        color = COLOR_HP_LOW;

    draw_rect(bb, bar_x, bar_y, bar_w, bar_h, color);
}
```

**What's happening:**

- `hp_ratio = creep->hp / creep->max_hp` (computed at the call site).
- The dark background bar is always full-width; the colored fill is proportional.
- Three threshold levels map to green/yellow/red — no floating-point multiply
  in the hot path beyond the single `(int)((float)bar_max_w * hp_ratio)`.

---

## Step 2: Call `draw_hp_bar` from the creep render loop

In `render.c` inside the creep rendering block:

```c
/* render.c — creep render loop (excerpt) */
for (int i = 0; i < MAX_CREEPS; i++) {
    Creep *c = &state->creeps[i];
    if (!c->active && c->death_timer <= 0.0f) continue;

    int px = (int)c->x;
    int py = (int)c->y;

    /* During death animation: shrink the sprite toward its center */
    float scale = 1.0f;
    if (c->death_timer > 0.0f)
        scale = c->death_timer / CREEP_DEATH_DURATION;

    int half = (int)((float)(CELL_SIZE / 2) * scale);
    int cx = px + CELL_SIZE / 2 - half;
    int cy = py + CELL_SIZE / 2 - half;
    int sz = half * 2;
    if (sz < 1) sz = 1;

    draw_rect(bb, cx, cy, sz, sz, CREEP_DEFS[c->type].color);

    /* Only draw HP bar while alive */
    if (c->active && c->max_hp > 0) {
        float ratio = (float)c->hp / (float)c->max_hp;
        draw_hp_bar(bb, px, py, ratio);
    }
}
```

---

## Step 3: `death_timer` field and `kill_creep()`

Add `death_timer` to the `Creep` struct in `game.h`:

```c
/* game.h — Creep struct (excerpt) */
typedef struct {
    float x, y;
    float speed;
    int   hp, max_hp;
    int   type;
    int   active;
    float death_timer;   /* > 0 while playing death animation */
    /* ... other fields ... */
} Creep;
```

Add the duration constant:

```c
#define CREEP_DEATH_DURATION  0.20f   /* seconds */
```

Rewrite `kill_creep()` in `creep.c`:

```c
/* creep.c */
void kill_creep(GameState *state, int idx) {
    Creep *c = &state->creeps[idx];
    if (!c->active) return;

    c->active = 0;
    c->death_timer = CREEP_DEATH_DURATION;   /* keep rendering briefly */

    state->gold += CREEP_DEFS[c->type].gold_reward;

    /* Spawn floating "+N gold" particle */
    char text[12];
    snprintf(text, sizeof(text), "+%d", CREEP_DEFS[c->type].gold_reward);
    spawn_particle(state,
                   c->x + CELL_SIZE / 2.0f,
                   c->y,
                   0.0f, -30.0f,          /* float upward */
                   1.2f,                  /* lifetime seconds */
                   GAME_RGB(0xFF, 0xDD, 0x00),
                   0,                     /* size unused for text particles */
                   text);
}
```

---

## Step 4: Tick `death_timer` in `update_creeps()`

```c
/* creep.c — update_creeps() excerpt */
void update_creeps(GameState *state, float dt) {
    for (int i = 0; i < MAX_CREEPS; i++) {
        Creep *c = &state->creeps[i];

        /* Tick death animation */
        if (c->death_timer > 0.0f) {
            c->death_timer -= dt;
            if (c->death_timer < 0.0f) c->death_timer = 0.0f;
            continue;   /* don't move while dying */
        }

        if (!c->active) continue;

        /* ... normal movement and damage logic ... */
    }
}
```

---

## Step 5: The `Particle` struct and pool

Add to `game.h`:

```c
/* game.h — Particle */
#define MAX_PARTICLES 128

typedef struct {
    float    x, y;
    float    vx, vy;
    float    lifetime;
    float    max_lifetime;
    uint32_t color;
    int      size;
    char     text[12];   /* non-empty → text particle */
    int      active;
} Particle;
```

Add `particles[MAX_PARTICLES]` to `GameState`.

---

## Step 6: `spawn_particle`, `update_particles`, and `draw_particles`

```c
/* particle.c */
#include "game.h"
#include "render.h"
#include <string.h>
#include <stdio.h>

void spawn_particle(GameState *state,
                    float x, float y,
                    float vx, float vy,
                    float lifetime,
                    uint32_t color,
                    int size,
                    const char *text)
{
    /* Find first inactive slot — O(MAX_PARTICLES), fine for 128 entries */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &state->particles[i];
        if (p->active) continue;

        p->x            = x;
        p->y            = y;
        p->vx           = vx;
        p->vy           = vy;
        p->lifetime     = lifetime;
        p->max_lifetime = lifetime;
        p->color        = color;
        p->size         = size;
        p->active       = 1;
        if (text)
            strncpy(p->text, text, sizeof(p->text) - 1);
        else
            p->text[0] = '\0';
        return;
    }
    /* Pool exhausted — silently drop. Never crash. */
}

void update_particles(GameState *state, float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &state->particles[i];
        if (!p->active) continue;

        p->x        += p->vx * dt;
        p->y        += p->vy * dt;
        p->lifetime -= dt;
        if (p->lifetime <= 0.0f) p->active = 0;
    }
}

void draw_particles(GameBackbuffer *bb, GameState *state) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &state->particles[i];
        if (!p->active) continue;

        /* Alpha fades from 255 → 0 over the particle's lifetime */
        uint8_t alpha = (uint8_t)(p->lifetime / p->max_lifetime * 255.0f);

        if (p->text[0]) {
            /* Text particle: blit string with fading alpha */
            uint32_t col = GAME_RGBA(
                (p->color >> 16) & 0xFF,
                (p->color >>  8) & 0xFF,
                (p->color      ) & 0xFF,
                alpha);
            draw_text(bb, (int)p->x, (int)p->y, p->text, col);
        } else {
            /* Shape particle: alpha-blended rectangle */
            uint32_t col = GAME_RGBA(
                (p->color >> 16) & 0xFF,
                (p->color >>  8) & 0xFF,
                (p->color      ) & 0xFF,
                alpha);
            draw_rect_blend(bb, (int)p->x, (int)p->y, p->size, p->size, col);
        }
    }
}
```

**JS analogy:** The particle pool is the object-pooling pattern used in JS game
engines like Phaser to avoid GC pressure.  Instead of `new Particle()` / garbage
collect, we recycle inactive slots from a fixed array — zero allocations after
startup.

---

## Build and run

```bash
# From repo root
clang -Wall -Wextra -std=c99 -O0 -g \
      -o build/dtd \
      src/main_raylib.c src/game.c src/creep.c \
      src/tower.c src/projectile.c src/render.c src/particle.c \
      -lraylib -lm -lpthread -ldl

./build/dtd
```

**Expected:** HP bars appear above every creep, turning yellow then red as they
take damage.  On kill, the sprite shrinks over 0.2 s before disappearing, and a
yellow "+N" text floats upward from the kill position.

---

## Exercises

1. **Beginner** — Change `CREEP_DEATH_DURATION` to `0.5f` and observe how the
   death animation feels.  Then change it to `0.05f`.  What is a good balance
   between feedback and responsiveness?

2. **Intermediate** — Add a fourth HP threshold: if `hp_ratio > 0.85f` the bar
   is bright blue (full health indicator).  Update the constants and the
   `draw_hp_bar` function accordingly.

3. **Challenge** — Currently `draw_particles` uses `draw_rect_blend` for shape
   particles.  Implement a simple `draw_circle_fill` in `render.c` (iterate a
   bounding box, draw pixels within radius) and use it instead for particles
   whose `size > 0`.  Spawn a round spark on each creep hit.

---

## What's next

In Lesson 12 we build the full 40-wave system: a `WaveDef` data table, HP
scaling with `powf`, and proper GAME_OVER / VICTORY state transitions.
