# Lesson 10: Tower Firing and Damage

## What we're building

Towers now fire.  A per-tower cooldown timer triggers `spawn_projectile()` which
launches a 4×4 yellow square toward the target creep's **current** position.  On
proximity hit, `apply_damage()` deducts HP.  Dead creeps trigger `kill_creep()`
which awards gold.  The gold counter in the HUD updates in real time.

## What you'll learn

- The **Projectile struct** and `projectiles[MAX_PROJECTILES]` pool
- How a **fire-rate cooldown timer** gates shots: `1.0f / fire_rate` seconds per
  shot
- `spawn_projectile()` — initialises velocity toward the target's current pixel
  position (non-predictive)
- `projectile_update()` — linear motion + proximity hit detection
- `apply_damage()` + `effective_damage()` — ARMOURED creep damage reduction
- `kill_creep()` — awards gold and marks the slot inactive
- `compact_projectiles()` — same swap-remove pattern as `compact_creeps()`

## Prerequisites

- Lesson 09 complete: `tower_update()` sets `tower->angle` and `tower->target_id`
- `find_best_target()` returns an index into `state->creeps[]`
- `Creep.id` is unique per creep; `Tower.target_id` stores the last known id

---

## Step 1: Confirm `Projectile` struct and pool in `game.h`

```c
/* src/game.h  —  Projectile struct (confirm present) */
typedef struct {
    float     x, y;
    float     vx, vy;
    int       damage;
    float     splash_radius;   /* 0 = single-target; > 0 = AoE (Lesson 14) */
    int       target_id;       /* creep id to track; -1 = no tracking */
    int       active;
    TowerType tower_type;      /* which tower fired this (for damage modifiers) */
} Projectile;
```

In `GameState`:

```c
Projectile projectiles[MAX_PROJECTILES];
int        projectile_count;
```

---

## Step 2: `spawn_projectile()`

```c
/* src/game.c  —  spawn_projectile() */

#define PROJECTILE_SPEED 300.0f   /* pixels per second */

/* Spawns one projectile aimed at target_id's CURRENT position.
 * sx, sy  : source pixel position (tower centre).
 * target_id: creep id of the intended target (-1 = free-flying).
 * dmg      : base damage before modifiers.
 * splash_r : 0 = single target (>0 handled in Lesson 14).
 * Returns the slot index, or -1 if the pool is full. */
static int spawn_projectile(GameState *state,
                             float sx, float sy,
                             int target_id,
                             int dmg,
                             float splash_r,
                             TowerType tower_type) {
    /* Find the target creep to aim at */
    float tx = sx, ty = sy; /* default: fly in place (should not happen) */
    for (int i = 0; i < state->creep_count; i++) {
        const Creep *c = &state->creeps[i];
        if (c->active && c->id == target_id) {
            tx = c->x;
            ty = c->y;
            break;
        }
    }

    /* Direction and velocity */
    float dx   = tx - sx;
    float dy   = ty - sy;
    float dist = sqrtf(dx * dx + dy * dy);
    float vx = 0.0f, vy = 0.0f;
    if (dist > 0.0f) {
        vx = (dx / dist) * PROJECTILE_SPEED;
        vy = (dy / dist) * PROJECTILE_SPEED;
    }

    /* Allocate a free slot */
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &state->projectiles[i];
        if (p->active) continue;

        p->x             = sx;
        p->y             = sy;
        p->vx            = vx;
        p->vy            = vy;
        p->damage        = dmg;
        p->splash_radius = splash_r;
        p->target_id     = target_id;
        p->tower_type    = tower_type;
        p->active        = 1;

        if (i >= state->projectile_count)
            state->projectile_count = i + 1;

        return i;
    }
    return -1; /* pool exhausted */
}
```

**What's happening:**

- We aim at the target's **current** position (non-predictive).  A fast-moving
  target may dodge, but this keeps the implementation simple.  Predictive leading
  is left as a challenge exercise.
- If the pool is exhausted, the shot is silently dropped.  With
  `MAX_PROJECTILES = 512` and typical fire rates this should never happen.

---

## Step 3: Tower cooldown and firing inside `tower_update()`

Extend `tower_update()` from Lesson 09:

```c
/* src/game.c  —  tower_update() (Lesson 10 version) */
static void tower_update(GameState *state, Tower *tower, float dt) {
    /* --- Targeting (from Lesson 09) --- */
    int target_idx = find_best_target(state, tower);

    if (target_idx >= 0) {
        const Creep *target = &state->creeps[target_idx];
        float dx = target->x - (float)tower->cx;
        float dy = target->y - (float)tower->cy;
        tower->angle     = atan2f(dy, dx);
        tower->target_id = target->id;
    } else {
        tower->target_id = -1;
    }

    /* --- Fire-rate cooldown --- */
    tower->cooldown_timer -= dt;

    if (tower->cooldown_timer <= 0.0f && tower->target_id >= 0) {
        /* Fire! */
        spawn_projectile(state,
                         (float)tower->cx, (float)tower->cy,
                         tower->target_id,
                         tower->damage,
                         0.0f,           /* no splash (Lesson 14) */
                         tower->type);

        /* Reset cooldown: fire_rate shots/s → 1/fire_rate seconds/shot */
        tower->cooldown_timer = 1.0f / tower->fire_rate;
    }
}
```

---

## Step 4: `projectile_update()`

```c
/* src/game.c  —  projectile_update() */

#define PROJECTILE_HIT_RADIUS 8.0f   /* pixels — proximity hit threshold */

static void projectile_update(GameState *state, Projectile *proj, float dt) {
    /* Move */
    proj->x += proj->vx * dt;
    proj->y += proj->vy * dt;

    /* Out-of-bounds check: discard if outside canvas */
    if (proj->x < 0 || proj->x >= CANVAS_W ||
        proj->y < 0 || proj->y >= CANVAS_H) {
        proj->active = 0;
        return;
    }

    /* Find target creep by id */
    int target_idx = -1;
    for (int i = 0; i < state->creep_count; i++) {
        if (state->creeps[i].active && state->creeps[i].id == proj->target_id) {
            target_idx = i;
            break;
        }
    }

    /* Target is gone (already dead or exited) — discard projectile */
    if (target_idx < 0) {
        proj->active = 0;
        return;
    }

    /* Proximity hit */
    const Creep *target = &state->creeps[target_idx];
    float dx = target->x - proj->x;
    float dy = target->y - proj->y;
    float dist2 = dx * dx + dy * dy;

    if (dist2 <= PROJECTILE_HIT_RADIUS * PROJECTILE_HIT_RADIUS) {
        apply_damage(state, target_idx, proj->damage, proj->tower_type);
        proj->active = 0;
    }
}
```

---

## Step 5: `effective_damage()` — armour reduction

```c
/* src/game.c  —  effective_damage() */

/* Returns the final damage dealt after creep type modifiers.
 * Rule: ARMOURED creeps receive 50% damage from all towers except TOWER_SWARM.
 * (TOWER_SWARM fires many weak projectiles — armour piercing by design.) */
static int effective_damage(int raw, CreepType creep_type, TowerType tower_type) {
    if (creep_type == CREEP_ARMOURED && tower_type != TOWER_SWARM)
        return raw / 2;
    return raw;
}
```

---

## Step 6: `apply_damage()`

```c
/* src/game.c  —  apply_damage() */

static void apply_damage(GameState *state, int creep_idx, int dmg,
                         TowerType tower_type) {
    Creep *c = &state->creeps[creep_idx];
    if (!c->active) return;

    int eff = effective_damage(dmg, c->type, tower_type);
    c->hp -= (float)eff;

    if (c->hp <= 0.0f)
        kill_creep(state, creep_idx);
}
```

---

## Step 7: `kill_creep()`

```c
/* src/game.c  —  kill_creep() */

/* Mark a creep dead and award gold.
 * NOTE: do NOT call compact_creeps() here — that is done once per frame at
 * the end of game_update() to avoid invalidating the loop index. */
static void kill_creep(GameState *state, int idx) {
    Creep *c = &state->creeps[idx];
    if (!c->active) return;

    /* Award kill gold */
    state->player_gold += CREEP_DEFS[c->type].kill_gold;

    c->active = 0;
}
```

---

## Step 8: `compact_projectiles()`

Identical swap-remove pattern to `compact_creeps()`:

```c
/* src/game.c  —  compact_projectiles() */
static void compact_projectiles(GameState *state) {
    int i = state->projectile_count - 1;
    while (i >= 0) {
        if (!state->projectiles[i].active) {
            state->projectile_count--;
            if (i < state->projectile_count) {
                state->projectiles[i] = state->projectiles[state->projectile_count];
                memset(&state->projectiles[state->projectile_count], 0,
                       sizeof(Projectile));
            }
        }
        i--;
    }
}
```

---

## Step 9: Wire projectile updates into `game_update()`

Inside `GAME_PHASE_WAVE`, after moving creeps:

```c
/* Inside GAME_PHASE_WAVE case of game_update() */

/* Update projectiles */
for (int i = 0; i < state->projectile_count; i++) {
    Projectile *p = &state->projectiles[i];
    if (p->active) projectile_update(state, p, dt);
}

/* Compact dead creeps and spent projectiles (once per frame) */
compact_creeps(state);
compact_projectiles(state);
```

---

## Step 10: Render projectiles in `game_render()`

Draw each active projectile as a 4×4 yellow square centred on its position:

```c
/* src/game.c  —  game_render() projectile loop */
for (int i = 0; i < state->projectile_count; i++) {
    const Projectile *p = &state->projectiles[i];
    if (!p->active) continue;

    int px = (int)(p->x) - 2;
    int py = (int)(p->y) - 2;
    draw_rect(bb, px, py, 4, 4, COLOR_PROJECTILE);
}
```

---

## Build and run

```bash
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG \
      -fsanitize=address,undefined \
      -o build/dtd \
      src/main_raylib.c \
      src/game.c \
      src/utils/draw-shapes.c \
      src/utils/draw-text.c \
      -lraylib -lm -lpthread -ldl

./build/dtd
```

**Expected output:** Place one or more towers near the entry.  Start the wave.
Yellow 4×4 dots launch from tower centres, fly toward creeps, and vanish on
contact.  Creep squares shrink in number as they die.  The gold counter in the
HUD increments by 1 each time a Normal creep is killed.  Multiple towers fire
simultaneously; each obeys its own `fire_rate` cooldown.

---

## Exercises

**Beginner:** Change `TOWER_DEFS[TOWER_PELLET].fire_rate` from its default value
to `5.0f` (5 shots/second) and confirm the projectile dots fire visibly faster.
Then restore the original value.

**Intermediate:** Implement **predictive aiming** in `spawn_projectile()`: instead
of aiming at `target->x, target->y`, compute where the creep will be when the
projectile arrives:

```
float travel_time = dist / PROJECTILE_SPEED;
float pred_x = target->x + target->vx_approx * travel_time;
float pred_y = target->y + target->vy_approx * travel_time;
```

You will need to store an approximate velocity on the `Creep` struct (update it
in `creep_move_toward_exit`).  Does accuracy visibly improve for fast creeps?

**Challenge:** Implement the `splash_radius` hit path in `projectile_update()`:
when `proj->splash_radius > 0`, do NOT use target-id tracking — instead,
on each frame check all active creeps and call `apply_damage()` for every creep
within `splash_radius` pixels of the projectile.  Mark the projectile inactive
after the first splash detonation.  Test with a manually placed Swarm tower.

---

## What's next

In Lesson 11 we add **HP bars** above each creep and a brief **death particle**
burst when a creep is killed, making the combat feel responsive and readable.
