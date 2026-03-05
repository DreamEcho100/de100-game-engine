# Lesson 14: Frost Slow and Swarm AoE

## What we're building

Two special tower types that bend the usual rules: **Frost**, which directly
slows all creeps in range to 50% speed with a blue tint overlay (no projectile),
and **Swarm**, which fires a projectile that explodes in an orange splash circle
damaging every creep within the blast radius.  We also wire up the **armour**
mechanic: Armoured creeps take half damage from every tower except Swarm.

## What you'll learn

- How to implement an area-effect tower that modifies creep state directly
  rather than via a projectile
- How a `slow_timer` / `slow_factor` pair drives speed reduction without extra
  branches in the movement code
- The `effective_damage()` abstraction that handles armour piercing cleanly
- How to trigger AoE damage on projectile impact using `splash_radius`

## Prerequisites

- Lessons 01–13 complete: all five single-target towers exist, projectiles fly
  and deal damage, creep types include `CREEP_ARMOURED`
- `TOWER_DEFS` table and `TowerType` enum are in place

---

## Step 1: Add Frost and Swarm to the tower table

Extend `TowerType` in `game.h`:

```c
typedef enum {
    TOWER_PELLET = 0,
    TOWER_SQUIRT,
    TOWER_DART,
    TOWER_SNAP,
    TOWER_BASH,
    TOWER_FROST,    /* new */
    TOWER_SWARM,    /* new */
    TOWER_COUNT,
} TowerType;
```

Add two rows to `TOWER_DEFS` in `tower_defs.c`:

```c
    [TOWER_FROST] = {
        .name         = "Frost",
        .cost         = 25,
        .damage       = 0,        /* no direct damage */
        .range_cells  = 2.5f,
        .fire_rate    = 0.5f,     /* re-apply slow every 2 s */
        .color        = GAME_RGB(0x88, 0xCC, 0xFF),
        .is_aoe       = 1,
        .splash_radius= 0.0f,     /* uses range directly */
    },
    [TOWER_SWARM] = {
        .name         = "Swarm",
        .cost         = 40,
        .damage       = 20,
        .range_cells  = 3.5f,
        .fire_rate    = 0.8f,
        .color        = GAME_RGB(0xFF, 0x66, 0x00),
        .is_aoe       = 1,
        .splash_radius= 30.0f,
    },
```

---

## Step 2: `slow_timer` and `slow_factor` on `Creep`

```c
/* game.h — Creep struct (excerpt) */
typedef struct {
    float x, y;
    float speed;
    int   hp, max_hp;
    int   type;
    int   active;
    float death_timer;
    float slow_timer;    /* seconds of slow remaining  */
    float slow_factor;   /* speed multiplier while slow (e.g. 0.5) */
    float stun_timer;    /* seconds of stun remaining  */
    /* ... */
} Creep;
```

---

## Step 3: Effective speed in `creep_move_toward_exit()`

```c
/* creep.c — movement excerpt */
void creep_move_toward_exit(GameState *state, Creep *c, float dt) {
    float effective_speed = c->speed;

    if (c->stun_timer > 0.0f)
        effective_speed = 0.0f;
    else if (c->slow_timer > 0.0f)
        effective_speed *= c->slow_factor;

    /* ... rest of pathfinding movement using effective_speed ... */
}
```

Tick timers outside the movement call, in `update_creeps()`:

```c
/* creep.c — update_creeps() timer tick (before movement) */
if (c->slow_timer  > 0.0f) { c->slow_timer  -= dt; if (c->slow_timer  < 0.0f) c->slow_timer  = 0.0f; }
if (c->stun_timer  > 0.0f) { c->stun_timer  -= dt; if (c->stun_timer  < 0.0f) c->stun_timer  = 0.0f; }
```

---

## Step 4: Blue tint overlay for slowed creeps

In the creep render loop:

```c
/* render.c — creep render loop (excerpt) */
/* Draw normal sprite first */
draw_rect(bb, cx, cy, sz, sz, CREEP_DEFS[c->type].color);

/* Overlay a semi-transparent blue tint when slowed */
if (c->slow_timer > 0.0f) {
    draw_rect_blend(bb, cx, cy, sz, sz,
                    GAME_RGBA(0x88, 0xCC, 0xFF, 0x80));
}
```

`draw_rect_blend` blends a color with per-pixel alpha over whatever is already
drawn at that region.  If your renderer doesn't have one yet, implement it as a
simple SRC_OVER blend:

```c
/* render.c — draw_rect_blend (if not already present) */
void draw_rect_blend(GameBackbuffer *bb,
                     int x, int y, int w, int h,
                     uint32_t color)
{
    uint8_t sa = (color >> 24) & 0xFF;
    if (sa == 0) return;

    uint8_t sr = (color >> 16) & 0xFF;
    uint8_t sg = (color >>  8) & 0xFF;
    uint8_t sb = (color      ) & 0xFF;

    for (int py = y; py < y + h; py++) {
        if (py < 0 || py >= bb->height) continue;
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= bb->width) continue;
            uint32_t *dst = &bb->pixels[py * bb->width + px];
            uint8_t dr = (*dst >> 16) & 0xFF;
            uint8_t dg = (*dst >>  8) & 0xFF;
            uint8_t db = (*dst      ) & 0xFF;
            /* SRC_OVER: out = src * alpha + dst * (1 - alpha) */
            uint8_t a = sa;
            dr = (uint8_t)((sr * a + dr * (255 - a)) / 255);
            dg = (uint8_t)((sg * a + dg * (255 - a)) / 255);
            db = (uint8_t)((sb * a + db * (255 - a)) / 255);
            *dst = GAME_RGB(dr, dg, db);
        }
    }
}
```

---

## Step 5: Frost tower update — direct AoE slow

Frost does not create a projectile.  Instead, `update_tower()` has a special
branch for it:

```c
/* tower.c — update_tower() excerpt */
void update_tower(GameState *state, Tower *t, float dt) {
    t->fire_timer -= dt;
    if (t->fire_timer > 0.0f) return;

    if (t->type == TOWER_FROST) {
        /* Apply slow to every creep in range */
        float range = TOWER_DEFS[TOWER_FROST].range_cells * (float)CELL_SIZE;
        float cx    = t->x + CELL_SIZE * 0.5f;
        float cy    = t->y + CELL_SIZE * 0.5f;

        for (int i = 0; i < MAX_CREEPS; i++) {
            Creep *c = &state->creeps[i];
            if (!c->active) continue;

            float dx = c->x + CELL_SIZE * 0.5f - cx;
            float dy = c->y + CELL_SIZE * 0.5f - cy;
            if (dx*dx + dy*dy <= range*range) {
                c->slow_timer  = 2.0f;
                c->slow_factor = 0.5f;
            }
        }
        t->fire_timer = 1.0f / TOWER_DEFS[TOWER_FROST].fire_rate;
        return;
    }

    /* Normal towers: find target and fire projectile */
    /* ... existing code ... */
}
```

---

## Step 6: `effective_damage()` — armour piercing

```c
/* creep.c — effective_damage() */
int effective_damage(int tower_type, int creep_type, int raw_damage) {
    /* Armoured creeps take half damage from all towers except Swarm */
    if (creep_type == CREEP_ARMOURED && tower_type != TOWER_SWARM)
        return raw_damage / 2;
    return raw_damage;
}
```

Call it everywhere damage is applied:

```c
/* projectile.c — on projectile hit */
int dmg = effective_damage(proj->tower_type, target->type, proj->damage);
target->hp -= dmg;
```

---

## Step 7: Swarm projectile — AoE on impact

Add `splash_radius` and `tower_type` to the `Projectile` struct:

```c
/* game.h — Projectile struct (excerpt) */
typedef struct {
    /* ... existing fields ... */
    float splash_radius;
    int   tower_type;
} Projectile;
```

In `fire_projectile()` for Swarm towers:

```c
/* tower.c — fire_projectile() excerpt */
proj->splash_radius = TOWER_DEFS[TOWER_SWARM].splash_radius;
proj->tower_type    = t->type;
```

In `projectile_update()`, on impact:

```c
/* projectile.c — on impact */
if (proj->splash_radius > 0.0f) {
    /* AoE: damage all creeps within splash_radius */
    float ix = proj->x;
    float iy = proj->y;
    float sr = proj->splash_radius;

    for (int i = 0; i < MAX_CREEPS; i++) {
        Creep *c = &state->creeps[i];
        if (!c->active) continue;

        float dx = c->x + CELL_SIZE * 0.5f - ix;
        float dy = c->y + CELL_SIZE * 0.5f - iy;
        if (dx*dx + dy*dy <= sr*sr) {
            int dmg = effective_damage(proj->tower_type, c->type, proj->damage);
            c->hp -= dmg;
            if (c->hp <= 0) kill_creep(state, i);
        }
    }

    /* Visual splash marker: spawn a short-lived ring particle */
    spawn_particle(state,
                   ix - sr, iy - sr,
                   0.0f, 0.0f,
                   0.15f,
                   GAME_RGBA(0xFF, 0x66, 0x00, 0xC0),
                   (int)(sr * 2),
                   NULL);
} else {
    /* Single-target hit */
    int dmg = effective_damage(proj->tower_type, target->type, proj->damage);
    target->hp -= dmg;
    if (target->hp <= 0) kill_creep(state, target_idx);
}

proj->active = 0;
```

---

## Build and run

```bash
clang -Wall -Wextra -std=c99 -O0 -g \
      -o build/dtd \
      src/main_raylib.c src/game.c src/creep.c \
      src/tower.c src/projectile.c src/render.c \
      src/particle.c src/wave.c src/levels.c \
      src/tower_defs.c src/ui.c \
      -lraylib -lm -lpthread -ldl

./build/dtd
```

**Expected:** Frost tower visibly slows nearby creeps (blue tint, clearly slower
movement); the slow wears off after 2 seconds.  Swarm projectile detonates in an
orange ring and damages every creep inside.  Armoured creeps soak many Pellet
shots but die quickly to Swarm blasts.

---

## Exercises

1. **Beginner** — Reduce `slow_factor` from `0.5f` to `0.25f` (75% speed
   reduction) for Frost.  Observe the difference.  Then restore it and instead
   increase `slow_timer` from `2.0f` to `4.0f`.  Which feels more impactful?

2. **Intermediate** — Add a `TOWER_ICE` variant that both slows (50%) *and*
   deals 5 damage per pulse.  Wire it through `effective_damage()` normally.
   Only change `tower_defs.c` and the Frost branch in `update_tower()`.

3. **Challenge** — The AoE splash particle is currently just a rectangle.  Draw
   a proper hollow circle outline for the blast radius: iterate pixels on the
   circumference `x = cx + r*cos(θ), y = cy + r*sin(θ)` for θ from 0 to 2π in
   small increments and `draw_pixel` each one.  Create a `draw_circle_outline`
   helper in `render.c`.

---

## What's next

In Lesson 15 we implement all five targeting modes (First / Last / Strongest /
Weakest / Closest), right-click cycling through modes on placed towers, and a
selection info box with a sell button.
