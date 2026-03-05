# Lesson 12: Wave System and Wave Table

## What we're building

A complete 40-wave progression system.  Wave 1 sends 10 slow normal creeps;
wave 4 introduces fast ones; later waves mix types with HP that grows 10% per
wave via `powf`.  Losing all lives transitions to `GAME_OVER`; clearing wave 40
transitions to `VICTORY`.  Between waves the player has a building phase.

## What you'll learn

- **C99 designated initialisers** — the cleanest way to write a data table
- How to scale a numeric value exponentially with `powf`
- A minimal finite-state machine for wave phase transitions
- How to separate static wave configuration (`levels.c`) from runtime wave state

## Prerequisites

- Lessons 01–11 complete: creeps move, towers fire, gold is tracked, HP bars
  and death effects render
- `GAME_PHASE_*` enum values exist; `change_phase()` is implemented
- `spawn_creep()` function accepts a type, HP, and starting position

---

## Step 1: `WaveDef` struct and the wave table header

Add to `game.h`:

```c
/* game.h — wave system */
#define WAVE_COUNT        40
#define HP_SCALE_PER_WAVE 1.10f   /* +10 % HP each wave */

typedef struct {
    int   creep_type;           /* CREEP_* constant                  */
    int   count;                /* number of creeps in this wave     */
    float spawn_interval;       /* seconds between each creep        */
    int   base_hp_override;     /* 0 → use CREEP_DEFS[type].base_hp  */
} WaveDef;

extern const WaveDef g_waves[WAVE_COUNT];
```

Also add wave runtime fields to `GameState`:

```c
/* game.h — GameState (excerpt) */
typedef struct {
    /* ... existing fields ... */
    int   current_wave;         /* 1-based; 0 = not started          */
    int   wave_spawn_index;     /* how many creeps spawned so far     */
    int   wave_creep_count;     /* total for this wave (from WaveDef) */
    float wave_spawn_timer;     /* countdown to next spawn            */
    float wave_spawn_interval;
    float phase_timer;          /* countdown used by WAVE_CLEAR       */
    int   lives;
} GameState;
```

---

## Step 2: `levels.c` — the wave table

```c
/* src/levels.c — Desktop Tower Defense | Wave Definitions */
#include "game.h"

/*
 * C99 designated initialisers:
 *   [0] = { ... }  sets index 0 explicitly.
 * Each row is self-documenting — wave 7 is literally [6] = { CREEP_FLYING, ... }.
 * JS analogy: like a JSON config where the key is the wave index.
 */
const WaveDef g_waves[WAVE_COUNT] = {
    /* --- early waves: normals only --- */
    [0]  = { CREEP_NORMAL,   10, 1.0f,  0 },
    [1]  = { CREEP_NORMAL,   12, 0.9f,  0 },
    [2]  = { CREEP_NORMAL,   14, 0.85f, 0 },
    [3]  = { CREEP_FAST,     10, 0.7f,  0 },   /* first fast wave */
    [4]  = { CREEP_NORMAL,   16, 0.8f,  0 },
    [5]  = { CREEP_FAST,     12, 0.65f, 0 },
    [6]  = { CREEP_FLYING,   10, 0.9f,  0 },   /* first flying wave */
    [7]  = { CREEP_NORMAL,   20, 0.75f, 0 },
    [8]  = { CREEP_ARMOURED,  8, 1.1f,  0 },   /* first armoured wave */
    [9]  = { CREEP_FAST,     15, 0.6f,  0 },
    /* --- mid waves: mixed types --- */
    [10] = { CREEP_FLYING,   14, 0.8f,  0 },
    [11] = { CREEP_ARMOURED, 12, 1.0f,  0 },
    [12] = { CREEP_FAST,     18, 0.55f, 0 },
    [13] = { CREEP_NORMAL,   25, 0.7f,  0 },
    [14] = { CREEP_FLYING,   16, 0.75f, 0 },
    [15] = { CREEP_ARMOURED, 15, 0.9f,  0 },
    [16] = { CREEP_FAST,     20, 0.5f,  0 },
    [17] = { CREEP_NORMAL,   30, 0.65f, 0 },
    [18] = { CREEP_FLYING,   18, 0.7f,  0 },
    [19] = { CREEP_ARMOURED, 18, 0.85f, 0 },
    /* --- late waves: harder counts, tighter intervals --- */
    [20] = { CREEP_FAST,     25, 0.45f, 0 },
    [21] = { CREEP_NORMAL,   35, 0.6f,  0 },
    [22] = { CREEP_FLYING,   22, 0.65f, 0 },
    [23] = { CREEP_ARMOURED, 20, 0.8f,  0 },
    [24] = { CREEP_FAST,     28, 0.4f,  0 },
    [25] = { CREEP_NORMAL,   40, 0.55f, 0 },
    [26] = { CREEP_FLYING,   25, 0.6f,  0 },
    [27] = { CREEP_ARMOURED, 22, 0.75f, 0 },
    [28] = { CREEP_FAST,     30, 0.38f, 0 },
    [29] = { CREEP_NORMAL,   45, 0.5f,  0 },
    /* --- final stretch --- */
    [30] = { CREEP_FLYING,   28, 0.55f, 0 },
    [31] = { CREEP_ARMOURED, 25, 0.7f,  0 },
    [32] = { CREEP_FAST,     35, 0.35f, 0 },
    [33] = { CREEP_NORMAL,   50, 0.45f, 0 },
    [34] = { CREEP_FLYING,   30, 0.5f,  0 },
    [35] = { CREEP_ARMOURED, 28, 0.65f, 0 },
    [36] = { CREEP_FAST,     40, 0.32f, 0 },
    [37] = { CREEP_NORMAL,   55, 0.4f,  0 },
    [38] = { CREEP_FLYING,   35, 0.45f, 0 },
    [39] = { CREEP_ARMOURED, 30, 0.6f,  0 },   /* wave 40 — final boss wave */
};
```

> **C99 designated initialisers** allow array elements to be set by index:
> `[0] = { ... }, [1] = { ... }`.  This makes the wave table self-documenting —
> wave 7 is literally `[6] = { CREEP_FLYING, ... }`.  Any unspecified elements
> are zero-initialised by the standard.

---

## Step 3: `start_wave()`

```c
/* wave.c */
#include "game.h"
#include "creep.h"
#include <math.h>   /* powf */

void start_wave(GameState *state) {
    if (state->current_wave < 1) state->current_wave = 1;

    const WaveDef *wd = &g_waves[state->current_wave - 1];

    /* Exponential HP scaling: wave 1 → base HP, each wave +10 % */
    float scale = powf(HP_SCALE_PER_WAVE, (float)(state->current_wave - 1));
    int base_hp = (wd->base_hp_override > 0)
                  ? wd->base_hp_override
                  : CREEP_DEFS[wd->creep_type].base_hp;
    int scaled_hp = (int)((float)base_hp * scale);
    if (scaled_hp < 1) scaled_hp = 1;

    state->wave_creep_count    = wd->count;
    state->wave_spawn_index    = 0;
    state->wave_spawn_interval = wd->spawn_interval;
    state->wave_spawn_timer    = 0.0f;   /* spawn first creep immediately */
    state->wave_scaled_hp      = scaled_hp;
    state->wave_creep_type     = wd->creep_type;

    change_phase(state, GAME_PHASE_WAVE);
}
```

---

## Step 4: `update_wave()`

```c
/* wave.c (continued) */
void update_wave(GameState *state, float dt) {
    /* Spawn creeps on interval */
    if (state->wave_spawn_index < state->wave_creep_count) {
        state->wave_spawn_timer -= dt;
        if (state->wave_spawn_timer <= 0.0f) {
            spawn_creep(state,
                        state->wave_creep_type,
                        state->wave_scaled_hp);
            state->wave_spawn_index++;
            state->wave_spawn_timer = state->wave_spawn_interval;
        }
    }

    check_wave_complete(state);
}
```

---

## Step 5: `check_wave_complete()` and phase transitions

```c
/* wave.c (continued) */
void check_wave_complete(GameState *state) {
    /* All creeps spawned and all off the field? */
    if (state->wave_spawn_index < state->wave_creep_count) return;

    int alive = 0;
    for (int i = 0; i < MAX_CREEPS; i++)
        if (state->creeps[i].active || state->creeps[i].death_timer > 0.0f)
            alive++;

    if (alive > 0) return;

    /* Wave cleared */
    if (state->current_wave >= WAVE_COUNT) {
        change_phase(state, GAME_PHASE_VICTORY);
    } else {
        change_phase(state, GAME_PHASE_WAVE_CLEAR);
        state->phase_timer = 3.0f;   /* 3-second celebration pause */
    }
}
```

Handle `GAME_PHASE_WAVE_CLEAR` in `game_update()`:

```c
/* game.c — game_update() excerpt */
case GAME_PHASE_WAVE_CLEAR:
    state->phase_timer -= dt;
    if (state->phase_timer <= 0.0f) {
        state->current_wave++;
        change_phase(state, GAME_PHASE_PLACING);
    }
    break;
```

---

## Step 6: Lives and GAME_OVER

When a creep reaches the exit in `creep_move_toward_exit()`:

```c
/* creep.c — on creep reaching exit */
state->lives -= 1;
c->active = 0;
c->death_timer = 0.0f;
if (state->lives <= 0)
    change_phase(state, GAME_PHASE_GAME_OVER);
```

---

## Step 7: "Start Wave" button

In `draw_ui_panel()` (covered fully in Lesson 13), wire the button for the
placing phase:

```c
/* render.c — UI panel excerpt */
if (state->phase == GAME_PHASE_PLACING) {
    int btn_y = CANVAS_H - 50;
    draw_rect(bb, GRID_PIXEL_W + 10, btn_y, 140, 30,
              COLOR_BTN_NORMAL);
    draw_text(bb, GRID_PIXEL_W + 20, btn_y + 8,
              state->current_wave == 0 ? "Start Wave 1"
                                        : "Next Wave",
              COLOR_TEXT);
    /* Click handling is in game_handle_input() */
}
```

---

## Build and run

```bash
clang -Wall -Wextra -std=c99 -O0 -g \
      -o build/dtd \
      src/main_raylib.c src/game.c src/creep.c \
      src/tower.c src/projectile.c src/render.c \
      src/particle.c src/wave.c src/levels.c \
      -lraylib -lm -lpthread -ldl

./build/dtd
```

**Expected:** Clicking "Start Wave 1" begins spawning 10 normal creeps.  After
they all die or escape, a 3-second celebration pause passes, then the player can
start wave 2.  Later waves visibly have tougher, faster creeps.  Losing all
lives shows the GAME_OVER screen; clearing wave 40 shows VICTORY.

---

## Exercises

1. **Beginner** — Change `HP_SCALE_PER_WAVE` from `1.10f` to `1.05f` and
   `1.20f`.  Play a few waves each time.  How does the scaling affect late-game
   balance?

2. **Intermediate** — Add a `wave_reward` field to `WaveDef` (e.g. `50` gold
   for clearing a wave).  Award it in `check_wave_complete()` and display it
   with a particle.

3. **Challenge** — The current system spawns one creep type per wave.  Extend
   `WaveDef` with a second `creep_type2` / `count2` pair and modify
   `update_wave()` to interleave both types in the spawn queue.

---

## What's next

In Lesson 13 we add a fully data-driven tower shop: a `TowerDef` table for all
five single-target tower types, a side panel with clickable buttons, and
gold-check feedback.
