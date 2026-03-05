# Lesson 27 — Weather Mod (MOD_WEATHER)

## Overview

`MOD_WEATHER` introduces a **time-based event cycle** that periodically alters
creep speed AND dynamically floods cells on the grid, forcing the BFS to
reroute creeps around temporary obstacles.  Four weather phases rotate on a
**60-second clock** and a rich particle system gives unambiguous visual
feedback for each.

---

## The weather cycle at a glance

| Phase | Time window | Speed mult | Flood cells | Visual                          |
|-------|-------------|------------|-------------|---------------------------------|
| CLEAR | 0 – 20 s    | ×1.0       | none        | Normal grid                     |
| RAIN  | 20 – 35 s   | ×0.8       | 8 cells     | 70 blue rain drops              |
| WIND  | 35 – 50 s   | ×1.3       | none        | 25 diagonal wind streaks        |
| STORM | 50 – 60 s   | ×0.65      | 12 cells    | 100 slanted drops + fog overlay |

After 60 s the timer wraps back to 0 and the cycle repeats.

**Key difference from the old design:** RAIN and STORM phases scatter
*flood cells* — temporary BFS walls that force all creeps to reroute.  This
makes weather a genuine pathfinding event, not just a cosmetic speed tweak.

---

## Concept 1 — Time-based event cycles with phase-transition detection

### The state added to GameState

```c
/* MOD_WEATHER: cycling weather state */
int     weather_phase;                         /* 0=clear 1=rain 2=wind 3=storm */
float   weather_timer;                         /* 0 – 60 s, wraps             */
int     prev_weather_phase;                    /* detect transitions            */
uint8_t weather_flood[GRID_ROWS * GRID_COLS];  /* 1 = flooded this phase        */
float   lightning_timer;                       /* countdown to next flash       */
float   lightning_flash;                       /* 0-1 brightness, decays        */
```

`weather_phase` is **derived from `weather_timer`** each frame, not stored
independently, to keep the two fields from drifting apart.

### Advancing the cycle

```c
if (s->active_mod == MOD_WEATHER) {
    s->weather_timer += dt;
    if (s->weather_timer >= 60.0f) s->weather_timer -= 60.0f;

    int new_phase;
    if      (s->weather_timer < 20.0f) new_phase = 0; /* CLEAR */
    else if (s->weather_timer < 35.0f) new_phase = 1; /* RAIN  */
    else if (s->weather_timer < 50.0f) new_phase = 2; /* WIND  */
    else                               new_phase = 3; /* STORM */
    s->weather_phase = new_phase;
}
```

The `if/else if` chain gives unequal time windows (clear gets 20 s, storm only
10 s) for better pacing.

### Detecting transitions

```c
if (new_phase != s->prev_weather_phase) {
    /* Phase changed — update flood cells and re-run BFS */
    …
    s->prev_weather_phase = new_phase;
}
```

Comparing the new phase to `prev_weather_phase` fires exactly once per
transition, avoiding per-frame BFS runs.

---

## Concept 2 — Weather flood cells and dynamic BFS rerouting

This is the core feature that makes `MOD_WEATHER` strategically meaningful.

### What are flood cells?

`weather_flood[]` is a flat array of `uint8_t` flags — one per grid cell.
When a cell is flooded (`weather_flood[idx] == 1`), the BFS treats it like a
wall.  Creeps cannot enter flooded cells and will reroute around them.

Flood cells are **separate from `terrain[]`** so they work independently of
`MOD_TERRAIN`.  They are also separate from `grid[]` so tower placement
remains consistent.

### When flooding occurs

RAIN phase:   8 specific cells near the south-centre flood.
STORM phase: 12 cells flood (the 8 rain cells + 4 extra in the northeast).

The positions are fixed (hardcoded) rather than random.  This makes the
rerouting **predictable and teachable** — players learn the flood pattern and
can plan their tower placement around it.

### Implementation

```c
static void scatter_flood_cells(GameState *s, int is_storm)
{
    /* South-centre basin: rows 14-16, cols 5-8 */
    static const int base_flood[][2] = {
        {14,5},{14,6},{14,7},{14,8},
        {15,5},{15,6},{15,7},{15,8}
    };
    for (int i = 0; i < 8; i++) {
        int idx = base_flood[i][0] * GRID_COLS + base_flood[i][1];
        if (s->grid[idx] == CELL_EMPTY)
            s->weather_flood[idx] = 1;
    }
    if (is_storm) {
        /* Northeast corner: rows 3-4, cols 20-21 */
        static const int storm_extra[][2] = {
            {3,20},{3,21},{4,20},{4,21}
        };
        for (int i = 0; i < 4; i++) {
            int idx = storm_extra[i][0] * GRID_COLS + storm_extra[i][1];
            if (s->grid[idx] == CELL_EMPTY)
                s->weather_flood[idx] = 1;
        }
    }
}
```

### BFS integration

A single extra condition in `bfs_fill_distance` treats flooded cells as walls:

```c
/* MOD_WEATHER: flooded cells are impassable */
if (s->active_mod == MOD_WEATHER && s->weather_flood[ni]) continue;
```

After flooding (or clearing), BFS is re-run:

```c
bfs_fill_distance(s); /* all active creeps will reroute next frame */
```

Creep rerouting is automatic: each creep reads the **current distance field**
every step to choose its next move.  No per-creep notification is needed.

### The full transition block

```c
if (new_phase != s->prev_weather_phase) {
    int was_wet = (s->prev_weather_phase == 1 || s->prev_weather_phase == 3);
    int is_wet  = (new_phase == 1 || new_phase == 3);
    if (is_wet && !was_wet) {
        /* Entering a wet phase: scatter flood cells */
        scatter_flood_cells(s, new_phase == 3);
        bfs_fill_distance(s);
    } else if (!is_wet && was_wet) {
        /* Leaving a wet phase: clear all flood cells */
        memset(s->weather_flood, 0, sizeof(s->weather_flood));
        bfs_fill_distance(s);
    } else if (is_wet && was_wet && new_phase != s->prev_weather_phase) {
        /* RAIN → STORM or STORM → RAIN: rescatter */
        memset(s->weather_flood, 0, sizeof(s->weather_flood));
        scatter_flood_cells(s, new_phase == 3);
        bfs_fill_distance(s);
    }
    s->prev_weather_phase = new_phase;
}
```

---

## Concept 3 — Rich weather visuals and lightning

### The deterministic LCG pattern (updated seed)

```c
int rng = (int)(s->weather_timer * 12.0f) * 1664525 + 1013904223;
#define WLCG(r)       ((r) = (r) * 1664525 + 1013904223)
#define WRAND(r, n)   ((int)((unsigned)(r) >> 16) % (n))
```

The seed advances 12 times per second — fast enough for visible drift, slow
enough for stable-looking drops.

### Rain rendering (70 drops, RAIN / STORM)

```c
int drop_count = (s->weather_phase == 3) ? 100 : 70;
for (int i = 0; i < drop_count; i++) {
    WLCG(rng); int rx  = WRAND(rng, GRID_PIXEL_W);
    WLCG(rng); int ry  = WRAND(rng, GRID_PIXEL_H);
    WLCG(rng); int len = 2 + WRAND(rng, 3);          /* 2–4 px tall */
    int slant = (s->weather_phase == 3) ? 1 : 0;
    draw_rect(bb, rx,     ry,     1, len, GAME_RGB(0x55, 0x99, 0xFF));
    if (slant)
        draw_rect(bb, rx - 1, ry + 1, 1, len - 1, GAME_RGB(0x44, 0x88, 0xEE));
}
```

Storm rain is diagonal (two-pixel-wide slant) and denser (100 drops) to
signal higher severity.

### Wind streaks (25, WIND / STORM)

```c
for (int i = 0; i < 25; i++) {
    WLCG(rng); int rx  = WRAND(rng, GRID_PIXEL_W);
    WLCG(rng); int ry  = WRAND(rng, GRID_PIXEL_H);
    WLCG(rng); int len = 12 + WRAND(rng, 22);    /* 12–33 px */
    draw_line(bb, rx, ry, rx + len, ry + len / 4, GAME_RGB(0xBB, 0xBB, 0xEE));
}
```

25 streaks at a shallow 4:1 slope in pale lilac read as horizontal gusts.

### Storm fog overlay

```c
if (s->weather_phase == 3)
    draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                    GAME_RGBA(0xAA, 0xBB, 0xCC, 0x28));
```

A light grey-blue wash at `alpha=40` takes the brightness down slightly —
subtle but perceptible over the full grid.

### Lightning flash

The storm randomly triggers a full-screen white flash:

```c
/* In update_wave: */
if (s->weather_phase == 3) {
    s->lightning_timer -= dt;
    if (s->lightning_timer <= 0.0f) {
        s->lightning_flash  = 1.0f;
        s->lightning_timer  = 3.0f + (float)(rand() % 5);
    }
}
if (s->lightning_flash > 0.0f) {
    s->lightning_flash -= dt * 3.0f;
    if (s->lightning_flash < 0.0f) s->lightning_flash = 0.0f;
}

/* In game_render: */
if (s->active_mod == MOD_WEATHER && s->lightning_flash > 0.0f) {
    uint8_t alpha = (uint8_t)(s->lightning_flash * 180.0f);
    draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                    GAME_RGBA(0xFF, 0xFF, 0xDD, alpha));
}
```

`lightning_flash` decays at 3× per second (full → gone in ≈ 0.33 s) for a
realistic fast-flash feel.

### Flood cells visual

Flooded cells use animated solid colors (no overlay):

```c
uint32_t flood_col = ((r + c + (int)(s->weather_timer * 2)) & 1)
                     ? GAME_RGB(0x22, 0x66, 0xBB)
                     : GAME_RGB(0x18, 0x55, 0xAA);
draw_rect(bb, c * CELL_SIZE, r * CELL_SIZE, CELL_SIZE, CELL_SIZE, flood_col);
```

The `weather_timer * 2` term makes the shimmer ripple at ~2 Hz, reinforcing
the water-rising illusion.

### HUD phase display

```c
const char *wn;
uint32_t    wc;
switch (s->weather_phase) {
    case 1: wn = "Rain";  wc = GAME_RGB(0x55, 0xAA, 0xFF); break;
    case 2: wn = "Wind";  wc = GAME_RGB(0xCC, 0xCC, 0xFF); break;
    case 3: wn = "Storm"; wc = GAME_RGB(0xFF, 0xCC, 0x44); break;
    default:wn = "Clear"; wc = GAME_RGB(0x88, 0xDD, 0x88); break;
}
```

Phase name and color change together so the HUD is always unambiguous.

---

## Gameplay tension

| Phase | Player advantage          | Player disadvantage            |
|-------|---------------------------|--------------------------------|
| CLEAR | Normal pacing             | None                           |
| RAIN  | Creeps slow (×0.8)        | South route flooded — BFS reroute |
| WIND  | None                      | Creeps speed up (×1.3)         |
| STORM | Even slower (×0.65)       | Both flood zones active; fog; lightning distracts |

**Strategic insight**: during RAIN and STORM the flood zone forces creeps north,
which may be poorly covered.  Players who front-load towers in the south will be
surprised — weather rewards flexible coverage.

---

## Full implementation map

| File     | Location                   | Change                                        |
|----------|----------------------------|-----------------------------------------------|
| `game.h` | `GameState`                | `weather_phase`, `weather_timer`, `prev_weather_phase`, `weather_flood[]`, `lightning_timer`, `lightning_flash` |
| `game.c` | `get_weather_speed_mult()` | 4-phase switch returning multipliers          |
| `game.c` | `bfs_fill_distance()`      | `weather_flood[ni]` treated as wall           |
| `game.c` | `can_place_tower()`        | flood cells also block placement legality     |
| `game.c` | `init_mod_state()`         | `weather_*` fields zeroed, `lightning_timer` set |
| `game.c` | `update_wave()`            | advance timer; derive phase; detect transition; scatter/clear flood; re-run BFS; tick lightning |
| `game.c` | `creep_move_toward_exit()` | `spd *= get_weather_speed_mult(s)`            |
| `game.c` | `creep_flying_move()`      | same multiplier for flyers                    |
| `game.c` | `game_render()`            | flood cells; rain (70/100 drops); wind (25 streaks); storm fog; lightning flash |
| `game.c` | `game_render()` HUD        | 4-phase label + per-phase color               |

---

## Summary

| Takeaway                              | Implementation detail                             |
|---------------------------------------|---------------------------------------------------|
| 4-phase 60s cycle                     | `weather_timer` wraps; phase derived via `if/else if` |
| Phase transitions detected once       | `new_phase != prev_weather_phase` guard           |
| Flood cells = temporary BFS walls     | `weather_flood[]` array; BFS re-run on transition |
| Speed multiplier, not per-creep timer | `get_weather_speed_mult()` called at move time    |
| Const-safe visual randomness          | Local LCG seeded from `weather_timer * 12`        |
| Lightning flash                       | `lightning_flash` decays 3× per second            |
| Animated flood cell color             | `(int)(weather_timer * 2)` driven checkerboard    |
