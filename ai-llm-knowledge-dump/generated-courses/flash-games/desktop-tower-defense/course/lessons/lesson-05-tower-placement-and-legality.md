# Lesson 05: Tower Placement and Legality Check

## What we're building

Left-click an empty grid cell to place a tower.  Right-click a placed tower to
sell it for 70% of its cost.  Hover preview shows a green tint when placement is
legal and red when it would block the only path to the exit.  The BFS distance
field is recomputed after every placement or removal.

## What you'll learn

- The `Tower` struct and a **fixed-size free-list pool** (no heap allocation)
- The **double-BFS legality check**: temporarily place the tower, re-run BFS,
  check connectivity, and either commit or undo
- `sell_tower()` with the `SELL_RATIO` refund constant

## Prerequisites

- Lesson 04 complete and building on both backends

---

## Step 1: `Tower` struct and pool — additions to `src/game.h`

```c
/* ===== LESSON 05 additions to src/game.h ===== */

/* -----------------------------------------------------------------------
 * POOL LIMITS AND ECONOMY CONSTANTS
 * ----------------------------------------------------------------------- */
#define MAX_TOWERS   256
#define SELL_RATIO   0.70f    /* 70% of tower cost refunded on sell */
#define STARTING_GOLD 50

/* -----------------------------------------------------------------------
 * TOWER TYPE ENUM
 * Only TOWER_PELLET is fully implemented in this lesson.
 * The rest are stubs for future lessons.
 * ----------------------------------------------------------------------- */
typedef enum {
    TOWER_NONE = 0,
    TOWER_PELLET,
    TOWER_COUNT,
} TowerType;

/* -----------------------------------------------------------------------
 * TOWER STRUCT
 *
 * A placed tower occupies one grid cell (col, row).
 * cx, cy are the pixel centre of that cell:
 *   cx = col * CELL_SIZE + CELL_SIZE / 2
 *   cy = row * CELL_SIZE + CELL_SIZE / 2
 *
 * JS analogy:
 *   class Tower {
 *     col; row; cx; cy; type; active; sell_value;
 *   }
 * ----------------------------------------------------------------------- */
typedef struct {
    int       col, row;      /* grid cell (integer coordinates)           */
    int       cx, cy;        /* pixel centre of the cell                  */
    TowerType type;
    int       active;        /* 0 = slot is empty (free-list pattern)     */
    int       sell_value;    /* gold returned on sell (cost * SELL_RATIO) */
} Tower;

/* -----------------------------------------------------------------------
 * GAME STATE  (additions)
 * ----------------------------------------------------------------------- */
/* Add to GameState struct: */
/*
    Tower     towers[MAX_TOWERS];
    int       tower_count;           // highest used index + 1
    TowerType selected_tower_type;   // which type is selected in the shop
    int       player_gold;
    int       shop_error_timer;      // > 0 = flash red (can't afford / illegal)
*/
```

---

## Step 2: `place_tower` and `can_place_tower` — complete implementation

```c
/* src/game.c  —  Tower placement (Lesson 05)
 *
 * can_place_tower: double-BFS legality check.
 *   Returns 1 if placing a tower at (col, row) keeps the path open.
 *   Returns 0 if placement would fully block creeps.
 *
 * Double-BFS explained:
 *   1. Temporarily mark grid[idx] = CELL_TOWER
 *   2. Run BFS — fills a temporary distance array
 *   3. If dist[entry_idx] == -1: the path is blocked → reject
 *   4. Undo: set grid[idx] back to CELL_EMPTY
 *
 * We pass a separate temp_dist array so we don't corrupt state->dist[]
 * with a rejected placement.
 *
 * JS analogy:
 *   function canPlace(grid, col, row) {
 *     grid[row][col] = TOWER;          // temporarily place
 *     const dist = bfs(grid);          // recompute BFS
 *     grid[row][col] = EMPTY;          // undo
 *     return dist[ENTRY_ROW][ENTRY_COL] !== -1;
 *   }
 */

/* Forward-declare bfs_fill_distance so can_place_tower can call it. */
static void bfs_fill_into(const GameState *state, int *dist_out);

/* bfs_fill_into: same BFS logic but writes into a caller-provided array.
 * Used by can_place_tower to test placement without modifying state->dist[]. */
static void bfs_fill_into(const GameState *state, int *dist_out) {
    for (int i = 0; i < GRID_ROWS * GRID_COLS; i++)
        dist_out[i] = -1;

    int exit_idx = EXIT_ROW * GRID_COLS + EXIT_COL;
    dist_out[exit_idx] = 0;

    int queue[GRID_ROWS * GRID_COLS];
    int head = 0, tail = 0;
    queue[tail++] = exit_idx;

    int cap = GRID_ROWS * GRID_COLS;

    while (head != tail) {
        int idx = queue[head++];
        if (head >= cap) head = 0;

        int row = idx / GRID_COLS;
        int col = idx % GRID_COLS;
        int d   = dist_out[idx];

        /* Expand 4 neighbours. */
        int dirs[4][2] = { {-1,0},{1,0},{0,-1},{0,1} };
        for (int di = 0; di < 4; di++) {
            int nr = row + dirs[di][0];
            int nc = col + dirs[di][1];
            if (nr < 0 || nr >= GRID_ROWS || nc < 0 || nc >= GRID_COLS) continue;
            int nidx = nr * GRID_COLS + nc;
            if ((CellState)state->grid[nidx] == CELL_TOWER) continue;
            if (dist_out[nidx] != -1) continue;
            dist_out[nidx] = d + 1;
            queue[tail++] = nidx;
            if (tail >= cap) tail = 0;
        }
    }
}

/* -----------------------------------------------------------------------
 * can_place_tower: returns 1 if legal, 0 if it would block the path.
 * Does NOT modify state->dist[].
 * ----------------------------------------------------------------------- */
static int can_place_tower(GameState *state, int col, int row) {
    int idx = row * GRID_COLS + col;

    /* Can't place on special cells. */
    CellState cell = (CellState)state->grid[idx];
    if (cell == CELL_ENTRY || cell == CELL_EXIT || cell == CELL_TOWER)
        return 0;

    /* Step 1: temporarily mark as tower. */
    state->grid[idx] = CELL_TOWER;

    /* Step 2: run BFS into a temporary array. */
    int temp_dist[GRID_ROWS * GRID_COLS];
    bfs_fill_into(state, temp_dist);

    /* Step 3: check if entry is still reachable. */
    int entry_idx = ENTRY_ROW * GRID_COLS + ENTRY_COL;
    int legal = (temp_dist[entry_idx] != -1);

    /* Step 4: undo — always restore the cell. */
    state->grid[idx] = CELL_EMPTY;

    return legal;
}

/* -----------------------------------------------------------------------
 * place_tower: finds a free pool slot, places the tower, updates state.
 * Returns the pool index of the new tower, or -1 on failure.
 *
 * Pool pattern: scan towers[] for the first slot where active == 0.
 * This avoids heap allocation and keeps the pool compact.
 *
 * JS analogy:
 *   const slot = towers.findIndex(t => !t.active);
 *   if (slot === -1) return -1;  // pool full
 *   towers[slot] = { active: true, col, row, ... };
 * ----------------------------------------------------------------------- */
static int place_tower(GameState *state, int col, int row, TowerType type) {
    /* Find a free slot in the pool. */
    int slot = -1;
    for (int i = 0; i < MAX_TOWERS; i++) {
        if (!state->towers[i].active) { slot = i; break; }
    }
    if (slot < 0) return -1; /* pool full */

    /* Cost table — only TOWER_PELLET for now. */
    int cost = (type == TOWER_PELLET) ? 10 : 0;
    if (state->player_gold < cost) return -1; /* can't afford */

    /* Deduct gold. */
    state->player_gold -= cost;

    /* Fill the slot. */
    Tower *t   = &state->towers[slot];
    t->col        = col;
    t->row        = row;
    t->cx         = col * CELL_SIZE + CELL_SIZE / 2;
    t->cy         = row * CELL_SIZE + CELL_SIZE / 2;
    t->type       = type;
    t->active     = 1;
    t->sell_value = (int)((float)cost * SELL_RATIO);

    /* Mark the grid cell as occupied. */
    state->grid[row * GRID_COLS + col] = CELL_TOWER;

    /* Keep tower_count updated so render only iterates active slots. */
    if (slot >= state->tower_count)
        state->tower_count = slot + 1;

    /* Re-run BFS now that the grid has changed.
     * bfs_fill_distance updates state->dist[] in-place. */
    bfs_fill_distance(state);

    return slot;
}

/* -----------------------------------------------------------------------
 * sell_tower: mark slot inactive, clear grid cell, re-run BFS, refund gold.
 * ----------------------------------------------------------------------- */
static void sell_tower(GameState *state, int idx) {
    if (idx < 0 || idx >= state->tower_count) return;
    Tower *t = &state->towers[idx];
    if (!t->active) return;

    /* Refund gold. */
    state->player_gold += t->sell_value;

    /* Clear grid cell. */
    state->grid[t->row * GRID_COLS + t->col] = CELL_EMPTY;

    /* Mark slot as free. */
    t->active = 0;
    t->type   = TOWER_NONE;

    /* Recompute BFS — the removed tower may have opened new paths. */
    bfs_fill_distance(state);
}
```

---

## Step 3: Hover preview in `draw_grid`

Update `draw_grid` in `game.c` to show a green or red preview tint when the player
is hovering over a cell with a tower type selected:

```c
static void draw_grid(const GameState *state, Backbuffer *bb) {
    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            int px = col * CELL_SIZE;
            int py = row * CELL_SIZE;

            int       idx  = row * GRID_COLS + col;
            CellState cell = (CellState)state->grid[idx];
            uint32_t  color;

            if (cell == CELL_ENTRY) {
                color = COLOR_ENTRY_CELL;
            } else if (cell == CELL_EXIT) {
                color = COLOR_EXIT_CELL;
            } else if (cell == CELL_TOWER) {
                /* Draw placed towers as a grey square with a white border. */
                color = GAME_RGB(0x88, 0x88, 0x88);
                draw_rect(bb, px, py, CELL_SIZE, CELL_SIZE, color);
                draw_rect_outline(bb, px + 2, py + 2,
                                  CELL_SIZE - 4, CELL_SIZE - 4,
                                  GAME_RGB(0xFF, 0xFF, 0xFF));
                continue; /* skip the normal fill below */
            } else {
                color = ((col + row) % 2 == 0) ? COLOR_GRID_EVEN : COLOR_GRID_ODD;
            }

            draw_rect(bb, px, py, CELL_SIZE, CELL_SIZE, color);

            /* ---- hover highlight ---- */
            if (col == state->hover_col && row == state->hover_row) {
                if (state->selected_tower_type != TOWER_NONE) {
                    /* Tower-placement preview: green = legal, red = illegal.
                     * We call can_place_tower here for simplicity; in a
                     * production build you'd cache this result in game_update. */
                    int legal = can_place_tower(
                        (GameState *)state, col, row);  /* const cast ok */
                    uint32_t tint = legal
                        ? GAME_RGB(0x00, 0xFF, 0x00)
                        : GAME_RGB(0xFF, 0x00, 0x00);
                    draw_rect_blend(bb, px, py, CELL_SIZE, CELL_SIZE, tint, 128);
                } else {
                    /* No tower selected — plain yellow hover. */
                    draw_rect_blend(bb, px, py, CELL_SIZE, CELL_SIZE,
                                    COLOR_HOVER, 128);
                }
            }
        }
    }
}
```

> ⚠️ **Note on `const` cast in `draw_grid`:** `can_place_tower` temporarily mutates
> `state->grid[]` and always restores it.  The cast is safe here, but in a larger
> codebase you'd cache the legality result in `game_update` to keep `game_render`
> truly read-only.

---

## Step 4: Input handling in `game_update`

```c
void game_update(GameState *state, float dt) {
    (void)dt;

    /* Tick the shop-error flash timer down. */
    if (state->shop_error_timer > 0.0f)
        state->shop_error_timer -= dt;

    /* ---- Hover cell computation (same as Lesson 03) ---- */
    if (mouse_in_grid(state->mouse.x, state->mouse.y)) {
        state->hover_col = state->mouse.x / CELL_SIZE;
        state->hover_row = state->mouse.y / CELL_SIZE;
    } else {
        state->hover_col = -1;
        state->hover_row = -1;
    }

    /* ---- Left-click on grid: place tower ---- */
    if (state->mouse.left_pressed &&
        state->hover_col >= 0 && state->hover_row >= 0) {

        int col = state->hover_col;
        int row = state->hover_row;
        int idx = row * GRID_COLS + col;
        CellState cell = (CellState)state->grid[idx];

        if (state->selected_tower_type != TOWER_NONE) {
            /* Legality check: can we place here without blocking the path? */
            if (can_place_tower(state, col, row)) {
                int result = place_tower(state, col, row,
                                         state->selected_tower_type);
                if (result < 0) {
                    /* Can't afford — flash the shop area red for 0.4 s. */
                    state->shop_error_timer = 0.4f;
                }
            } else {
                /* Placement would block path — flash red. */
                state->shop_error_timer = 0.4f;
            }
        } else if (cell == CELL_TOWER) {
            /* No tower type selected + click on existing tower = sell it.
             * Find the tower in the pool by matching col/row. */
            for (int i = 0; i < state->tower_count; i++) {
                if (state->towers[i].active &&
                    state->towers[i].col == col &&
                    state->towers[i].row == row) {
                    sell_tower(state, i);
                    break;
                }
            }
        }
    }

    /* ---- Right-click: deselect tower type ---- */
    if (state->mouse.right_pressed) {
        state->selected_tower_type = TOWER_NONE;
    }
}
```

---

## Step 5: Additions to `game_init`

```c
void game_init(GameState *state) {
    memset(state, 0, sizeof(*state));
    state->hover_col           = -1;
    state->hover_row           = -1;
    state->player_gold         = STARTING_GOLD;
    state->selected_tower_type = TOWER_PELLET; /* start with pellet selected */

    state->grid[ENTRY_ROW * GRID_COLS + ENTRY_COL] = CELL_ENTRY;
    state->grid[EXIT_ROW  * GRID_COLS + EXIT_COL ] = CELL_EXIT;

    bfs_fill_distance(state);
}
```

---

## Step 6: Temporary key binding — press `P` to select Pellet tower

Add this to `game_update` (requires reading Raylib key input via a thin wrapper,
or using `XLookupKeysym` in the X11 backend).  For now we use a simpler approach:
expose a global flag from the platform:

```c
/* In game_update, after the hover section: */
/* Press [P] to select pellet tower (simple keyboard shortcut for testing).
 * In Lesson 06 we will add a proper shop UI panel.
 * For now, platform_get_input sets a key flag we can read. */
/* (keyboard input integration is covered in Lesson 06)                    */

/* Temporary workaround for testing: start with TOWER_PELLET selected in
 * game_init (done above).  Right-click deselects.  Left-click places.     */
```

---

## Build and run

```bash
# Raylib
./build-dev.sh --backend=raylib -r

# X11
./build-dev.sh --backend=x11 -r
```

**Expected output:**
- The game starts with 50 gold and Pellet tower selected.
- Hover over an empty cell: green tint (legal placement).
- Left-click: a grey square appears; gold decreases by 10.
- Try to build a complete wall across the grid (20 towers in a column):
  the last tower that would block the path shows a red tint and cannot be placed.
- Left-click an existing tower with no type selected: tower disappears and gold
  increases by 7 (70% of 10).
- Right-click anywhere: deselects the tower type (yellow hover returns).

---

## Double-BFS legality check — walkthrough

```
can_place_tower(state, col=15, row=9):

  1. state->grid[9 * 30 + 15] = CELL_TOWER   (temporary)

  2. bfs_fill_into(state, temp_dist):
       BFS starts at exit (29, 9).
       The wave reaches (14, 9) but hits the new CELL_TOWER at (15, 9).
       If other rows provide an alternate route, entry (0, 9) is still reachable.
       If tower at (15, 9) joins a complete column of towers, BFS cannot
       reach the left side → temp_dist[entry_idx] == -1.

  3. legal = (temp_dist[entry_idx] != -1)

  4. state->grid[9 * 30 + 15] = CELL_EMPTY   (always undo)

  return legal
```

The key insight: we **always** undo the temporary placement before returning.
Even if the placement is legal, `can_place_tower` restores the grid; `place_tower`
then sets `CELL_TOWER` permanently only after the legality check passes.

---

## Exercises

1. **Beginner:** Try placing towers in a line from row 0 to row 19 in column 15.
   Which tower triggers the red "illegal" preview?  Confirm by counting: 20 towers
   in a vertical column completely blocks all paths — but the BFS check stops you
   at tower 20.

2. **Intermediate:** Add a `TOWER_SQUIRT` type with a cost of 20.  Add it to the
   enum, give it a blue color (`GAME_RGB(0x44, 0x88, 0xCC)`), and draw it in
   `draw_grid` with the same grey-square-plus-white-outline pattern but using its
   blue color for the fill.  Temporarily select it in `game_init` and verify it
   costs 20 gold.

3. **Challenge:** The current hover-preview calls `can_place_tower` every frame for
   the hovered cell — which runs a full BFS every frame.  Cache the result in a
   `hover_is_legal` field on `GameState`, computed once in `game_update` only when
   `hover_col` or `hover_row` changed.  Measure: how many BFS calls per second does
   the naive version run at 60 fps?  How many does the cached version run?

---

## What's next

In Lesson 06 we add the side panel UI: a shop that lets the player click to select
different tower types, a gold display, and a "Start Wave" button.  We'll also add
basic keyboard shortcuts for tower selection.
