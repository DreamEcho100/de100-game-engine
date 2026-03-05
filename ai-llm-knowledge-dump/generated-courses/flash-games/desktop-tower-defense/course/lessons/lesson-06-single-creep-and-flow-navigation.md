# Lesson 06: Single Creep and Flow Field Navigation

## What we're building

A single grey Normal creep spawns at the entry cell (`col=0, row=9`) and smoothly
navigates through the BFS distance field — built in Lesson 04 — to the exit cell
(`col=29, row=9`).  The creep threads around any towers you have placed and
disappears when it reaches the exit.  No wave system yet; just one creep proving
that `dist[]` drives real movement.

## What you'll learn

- How a `Creep` struct stores both a **float pixel position** (`x`, `y`) and an
  **integer grid cell** (`col`, `row`) simultaneously
- How `creep_move_toward_exit()` samples the BFS `dist[]` array to choose the
  next cell, then interpolates the float position toward that cell's centre at a
  constant speed
- The arrival threshold pattern: snap `col`/`row` when the float position is
  within 1 px of the cell centre
- How to detect exit arrival and mark a creep inactive
- Rendering a moving creep as a centred 14×14 square

## Prerequisites

- Lesson 04 complete: `bfs_update(state)` populates `state->dist[]`
- Lesson 05 complete: towers can be placed; `grid[]` is accurate
- `draw_rect()` from `utils/draw-shapes.h`

---

## Step 1: Add the `Creep` struct and creep pool to `game.h`

The `Creep` struct is already present in the final `game.h` shown in the course
plan.  Make sure these fields exist before continuing:

```c
/* src/game.h  —  Creep entity (excerpt, add if not already present) */

typedef struct {
    float     x, y;        /* float pixel position (centre of creep) */
    int       col, row;    /* integer grid cell the creep currently occupies */
    CreepType type;
    float     hp;
    float     max_hp;
    float     speed;       /* base speed in pixels/second */
    int       active;      /* 0 = slot is free */
    int       id;          /* unique ID assigned at spawn */
    int       is_flying;   /* 1 = ignores dist[] field (added in Lesson 16) */
    float     slow_timer;  /* seconds of slow remaining (Lesson 14) */
    float     slow_factor; /* speed multiplier when slowed, e.g. 0.5 */
    float     stun_timer;  /* seconds of stun remaining (Lesson 14) */
    float     death_timer; /* counting-down death animation (Lesson 11) */
    int       lives_cost;  /* lives lost if this creep exits */
    float     fly_dx, fly_dy; /* unit direction for flying creeps */
} Creep;
```

In `GameState` ensure the pool and counter exist:

```c
/* Inside GameState (game.h) */
Creep  creeps[MAX_CREEPS];
int    creep_count;
int    next_creep_id;   /* monotonically incremented; gives each creep a unique ID */
```

---

## Step 2: Spawn one creep in `game_init()`

For this lesson we hard-code a single Normal creep.  The full wave system arrives
in Lesson 07.

```c
/* src/game.c  —  game_init() addition */
void game_init(GameState *state) {
    memset(state, 0, sizeof(*state));

    /* Economy */
    state->player_gold  = STARTING_GOLD;
    state->player_lives = STARTING_LIVES;

    /* Grid: mark entry and exit cells */
    state->grid[ENTRY_ROW * GRID_COLS + ENTRY_COL] = CELL_ENTRY;
    state->grid[EXIT_ROW  * GRID_COLS + EXIT_COL]  = CELL_EXIT;

    /* Initial BFS (no towers yet) */
    bfs_update(state);

    /* ---- Lesson 06: spawn one Normal creep at entry ---- */
    Creep *c = &state->creeps[0];
    c->type       = CREEP_NORMAL;
    c->col        = ENTRY_COL;
    c->row        = ENTRY_ROW;
    /* Centre the float position on the entry cell */
    c->x          = ENTRY_COL * CELL_SIZE + CELL_SIZE / 2.0f;
    c->y          = ENTRY_ROW * CELL_SIZE + CELL_SIZE / 2.0f;
    c->hp         = CREEP_DEFS[CREEP_NORMAL].base_hp;
    c->max_hp     = c->hp;
    c->speed      = CREEP_DEFS[CREEP_NORMAL].speed;
    c->lives_cost = CREEP_DEFS[CREEP_NORMAL].lives_cost;
    c->slow_factor = 1.0f;   /* not slowed */
    c->active     = 1;
    c->id         = ++state->next_creep_id;
    state->creep_count = 1;
}
```

**What's happening:**

- `x` and `y` are the **centre** of the creep sprite, not the top-left corner.
  Centering on `col * CELL_SIZE + CELL_SIZE/2` puts the creep in the middle of
  its starting cell.
- `slow_factor = 1.0f` means "full speed" — the slow system (Lesson 14) will
  multiply the effective speed by this value.

---

## Step 3: Implement `creep_move_toward_exit()`

This is the core movement function.  It reads `dist[]` to pick the best
neighbouring cell, then smoothly moves the float position toward that cell's
pixel centre.

```c
/* src/game.c  —  creep_move_toward_exit() */

/* Offsets for the four cardinal neighbours */
static const int NEIGHBOUR_DC[4] = { 0,  0, -1,  1 };
static const int NEIGHBOUR_DR[4] = {-1,  1,  0,  0 };

static void creep_move_toward_exit(GameState *state, Creep *creep, float dt) {
    /* 1. If already at exit, deactivate */
    if (creep->col == EXIT_COL && creep->row == EXIT_ROW) {
        state->player_lives -= creep->lives_cost;
        if (state->player_lives < 0) state->player_lives = 0;
        creep->active = 0;
        return;
    }

    /* 2. Sample dist[] at current integer cell */
    int cur_idx  = creep->row * GRID_COLS + creep->col;
    int cur_dist = state->dist[cur_idx];

    /* 3. Find the neighbour with the smallest dist value (closest to exit) */
    int best_col = creep->col;
    int best_row = creep->row;
    int best_dist = cur_dist;

    for (int i = 0; i < 4; i++) {
        int nc = creep->col + NEIGHBOUR_DC[i];
        int nr = creep->row + NEIGHBOUR_DR[i];

        /* Bounds check */
        if (nc < 0 || nc >= GRID_COLS || nr < 0 || nr >= GRID_ROWS) continue;

        /* Cannot walk through a tower cell (unless it IS the exit) */
        uint8_t cell_state = state->grid[nr * GRID_COLS + nc];
        if (cell_state == CELL_TOWER) continue;

        int nd = state->dist[nr * GRID_COLS + nc];
        if (nd < best_dist) {
            best_dist = nd;
            best_col  = nc;
            best_row  = nr;
        }
    }

    /* 4. Compute the pixel centre of the best neighbour */
    float target_cx = best_col * CELL_SIZE + CELL_SIZE / 2.0f;
    float target_cy = best_row * CELL_SIZE + CELL_SIZE / 2.0f;

    /* 5. Move float position toward target centre at (speed * slow_factor) px/s */
    float effective_speed = creep->speed * creep->slow_factor;
    float dx = target_cx - creep->x;
    float dy = target_cy - creep->y;
    float dist_to_target = sqrtf(dx * dx + dy * dy);
    float step = effective_speed * dt;

    if (dist_to_target <= 1.0f || step >= dist_to_target) {
        /* 6. Arrival: snap position and update integer cell */
        creep->x   = target_cx;
        creep->y   = target_cy;
        creep->col = best_col;
        creep->row = best_row;
    } else {
        /* Still moving: advance along the direction vector */
        float inv = step / dist_to_target;
        creep->x += dx * inv;
        creep->y += dy * inv;
    }
}
```

**What's happening:**

- **Step 2 — sample `dist[]`:** The BFS stored the integer distance (in cells)
  from each cell to the exit.  `cur_dist` is the creep's current distance.
- **Step 3 — pick best neighbour:** Any neighbour whose `dist` is smaller is
  "closer to exit".  We take the smallest one, which greedy-follows the gradient
  of the distance field.  If all neighbours have equal or larger distances, the
  creep stays put (it is either stuck or at the exit).
- **Step 4 — target centre:** `col * CELL_SIZE + CELL_SIZE/2` gives the exact
  pixel centre of a cell.  `CELL_SIZE = 20`, so cell (2,3) has centre (50, 70).
- **Step 5 — interpolate:** Rather than teleporting, we advance a fixed number
  of pixels this frame.  Dividing `(dx, dy)` by `dist_to_target` normalises the
  direction; multiplying by `step` gives the correct distance to advance.
- **Arrival threshold (1 px):** Floating-point arithmetic rarely produces an
  exact zero; checking `<= 1.0f` prevents a creep from oscillating around the
  centre forever.

JS analogy: `creep.x += dx * speed * dt` is identical to `element.style.left`
being driven by `requestAnimationFrame` deltas — same formula, different
execution context.

---

## Step 4: Call `creep_move_toward_exit()` from `game_update()`

```c
/* src/game.c  —  game_update() */
void game_update(GameState *state, float dt) {
    /* Clamp to prevent huge time steps (e.g. after a debugger pause) */
    if (dt > 0.1f) dt = 0.1f;

    /* Move all active creeps */
    for (int i = 0; i < state->creep_count; i++) {
        Creep *c = &state->creeps[i];
        if (!c->active) continue;
        creep_move_toward_exit(state, c, dt);
    }
}
```

---

## Step 5: Render the creep in `game_render()`

Draw the creep as a 14×14 square, centred on its float position.  The 14 px size
leaves a 3-pixel gap to the cell borders (cell is 20 px wide), making individual
creeps easy to distinguish when multiple creeps occupy adjacent cells.

```c
/* src/game.c  —  game_render() addition */
void game_render(const GameState *state, Backbuffer *bb) {
    /* --- Draw grid (from earlier lessons) --- */
    /* ... existing grid drawing code ... */

    /* --- Draw creeps --- */
    for (int i = 0; i < state->creep_count; i++) {
        const Creep *c = &state->creeps[i];
        if (!c->active) continue;

        uint32_t color = CREEP_DEFS[c->type].color;
        int size       = CREEP_DEFS[c->type].size;   /* e.g. 14 for Normal */

        /* Centre the rectangle on c->x, c->y */
        int px = (int)(c->x) - size / 2;
        int py = (int)(c->y) - size / 2;
        draw_rect(bb, px, py, size, size, color);
    }
}
```

Make sure `CREEP_DEFS` is populated in `game.c`.  A minimal version for this
lesson:

```c
/* src/game.c  —  CREEP_DEFS table (lesson-06 subset) */
const CreepDef CREEP_DEFS[CREEP_COUNT] = {
    /* CREEP_NONE */      { 0,   0, 0, COLOR_BLACK,        0, 0, 0,  0 },
    /* CREEP_NORMAL */    { 60,  10, 1, COLOR_CREEP_NORMAL, 0, 0, 1, 14 },
    /* CREEP_FAST */      { 120, 6,  1, COLOR_CREEP_FAST,   0, 0, 1, 12 },
    /* CREEP_FLYING */    { 90,  8,  1, COLOR_CREEP_FLYING, 1, 0, 2, 12 },
    /* CREEP_ARMOURED */  { 45,  20, 1, COLOR_CREEP_ARMOURED,0,1, 2, 16 },
    /* CREEP_SPAWN */     { 55,  15, 2, COLOR_CREEP_SPAWN,  0, 0, 3, 16 },
    /* CREEP_SPAWN_CHILD*/{ 80,  5,  1, COLOR_CREEP_NORMAL, 0, 0, 0, 10 },
    /* CREEP_BOSS */      { 30,  200,5, COLOR_CREEP_BOSS,   0, 1, 20,18 },
};
```

---

## Build and run

```bash
# From the course/ directory
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

**Expected output:** A 760×520 window showing the grey grid from earlier lessons,
with a single 14×14 grey square spawning at the left-middle cell (col 0, row 9)
and smoothly walking right toward col 29, navigating around any towers you have
placed.  When it reaches the exit cell it disappears and `player_lives` decreases
by 1 (visible in a later lesson's HUD).

---

## Exercises

**Beginner:** Change the spawn position to `ENTRY_COL=0, ENTRY_ROW=0` (top-left
corner).  Rebuild and confirm the creep navigates from the corner to exit row 9.
Then restore the original entry.

**Intermediate:** Add a second creep to `game_init()` at `col=0, row=10`, with
`speed = 90.0f`.  Confirm both creeps navigate simultaneously and the faster one
overtakes the first.

**Challenge:** Modify `creep_move_toward_exit()` so the creep prefers
**diagonally** adjacent cells as well as cardinal ones (expand the neighbour
offsets to all 8 directions).  Does the path look more natural?  What happens to
the arrival threshold?  Revert afterward — BFS on a 4-connected grid is the
correct approach for this game.

---

## What's next

In Lesson 07 we replace the single hard-coded creep with a **pool of 256 slots**,
a spawn timer that releases one creep every 0.5 s, and a `compact_creeps()` swap-
remove function that keeps the active slice tight.
