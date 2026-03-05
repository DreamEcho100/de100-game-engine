# Lesson 15: All Five Targeting Modes

## What we're building

All five targeting modes — **First**, **Last**, **Strongest**, **Weakest**, and
**Closest** — selectable per tower.  Right-clicking a placed tower cycles
through modes.  Left-clicking a placed tower opens an info box in the side panel
bottom showing the tower's type, current mode, and a **Sell** button that
refunds half the tower cost.

## What you'll learn

- How a `switch` on an enum cleanly replaces five `if/else` branches
  (the **comparator pattern**)
- How to use the pre-computed BFS distance map for First/Last targeting without
  extra pathfinding
- Right-click vs left-click handling for the same set of grid cells
- How to build a minimal info panel that responds to `selected_tower_idx`

## Prerequisites

- Lessons 01–14 complete: all seven tower types exist, projectiles fire, BFS
  distance map is computed and stored in `state->dist[][]`
- `mouse_in_rect()` helper exists in `ui.c`

---

## Step 1: `TargetMode` enum and `TARGET_MODE_NAMES`

```c
/* game.h — targeting */
typedef enum {
    TARGET_FIRST    = 0,  /* creep furthest along the path   */
    TARGET_LAST,          /* creep least far along the path  */
    TARGET_STRONGEST,     /* creep with highest current HP   */
    TARGET_WEAKEST,       /* creep with lowest  current HP   */
    TARGET_CLOSEST,       /* creep nearest the tower center  */
    TARGET_COUNT,
} TargetMode;
```

```c
/* ui.c (or game.c) — name table */
static const char *TARGET_MODE_NAMES[TARGET_COUNT] = {
    "First",
    "Last",
    "Strongest",
    "Weakest",
    "Closest",
};
```

Add `target_mode` to the `Tower` struct:

```c
/* game.h — Tower struct (excerpt) */
typedef struct {
    /* ... existing fields ... */
    TargetMode target_mode;
} Tower;
```

Default to `TARGET_FIRST` when a tower is placed:

```c
/* game.c — place_tower() */
t->target_mode = TARGET_FIRST;
```

---

## Step 2: `find_best_target()` — the comparator pattern

```c
/* tower.c — find_best_target() */
/*
 * Comparator pattern: each case performs exactly one comparison.
 * A switch on 5 cases is more readable than function pointers and
 * just as fast for this number of variants.
 */
int find_best_target(GameState *state, Tower *tower) {
    float tx = tower->x + CELL_SIZE * 0.5f;
    float ty = tower->y + CELL_SIZE * 0.5f;
    float range = tower->range;

    int   best_idx   = -1;
    int   best_int   = 0;      /* used for STRONGEST / WEAKEST      */
    float best_float = 0.0f;   /* used for CLOSEST                  */
    int   best_dist  = 0;      /* used for FIRST / LAST (BFS units) */

    for (int i = 0; i < MAX_CREEPS; i++) {
        Creep *c = &state->creeps[i];
        if (!c->active) continue;

        /* Range check */
        float dx = c->x + CELL_SIZE * 0.5f - tx;
        float dy = c->y + CELL_SIZE * 0.5f - ty;
        if (dx*dx + dy*dy > range*range) continue;

        /* BFS distance at the creep's current cell */
        int col = (int)(c->x / CELL_SIZE);
        int row = (int)(c->y / CELL_SIZE);
        int cell_dist = state->dist[row][col];   /* -1 = unreachable */
        if (cell_dist < 0) continue;

        switch (tower->target_mode) {
            case TARGET_FIRST:
                /* Lowest BFS dist = closest to exit = furthest along path */
                if (best_idx < 0 || cell_dist < best_dist) {
                    best_idx  = i;
                    best_dist = cell_dist;
                }
                break;

            case TARGET_LAST:
                /* Highest BFS dist = furthest from exit = least far along */
                if (best_idx < 0 || cell_dist > best_dist) {
                    best_idx  = i;
                    best_dist = cell_dist;
                }
                break;

            case TARGET_STRONGEST:
                if (best_idx < 0 || c->hp > best_int) {
                    best_idx = i;
                    best_int = c->hp;
                }
                break;

            case TARGET_WEAKEST:
                if (best_idx < 0 || c->hp < best_int) {
                    best_idx = i;
                    best_int = c->hp;
                }
                break;

            case TARGET_CLOSEST: {
                /* No sqrt needed — compare squared distances */
                float dist2 = dx*dx + dy*dy;
                if (best_idx < 0 || dist2 < best_float) {
                    best_idx   = i;
                    best_float = dist2;
                }
                break;
            }

            default: break;
        }
    }

    return best_idx;
}
```

**Comparator pattern note:**  
`find_best_target()` uses a `switch` on the targeting mode — each case is just
one comparison.  This is more readable than function pointers and just as fast
for 5 cases.  Adding a sixth mode means adding one `case`.

---

## Step 3: Right-click cycles target mode

In `game_handle_input()`:

```c
/* game.c — right-click on grid */
if (input->right_pressed) {
    int mx = input->mouse_x;
    int my = input->mouse_y;

    /* Only process clicks on the grid area */
    if (mx < GRID_PIXEL_W && my < GRID_PIXEL_H) {
        int col = mx / CELL_SIZE;
        int row = my / CELL_SIZE;

        for (int i = 0; i < MAX_TOWERS; i++) {
            Tower *t = &state->towers[i];
            if (!t->active) continue;
            if (t->col != col || t->row != row) continue;

            /* Cycle mode */
            t->target_mode = (TargetMode)((t->target_mode + 1) % TARGET_COUNT);
            state->selected_tower_idx = -1;   /* deselect */
            break;
        }
    }
}
```

---

## Step 4: Left-click selects a placed tower

```c
/* game.c — left-click on grid (in PLACING phase) */
if (input->left_pressed && mx < GRID_PIXEL_W && my < GRID_PIXEL_H) {
    int col = mx / CELL_SIZE;
    int row = my / CELL_SIZE;

    int hit_tower = -1;
    for (int i = 0; i < MAX_TOWERS; i++) {
        Tower *t = &state->towers[i];
        if (!t->active) continue;
        if (t->col == col && t->row == row) { hit_tower = i; break; }
    }

    if (hit_tower >= 0) {
        /* Select existing tower — show info box */
        state->selected_tower_idx  = hit_tower;
        state->selected_tower_type = -1;   /* deselect shop */
    } else if (state->selected_tower_type >= 0) {
        /* Place new tower */
        place_tower(state, col, row);
    }
}
```

`selected_tower_idx` defaults to `-1` in `GameState`.

---

## Step 5: Tower info box in the side panel

```c
/* render.c — draw_tower_info_box() */
#define INFO_BOX_Y  400

void draw_tower_info_box(GameBackbuffer *bb, GameState *state) {
    int idx = state->selected_tower_idx;
    if (idx < 0) return;

    Tower    *t  = &state->towers[idx];
    if (!t->active) { state->selected_tower_idx = -1; return; }

    const TowerDef *td = &TOWER_DEFS[t->type];

    /* Background */
    draw_rect(bb, PANEL_X, INFO_BOX_Y, PANEL_W, CANVAS_H - INFO_BOX_Y - 4,
              GAME_RGB(0x1A, 0x1A, 0x28));

    /* Tower type name */
    draw_text(bb, PANEL_X + 4, INFO_BOX_Y + 6,  td->name,
              GAME_RGB(0xFF, 0xFF, 0xFF));

    /* Targeting mode */
    char mode_buf[32];
    snprintf(mode_buf, sizeof(mode_buf),
             "Mode: %s", TARGET_MODE_NAMES[t->target_mode]);
    draw_text(bb, PANEL_X + 4, INFO_BOX_Y + 22, mode_buf,
              GAME_RGB(0xAA, 0xDD, 0xFF));

    /* Hint */
    draw_text(bb, PANEL_X + 4, INFO_BOX_Y + 38,
              "[RMB] cycle mode",
              GAME_RGB(0x77, 0x77, 0x88));

    /* Sell button: refund = cost / 2 */
    int sell_y = INFO_BOX_Y + 58;
    char sell_buf[24];
    snprintf(sell_buf, sizeof(sell_buf), "Sell: $%d", td->cost / 2);
    draw_rect(bb, PANEL_X + 4, sell_y, PANEL_W - 8, 24,
              COLOR_BTN_NORMAL);
    draw_text(bb, PANEL_X + 8, sell_y + 6, sell_buf,
              GAME_RGB(0xFF, 0xDD, 0x00));
}
```

Call `draw_tower_info_box` at the end of `draw_ui_panel`.

---

## Step 6: Sell button click

```c
/* game.c — sell button click (in left-click handler, after grid check) */
if (state->selected_tower_idx >= 0 && mouse_in_rect(mx, my,
        PANEL_X + 4, INFO_BOX_Y + 58, PANEL_W - 8, 24)) {
    sell_tower(state, state->selected_tower_idx);
    state->selected_tower_idx = -1;
}
```

```c
/* tower.c — sell_tower() */
void sell_tower(GameState *state, int idx) {
    Tower *t = &state->towers[idx];
    if (!t->active) return;

    state->gold += TOWER_DEFS[t->type].cost / 2;

    state->grid[t->row][t->col] = CELL_EMPTY;
    t->active = 0;

    recompute_path(state);
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
      src/tower_defs.c src/ui.c \
      -lraylib -lm -lpthread -ldl

./build/dtd
```

**Expected:** Right-clicking a placed tower cycles its label through
First→Last→Strongest→Weakest→Closest.  Left-clicking a tower opens the info box
with the current mode and a sell button.  Clicking **Sell** removes the tower
and refunds half its gold.  Different modes visibly target different creeps
during a wave — most dramatically, Last hits the newest arrivals while First
targets those almost at the exit.

---

## Exercises

1. **Beginner** — Add a sixth targeting mode `TARGET_RANDOM` that picks a
   random creep in range.  Add the enum value, increment `TARGET_COUNT`, add
   `"Random"` to `TARGET_MODE_NAMES`, and add a `case TARGET_RANDOM:` branch in
   `find_best_target()` using `rand() % MAX_CREEPS` with an active/in-range
   guard.

2. **Intermediate** — Display the sell amount in the info box in real time:
   towers degrade in value over time at a rate of 1 gold per 10 seconds of
   being placed.  Add a `sell_value` field to `Tower`, initialise it to
   `cost / 2`, and decrement it by `dt / 10.0f` each frame.  Show
   `"Sell: $N"` using the live `sell_value`.

3. **Challenge** — Highlight the tower's range circle on the grid when it is
   selected.  In `draw_grid()`, after rendering tiles, check
   `state->selected_tower_idx`.  If valid, draw a translucent circle overlay of
   radius `tower->range` centred on the tower cell using `draw_circle_outline`
   (implemented in Lesson 14's challenge exercise).

---

## What's next

The core game is now feature-complete.  Future lessons cover sound effects and
music (platform audio), saving high scores to disk with `fopen`/`fwrite`, and
polishing the UI with animated buttons and wave-clear fanfare.
