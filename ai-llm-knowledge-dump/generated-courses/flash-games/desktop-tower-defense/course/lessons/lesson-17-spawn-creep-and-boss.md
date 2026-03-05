# Lesson 17: Spawn Creep and Boss

## What we're building

Two high-stakes creep types.  **Spawn** creeps look menacing but their real
danger is that each one killed splits into four smaller `CREEP_SPAWN_CHILD`
creeps at the parent's position.  The **Boss** creep is a giant, slow,
red tank with a visible HP bar and a ring drawn around it; it costs multiple
lives when it reaches the exit and is worth a large gold reward.  After wave 40
clears with zero creeps remaining, a victory screen appears.

## What you'll learn

- Spawn-on-death: how to safely insert new creeps from inside a kill callback
  without corrupting the active loop
- Boss `lives_cost` computed from current HP, capped with `CLAMP`
- A simple linear-congruential pseudo-RNG for positional jitter
- Victory trigger logic at the end of `check_wave_complete()`

## Prerequisites

- Lessons 01–16 complete: `CREEP_SPAWN` and `CREEP_BOSS` entries exist in
  `CREEP_DEFS`; `spawn_creep()` correctly initialises type from `CreepDef`
- `kill_creep(state, idx)` already awards gold and deactivates the slot
- `change_phase(state, phase)` exists in `game.c`
- `WAVE_COUNT = 40` defined in `game.h`

---

## Step 1: Simple pseudo-RNG

Add a file-scope RNG to `creeps.c`.  We only need it for spawn-child jitter
and boss visuals, so a 32-bit LCG is more than adequate.

```c
/* src/creeps.c — top of file */
static unsigned int g_rng = 12345u;

static unsigned int rng_next(void) {
    /* Knuth MMIX LCG — cheap, no dependencies */
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng;
}

/* Return a float in [0, 1) */
static float rng_float(void) {
    return (float)(rng_next() & 0xFFFFu) / 65536.0f;
}
```

**What's happening:**

- This is the same multiplier/increment pair used in many game engines.
- `g_rng` is `static` — it persists across frames but is not serialised, so
  replays differ.  That is fine for cosmetic jitter.
- `rng_float()` extracts the top 16 bits; the lower bits of an LCG have weak
  statistical properties, so we always discard them.

---

## Step 2: Spawn children in `kill_creep()`

```c
/* src/creeps.c — kill_creep(): award gold, deactivate, spawn children */
void kill_creep(GameState *state, int idx) {
    ASSERT(idx >= 0 && idx < MAX_CREEPS);
    Creep *creep = &state->creeps[idx];
    if (!creep->active) return;

    /* Award gold */
    const CreepDef *def = &CREEP_DEFS[creep->type];
    state->player_gold += def->gold_reward;

    /* Spawn children BEFORE deactivating so parent position is still valid */
    if (creep->type == CREEP_SPAWN) {
        for (int i = 0; i < 4; i++) {
            int child_idx = spawn_creep(state, CREEP_SPAWN_CHILD, state->current_wave);
            if (child_idx >= 0) {
                /* Small random offset so children don't all stack on the same pixel */
                float jitter_x = (rng_float() * 20.0f) - 10.0f;
                float jitter_y = (rng_float() * 20.0f) - 10.0f;
                state->creeps[child_idx].x = creep->x + jitter_x;
                state->creeps[child_idx].y = creep->y + jitter_y;

                /* Children inherit the parent's BFS waypoint so they don't
                   teleport back to the entry cell */
                state->creeps[child_idx].path_index = creep->path_index;
            }
        }
    }

    /* Now safe to deactivate */
    creep->active = 0;
    memset(creep, 0, sizeof(*c));   /* clear slot for next use */
}
```

> **Why spawn before deactivating?**
>
> `spawn_creep()` scans `state->creeps` for the first inactive slot.  If we
> cleared the parent first, that slot would be immediately reused for `child[0]`
> — which would then be overwritten by `child[1]`, losing the first child.
> Spawning first guarantees the parent's slot stays occupied until all four
> children have been assigned different slots.

---

## Step 3: Boss `lives_cost` and the exit handler

The Boss's lives cost scales with its remaining HP: a full-health boss that
walks off-screen is catastrophic; one barely limping out still hurts.

```c
/* src/game.c — exit handler inside the creep movement loop */
if (c->reached_exit) {
    int cost;
    if (c->type == CREEP_BOSS) {
        /* Scale cost from remaining HP; min 1, max 20 */
        int hp_cost = (int)(c->hp / 100.0f);
        cost = hp_cost < 1 ? 1 : (hp_cost > 20 ? 20 : hp_cost);
    } else {
        cost = CREEP_DEFS[c->type].lives_cost;
    }

    state->player_lives -= cost;
    c->active = 0;

    if (state->player_lives <= 0) {
        state->player_lives = 0;
        change_phase(state, GAME_PHASE_GAME_OVER);
    }
}
```

**What's happening:**

- `CLAMP(hp/100, 1, 20)` written inline avoids a macro dependency.  A fresh
  Boss has `base_hp = 1000`, so `1000 / 100 = 10` lives at wave 10; later
  waves scale HP further so the cost can reach the 20-life cap.
- All other types use the flat `lives_cost` from their `CreepDef` row.

---

## Step 4: Boss visual — large body, HP bar, and ring

```c
/* src/render.c — draw_creep(): Boss special-case visuals */
if (c->type == CREEP_BOSS) {
    int half = c->size / 2;  /* size = 24 from CREEP_DEFS */
    int px   = (int)(c->x - half);
    int py   = (int)(c->y - half);

    /* Outer ring drawn two pixels beyond the body */
    draw_rect_outline(bb, px - 3, py - 3, c->size + 6, c->size + 6,
                      GAME_RGB(0xFF, 0x44, 0x00));

    /* Body */
    draw_rect(bb, px - 1, py - 1, c->size + 2, c->size + 2,
              GAME_RGB(0x88, 0x00, 0x00));  /* dark red border */
    draw_rect(bb, px, py, c->size, c->size, COLOR_CREEP_BOSS);

    /* HP bar — wider than normal, positioned above the ring */
    float ratio = c->max_hp > 0 ? c->hp / c->max_hp : 0.0f;
    draw_hp_bar_wide(bb, px - 3, py - 10, c->size + 6, ratio);
    return;  /* skip generic draw_creep path */
}
```

Add the wider HP bar helper to `render.c`:

```c
/* src/render.c — boss-sized HP bar */
static void draw_hp_bar_wide(GameBackbuffer *bb,
                              int x, int y, int width, float ratio) {
    draw_rect(bb, x, y, width, 4, COLOR_HP_BG);

    int filled = (int)((float)width * ratio);
    if (filled < 1) filled = 1;

    uint32_t col = (ratio > 0.60f) ? COLOR_HP_FULL :
                   (ratio > 0.33f) ? COLOR_HP_MID  : COLOR_HP_LOW;
    draw_rect(bb, x, y, filled, 4, col);
}
```

---

## Step 5: Victory trigger in `check_wave_complete()`

```c
/* src/game.c — check_wave_complete() */
static void check_wave_complete(GameState *state) {
    /* Count active creeps */
    int creep_count = 0;
    for (int i = 0; i < MAX_CREEPS; i++) {
        if (state->creeps[i].active) creep_count++;
    }

    /* Victory: all waves done AND no creeps remain */
    if (state->current_wave >= WAVE_COUNT && creep_count == 0) {
        change_phase(state, GAME_PHASE_VICTORY);
        return;
    }

    /* Normal wave completion: enter WAVE_CLEAR prep phase */
    if (state->wave_spawned_count >= state->wave_total_count && creep_count == 0) {
        change_phase(state, GAME_PHASE_WAVE_CLEAR);
        state->wave_clear_timer = WAVE_CLEAR_DURATION;
    }
}
```

**What's happening:**

- `state->current_wave >= WAVE_COUNT` is checked first so it takes priority
  over the normal WAVE_CLEAR transition.
- `WAVE_COUNT = 40` is defined in `game.h`.
- `GAME_PHASE_VICTORY` triggers a simple overlay (wired in Lesson 20).

---

## Step 6: Schedule Spawn and Boss waves in `levels.c`

```c
/* src/levels.c — add spawn and boss waves (showing waves 10-15) */
/* wave 10 */ {  1, CREEP_BOSS,   0.0f },
/* wave 11 */ { 20, CREEP_NORMAL, 0.45f},
/* wave 12 */ { 18, CREEP_FAST,   0.40f},
/* wave 13 */ { 12, CREEP_SPAWN,  0.60f},  /* Spawn creeps debut */
/* wave 14 */ { 25, CREEP_NORMAL, 0.35f},
/* wave 15 */ { 15, CREEP_ARMOURED,0.65f},
```

---

## Build and run

```bash
mkdir -p build

# Raylib
clang -Wall -Wextra -std=c99 -O0 -g \
      -o build/dtd \
      src/main_raylib.c src/game.c src/render.c \
      src/creeps.c src/towers.c src/levels.c src/bfs.c \
      -lraylib -lm
./build/dtd

# X11
clang -Wall -Wextra -std=c99 -O0 -g \
      -o build/dtd \
      src/main_x11.c src/game.c src/render.c \
      src/creeps.c src/towers.c src/levels.c src/bfs.c \
      -lX11 -lm
./build/dtd
```

**Expected:** Wave 10 spawns one giant red Boss with an orange ring and a wide
HP bar.  Wave 13 spawns purple Spawn creeps; killing one immediately produces
four smaller purple children at the same position.  After defeating all 40
waves with no creeps remaining, the game transitions to `GAME_PHASE_VICTORY`.

---

## Exercises

1. **Beginner** — Change the number of children spawned by a Spawn creep from
   4 to 6.  Add a `CREEP_SPAWN_CHILD_COUNT` constant to `game.h` rather than
   hardcoding the value in `kill_creep`.

2. **Intermediate** — Boss creeps currently cost up to 20 lives based on
   remaining HP.  Add a minimum cost of 5 lives regardless of damage dealt.
   Verify that a freshly-spawned Boss that instantly reaches the exit costs the
   correct 10 lives, not fewer.

3. **Challenge** — Spawn children currently start at the parent's `path_index`.
   This means they all immediately follow the same path segment.  Instead,
   assign each child a `path_index` that is 1-2 steps behind the parent so they
   spread out and approach the player from slightly different angles.

---

## What's next

In Lesson 18 we add two economy mechanics: a **Send Next Wave** button that lets
players trade time for bonus gold, and an **interest** system that rewards
saving gold between waves.  Both add strategic depth and are each under 30 lines
of new code.
