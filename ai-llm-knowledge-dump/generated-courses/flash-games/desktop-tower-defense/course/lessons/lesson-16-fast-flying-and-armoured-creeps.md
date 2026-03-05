# Lesson 16: Fast, Flying, and Armoured Creeps

## What we're building

Three new creep types appear in their correct waves.  Fast creeps sprint along
the BFS path at twice normal speed.  Flying creeps ignore the maze entirely and
fly in a straight diagonal line from the entry cell to the exit cell — they
require dedicated movement code.  Armoured creeps are large, slow, and soak
many projectiles before dying.  By the end of this lesson the wave schedule has
real variety and players must start thinking about tower positioning differently.

## What you'll learn

- How to add new creep variants to a **data-table** (`CreepDef CREEP_DEFS[]`)
  so future types cost zero new code
- `creep_flying_move()`: straight-line movement bypassing BFS entirely
- How to dispatch per-type movement in `game_update()` with a single branch
- How to differentiate creep visuals with colour and simple geometry (wing rects)
- How `levels.c` schedules new types across waves 4, 7, and 9

## Prerequisites

- Lessons 01–15 complete: BFS pathfinding, path-following, shooting, and gold
  all work; the wave system produces CREEP_NORMAL in multiple waves
- `spawn_creep(state, type, wave_index)` exists in `creeps.c`
- `creep_move_toward_exit(state, creep, dt)` already implemented (BFS walk)
- `draw_rect` and `draw_text` exist in `render.c`

---

## Step 1: Extend `CreepType` and define `CreepDef`

Open `game.h`.  Extend the enum and add the definition table struct:

```c
/* src/game.h — CreepType enum (add new variants) */
typedef enum {
    CREEP_NORMAL    = 0,
    CREEP_FAST      = 1,
    CREEP_FLYING    = 2,
    CREEP_ARMOURED  = 3,
    CREEP_SPAWN     = 4,   /* introduced in lesson 17 */
    CREEP_SPAWN_CHILD = 5,
    CREEP_BOSS      = 6,
    CREEP_COUNT     = 7
} CreepType;

/* Per-type definition: all balance numbers in one place */
typedef struct {
    const char *name;
    float       base_hp;        /* HP at wave 1; scaled by wave power */
    float       speed;          /* pixels per second                  */
    int         size;           /* render square side length (px)     */
    int         gold_reward;    /* gold awarded on kill                */
    int         lives_cost;     /* lives subtracted when it exits      */
    int         is_flying;      /* 1 = skip BFS, fly straight          */
    uint32_t    color;          /* fill color                          */
    uint32_t    border_color;   /* border / outline color              */
} CreepDef;
```

**What's happening:**

- Every magic number for a creep type lives in one row of the table.
- `is_flying` is a boolean flag on the *definition*, not on the instance, but
  we copy it to `Creep.is_flying` at spawn time so the hot movement path is a
  simple integer check.
- `size` lets the boss be drawn larger without special-casing the render loop.

---

## Step 2: Fill `CREEP_DEFS[]` in `creeps.c`

```c
/* src/creeps.c — full definition table */
#include "game.h"
#include <math.h>

/* Colors shared with other modules */
#define COLOR_CREEP_NORMAL   GAME_RGB(0x33, 0xAA, 0x33)
#define COLOR_CREEP_FAST     GAME_RGB(0xFF, 0xEE, 0x00)
#define COLOR_CREEP_FLYING   GAME_RGB(0xFF, 0xFF, 0xFF)
#define COLOR_CREEP_ARMOURED GAME_RGB(0x55, 0x55, 0x55)
#define COLOR_CREEP_SPAWN    GAME_RGB(0xCC, 0x44, 0xCC)
#define COLOR_CREEP_CHILD    GAME_RGB(0xAA, 0x33, 0xAA)
#define COLOR_CREEP_BOSS     GAME_RGB(0xCC, 0x11, 0x11)

#define COLOR_BORDER_DARK    GAME_RGB(0x11, 0x11, 0x11)
#define COLOR_BORDER_ARMOUR  GAME_RGB(0x22, 0x22, 0x22)

const CreepDef CREEP_DEFS[CREEP_COUNT] = {
/*  name            base_hp  speed  size  gold  lives  fly  color                border             */
  { "Normal",        100.0f,  60.0f,  12,   1,    1,    0,  COLOR_CREEP_NORMAL,  COLOR_BORDER_DARK  },
  { "Fast",           80.0f, 120.0f,  10,   2,    1,    0,  COLOR_CREEP_FAST,    COLOR_BORDER_DARK  },
  { "Flying",         90.0f,  80.0f,  11,   3,    1,    1,  COLOR_CREEP_FLYING,  COLOR_BORDER_DARK  },
  { "Armoured",      300.0f,  40.0f,  14,   4,    1,    0,  COLOR_CREEP_ARMOURED,COLOR_BORDER_ARMOUR},
  { "Spawn",         200.0f,  55.0f,  13,   3,    1,    0,  COLOR_CREEP_SPAWN,   COLOR_BORDER_DARK  },
  { "SpawnChild",     50.0f,  70.0f,   8,   1,    1,    0,  COLOR_CREEP_CHILD,   COLOR_BORDER_DARK  },
  { "Boss",         1000.0f,  25.0f,  24,  20,   10,    0,  COLOR_CREEP_BOSS,    COLOR_BORDER_DARK  },
};
```

**What's happening:**

- `CREEP_FAST` has `speed = 120.0f` — exactly double normal.  No movement code
  changes; the BFS walker already multiplies by `creep->speed`.
- `CREEP_ARMOURED` has `base_hp = 300.0f` to absorb many shots.
- `CREEP_FLYING` has `is_flying = 1`; the movement dispatcher reads this flag.

---

## Step 3: Add `fly_dx` / `fly_dy` to the `Creep` struct

```c
/* src/game.h — Creep struct additions */
typedef struct Creep {
    /* ... existing fields ... */
    int     is_flying;   /* copied from CreepDef at spawn         */
    float   fly_dx;      /* normalised X direction (flying only)  */
    float   fly_dy;      /* normalised Y direction (flying only)  */
} Creep;
```

---

## Step 4: Initialise flying direction in `spawn_creep()`

When we spawn a flying creep we compute the normalised vector from the entry
cell centre to the exit cell centre once; movement code then multiplies by
`speed * dt` every frame.

```c
/* src/creeps.c — inside spawn_creep(), after copying CreepDef fields */
int spawn_creep(GameState *state, CreepType type, int wave_index) {
    /* Find a free slot */
    int idx = -1;
    for (int i = 0; i < MAX_CREEPS; i++) {
        if (!state->creeps[i].active) { idx = i; break; }
    }
    if (idx < 0) return -1;

    Creep *c = &state->creeps[idx];
    memset(c, 0, sizeof(*c));

    const CreepDef *def = &CREEP_DEFS[type];
    c->active    = 1;
    c->type      = type;
    c->speed     = def->speed;
    c->size      = def->size;
    c->is_flying = def->is_flying;
    c->lives_cost= def->lives_cost;

    /* Scale HP by wave index so later waves are harder */
    c->max_hp = def->base_hp * powf(1.12f, (float)wave_index);
    c->hp     = c->max_hp;

    /* Place creep at entry cell centre */
    c->x = ENTRY_COL * CELL_SIZE + CELL_SIZE * 0.5f;
    c->y = ENTRY_ROW * CELL_SIZE + CELL_SIZE * 0.5f;

    /* Compute straight-line direction for flying creeps */
    if (c->is_flying) {
        float exit_cx = EXIT_COL * CELL_SIZE + CELL_SIZE * 0.5f;
        float exit_cy = EXIT_ROW * CELL_SIZE + CELL_SIZE * 0.5f;
        float dx = exit_cx - c->x;
        float dy = exit_cy - c->y;
        float len = sqrtf(dx*dx + dy*dy);
        if (len > 0.001f) { c->fly_dx = dx / len; c->fly_dy = dy / len; }
        else              { c->fly_dx = 1.0f;      c->fly_dy = 0.0f; }
    }

    return idx;
}
```

**What's happening:**

- `powf(1.12f, wave_index)` scales HP 12 % per wave — the same formula used by
  the original DTD flash game.
- The normalised vector is computed once at spawn.  During movement we just
  scale it by `speed * dt` — no `sqrtf` in the hot path.

---

## Step 5: Implement `creep_flying_move()`

```c
/* src/creeps.c — straight-line movement for flying creeps */
static void creep_flying_move(GameState *state, Creep *creep, float dt) {
    /* Advance along the precomputed direction */
    creep->x += creep->fly_dx * creep->speed * dt;
    creep->y += creep->fly_dy * creep->speed * dt;

    /* Arrival check: distance to exit centre < 8 px */
    float exit_cx = EXIT_COL * CELL_SIZE + CELL_SIZE * 0.5f;
    float exit_cy = EXIT_ROW * CELL_SIZE + CELL_SIZE * 0.5f;
    float dx = exit_cx - creep->x;
    float dy = exit_cy - creep->y;
    if (dx*dx + dy*dy < 8.0f * 8.0f) {
        /* Reached the exit — handled by the caller same as ground creeps */
        creep->reached_exit = 1;
    }
}
```

> **Flying creeps and the maze**
>
> Flying creeps are immune to maze strategies — their path is always a straight
> line to the exit, regardless of how many towers block the grid.  They still
> take damage from towers in the normal way.  The BFS distance field is
> entirely irrelevant to them.  This forces the player to keep at least some
> towers covering the direct diagonal, not just the maze corridors.

---

## Step 6: Dispatch movement in `game_update()`

```c
/* src/game.c — inside game_update(), creep movement loop */
for (int i = 0; i < MAX_CREEPS; i++) {
    Creep *c = &state->creeps[i];
    if (!c->active) continue;

    if (c->is_flying) {
        creep_flying_move(state, c, dt);
    } else {
        creep_move_toward_exit(state, c, dt);
    }

    if (c->reached_exit) {
        state->player_lives -= c->lives_cost;
        c->active = 0;
        if (state->player_lives <= 0) {
            state->player_lives = 0;
            change_phase(state, GAME_PHASE_GAME_OVER);
        }
    }
}
```

**What's happening:**

- A single `if (c->is_flying)` keeps the fast path for normal creeps intact.
- `reached_exit` is a flag set by either movement function; the exit handler is
  shared, so lives/game-over logic only lives in one place.

---

## Step 7: Visual distinction in `render.c`

```c
/* src/render.c — draw_creep(): type-specific visuals */
static void draw_creep(GameBackbuffer *bb, const Creep *c) {
    if (!c->active) return;

    int half = c->size / 2;
    int px   = (int)(c->x - half);
    int py   = (int)(c->y - half);
    const CreepDef *def = &CREEP_DEFS[c->type];

    /* Draw border one pixel larger */
    draw_rect(bb, px - 1, py - 1, c->size + 2, c->size + 2, def->border_color);

    /* Main body */
    draw_rect(bb, px, py, c->size, c->size, def->color);

    /* Flying creep: two horizontal wing rects */
    if (c->is_flying) {
        /* left wing */
        draw_rect(bb, px - 4, (int)c->y - 2, 3, 4, COLOR_CREEP_FLYING);
        /* right wing */
        draw_rect(bb, px + c->size + 1, (int)c->y - 2, 3, 4, COLOR_CREEP_FLYING);
    }

    /* Armoured creep: extra inner border to suggest plating */
    if (c->type == CREEP_ARMOURED) {
        draw_rect_outline(bb, px + 2, py + 2, c->size - 4, c->size - 4,
                          COLOR_BORDER_ARMOUR);
    }

    /* HP bar */
    float ratio = (c->max_hp > 0) ? (c->hp / c->max_hp) : 0.0f;
    draw_hp_bar(bb, px, py, ratio);
}
```

**What's happening:**

- Flying creeps get two small 3×4 rectangles extending left and right of the
  body to suggest wings — zero extra structs, zero allocations.
- Armoured creeps get an inner outline drawn with `draw_rect_outline`, giving
  a plated appearance from two nested rectangles.
- Everything uses the colour already stored in `CREEP_DEFS` — no per-type
  colour branches in the render function.

---

## Step 8: Schedule new types in `levels.c`

```c
/* src/levels.c — wave definitions (partial — waves 1-10 shown) */
#include "game.h"

/* WaveDef: how many creeps, of what type, with what spawn interval */
typedef struct {
    int       count;
    CreepType type;
    float     spawn_interval; /* seconds between spawns */
} WaveDef;

const WaveDef WAVE_DEFS[WAVE_COUNT] = {
    /* wave  1 */ { 10, CREEP_NORMAL,   0.8f },
    /* wave  2 */ { 15, CREEP_NORMAL,   0.7f },
    /* wave  3 */ { 18, CREEP_NORMAL,   0.6f },
    /* wave  4 */ { 20, CREEP_FAST,     0.5f },   /* Fast creeps debut */
    /* wave  5 */ { 20, CREEP_NORMAL,   0.5f },
    /* wave  6 */ { 22, CREEP_FAST,     0.45f},
    /* wave  7 */ { 15, CREEP_FLYING,   0.6f },   /* Flying creeps debut */
    /* wave  8 */ { 25, CREEP_NORMAL,   0.4f },
    /* wave  9 */ { 18, CREEP_ARMOURED, 0.7f },   /* Armoured creeps debut */
    /* wave 10 */ {  1, CREEP_BOSS,     0.0f },
    /* waves 11-40 defined similarly … */
};
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

**Expected:** Wave 4 shows small yellow fast creeps zipping along the path.
Wave 7 shows white creeps with wing nubs flying diagonally across the grid,
ignoring towers that block path cells.  Wave 9 shows large dark-grey armoured
creeps that require many hits.

---

## Exercises

1. **Beginner** — Add a `CREEP_SLOW` type to `CREEP_DEFS` with `speed = 30.0f`
   and `base_hp = 500.0f`.  Schedule it in wave 5 by editing `WAVE_DEFS`.
   Verify it moves visibly slower than normal creeps.

2. **Intermediate** — Flying creeps currently fly in a perfectly straight line
   set at spawn.  Add a subtle sine-wave wobble to their trajectory:
   `creep->x += (creep->fly_dx + sinf(creep->y * 0.05f) * 0.3f) * creep->speed * dt;`
   Verify they still reach the exit and are correctly killed by towers.

3. **Challenge** — The current visual for armoured creeps is a double rectangle.
   Replace it with a proper hexagonal outline drawn using six `draw_line` calls
   centred on `(c->x, c->y)`.  Ensure the outline scales with `c->size`.

---

## What's next

In Lesson 17 we add the last two complex creep types: **Spawn** creeps that
split into four children on death, and the **Boss** creep — a large slow tank
that costs multiple lives.  We also wire the victory condition so the game ends
after wave 40.
