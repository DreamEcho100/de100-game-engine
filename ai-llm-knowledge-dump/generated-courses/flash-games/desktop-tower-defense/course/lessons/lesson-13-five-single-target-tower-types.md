# Lesson 13: Five Single-Target Tower Types

## What we're building

A complete data-driven tower shop with five single-target tower types: Pellet,
Squirt, Dart, Snap, and Bash.  A 160-pixel side panel shows five clickable
buttons.  The player selects a type, clicks a grid cell, and a tower is placed
with stats read directly from the `TowerDef` table — no `if (type == ...)` in
the update logic anywhere.

## What you'll learn

- How to express game balance in a **data table** (`TowerDef`) rather than
  branching code
- Simple UI hit-testing with `mouse_in_rect()`
- Visual feedback for selection state and insufficient gold
- Why data-driven design makes adding a new tower trivial

## Prerequisites

- Lessons 01–12 complete: waves run, creeps move and die, gold is tracked
- A side panel region already reserved (right of the grid)
- `draw_rect` and `draw_text` helpers available

---

## Step 1: `TowerDef` struct and the five-tower table

Add to `game.h`:

```c
/* game.h — TowerDef */
#define TOWER_COUNT 5

typedef struct {
    const char *name;
    int         cost;
    int         damage;
    float       range_cells;   /* range expressed in grid cells */
    float       fire_rate;     /* shots per second              */
    uint32_t    color;
    int         is_aoe;        /* 1 if splash, 0 if single      */
    float       splash_radius; /* pixels; 0 if not AoE          */
} TowerDef;

extern const TowerDef TOWER_DEFS[TOWER_COUNT];
```

In `levels.c` (or a new `tower_defs.c`), define the table:

```c
/* src/tower_defs.c — Desktop Tower Defense | Tower Definitions */
#include "game.h"

/*
 * Data-driven design: every tower reads its stats from this table.
 * There are no "if (type == TOWER_PELLET)" branches in the update logic.
 * Adding a new tower means adding one row here.
 * JS analogy: like a JSON config file where each key is a tower type.
 */
const TowerDef TOWER_DEFS[TOWER_COUNT] = {
    /* TOWER_PELLET — cheap, slow, low damage */
    [TOWER_PELLET] = {
        .name         = "Pellet",
        .cost         = 5,
        .damage       = 4,
        .range_cells  = 2.5f,
        .fire_rate    = 1.0f,
        .color        = GAME_RGB(0xAA, 0xAA, 0xAA),
        .is_aoe       = 0,
        .splash_radius= 0.0f,
    },
    /* TOWER_SQUIRT — medium cost, good range, medium damage */
    [TOWER_SQUIRT] = {
        .name         = "Squirt",
        .cost         = 15,
        .damage       = 10,
        .range_cells  = 3.0f,
        .fire_rate    = 1.5f,
        .color        = GAME_RGB(0x33, 0x99, 0xFF),
        .is_aoe       = 0,
        .splash_radius= 0.0f,
    },
    /* TOWER_DART — fast fire rate, medium damage, short range */
    [TOWER_DART] = {
        .name         = "Dart",
        .cost         = 20,
        .damage       = 8,
        .range_cells  = 2.0f,
        .fire_rate    = 3.0f,
        .color        = GAME_RGB(0xFF, 0x88, 0x00),
        .is_aoe       = 0,
        .splash_radius= 0.0f,
    },
    /* TOWER_SNAP — expensive, high damage, long range, slow */
    [TOWER_SNAP] = {
        .name         = "Snap",
        .cost         = 35,
        .damage       = 30,
        .range_cells  = 4.0f,
        .fire_rate    = 0.6f,
        .color        = GAME_RGB(0xFF, 0xEE, 0x22),
        .is_aoe       = 0,
        .splash_radius= 0.0f,
    },
    /* TOWER_BASH — highest damage per shot, very slow, medium range */
    [TOWER_BASH] = {
        .name         = "Bash",
        .cost         = 50,
        .damage       = 60,
        .range_cells  = 2.8f,
        .fire_rate    = 0.4f,
        .color        = GAME_RGB(0xCC, 0x44, 0xFF),
        .is_aoe       = 0,
        .splash_radius= 0.0f,
    },
};
```

---

## Step 2: Tower type constants and shop state

```c
/* game.h — tower type enum */
typedef enum {
    TOWER_PELLET = 0,
    TOWER_SQUIRT,
    TOWER_DART,
    TOWER_SNAP,
    TOWER_BASH,
} TowerType;
```

Add to `GameState`:

```c
/* game.h — GameState (excerpt) */
    int   selected_tower_type;   /* -1 = none selected */
    float shop_error_timer;      /* > 0 = flash red on button */
    int   shop_error_type;       /* which button errored */
```

---

## Step 3: `mouse_in_rect()` helper

```c
/* ui.c */
#include "game.h"

int mouse_in_rect(int mx, int my, int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}
```

---

## Step 4: `draw_ui_panel()` — tower shop buttons

```c
/* render.c — draw_ui_panel() */

#define PANEL_X      (GRID_PIXEL_W + 4)
#define PANEL_W      152
#define BTN_H        35
#define BTN_START_Y  90

void draw_ui_panel(GameBackbuffer *bb, GameState *state) {
    /* Panel background */
    draw_rect(bb, GRID_PIXEL_W, 0, SIDE_PANEL_W, CANVAS_H,
              GAME_RGB(0x22, 0x22, 0x2A));

    /* Header */
    draw_text(bb, PANEL_X, 10, "TOWERS", GAME_RGB(0xFF, 0xFF, 0xFF));
    draw_text(bb, PANEL_X, 28,
              state->phase == GAME_PHASE_PLACING ? "Place mode"
                                                  : "Wave active",
              GAME_RGB(0xAA, 0xAA, 0xAA));

    /* Gold display */
    char gold_buf[32];
    snprintf(gold_buf, sizeof(gold_buf), "Gold: %d", state->gold);
    draw_text(bb, PANEL_X, 50, gold_buf, GAME_RGB(0xFF, 0xDD, 0x00));

    /* Lives display */
    char lives_buf[24];
    snprintf(lives_buf, sizeof(lives_buf), "Lives: %d", state->lives);
    draw_text(bb, PANEL_X, 66, lives_buf, GAME_RGB(0xFF, 0x44, 0x44));

    /* Tower buttons */
    for (int i = 0; i < TOWER_COUNT; i++) {
        int by = BTN_START_Y + i * (BTN_H + 4);

        /* Button background color */
        uint32_t bg;
        if (state->shop_error_timer > 0.0f && state->shop_error_type == i)
            bg = COLOR_BTN_ERROR;
        else if (state->selected_tower_type == i)
            bg = COLOR_BTN_SELECTED;
        else
            bg = COLOR_BTN_NORMAL;

        draw_rect(bb, PANEL_X, by, PANEL_W, BTN_H, bg);

        /* Tower color swatch (4×4 square on left) */
        draw_rect(bb, PANEL_X + 4, by + (BTN_H - 8) / 2, 8, 8,
                  TOWER_DEFS[i].color);

        /* Tower name — left aligned */
        draw_text(bb, PANEL_X + 18, by + (BTN_H - 8) / 2,
                  TOWER_DEFS[i].name, GAME_RGB(0xFF, 0xFF, 0xFF));

        /* Cost — right aligned (approximate with fixed offset) */
        char cost_buf[12];
        snprintf(cost_buf, sizeof(cost_buf), "$%d", TOWER_DEFS[i].cost);
        draw_text(bb, PANEL_X + PANEL_W - 28, by + (BTN_H - 8) / 2,
                  cost_buf, GAME_RGB(0xFF, 0xDD, 0x00));
    }
}
```

Button color constants in `game.h`:

```c
#define COLOR_BTN_NORMAL   GAME_RGB(0x44, 0x44, 0x55)
#define COLOR_BTN_SELECTED GAME_RGB(0x22, 0x88, 0xFF)
#define COLOR_BTN_ERROR    GAME_RGB(0xCC, 0x22, 0x22)
```

---

## Step 5: Handling shop button clicks

In `game_handle_input()`:

```c
/* game.c — shop button click handling */
if (input->left_pressed && state->phase == GAME_PHASE_PLACING) {
    int mx = input->mouse_x;
    int my = input->mouse_y;

    for (int i = 0; i < TOWER_COUNT; i++) {
        int by = BTN_START_Y + i * (BTN_H + 4);
        if (mouse_in_rect(mx, my, PANEL_X, by, PANEL_W, BTN_H)) {
            if (state->gold < TOWER_DEFS[i].cost) {
                /* Not enough gold — flash error */
                state->shop_error_timer = 0.3f;
                state->shop_error_type  = i;
                /* play_sound(state, SOUND_ERROR); */
            } else {
                state->selected_tower_type = i;
            }
            return;
        }
    }
}

/* Tick error flash timer */
if (state->shop_error_timer > 0.0f) {
    state->shop_error_timer -= dt;
    if (state->shop_error_timer < 0.0f) state->shop_error_timer = 0.0f;
}
```

---

## Step 6: Tower placement reads from `TOWER_DEFS`

```c
/* game.c — place_tower() */
void place_tower(GameState *state, int grid_col, int grid_row) {
    int type = state->selected_tower_type;
    if (type < 0) return;
    if (state->gold < TOWER_DEFS[type].cost) return;
    if (!cell_is_buildable(state, grid_col, grid_row)) return;

    Tower *t = find_free_tower_slot(state);
    if (!t) return;

    t->type      = type;
    t->col       = grid_col;
    t->row       = grid_row;
    t->x         = (float)(grid_col * CELL_SIZE);
    t->y         = (float)(grid_row * CELL_SIZE);
    t->damage    = TOWER_DEFS[type].damage;
    t->range     = TOWER_DEFS[type].range_cells * (float)CELL_SIZE;
    t->fire_rate = TOWER_DEFS[type].fire_rate;
    t->fire_timer= 0.0f;
    t->active    = 1;

    state->gold -= TOWER_DEFS[type].cost;

    /* Mark cell occupied and recompute path */
    state->grid[grid_row][grid_col] = CELL_TOWER;
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

**Expected:** Five tower buttons appear in the side panel with names and costs.
Clicking a button highlights it (blue).  Clicking a grid cell places the tower
with the correct color, range ring, and fire rate.  Attempting to buy when broke
flashes the button red briefly.

---

## Exercises

1. **Beginner** — Change `TOWER_SNAP`'s `cost` from 35 to 25 and `TOWER_BASH`'s
   from 50 to 40 in the table.  Rebuild.  No other file needs touching — this
   demonstrates the data-driven benefit directly.

2. **Intermediate** — Add a sixth tower type `TOWER_TESLA` to the table (arc
   lightning, 45 gold, 3 range, 2.0 fire rate, bright cyan).  Add the enum
   value, increase `TOWER_COUNT` to 6, and verify it appears as a new button.

3. **Challenge** — Right now the shop shows all towers even if the player can't
   afford them (just flashes red on click).  Draw the tower name and cost in a
   dimmed color (`GAME_RGB(0x66,0x66,0x66)`) when `state->gold < TOWER_DEFS[i].cost`
   so the player can see at a glance what they can and cannot afford.

---

## What's next

In Lesson 14 we add two special tower types that bend the rules: Frost, which
slows all creeps in range directly (no projectile), and Swarm, which fires an
exploding AoE projectile — plus the armour mechanic that makes Armoured creeps
resist all other towers.
