# Lesson 07: Creep Pool and Wave Spawning

## What we're building

Ten Normal creeps spawn from the entry cell, one every 0.5 s.  Each creep
navigates to the exit via the BFS distance field.  A `compact_creeps()` function
using the **swap-remove** pattern removes dead creeps in O(n) time, keeping
`creep_count` accurate at all times.

## What you'll learn

- The **fixed free-list pool** pattern: a statically-sized array where each slot
  has an `active` flag, eliminating dynamic allocation entirely
- How a **spawn timer + spawn index** stagger creep releases without any heap
  allocations
- The **swap-remove** pattern for O(n) compaction of a flat array
- Why `next_creep_id` matters for projectile target tracking

## Prerequisites

- Lesson 06 complete: `creep_move_toward_exit()` works for a single creep
- `Creep` struct and `creeps[MAX_CREEPS]` pool in `GameState`

---

## Step 1: Add spawn-related fields to `GameState`

These fields are already declared in the final `game.h`.  Confirm they are
present:

```c
/* Inside GameState (game.h) — spawn bookkeeping */
int   wave_spawn_index;   /* how many creeps have been emitted this wave */
float wave_spawn_timer;   /* seconds until the next creep is released */
int   wave_spawn_count;   /* total creeps to emit (set to 10 for this lesson) */
```

---

## Step 2: `spawn_creep()` — find a free slot and initialise it

```c
/* src/game.c  —  spawn_creep() */

/* Spawn one creep of the given type at the entry cell.
 * hp_override: if > 0, use this HP instead of CREEP_DEFS[type].base_hp.
 * Returns the index of the new creep, or -1 if the pool is full. */
static int spawn_creep(GameState *state, CreepType type, float hp_override) {
    /* Search for an inactive slot */
    for (int i = 0; i < MAX_CREEPS; i++) {
        Creep *c = &state->creeps[i];
        if (c->active) continue;

        /* Found a free slot — initialise */
        c->type       = type;
        c->col        = ENTRY_COL;
        c->row        = ENTRY_ROW;
        c->x          = ENTRY_COL * CELL_SIZE + CELL_SIZE / 2.0f;
        c->y          = ENTRY_ROW * CELL_SIZE + CELL_SIZE / 2.0f;
        c->hp         = (hp_override > 0.0f) ? hp_override
                                              : (float)CREEP_DEFS[type].base_hp;
        c->max_hp     = c->hp;
        c->speed      = CREEP_DEFS[type].speed;
        c->lives_cost = CREEP_DEFS[type].lives_cost;
        c->slow_factor = 1.0f;
        c->is_flying  = CREEP_DEFS[type].is_flying;
        c->active     = 1;
        c->id         = ++state->next_creep_id;

        /* Keep creep_count = highest active index + 1 */
        if (i >= state->creep_count)
            state->creep_count = i + 1;

        return i;
    }
    return -1; /* pool full — should not happen with MAX_CREEPS=256 per wave */
}
```

**What's happening:**

- We scan from index 0 and take the **first** inactive slot.  For small waves
  (≤ 40 creeps) this is fast enough; later chapters introduce a free-list head
  pointer for O(1) allocation.
- `hp_override` lets the wave table scale HP by `HP_SCALE_PER_WAVE` in Lesson 12
  without changing this function.
- Updating `creep_count` eagerly avoids a separate scan; the compact step (Step 4)
  will shrink it back down when slots are freed.

---

## Step 3: Initialise the wave in `game_init()`

Replace the single hard-coded creep from Lesson 06 with the spawn-timer setup:

```c
/* src/game.c  —  game_init() updated for Lesson 07 */
void game_init(GameState *state) {
    memset(state, 0, sizeof(*state));

    state->player_gold  = STARTING_GOLD;
    state->player_lives = STARTING_LIVES;

    state->grid[ENTRY_ROW * GRID_COLS + ENTRY_COL] = CELL_ENTRY;
    state->grid[EXIT_ROW  * GRID_COLS + EXIT_COL]  = CELL_EXIT;

    bfs_update(state);

    /* ---- Lesson 07: set up a 10-creep demo wave ---- */
    state->wave_spawn_count = 10;   /* total creeps to spawn */
    state->wave_spawn_index = 0;    /* none spawned yet */
    state->wave_spawn_timer = 0.0f; /* spawn first creep immediately */
}
```

---

## Step 4: Tick the spawn timer in `game_update()`

```c
/* src/game.c  —  game_update() (Lesson 07 version) */

#define SPAWN_INTERVAL 0.5f   /* seconds between consecutive creep spawns */

void game_update(GameState *state, float dt) {
    if (dt > 0.1f) dt = 0.1f;

    /* ---- Spawn timer ---- */
    if (state->wave_spawn_index < state->wave_spawn_count) {
        state->wave_spawn_timer -= dt;
        if (state->wave_spawn_timer <= 0.0f) {
            spawn_creep(state, CREEP_NORMAL, 0.0f);
            state->wave_spawn_index++;
            state->wave_spawn_timer = SPAWN_INTERVAL;
        }
    }

    /* ---- Move all active creeps ---- */
    for (int i = 0; i < state->creep_count; i++) {
        Creep *c = &state->creeps[i];
        if (!c->active) continue;
        creep_move_toward_exit(state, c, dt);
    }

    /* ---- Remove dead/exited creeps ---- */
    compact_creeps(state);
}
```

---

## Step 5: `compact_creeps()` — swap-remove

```c
/* src/game.c  —  compact_creeps() */

/* Remove all inactive creeps from the active prefix using swap-remove.
 *
 * Invariant maintained: creeps[0 .. creep_count-1] contains ALL active creeps
 * (plus possibly a few inactive ones right before the next compact call).
 *
 * Algorithm:
 *   Scan from the end.  If creeps[i].active == 0:
 *     • Swap creeps[i] with creeps[creep_count-1].
 *     • Decrement creep_count.
 *   Continue until i passes the new end.
 *
 * O(n) — each element is visited at most twice (once as candidate, once as
 * the swap partner moved down from the tail).
 */
static void compact_creeps(GameState *state) {
    int i = state->creep_count - 1;
    while (i >= 0) {
        if (!state->creeps[i].active) {
            /* Swap with last active slot */
            state->creep_count--;
            if (i < state->creep_count) {
                state->creeps[i] = state->creeps[state->creep_count];
                /* Zero out the vacated tail slot for cleanliness */
                memset(&state->creeps[state->creep_count], 0, sizeof(Creep));
            }
        }
        i--;
    }
}
```

**What's happening:**

- After the swap, `creeps[state->creep_count]` holds the old slot's data —
  it is now outside the active range and will be overwritten the next time
  `spawn_creep` allocates that index.  The `memset` is defensive.
- Order within `creeps[]` is **not preserved** — but that is fine because the
  renderer iterates the entire range and checks `active`.
- This function is called **once per frame**, at the end of `game_update()`.
  Never call it mid-loop while iterating creeps.

JS analogy: `enemies[i] = enemies[enemies.length - 1]; enemies.pop()` —
exactly the same pattern in JavaScript.

---

## Step 6: Render all active creeps

No change from Lesson 06 — the loop already renders all active creeps:

```c
/* src/game.c  —  game_render() creep loop (unchanged) */
for (int i = 0; i < state->creep_count; i++) {
    const Creep *c = &state->creeps[i];
    if (!c->active) continue;

    uint32_t color = CREEP_DEFS[c->type].color;
    int size       = CREEP_DEFS[c->type].size;
    int px = (int)(c->x) - size / 2;
    int py = (int)(c->y) - size / 2;
    draw_rect(bb, px, py, size, size, color);
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

**Expected output:** Ten grey squares appear at the entry cell 0.5 s apart.  They
navigate along the BFS path to the exit, then disappear.  After the last creep
exits, `creep_count` returns to 0.  Place a tower mid-path and all creeps reroute
around it.

---

## Exercises

**Beginner:** Change `SPAWN_INTERVAL` from `0.5f` to `0.1f` and increase
`wave_spawn_count` to 50.  Confirm that all 50 creeps spawn, navigate, and that
`compact_creeps()` keeps `creep_count` correct throughout.  Restore both values
afterward.

**Intermediate:** Modify `spawn_creep()` to alternate between `CREEP_NORMAL` and
`CREEP_FAST` (use `wave_spawn_index % 2`).  Observe that the faster creep type
overtakes the slower ones.  Revert to `CREEP_NORMAL` only for now.

**Challenge:** Replace the linear scan in `spawn_creep()` with a **free-list
head**: add `int creep_free_head` to `GameState`, initialise it to 0, and chain
inactive slots via a separate `int next_free[MAX_CREEPS]` array so allocation is
O(1).  `compact_creeps()` should rebuild the free list as it removes inactive
entries.

---

## What's next

In Lesson 08 we introduce the **game-phase state machine** (`TITLE`, `PLACING`,
`WAVE`, `WAVE_CLEAR`, `GAME_OVER`, `VICTORY`), wire up the HUD displaying lives
and gold, and add an 8×8 bitmap font for all on-screen text.
