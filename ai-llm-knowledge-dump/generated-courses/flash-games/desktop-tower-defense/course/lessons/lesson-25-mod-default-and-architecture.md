# Lesson 25 — Mod Architecture and MOD_DEFAULT

## Overview

In this lesson we introduce the concept of **game mods** — a clean way to add
variant gameplay without duplicating logic.  We look at the `GameMod` enum, how
`active_mod` is stored in `GameState`, and use `MOD_DEFAULT` as the baseline
that every other mod extends.

---

## Concept 1 — What "mod" means in game architecture

A *game mod* is a named variant of the simulation that changes a subset of
rules while leaving all other systems intact.  The key design principle is:

> **The mod is a tag, not a fork.**

Instead of copying `game.c` for every variant, we keep a single code-path and
insert *conditional branches* that are only active when the relevant tag is set.
This means:

- Existing tests / tooling still work on the default path.
- Each mod touches only the systems it actually changes.
- Adding a new mod never risks breaking another mod.

Classic examples from commercial games:
| Game          | "Mod"          | What changes                      |
|---------------|----------------|-----------------------------------|
| StarCraft     | Fastest map    | resource / speed multipliers      |
| Tower defence | Hard mode      | creep HP scaled up                |
| Our DTD       | MOD_NIGHT      | range penalty + per-type bonuses  |

The pattern is always the same: **one enum value → one cluster of conditional
branches scattered across the simulation**.

---

## Concept 2 — GameMod enum and `active_mod` in GameState

In `src/game.h` we declare:

```c
/* Game mod — runtime variant selected before the game starts */
typedef enum {
    MOD_DEFAULT = 0,  /* standard gameplay; no modifications */
    MOD_TERRAIN,      /* water/mountain terrain affects movement and placement */
    MOD_WEATHER,      /* periodic weather cycles that alter creep speed */
    MOD_NIGHT,        /* reduced range; per-type night-vision bonuses */
    MOD_BOSS,         /* boss waves every 5 waves; shields and mid-HP splits */
    MOD_COUNT,
} GameMod;
```

`MOD_DEFAULT = 0` is deliberate: `memset(s, 0, ...)` in `game_init` will zero
every field, so forgetting to set `active_mod` naturally gives the default
behaviour.

`active_mod` lives inside `GameState`:

```c
typedef struct {
    GamePhase  phase;
    float      phase_timer;

    /* Active mod — set before game_init(), read throughout simulation */
    GameMod    active_mod;

    uint8_t    grid[GRID_ROWS * GRID_COLS];
    /* ... */
} GameState;
```

The important contract:

1. `active_mod` is **set by the caller before `game_init()`**.
2. `game_init()` reads it to perform mod-specific initialisation (e.g., laying
   out terrain cells for MOD_TERRAIN).
3. Every system that cares about the mod checks `s->active_mod` with a simple
   `if` or `switch`.

---

## Concept 3 — Mod selection and branching

The mod-select screen (handled by a separate part of the codebase) sets
`s->active_mod` and then calls `game_init(s)`.  After that, every conditional
branch follows the same pattern:

```c
/* Example from apply_damage() */
if (s->active_mod == MOD_BOSS && c->shield_timer > 0.0f) return; /* immune */
```

Branching rules we follow throughout DTD:

| Rule | Rationale |
|------|-----------|
| Always check `active_mod` first (short-circuit) | zero overhead in MOD_DEFAULT |
| Never mutate global tables (`TOWER_DEFS`, `CREEP_DEFS`) | other mods share them |
| Store mod-specific state in `GameState`, not in statics | allows future save/load |
| Mod rendering is additive (draw on top) | never remove existing draw calls |

---

## MOD_DEFAULT — the baseline

`MOD_DEFAULT` requires **zero** additional branches anywhere.  It is the
"null mod" and exists to give the enum a stable 0 value.  Every system in
`game.c` already implements the default rules, so no code ever writes:

```c
if (s->active_mod == MOD_DEFAULT) { /* do something */ }
```

That would be redundant — the unconditional code is already the default path.

### What the default mod provides

| System            | Default behaviour                                          |
|-------------------|------------------------------------------------------------|
| BFS pathfinding   | Blocks on CELL_TOWER only                                  |
| Tower placement   | Any CELL_EMPTY cell that keeps the path open               |
| Creep movement    | BFS gradient + slow/stun from tower effects                |
| Wave spawning     | `g_waves[]` table from levels.c, bosses at waves 10/15/20+ |
| Economy           | 5 % interest, SELL_RATIO = 0.70                            |
| Rendering         | Grid, towers, creeps, projectiles, particles, HUD          |

All other mods **extend** this list by adding branches that fire only when
`active_mod != MOD_DEFAULT`.

---

## How branches are written — worked examples

### A — bfs_fill_distance (MOD_TERRAIN adds mountain walls)

```c
for (int d = 0; d < 4; d++) {
    /* ... bounds checks ... */
    int ni = nr * GRID_COLS + nc;
    if (s->dist[ni] >= 0)          continue;
    if (s->grid[ni] == CELL_TOWER) continue;   /* default wall */
    /* MOD_TERRAIN: mountains are impassable to creeps */
    if (s->active_mod == MOD_TERRAIN &&
        s->terrain[ni] == CELL_MOUNTAIN) continue;
    s->dist[ni] = s->dist[idx] + 1;
    queue[tail++] = ni;
}
```

The default path never enters the `MOD_TERRAIN` branch, so performance is
unchanged for `MOD_DEFAULT`.

### B — update_wave (MOD_WEATHER adds the weather cycle)

```c
static void update_wave(GameState *s, float dt)
{
    /* ... */

    /* MOD_WEATHER: advance the weather cycle (0–45 s) */
    if (s->active_mod == MOD_WEATHER) {
        s->weather_timer += dt;
        if (s->weather_timer >= 45.0f) s->weather_timer -= 45.0f;
        s->weather_phase = (int)(s->weather_timer / 15.0f);
        if (s->weather_phase > 2) s->weather_phase = 2;
    }
    /* ... */
}
```

### C — game_render (MOD_NIGHT adds a dark overlay)

```c
/* Night overlay (MOD_NIGHT): dark blue semi-transparent wash */
if (s->active_mod == MOD_NIGHT) {
    draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                    GAME_RGBA(0x00, 0x00, 0x20, 0x80));
}
```

The overlay is drawn *after* the grid but *before* towers, so it visually dims
the terrain while leaving the UI panel fully visible.

---

## Summary

| Takeaway                             | Detail                                    |
|--------------------------------------|-------------------------------------------|
| One enum, one field                  | `GameMod active_mod` in `GameState`       |
| Set before `game_init()`             | so init can do mod-specific setup         |
| Conditional branches, never forks    | all mods share a single code-path         |
| MOD_DEFAULT = 0 = free               | memset leaves `active_mod` at default     |
| Additive rendering                   | mod visuals layer on top of base visuals  |

In the following lessons we walk through each mod in detail, starting with
MOD_TERRAIN in lesson 26.

---

## ⚠️ Critical bug fix: init_mod_state()

The original design had a **sequencing bug**: `game_init()` attempted to set up
terrain inside the function, but `active_mod` is always `MOD_DEFAULT` at that
point (the player hasn't picked a mod yet — it happens later on the mod-select
screen).  The terrain setup block was dead code that never ran.

**Root cause:** `game_init()` is called from `main()` before the mod-select
screen is shown.  So `active_mod == MOD_DEFAULT` when init runs, regardless of
what the player will pick.

**Fix:** Extract mod-specific setup into `static void init_mod_state(GameState *s)`
and call it immediately after the player clicks a mod button:

```c
/* In the GAME_PHASE_MOD_SELECT input handler: */
if (s->mouse.left_pressed) {
    s->active_mod = (GameMod)i;
    init_mod_state(s);   /* ← scatter terrain, re-run BFS NOW that mod is known */
    change_phase(s, GAME_PHASE_PLACING);
}
```

`init_mod_state()` fills `terrain_slow[]` with baseline 1.0 values, then — for
`MOD_TERRAIN` only — scatters water and mountain cells and re-runs `bfs_fill_distance()`
so mountains become BFS walls.  For all other mods the function is a no-op (the
per-frame `game_update()` logic handles everything dynamically via `active_mod`).

**Lesson for mod systems:** Never do mod-specific setup in a general init
function that runs before the player has made their choice.  Use a dedicated
`init_mod_state()` / `apply_mod()` call at the moment the selection is confirmed.
