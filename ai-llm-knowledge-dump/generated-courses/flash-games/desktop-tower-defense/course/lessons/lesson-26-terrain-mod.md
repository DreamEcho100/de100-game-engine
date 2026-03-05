# Lesson 26 — Terrain Mod (MOD_TERRAIN)

## Overview

`MOD_TERRAIN` overlays the grid with **three** new terrain types — **water
(river)**, **mountain**, and **swamp** — that affect creep movement, BFS
pathfinding, and tower placement rules.  Terrain is stored in a *separate*
array from `grid[]` so the core game systems remain clean.

The map layout is carefully designed to create strategic tension: a mountain
wall blocks the direct east–west corridor, forcing creeps to route north or
south, where each route has its own costs (rivers in the north, swamp in the
south).

---

## New cell types added to CellState

```c
typedef enum {
    CELL_EMPTY = 0,
    CELL_TOWER,
    CELL_ENTRY,
    CELL_EXIT,
    /* Terrain types stored in GameState::terrain[] (MOD_TERRAIN only) */
    CELL_WATER,     /* creeps slowed 50%;  towers cannot be placed here */
    CELL_MOUNTAIN,  /* impassable to creeps; towers get +20 % range     */
    CELL_SWAMP,     /* creeps slowed to 25% speed; towers cannot be placed */
} CellState;
```

These values are *never* written into `grid[]`.  They live in the dedicated
`terrain[]` array inside `GameState`.

---

## Concept 1 — Three terrain types and the strategic map layout

### Two parallel arrays

```
grid[]    — CELL_EMPTY / CELL_TOWER / CELL_ENTRY / CELL_EXIT
terrain[] — CELL_WATER / CELL_MOUNTAIN / CELL_SWAMP / 0 (= no terrain)
```

`grid[]` drives all existing logic (BFS walls, placement legality).
`terrain[]` adds rules that only apply in `MOD_TERRAIN`.

### The terrain map layout (30 × 20 grid, entry row=10 col=0, exit row=10 col=29)

```
 ╔══════╗                     ╔══════╗
 ║river ║  mountain wall      ║river ║
 ║(NW)  ║  cols 12-15        ║(SE)  ║
 ╚══════╝  rows 0-6 (N half) ╚══════╝
           rows 13-19 (S half)
                │  6-cell chokepoint rows 7-12
          ╔════╗│╔════╗
          ║SWAMP│(S-center bog)
          ╚════╝╚════╝
```

**Mountain wall**: columns 12–15, rows 0–6 (north) and rows 13–19 (south)
— creates a **6-row chokepoint** at rows 7–12.  Creeps travelling east
must funnel through this narrow gap.  AoE towers at the chokepoint
deal maximum splash damage.

**River NW**: columns 4–6, rows 0–7
— slows the "go north" detour route.

**River SE**: columns 23–25, rows 12–19
— slows the "go south" detour route.

**Swamp**: columns 7–11, rows 14–18
— punishes the south route severely (25 % speed).

Strategic read:
- Through the chokepoint: rewarding but requires careful tower coverage.
- North route: slowed by NW river — still viable with good frost placement.
- South route: hits swamp *and* SE river — only worth it for dedicated slow-combo builds.

### CELL_WATER (rivers)

`terrain_slow[idx] = 0.5f` — creeps move at half speed.  Tower placement
is forbidden on water cells.

### CELL_MOUNTAIN

Impassable in BFS — creeps cannot enter.  Towers *can* be placed on mountain
cells and receive a **+20 % range bonus** at placement time (elevated
sightline advantage).

### CELL_SWAMP

`terrain_slow[idx] = 0.25f` — creeps move at quarter speed.  Tower placement
is forbidden (soft muddy ground cannot support structures).  Swamp cells are
*not* BFS walls — creeps can enter them, just very slowly.

---

## Concept 2 — Terrain-aware tower placement legality

### Modifications to can_place_tower

```c
static int can_place_tower(GameState *s, int col, int row)
{
    if (col < 0 || col >= GRID_COLS || row < 0 || row >= GRID_ROWS) return 0;
    int idx = row * GRID_COLS + col;
    if (s->grid[idx] != CELL_EMPTY) return 0;
    /* MOD_TERRAIN: cannot place on water or swamp */
    if (s->active_mod == MOD_TERRAIN &&
        (s->terrain[idx] == CELL_WATER || s->terrain[idx] == CELL_SWAMP)) return 0;

    s->grid[idx] = CELL_TOWER; /* temporary block */
    /* ... double-BFS path check ... */
    /* Inside the path-BFS loop: mountain cells also block the path */
    if (s->active_mod == MOD_TERRAIN &&
        s->terrain[ni] == CELL_MOUNTAIN) continue;
}
```

Three terrain-driven placement rules:

1. **Water rejection** — water cells are flagged as no-build immediately.
2. **Swamp rejection** — swamp cells are also flagged as no-build.
3. **Path-BFS blocker** — mountains act as walls in the legality BFS so
   the simulation of "path after placement" correctly treats them as obstacles.

### Mountain range bonus at placement time

```c
/* MOD_TERRAIN: mountain towers get +20% range */
if (s->active_mod == MOD_TERRAIN &&
    s->terrain[row * GRID_COLS + col] == CELL_MOUNTAIN) {
    t->range *= 1.2f;
}
```

Applied once at placement.  The tower struct stores the final pixel range —
no per-frame lookup required.

---

## Concept 3 — Solid terrain base colors (replacing overlay blending)

In the redesigned renderer, terrain cells are drawn as **solid base colors**
rather than translucent overlays.  This makes each terrain type immediately
readable:

```c
/* Inside the grid cell loop for MOD_TERRAIN */
switch (s->terrain[idx]) {
    case CELL_WATER:
        /* Animated shimmer: alternate between two blues each ~1 s */
        cell_col = ((r + c + (int)(s->weather_timer)) & 1)
                   ? GAME_RGB(0x22, 0x88, 0xCC)
                   : GAME_RGB(0x11, 0x66, 0xAA);
        break;
    case CELL_MOUNTAIN:
        /* Rocky grey checkerboard gives texture */
        cell_col = ((r + c) & 1)
                   ? GAME_RGB(0x66, 0x60, 0x58)
                   : GAME_RGB(0x50, 0x4A, 0x44);
        break;
    case CELL_SWAMP:
        /* Dark greenish-brown bog */
        cell_col = ((r + c) & 1)
                   ? GAME_RGB(0x2A, 0x3A, 0x18)
                   : GAME_RGB(0x20, 0x2E, 0x12);
        break;
    default:
        cell_col = ((c + r) & 1) ? COLOR_GRID_ODD : COLOR_GRID_EVEN;
}
```

**Why solid base colors instead of blended overlays?**

The previous implementation used `draw_rect_blend` to paint a semi-transparent
tint on top of the normal checker tile.  The visual result was ambiguous — at
low alpha values the tint was barely noticeable; at high alpha values the
checker pattern disappeared.  By replacing the checker tile with a distinct solid
color in the first place, each terrain type has a clear unambiguous look at any
brightness setting.

### Mountain peak indicators

A small white "peak" marker is drawn on each mountain cell using `draw_rect`:

```c
if (s->terrain[idx2] == CELL_MOUNTAIN) {
    int px2 = c * CELL_SIZE + CELL_SIZE / 2;
    int py2 = r * CELL_SIZE + 3;
    draw_rect(bb, px2 - 1, py2, 3, 2, GAME_RGB(0xDD, 0xDD, 0xCC));
}
```

This gives mountain zones an immediate "rocky peaks" silhouette that is
legible even at small cell sizes.

### Swamp bubble indicators

Sparse green dots mark swamp cells using a deterministic pattern:

```c
if (s->terrain[idx2] == CELL_SWAMP) {
    if ((r * 7 + c * 13) % 3 == 0)
        draw_rect(bb, px2, py2, 2, 2, GAME_RGB(0x40, 0x58, 0x28));
}
```

The `%3` filter ensures only a third of cells show a bubble, creating a
natural scattered look without any per-frame RNG.

---

## Full implementation walkthrough

### Step 1 — Declare the new fields in game.h

```c
/* MOD_TERRAIN: per-cell terrain type and pre-computed slow multiplier */
uint8_t    terrain[GRID_ROWS * GRID_COLS];
float      terrain_slow[GRID_ROWS * GRID_COLS]; /* 0.5 water, 0.25 swamp, 1.0 else */
```

### Step 2 — Initialise terrain in init_mod_state

`init_mod_state()` is called *after* the player selects a mod from the mod
selection screen, ensuring `active_mod` is already set:

```c
void init_mod_state(GameState *s)
{
    memset(s->terrain,      0, sizeof(s->terrain));
    memset(s->terrain_slow, 0, sizeof(s->terrain_slow));
    memset(s->weather_flood,0, sizeof(s->weather_flood));

    if (s->active_mod == MOD_TERRAIN) {
        /* Mountain wall: cols 12-15, rows 0-6 (north) and rows 13-19 (south) */
        for (int r = 0; r <= 6; r++)
            for (int c = 12; c <= 15; c++) {
                int i = r * GRID_COLS + c;
                if (s->grid[i] == CELL_EMPTY) s->terrain[i] = CELL_MOUNTAIN;
            }
        for (int r = 13; r <= 19; r++)
            for (int c = 12; c <= 15; c++) {
                int i = r * GRID_COLS + c;
                if (s->grid[i] == CELL_EMPTY) s->terrain[i] = CELL_MOUNTAIN;
            }
        /* River NW: cols 4-6, rows 0-7 */
        for (int r = 0; r <= 7; r++)
            for (int c = 4; c <= 6; c++) {
                int i = r * GRID_COLS + c;
                if (s->grid[i] == CELL_EMPTY) s->terrain[i] = CELL_WATER;
            }
        /* River SE: cols 23-25, rows 12-19 */
        for (int r = 12; r <= 19; r++)
            for (int c = 23; c <= 25; c++) {
                int i = r * GRID_COLS + c;
                if (s->grid[i] == CELL_EMPTY) s->terrain[i] = CELL_WATER;
            }
        /* Swamp: cols 7-11, rows 14-18 */
        for (int r = 14; r <= 18; r++)
            for (int c = 7; c <= 11; c++) {
                int i = r * GRID_COLS + c;
                if (s->grid[i] == CELL_EMPTY) s->terrain[i] = CELL_SWAMP;
            }
        /* Pre-compute slow factors */
        for (int i = 0; i < GRID_ROWS * GRID_COLS; i++) {
            if      (s->terrain[i] == CELL_WATER)    s->terrain_slow[i] = 0.5f;
            else if (s->terrain[i] == CELL_SWAMP)    s->terrain_slow[i] = 0.25f;
            else                                      s->terrain_slow[i] = 1.0f;
        }
        bfs_fill_distance(s); /* re-run: mountains now block BFS */
    }
}
```

### Step 3 — BFS wall check

```c
if (s->active_mod == MOD_TERRAIN &&
    s->terrain[ni] == CELL_MOUNTAIN) continue;
```

This single line inside the BFS expansion loop is the entire mountain-blocking
mechanism.

### Step 4 — Creep movement slow

In `creep_move_toward_exit`:

```c
/* terrain slow (water 0.5, swamp 0.25) */
if ((s->active_mod == MOD_TERRAIN || s->active_mod == MOD_DEFAULT)
    && s->terrain_slow[ci_t] > 0.0f)
    spd *= s->terrain_slow[ci_t];
```

The slow factor is read from the pre-computed array, not re-computed each
frame.  Flying creeps do not consult `terrain_slow` because they ignore ground
topology.

### Step 5 — HUD indicator

The panel shows a green "Terrain Mod" label when this mod is active.

---

## Gameplay implications

| Feature           | Effect                                              |
|-------------------|-----------------------------------------------------|
| Mountain chokepoint | Forces all creeps through rows 7–12 unless they detour |
| NW river          | North detour is slow — good for frost+damage combos |
| SE river + Swamp  | South detour is very slow — most punishing terrain  |
| Mountain towers   | +20 % range — high-ground reward for careful placement |
| Water/Swamp no-build | Limits tower spots — requires strategic planning |

The combination of reduced routing options (mountains) and slow zones (rivers,
swamp) creates a fundamentally different spatial puzzle compared to MOD_DEFAULT.

---

## Summary

| Change                 | Where                  | Key code                                        |
|------------------------|------------------------|-------------------------------------------------|
| New cell types         | `game.h`               | `CELL_WATER`, `CELL_MOUNTAIN`, `CELL_SWAMP`     |
| New terrain arrays     | `game.h` (GameState)   | `terrain[]`, `terrain_slow[]`                   |
| Terrain init           | `init_mod_state()`     | scatter water/mountains/swamp, pre-compute slow |
| BFS mountain wall      | `bfs_fill_distance()`  | `continue` on `CELL_MOUNTAIN`                   |
| Placement legality     | `can_place_tower()`    | reject water + swamp; mountain blocks path-BFS  |
| Mountain range bonus   | `handle_placement_input()` | `t->range *= 1.2f`                         |
| Creep slow             | `creep_move_toward_exit()` | `spd *= s->terrain_slow[ci_t]`             |
| Terrain rendering      | `game_render()`        | solid base colors + peak/bubble indicators      |
