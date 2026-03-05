# Desktop Tower Defense — Course Plan

> **Research foundation:** `ai-llm-knowledge-dump/prompt/research-desktop-tower-defense.md`
> **Course-builder principles:** `ai-llm-knowledge-dump/prompt/course-builder.md`
> **Reference implementation (patterns):** `games/tetris/`
> **Sibling course references:** `ai-llm-knowledge-dump/generated-courses/flash-games/sugar-sugar/`, `…/asteroids/`, `…/tetris/`

---

## What the final game does

Desktop Tower Defense (DTD) is a **grid-based tower defense game** originally built in ActionScript 2 / Flash Player 8 by Paul Preece (March 2007). The player places towers on a 30 × 20 tile grid to destroy waves of incoming "creeps" that travel from an **entry cell** to an **exit cell**. Unlike most tower-defense games, DTD has **no pre-drawn path** — creeps use a real-time **BFS distance field** (flow field) to navigate around whatever the player places. Placing a tower that would make the exit completely unreachable is illegal; the game enforces this with a BFS reachability check on every placement.

**The distinguishing mechanic:** Dynamic pathfinding driven by a backwards BFS from the exit. One BFS computation serves all creeps simultaneously. This single design decision (live re-routing instead of a fixed path) is the game's main technical puzzle for both the player and the programmer.

### Feature set (faithful recreation)

| Feature | Details |
|---------|---------|
| Grid | 30 cols × 20 rows, Y-down pixel coordinates, `CELL_SIZE = 20 px` |
| Pathfinding | BFS distance field from exit; re-runs on every tower placement/sell |
| Legality check | Tower placement rejected if it disconnects entry from exit |
| Tower types | 7: Pellet, Squirt, Dart, Snap, Swarm (AoE), Frost (slow), Bash (stun) |
| Targeting modes | 5: First, Last, Strongest, Weakest, Closest |
| Creep types | 7: Normal, Fast, Flying, Armoured, Spawn, Spawn Child, Boss |
| Wave system | 40 waves; HP scales ×1.10 per wave; waves can overlap |
| Economy | Starting gold 50; kill rewards; sell at 70%; send-wave-early bonus |
| Lives | Each creep reaching the exit subtracts lives (boss = more lives) |
| Rendering | Fully procedural (no sprites): coloured squares + barrel lines |
| Audio | PCM mixer; per-tower fire SFX; background ambient loop |

### Stretch features (not in initial lessons; documented for self-study)

- DTD 1.5 interest mechanic (+5% gold per wave)
- Flying Fortress orbital jets
- Multiple entry/exit configurations (Medium = top→bottom, Hard = corner→corner)
- 100-wave challenge mode
- In-place tower upgrades

---

## Final file structure

```
desktop-tower-defense/
├── PLAN.md                          ← this file
├── README.md
├── PLAN-TRACKER.md
└── course/
    ├── build-dev.sh                 # --backend=x11|raylib -r -d
    ├── src/
    │   ├── platform.h               # platform contract (4 functions)
    │   ├── game.h                   # all types, enums, structs, API
    │   ├── game.c                   # all game logic (update + render)
    │   ├── audio.c                  # game_get_audio_samples(), game_audio_update()
    │   ├── levels.c                 # WaveDef g_waves[40] table
    │   ├── main_x11.c               # X11/OpenGL backend
    │   ├── main_raylib.c            # Raylib backend
    │   └── utils/
    │       ├── backbuffer.h         # Backbuffer type, GAME_RGB/RGBA macros
    │       ├── draw-shapes.c/.h     # draw_rect, draw_rect_blend, draw_line, draw_circle
    │       ├── draw-text.c/.h       # 8×8 bitmap font, draw_char, draw_text
    │       ├── math.h               # MIN, MAX, CLAMP, ABS macros
    │       └── audio.h              # AudioOutputBuffer, SoundInstance, SoundDef
    └── lessons/
        ├── lesson-01-window-canvas-and-build-script.md
        ├── lesson-02-grid-system-and-cell-states.md
        ├── lesson-03-mouse-input-and-cell-hit-testing.md
        ├── lesson-04-bfs-distance-field.md
        ├── lesson-05-tower-placement-and-legality.md
        ├── lesson-06-single-creep-and-flow-navigation.md
        ├── lesson-07-creep-pool-and-wave-spawning.md
        ├── lesson-08-state-machine-and-economy.md
        ├── lesson-09-tower-barrel-and-targeting.md
        ├── lesson-10-tower-firing-and-damage.md
        ├── lesson-11-hp-bars-and-death-effects.md
        ├── lesson-12-wave-system-and-wave-table.md
        ├── lesson-13-five-single-target-tower-types.md
        ├── lesson-14-frost-slow-and-swarm-aoe.md
        ├── lesson-15-all-five-targeting-modes.md
        ├── lesson-16-fast-flying-and-armoured-creeps.md
        ├── lesson-17-spawn-creep-and-boss.md
        ├── lesson-18-send-wave-early-and-interest.md
        ├── lesson-19-side-panel-ui-and-hud.md
        ├── lesson-20-visual-polish-and-particles.md
        └── lesson-21-audio-system.md
```

> ⚠ `src/` root contains only `game.c`, `game.h`, `audio.c`, `levels.c`, `platform.h`, `main_x11.c`, `main_raylib.c`. All reusable helpers belong in `utils/`. No shim headers at `src/` root.

---

## Full topic inventory

Every item below must appear in exactly one "New concepts" cell in the skill inventory table. Nothing is omitted.

### `src/utils/backbuffer.h`

| Item | Category |
|------|----------|
| `Backbuffer` struct (`pixels`, `width`, `height`, `pitch`) | platform / rendering |
| `GAME_RGB(r,g,b)` macro (`0xAARRGGBB`) | rendering |
| `GAME_RGBA(r,g,b,a)` macro | rendering |
| `pitch` field — why stride ≠ width × bpp | rendering |

### `src/utils/draw-shapes.c/.h`

| Item | Category |
|------|----------|
| `draw_rect(bb, x, y, w, h, color)` | rendering |
| `draw_rect_blend(bb, x, y, w, h, color)` — alpha compositing | rendering |
| `draw_line(bb, x0, y0, x1, y1, color)` — Bresenham line | rendering |
| `draw_circle(bb, cx, cy, r, color)` | rendering |
| Clipping logic (`x0/y0/x1/y1` clamped to bb bounds) | rendering |

### `src/utils/draw-text.c/.h`

| Item | Category |
|------|----------|
| 8×8 bitmap font static array `FONT_8X8[128][8]` | rendering |
| Bit-endianness (BIT7-left convention, `0x80 >> col`) | rendering |
| `draw_char(bb, x, y, c, color, scale)` | rendering |
| `draw_text(bb, x, y, text, color, scale)` | rendering |
| `snprintf` for integer-to-string (never `itoa`) | rendering |

### `src/utils/math.h`

| Item | Category |
|------|----------|
| `MIN(a,b)`, `MAX(a,b)`, `CLAMP(v,lo,hi)`, `ABS(a)` macros | utility |

### `src/utils/audio.h`

| Item | Category |
|------|----------|
| `AudioOutputBuffer` struct (`samples`, `samples_per_second`, `sample_count`) | audio |
| `SoundDef` struct (`frequency`, `frequency_end`, `duration_ms`, `volume`) | audio |
| `SoundInstance` struct (all fields including `phase`, `frequency_slide`, `pan_position`) | audio |
| `GameAudioState` struct (`active_sounds[]`, `master_volume`, `sfx_volume`, `music_volume`) | audio |
| `ToneGenerator` struct (`frequency`, `current_volume`, `target_volume`, `is_playing`) | audio |
| `MAX_SIMULTANEOUS_SOUNDS` constant | audio |

### `src/platform.h`

| Item | Category |
|------|----------|
| `PlatformGameProps` struct (`title`, `width`, `height`, `is_running`, backbuffer) | platform |
| `PlatformAudioConfig` struct (latency model fields) | platform |
| `platform_init()` — window + GL + audio init | platform |
| `platform_get_time()` — high-res clock | platform |
| `platform_get_input()` — mouse state | platform |
| `platform_shutdown()` — cleanup | platform |
| `platform_display_backbuffer()` — upload pixels to GPU | platform |
| `platform_audio_update()` — fill audio stream | platform |
| `UPDATE_BUTTON` macro | input |

### `src/game.h` (types and enums)

| Item | Category |
|------|----------|
| `CellState` enum (`CELL_EMPTY`, `CELL_TOWER`, `CELL_ENTRY`, `CELL_EXIT`) | grid |
| `GRID_COLS = 30`, `GRID_ROWS = 20`, `CELL_SIZE = 20` | grid |
| `CANVAS_W`, `CANVAS_H` (grid area + side panel) | rendering |
| `TowerType` enum (8 values including `TOWER_NONE`) | tower |
| `TargetMode` enum (`TARGET_FIRST`, `TARGET_LAST`, `TARGET_STRONGEST`, `TARGET_WEAKEST`, `TARGET_CLOSEST`) | tower |
| `CreepType` enum (8 values) | creep |
| `GamePhase` enum (6 phases) | state machine |
| `SfxId` enum (10+ values) | audio |
| `Tower` struct (col, row, cx, cy, type, range, damage, fire_rate, cooldown_timer, angle, target_id, active, sell_value, target_mode) | tower |
| `Creep` struct (x, y, col, row, type, hp, max_hp, speed, active, id, is_flying, slow_timer, stun_timer, slow_factor) | creep |
| `Projectile` struct (x, y, vx, vy, damage, splash_radius, target_id, active, tower_type) | projectile |
| `WaveDef` struct (creep_type, count, spawn_interval) | wave |
| `TowerDef` struct (name, cost, damage, range_cells, fire_rate, color) — data table | tower |
| `CreepDef` struct (speed, base_hp, lives_cost, color, special flags) — data table | creep |
| `MouseState` struct | input |
| `GameState` struct (all fields) | state |
| `MAX_TOWERS`, `MAX_CREEPS`, `MAX_PROJECTILES`, `MAX_PARTICLES` | state |
| `SELL_RATIO = 0.70f` | economy |
| `INTEREST_RATE = 0.05f` | economy |
| `HP_SCALE_PER_WAVE = 1.10f` | wave |
| Color constants (20+ named colors) | rendering |

### `src/game.c` (functions and algorithms)

| Item | Category |
|------|----------|
| `game_init()` | state |
| `game_update(state, input, dt)` | game loop |
| `game_render(bb, state)` | rendering |
| `change_phase(state, new_phase)` | state machine |
| `bfs_fill_distance(grid, dist, rows, cols, exit_col, exit_row)` | pathfinding |
| BFS flat-array queue (no heap allocation) | pathfinding |
| `can_place_tower(state, col, row)` — double-BFS legality | pathfinding |
| `place_tower(state, col, row, type)` | tower |
| `sell_tower(state, tower_idx)` | tower / economy |
| `creep_move_toward_exit(state, creep, dt)` — BFS navigation | creep |
| `creep_flying_move(state, creep, dt)` — straight-line to exit | creep |
| `spawn_creep(state, type, base_hp)` | creep / wave |
| `compact_creeps(state)` — swap-remove | creep |
| `compact_projectiles(state)` | projectile |
| `compact_particles(state)` | particles |
| `tower_update(state, tower, dt)` | tower |
| `in_range(tower, creep)` — distance² check | tower |
| `find_best_target(state, tower)` — iterate creep pool, apply comparator | tower |
| `effective_damage(raw, creep_type, tower_type)` — armour reduction | creep |
| `spawn_projectile(state, tx, ty, target_id, dmg, splash_r, type)` | projectile |
| `projectile_update(state, proj, dt)` | projectile |
| `apply_damage(state, creep, dmg)` | creep |
| `kill_creep(state, idx)` — gold award, spawn children, compact | creep / economy |
| `start_wave(state)` | wave |
| `update_wave(state, dt)` — spawn timer, check wave complete | wave |
| `check_wave_complete(state)` — all creeps dead or exited? | wave |
| `send_wave_early(state)` — early gold bonus | economy |
| `apply_interest(state)` — +5% gold at wave start | economy |
| `draw_grid(bb, state)` — checkerboard + entry/exit highlights | rendering |
| `draw_towers(bb, state)` — coloured squares + barrel lines | rendering |
| `draw_creeps(bb, state)` — squares + HP bars | rendering |
| `draw_projectiles(bb, state)` — small coloured dots | rendering |
| `draw_ui_panel(bb, state)` — side panel with shop, wave, lives, gold | rendering |
| `draw_hover_preview(bb, state)` — green/red cell preview | rendering |
| `draw_particles(bb, state)` — floating text, death pops | rendering |
| `draw_phase_overlay(bb, state)` — title, game over, victory screens | rendering |
| `Particle` struct and pool | particles |
| `spawn_particle(state, x, y, ...)` | particles |
| Rotation math: `atan2f(dy, dx)`, `cosf`/`sinf` barrel tip position | rendering |
| `DEBUG_TRAP()` / `ASSERT()` macros | debug |

### `src/audio.c`

| Item | Category |
|------|----------|
| `SOUND_DEFS[SFX_COUNT]` static table | audio |
| `game_play_sound_at(audio, id, pan)` | audio |
| `game_play_sound(audio, id)` | audio |
| `game_audio_update(audio, dt)` — music sequencer tick | audio |
| `game_get_audio_samples(state, buffer)` — PCM loop | audio |
| Phase accumulator oscillator (`phase += freq * inv_sr`) | audio |
| Click prevention (fade-in / fade-out envelope) | audio |
| Stereo panning (linear law) | audio |
| `ToneGenerator` + volume ramping per sample | audio |

### `src/levels.c`

| Item | Category |
|------|----------|
| `g_waves[40]` — `WaveDef` array with designated initialisers | wave |
| Wave type ordering (Normal 1–3, Fast 4, Flying 7, Armoured 9, Boss 10…) | wave |

### `src/main_x11.c` / `src/main_raylib.c`

| Item | Category |
|------|----------|
| `platform_init()` — window, OpenGL context / Raylib window | platform |
| Delta-time game loop | game loop |
| VSync + manual frame cap fallback (X11) | platform |
| Letterbox centering + mouse coordinate transform | rendering / input |
| `platform_get_input()` — mouse polling | input |
| Audio stream setup (ALSA / Raylib AudioStream) | audio |
| Backbuffer upload (glTexSubImage2D / UpdateTexture) | rendering |
| R↔B channel swap (Raylib `0xAARRGGBB` → RGBA bytes) | rendering |

---

## Lesson sequence

> **Rule:** Every item in the topic inventory above must appear in exactly one "New concepts" cell below.

### Lesson 01 — Window, Canvas, and Build Script

**What gets built:** `build-dev.sh`, both platform backends (`main_raylib.c`, `main_x11.c`), blank window with a dark-grey background.

**What the student sees/hears:** A 760 × 520 window opens (600 px grid area + 160 px side panel) with a solid dark-grey background. `./build-dev.sh --backend=raylib -r` works. `--backend=x11 -r` works.

> **Canvas dimensions:** `CANVAS_W = 760`, `CANVAS_H = 520`. Grid area = 600 × 400 px (30 × 20 cells × 20 px). Side panel = 160 × 520 px. (Ref: research §3, §13.)

**New concepts introduced (≤ 3):**
1. Backbuffer pipeline: `Backbuffer` struct (`pixels`, `width`, `height`, `pitch`), `GAME_RGB`/`GAME_RGBA` macros, backbuffer upload to GPU
2. Platform init and game loop: `PlatformGameProps`, `platform_init()`, delta-time loop, `platform_shutdown()`
3. Build script: `build-dev.sh` flags (`--backend=x11|raylib`, `-r`), separate `SOURCES` variable, `clang` invocation

**Files created:**
- `build-dev.sh`
- `src/platform.h` (stub — 4 function signatures)
- `src/main_raylib.c` (complete, letterbox + R↔B swap)
- `src/main_x11.c` (complete, VSync + manual cap)
- `src/utils/backbuffer.h`
- `src/game.h` (minimal — only `game_init`, `game_update`, `game_render` stubs)
- `src/game.c` (stubs returning immediately)

---

### Lesson 02 — Grid System and Cell States

**What gets built:** `CellState` enum, 30 × 20 flat-array grid, `draw_grid()` rendering a checkerboard with highlighted entry (green) and exit (red) cells.

**What the student sees:** A 30 × 20 checkerboard grid fills the left portion of the window. The entry cell (column 0, row 9–10 area) is green; the exit cell (column 29, row 9–10) is red.

> Grid layout and cell colours: research §3. Coordinate system (Y-down): research §12, §20.

**New concepts introduced (≤ 3):**
1. `CellState` enum + flat-array grid: `grid[GRID_ROWS * GRID_COLS]`, `idx = row * GRID_COLS + col`, `GRID_COLS`, `GRID_ROWS`, `CELL_SIZE`
2. Y-down pixel coordinate system (intentional exception from course-builder.md default Y-up): `pixel_x = col * CELL_SIZE`, `pixel_y = row * CELL_SIZE`; no world-to-screen flip needed
3. `draw_rect()` + clipping logic; color constants; `CANVAS_W`/`CANVAS_H`

**Files created:**
- `src/utils/draw-shapes.c/.h` (draw_rect, draw_rect_blend initial versions)
- `src/utils/math.h` (MIN, MAX, CLAMP, ABS macros)

**Files modified:** `src/game.h` (adds `CellState`, grid constants, `CANVAS_W`/`CANVAS_H`), `src/game.c` (`game_init` initializes grid; `game_render` calls `draw_grid`)

---

### Lesson 03 — Mouse Input and Cell Hit Testing

**What gets built:** `MouseState` struct, letterbox mouse coordinate transform, mouse-to-cell conversion, highlighted hovered cell.

**What the student sees:** Moving the mouse over the grid highlights the hovered cell in yellow. Hovering over the side panel area does nothing.

> Mouse input model: research §11, §20 (MouseState struct). Letterbox mouse transform: course-builder.md §Letterbox centering.

**New concepts introduced (≤ 3):**
1. `MouseState` struct (`x`, `y`, `prev_x`, `prev_y`, `left_down`, `left_pressed`, `left_released`); `platform_get_input()` fills it
2. Letterbox scale + offset: `g_scale`, `g_offset_x`, `g_offset_y` globals; transforming raw window pixels to canvas coordinates before writing to `MouseState`
3. Pixel-to-cell math: `col = mouse_x / CELL_SIZE`, `row = mouse_y / CELL_SIZE`; bounds check; `mouse_in_grid()` helper

**Files modified:** `src/platform.h` (adds `MouseState`), `src/main_raylib.c` + `src/main_x11.c` (fill `MouseState` with transformed coords), `src/game.h` (adds `MouseState` field to `GameState`), `src/game.c` (hover highlight in `game_render`)

---

### Lesson 04 — BFS Distance Field

**What gets built:** `bfs_fill_distance()`, `dist[]` flat int array, backward BFS from exit, distance field visualized as a color gradient.

**What the student sees:** The grid shows a color gradient: cells nearest the exit are bright green, cells farthest are dark red. The BFS result is correct even with no towers placed.

> This is the heart of DTD. Full pseudocode + explanation: research §4. BFS vs A*: research §4. Complexity analysis: research §17.

**New concepts introduced (≤ 3):**
1. BFS algorithm: queue-based breadth-first search, 4-directional neighbours, `dx[]`/`dy[]` arrays, `CELL_TOWER` cells skipped
2. Flow field / distance map: backward BFS from exit fills every reachable cell with distance; creeps follow gradient to reach exit
3. Flat-array BFS queue (no heap allocation): `int queue[GRID_ROWS * GRID_COLS]`, `head`/`tail` indices; `memset(dist, -1, ...)` for unreachable init

**Files modified:** `src/game.h` (adds `dist[]` array to `GameState`), `src/game.c` (adds `bfs_fill_distance()`, calls it from `game_init()`, visualizes in `game_render`)

---

### Lesson 05 — Tower Placement and Legality

**What gets built:** `Tower` struct, `towers[]` fixed pool, `can_place_tower()` (BFS legality check), `place_tower()`, `sell_tower()`, hover preview (green/red cell).

**What the student sees:** Clicking an empty grid cell places a grey tower square. Clicking a cell that would block all paths shows a red flash and does nothing. Right-clicking a placed tower sells it (tower disappears; grid re-opens).

> Tower placement legality: research §4 (`can_place_tower` pseudocode). Sell ratio 70%: research §8. Double-BFS-per-click: research §4.

**New concepts introduced (≤ 3):**
1. `Tower` struct and fixed pool: `Tower towers[MAX_TOWERS]`, `tower_count`, `active` flag, `place_tower()` finds first inactive slot
2. BFS legality check: place tower temporarily → re-run BFS → if `dist[entry_idx] == -1` → reject and restore; double-BFS-per-click is O(1200) — negligible
3. `sell_tower()`: mark inactive, set grid cell back to `CELL_EMPTY`, re-run BFS; `sell_value = cost * SELL_RATIO`

**Files modified:** `src/game.h` (adds `Tower`, `TowerType` stub, `MAX_TOWERS`, `SELL_RATIO`), `src/game.c` (adds placement/sell functions, hover preview in render)

---

### Lesson 06 — Single Creep and Flow Field Navigation

**What gets built:** `Creep` struct, float pixel position for a single normal creep, `creep_move_toward_exit()` using the BFS distance field.

**What the student sees:** A single grey creep spawns at the entry cell and smoothly walks cell by cell to the exit, threading through any towers placed on the grid. When it reaches the exit it is removed.

> Creep movement via BFS distance field: research §4 (creep movement pseudocode). Float sub-cell positions: research §6.

**New concepts introduced (≤ 3):**
1. `Creep` struct: `x`/`y` (float pixels), `col`/`row` (integer grid), `speed` (px/s), `hp`, `active`, `id`
2. BFS-driven navigation: each frame, sample `dist[]` at current integer cell, pick neighbour with lowest dist as target cell; interpolate float position toward target cell centre
3. Smooth interpolation: `float target_x = col * CELL_SIZE + CELL_SIZE/2.0f`; creep moves at `speed * dt` px per frame; switches to next cell on arrival at cell centre

**Files modified:** `src/game.h` (adds `Creep`, `CreepType` stub, spawn helpers), `src/game.c` (adds `creep_move_toward_exit()`, single creep spawned in `game_init`, updated and rendered each frame)

---

### Lesson 07 — Creep Pool and Wave Spawning

**What gets built:** `creeps[MAX_CREEPS]` fixed pool, `spawn_creep()`, spawn timer, 10 creeps spawned with staggered intervals, `compact_creeps()` swap-remove pattern.

**What the student sees:** Ten normal creeps spawn from the entry one after another, 0.5 s apart. They walk the BFS path to the exit and disappear.

> Creep pool and spawn mechanics: research §7. Compact/swap-remove: course-builder.md §Fixed free-list pool. Max creep count reasoning: research §17, §20.

**New concepts introduced (≤ 3):**
1. Fixed free-list pool: `creeps[MAX_CREEPS]`, `creep_count`, `spawn_creep()` finds first inactive slot; pool full → silently drops (acceptable for debug build)
2. Spawn timer: `spawn_timer` accumulates `dt`; when `>= spawn_interval` spawn next creep, decrement `spawn_index`
3. `compact_creeps()` — swap-remove pattern: mark inactive during iteration; after all loops, compact by overwriting hole with tail element; `O(n)`, keeps live prefix dense

**Files modified:** `src/game.h` (adds `MAX_CREEPS`, spawn fields to `GameState`), `src/game.c` (pool update + compact)

---

### Lesson 08 — State Machine and Economy

**What gets built:** `GamePhase` enum (TITLE, PLACING, WAVE, WAVE_CLEAR, GAME_OVER, VICTORY), `change_phase()`, `player_lives`, `player_gold` (starting 50), phase-specific update/render dispatch.

**What the student sees:** A TITLE screen with "Desktop Tower Defense — Click to Play" text. Clicking starts the game. Lives (20) and gold (50) are displayed in the HUD. Losing all lives triggers a GAME_OVER screen.

> State machine phases: research §9. Economy starting values: research §8. Phase transitions: research §9 phase transition table.

**New concepts introduced (≤ 3):**
1. `GamePhase` enum + `switch(state->phase)` dispatch in `game_update()` and `game_render()`; `GAME_PHASE_*` naming convention
2. `change_phase(state, new_phase)` — centralized transition function that resets timers, triggers audio, sets flags; called from one place only
3. Economy init: `player_gold = 50`, `player_lives = 20`, `STARTING_GOLD`, `STARTING_LIVES` constants; lives decremented when creep reaches exit

**Files created:** `src/utils/draw-text.c/.h` (8×8 bitmap font, `draw_char`, `draw_text`, `snprintf` for HUD integers)
**Files modified:** `src/game.h` (adds `GamePhase`, `GameState` phase/lives/gold fields), `src/game.c` (state machine dispatch, phase overlays in render)

---

### Lesson 09 — Tower Barrel and Targeting

**What gets built:** Tower visual (coloured square + rotating barrel line), `in_range()` check, `find_best_target()` (FIRST mode only — lowest `dist[]` value), barrel angle tracking.

**What the student sees:** Placed towers rotate their barrel lines to point at the closest-to-exit creep in range. Towers with no target in range point straight right (default angle).

> Tower visual design + barrel rotation math: research §5 (`atan2f` formula), §13 (visual design). Range check: research §10 (in_range). Targeting FIRST mode: research §5 (targeting modes).

**New concepts introduced (≤ 3):**
1. Tower rendering: `draw_rect` for tower body; barrel tip = `tower.cx + cosf(angle) * BARREL_LEN`; update `tower.angle = atan2f(target_y - tower.cy, target_x - tower.cx)` each frame when target found
2. Range check: `in_range(tower, creep)` — `dx*dx + dy*dy <= range*range` (no `sqrtf`); `range` stored in pixels = `range_cells * CELL_SIZE`
3. `find_best_target(state, tower)` — iterate `creeps[]`, skip inactive/out-of-range; FIRST mode = pick creep with lowest `dist[row*GRID_COLS + col]` value; store `tower.target_id`

**Files modified:** `src/game.c` (`tower_update()` calls `find_best_target`, updates angle; `draw_towers()` uses `draw_line` for barrel)
**Files modified:** `src/utils/draw-shapes.c` (adds `draw_line`)

---

### Lesson 10 — Tower Firing and Damage

**What gets built:** `Projectile` struct, `projectiles[MAX_PROJECTILES]` pool, `spawn_projectile()`, fire rate cooldown, `projectile_update()` homing movement, `apply_damage()`, kill → gold award.

**What the student sees:** Towers fire small coloured dots at creeps. Creeps take damage and disappear when HP reaches 0. Gold counter increments on each kill.

> Projectile struct: research §5. Projectile homing (no predictive leading): research §5. Kill gold formula: research §8. Cooldown timer: research §10.

**New concepts introduced (≤ 3):**
1. `Projectile` struct + pool: `x`, `y`, `vx`, `vy`, `damage`, `splash_radius`, `target_id`, `active`; `spawn_projectile()` initializes with velocity toward current target position (not predictive)
2. Fire rate cooldown: `tower.cooldown_timer -= dt`; when ≤ 0, fire and reset to `1.0f / tower.fire_rate`; first shot fires immediately on target acquisition
3. `apply_damage(state, creep_idx, dmg)` + `kill_creep(state, idx)`: subtract HP; if ≤ 0 → mark inactive, award `kill_gold`, compact pool at end of frame

**Files modified:** `src/game.h` (adds `Projectile`, `MAX_PROJECTILES`), `src/game.c` (full projectile lifecycle), `src/game.c` (`kill_creep()` with gold award)

---

### Lesson 11 — HP Bars and Death Effects

**What gets built:** HP bar rendered above each creep (green → yellow → red), brief death flash animation (creep shrinks/fades for ~6 frames), floating gold text on kill.

**What the student sees:** HP bars shrink and change color as creeps take damage. A brief pop animation plays on death. "+N gold" text floats up from the kill position.

> HP bar colour thresholds: research §13 (HP bar entry). Death animation timing: research §19. Gold float-up: research §19 (UI timing).

**New concepts introduced (≤ 3):**
1. HP bar rendering: `hp_ratio = creep.hp / creep.max_hp`; bar width = `CELL_SIZE * hp_ratio`; color = CLAMP-based green → yellow → red at 33% threshold; `draw_rect_blend` for semi-transparent background
2. Brief animation timer on creep: `death_timer` field counts down; during death, creep still rendered but shrinking (`size = CELL_SIZE * death_timer / DEATH_DURATION`); after timer expires, actually mark inactive
3. Floating text `Particle` struct: `x`, `y`, `vel_y` (upward), `lifetime`, `text[8]`; `draw_text` with fading alpha (`GAME_RGBA` with alpha = `lifetime / max_lifetime * 255`)

**Files modified:** `src/game.h` (adds `death_timer` to `Creep`, adds `Particle` struct + pool, `MAX_PARTICLES`), `src/game.c` (`draw_creeps` + `draw_particles`)

---

### Lesson 12 — Wave System and Wave Table

**What gets built:** `WaveDef` struct, `g_waves[40]` table in `levels.c`, HP scaling formula, `start_wave()`, `update_wave()`, `check_wave_complete()`, WAVE → WAVE_CLEAR → PLACING → WAVE transition, game-over/victory detection.

**What the student sees:** Waves progress automatically. Wave 1 sends 10 normal creeps; wave 4 sends fast ones; wave 10 sends a boss. The wave counter increments. The game ends at wave 40 or when lives = 0.

> Wave structure and 40-wave order: research §7 (wave order table). HP scaling: research §6 (formula `base * 1.10^(wave-1)`). Wave overlap: research §7 (overlapping waves). Victory/game-over conditions: research §2.

**New concepts introduced (≤ 3):**
1. `WaveDef` struct + designated initialisers in `levels.c`: `[0] = { .creep_type = CREEP_NORMAL, .count = 10, .spawn_interval = 0.7f }`, etc.; `extern const WaveDef g_waves[40]` across translation units
2. HP scaling formula: `int scaled_hp = (int)(def->base_hp * powf(HP_SCALE_PER_WAVE, wave_number - 1))`; `HP_SCALE_PER_WAVE = 1.10f`; `powf` included via `<math.h>`
3. Wave state transitions: `GAME_PHASE_WAVE` → `check_wave_complete()` → `GAME_PHASE_WAVE_CLEAR` (2 s timer) → `GAME_PHASE_PLACING`; `player_lives == 0` → `GAME_PHASE_GAME_OVER`; wave 40 complete → `GAME_PHASE_VICTORY`

**Files created:** `src/levels.c`
**Files modified:** `src/game.h` (adds wave fields to `GameState`), `src/game.c` (wave update logic)

---

### Lesson 13 — Five Single-Target Tower Types

**What gets built:** Full `TowerDef` data table for Pellet, Squirt, Dart, Snap, Bash; tower shop UI buttons in side panel; player can select and place any tower type.

**What the student sees:** The side panel shows 5 tower buttons with names and costs. Clicking one selects it (highlighted). Clicking the grid places it with correct color and stats. Insufficient gold shows a brief red flash on the button.

> Tower stats table: research §5 (tower stats table). Tower visual design: research §5 (visual design). Side panel layout: research §13 (UI panel layout).

**New concepts introduced (≤ 3):**
1. `TowerDef` data table (type → cost, damage, range, fire_rate, color): `static const TowerDef TOWER_DEFS[TOWER_COUNT]`; towers read stats from table at placement — no hardcoded per-tower branches in update logic
2. Tower shop UI hit testing: side panel buttons at fixed pixel regions; `mouse_in_rect(mx, my, x, y, w, h)` helper; `selected_tower_type` field in `GameState`
3. Insufficient gold feedback: `player_gold < TOWER_DEFS[type].cost` → play error sound, flash button red (1-frame `error_timer` on shop entry), do not place

**Files modified:** `src/game.h` (adds `TowerDef`, `TowerType` full enum, `selected_tower_type`), `src/game.c` (`draw_ui_panel()` shop buttons, placement cost check)

---

### Lesson 14 — Frost Slow and Swarm AoE

**What gets built:** `TOWER_FROST` (AoE pulse, sets `slow_timer`/`slow_factor` on creeps), `TOWER_SWARM` (splash projectile, AoE damage on impact, bypasses armour), visual effects (blue tint for slow, splash circle).

**What the student sees:** Frost tower sends a ring pulse; creeps within range turn blue-tinted and visibly slow. Swarm fires a projectile that explodes in an orange circle, damaging all creeps in the area.

> Frost slow: research §5 (Frost entry), §6 (slow_factor = 0.5). Swarm AoE: research §5 (Swarm entry), §10 (AoE splash). Armour bypass: research §6 (armoured creep damage reduction).

**New concepts introduced (≤ 3):**
1. `slow_timer` + `slow_factor` on `Creep`: Frost pulse sets `slow_timer = FROST_DURATION`, `slow_factor = 0.5f`; `creep_move_toward_exit` multiplies speed by `slow_factor` if `slow_timer > 0`; timer counts down each frame
2. Swarm AoE splash on projectile impact: `proj.splash_radius > 0` → iterate creeps, apply `apply_damage` to all within splash circle; `draw_circle` flash at impact point for 4 frames
3. `effective_damage(raw, creep_type, tower_type)` — armoured creep takes `raw / 2` from all towers except `TOWER_SWARM` (AoE bypasses armour); centralises damage calculation

**Files modified:** `src/game.h` (adds `slow_timer`, `slow_factor`, `stun_timer` to `Creep`), `src/game.c` (`projectile_update` AoE branch, `effective_damage` function)

---

### Lesson 15 — All Five Targeting Modes

**What gets built:** `TARGET_LAST`, `TARGET_STRONGEST`, `TARGET_WEAKEST`, `TARGET_CLOSEST` comparators in `find_best_target()`; right-click placed tower cycles through modes; mode label drawn above selected tower.

**What the student sees:** Right-clicking a placed tower cycles "First → Last → Strongest → Weakest → Closest → First". The tower visibly retargets based on its mode (e.g., LAST picks the creep farthest from exit).

> Targeting modes: research §5 (targeting modes table). Target acquisition O(towers × creeps): research §10, §17.

**New concepts introduced (≤ 3):**
1. Targeting comparator pattern: `find_best_target()` uses `tower.target_mode` in a `switch`; each mode selects the "best" creep by a different ranking field (`dist`, `hp`, Euclidean distance to tower); comparator is a single integer comparison, no function pointers needed
2. `TargetMode` enum full (`TARGET_FIRST`, `TARGET_LAST`, `TARGET_STRONGEST`, `TARGET_WEAKEST`, `TARGET_CLOSEST`); cycled with `tower.target_mode = (tower.target_mode + 1) % TARGET_COUNT`
3. Tower selection UI: left-click placed tower → `selected_tower_idx` set; draw info box (type, mode, sell button) near tower; right-click elsewhere or Escape → deselect

**Files modified:** `src/game.h` (full `TargetMode` enum), `src/game.c` (`find_best_target` all modes, tower selection state)

---

### Lesson 16 — Fast, Flying, and Armoured Creeps

**What gets built:** `CREEP_FAST` (2× speed), `CREEP_FLYING` (straight-line movement ignoring BFS, `creep_flying_move()`), `CREEP_ARMOURED` (50% DR), updated `CreepDef` table, correct wave appearances (wave 4, 7, 9).

**What the student sees:** Wave 4 brings yellow fast creeps visibly zooming. Wave 7 brings white flying creeps crossing diagonally, immune to maze layouts. Wave 9 brings dark armoured creeps that soak damage until Swarm is used.

> Flying creep movement formula: research §4 (flying creep pseudocode). Armoured damage: research §6. Fast speed: research §6 (creep types table). Wave order: research §7.

**New concepts introduced (≤ 3):**
1. `creep_flying_move(state, creep, dt)`: ignores `dist[]` entirely; computes `dx = exit_cx - creep.x`, `dy = exit_cy - creep.y`, normalizes, multiplies by `speed * dt`; flying creep still shootable by towers
2. `effective_damage` for armoured creeps: already introduced in L14; here we emphasise the creep-side flag `is_armoured` and connect it to the wave data; Swarm bypass revisited
3. `CreepDef` data table: `static const CreepDef CREEP_DEFS[CREEP_COUNT]` (speed, base_hp, lives_cost, color, `is_flying`, `is_armoured`); `spawn_creep` reads stats from table

**Files modified:** `src/game.h` (full `CreepType` enum + `CreepDef` table), `src/game.c` (flying move path), `src/levels.c` (wave 4/7/9 entries use correct types)

---

### Lesson 17 — Spawn Creep and Boss

**What gets built:** `CREEP_SPAWN` splits into 4 `CREEP_SPAWN_CHILD` on death; `CREEP_BOSS` with high HP, large visual, lives cost > 1; victory condition after wave 40.

**What the student sees:** Wave 13 sends green spawn creeps; each death spawns 4 small children that scatter and continue toward the exit. Wave 10 sends a large red boss that costs 5 lives if it escapes.

> Spawn mechanics: research §6 (spawn creep split), §16 (edge case: spawn child at exit). Boss HP/lives cost: research §6. Victory trigger: research §7.

**New concepts introduced (≤ 3):**
1. Spawn-on-death: `kill_creep()` checks `creep.type == CREEP_SPAWN`; if so, spawns 4 `CREEP_SPAWN_CHILD` at death position with small random velocity offsets; children inherit `row`/`col` from parent
2. Boss lives cost: `boss_lives_cost = CLAMP(boss.hp / 100, 1, 20)`; `player_lives -= creep_lives_cost` (not always 1) when creep reaches exit; check `player_lives <= 0` → game over
3. Victory trigger: `current_wave > 40 && creep_count == 0 && spawn_index == wave_count` → `change_phase(GAME_PHASE_VICTORY)`; victory screen drawn as overlay

**Files modified:** `src/game.c` (`kill_creep` spawn branch, exit handler lives cost, victory check), `src/game.h` (`lives_cost` field on `Creep`)

---

### Lesson 18 — Send Wave Early and Interest Mechanic

**What gets built:** "Send Next Wave" button in side panel, early gold bonus formula, DTD 1.5 interest mechanic (+5% gold per wave), mid-wave tower selling that re-runs BFS.

**What the student sees:** Clicking "Send Next Wave" during WAVE_CLEAR phase gives a gold bonus and immediately starts the next wave. At wave start, gold briefly flashes "+N interest" if the mechanic is enabled.

> Send early mechanics: research §7 (early send formula). Interest mechanic: research §8. Sell during wave (BFS re-run): research §16 edge cases.

**New concepts introduced (≤ 3):**
1. `send_wave_early(state)`: if `phase == GAME_PHASE_WAVE_CLEAR` or `WAVE`, compute `early_gold_bonus = (int)(remaining_wave_time * GOLD_PER_SECOND_EARLY)`; add to gold; spawn float text; advance wave
2. `apply_interest(state)`: `interest_bonus = (int)(player_gold * INTEREST_RATE)`; add at wave start; spawn float text "+N interest"; `INTEREST_RATE = 0.05f`
3. BFS re-run on mid-wave sell: `sell_tower()` already re-runs BFS; creeps currently moving re-pick next target cell at their current integer cell position next frame — no illegal state; documented as expected behaviour

**Files modified:** `src/game.c` (wave start hook for interest, send button handler), `src/game.h` (`remaining_wave_time` field, `INTEREST_RATE`)

---

### Lesson 19 — Side Panel UI and HUD

**What gets built:** Complete side panel layout (tower shop, wave counter, lives, gold, "Send Next Wave" button, selected tower info with sell option), HUD using bitmap font.

**What the student sees:** The full DTD-style side panel is visible with all information. Selecting a tower shows its stats and a Sell button. All text uses the 8×8 bitmap font at 2× scale.

> Side panel pixel layout: research §13 (UI panel layout diagram). Color palette: research §13 (color palette table). `snprintf` for integers: course-builder.md §Bitmap font rendering.

**New concepts introduced (≤ 3):**
1. `draw_ui_panel(bb, state)` complete layout: fixed pixel regions for each UI zone (shop rows, info box, wave bar); `draw_rect` for backgrounds + borders, `draw_text` for all labels; `snprintf(buf, sizeof(buf), "Wave: %d/40", state->current_wave)` for dynamic strings
2. Sell button interaction: when `selected_tower_idx >= 0`, draw info box with `[Sell: %dG]` button; left-click on sell rect calls `sell_tower()`; deselect after sell
3. `DEBUG_TRAP()` / `ASSERT()` macros: introduced here as part of polish phase; `ASSERT(col >= 0 && col < GRID_COLS)` guards all grid accesses; `#ifdef DEBUG` gate; `-DDEBUG` in build script

**Files modified:** `src/game.c` (full `draw_ui_panel` implementation), `src/game.h` (adds `ASSERT` macro, `DEBUG_TRAP` macro, any remaining UI state fields)

---

### Lesson 20 — Visual Polish and Particles

**What gets built:** `Particle` pool fully wired (death pop particles, stun stars above bashed creeps, frost ring effect, placement flash, floating gold/interest text), `draw_rect_blend` for overlays.

**What the student sees:** The game feels alive: creep deaths produce pop particles; bashed creeps show spinning stars; Frost towers emit ice rings; tower placement briefly flashes bright; gold text floats upward and fades.

> Timing data for all animations: research §19. Frost/Bash visual: research §13 (frost effect, bash stun). Particle system pattern: course-builder.md §Particle system.

**New concepts introduced (≤ 3):**
1. Particle pool fully implemented: `spawn_particle(state, x, y, vx, vy, lifetime, color, size)` uses `MAX_PARTICLES = 256` pool; `update_particles()` advances position + decrements lifetime; `compact_particles()` at end of frame; particles are cosmetic — pool-full drops silently
2. `draw_rect_blend` for semi-transparent overlays: game-over screen (`GAME_RGBA(0,0,0,160)` overlay + white text), stun star drawn at 50% alpha, frost ring as expanding transparent circle
3. `spawn_explosion(state, x, y, color, count)` helper: `count` particles with random polar velocity (`cosf(angle)*speed`, `sinf(angle)*speed`); used for creep death and boss death with different particle counts

**Files modified:** `src/game.c` (particle spawn calls wired into kill/stun/frost handlers, `draw_particles` complete, overlays), `src/utils/draw-shapes.c` (draws `draw_circle`)

---

### Lesson 21 — Audio System

**What gets built:** `SfxId` enum, `SOUND_DEFS[]` table, `game_play_sound()`, `game_get_audio_samples()` PCM loop, `game_audio_update()`, background ambient music tone, audio wired to all game events.

**What the student sees/hears:** Game has distinct sound effects: tower fire clicks/cracks, creep death pops, boss death boom, life-lost alarm, tower placement click, wave complete chime, game-over sting, victory fanfare. Soft ambient music loops throughout.

> Sound effects table: research §14. Audio tier choice (custom PCM mixer): course-builder.md §Audio system. PCM loop implementation: course-builder.md (full `game_get_audio_samples` code). ALSA startup notes: course-builder.md §Platform audio integration.

**New concepts introduced (≤ 3):**
1. `SfxId` enum + `SoundDef` table: `static const SoundDef SOUND_DEFS[SFX_COUNT]` with frequency, slide, duration, volume per sound ID; named by intent (`SFX_TOWER_FIRE_PELLET`, `SFX_CREEP_DEATH`, etc.)
2. `game_get_audio_samples()` PCM loop: `AudioOutputBuffer`, phase accumulator oscillator, click prevention (fade-in/fade-out envelope), stereo panning (pan = 0.0 for all DTD sounds — no spatial audio), master volume scale to int16
3. Platform audio integration: Raylib `IsAudioStreamProcessed()` gate + `UpdateAudioStream()` with `AUDIO_CHUNK_SIZE = 2048`; X11 ALSA `snd_pcm_writei()` with `platform_audio_get_samples_to_write()`; `game_audio_init()` / `game_init()` ordering fix (retrigger music after `platform_audio_init`)

**Files created:** `src/audio.c`
**Files modified:** `src/game.h` (adds `SfxId`, `GameAudioState` to `GameState`), `src/main_raylib.c` + `src/main_x11.c` (audio init + per-frame update), `src/game.c` (audio calls wired at every game event)

---

## Concept dependency map

```
Backbuffer struct + GAME_RGB macros → draw_rect → draw_grid → draw_towers → draw_creeps
platform_init → delta-time loop → game_update → game_render
CellState enum + flat-array grid → Y-down pixel coords → cell-from-pixel math
MouseState + letterbox transform → cell hit testing → tower placement UI → sell button UI
BFS algorithm + flat-array queue → flow field / distance map → tower legality check → hover preview
BFS legality check → can_place_tower → place_tower → sell_tower
Creep float position → BFS navigation → smooth interpolation
Fixed free-list pool → spawn_creep → compact_pool → spawn_projectile → compact_projectiles
Spawn timer → wave spawning → WaveDef table → HP scaling → wave state transitions
GAME_PHASE + change_phase → phase dispatch → game-over → victory
Economy init → kill gold → sell value → send-wave-early bonus → interest mechanic
Tower struct + pool → TowerDef data table → tower shop UI → insufficient gold
atan2f barrel rotation → in_range → find_best_target (FIRST) → targeting comparator → all 5 modes
Projectile pool + spawn_projectile → fire cooldown → apply_damage → kill_creep → spawn children
effective_damage (armour) → slow_timer + slow_factor → stun_timer → Frost pulse → Swarm AoE
Flying creep linear movement → Armoured creep → Fast creep → Boss lives cost
draw_rect_blend → HP bar + CLAMP colour → death_timer → Particle pool → spawn_explosion
draw_text + snprintf → draw_ui_panel → sell button → DEBUG_TRAP + ASSERT
SfxId + SoundDef table → game_play_sound → game_get_audio_samples → phase accumulator
AudioOutputBuffer → platform audio init → ALSA + Raylib audio stream
```

---

## Per-lesson skill inventory table

> Verify: every item from the topic inventory appears in exactly one "New concepts" cell. Every "Concepts re-used" item must have appeared in a prior "New concepts" row.

| Lesson | New concepts | Concepts re-used from prior lessons |
|--------|-------------|-------------------------------------|
| 01 | Backbuffer struct + pitch, GAME_RGB/RGBA macros, platform_init + delta-time loop, build-dev.sh flags | — |
| 02 | CellState enum + flat-array grid, GRID_COLS/ROWS/CELL_SIZE, Y-down pixel coords (exception), draw_rect + clipping, MIN/MAX/CLAMP/ABS macros, CANVAS_W/H, color constants | Backbuffer, GAME_RGB, platform loop |
| 03 | MouseState struct, letterbox scale/offset transform, pixel-to-cell math, mouse_in_grid() | platform_get_input stub, draw_rect, grid coords |
| 04 | BFS algorithm, flow field / distance map, flat-array BFS queue, memset for dist[] init | CellState, flat-array grid, CELL_TOWER check |
| 05 | Tower struct + fixed pool, BFS legality check (double-BFS), place_tower(), sell_tower(), SELL_RATIO | BFS, MouseState, draw_rect, compact_pool (introduced here for towers via sell), CellState |
| 06 | Creep struct, float pixel position, BFS-driven navigation, smooth cell interpolation | dist[] array, MouseState (for spawning), draw_rect |
| 07 | Fixed free-list pool (creeps), spawn_creep(), spawn timer, compact_creeps() swap-remove | Creep struct, BFS navigation, delta-time |
| 08 | GamePhase enum, change_phase(), player_lives/gold init, draw_text + 8×8 bitmap font, snprintf for HUD | delta-time, draw_rect, grid, CANVAS coords |
| 09 | Tower barrel + draw_line, atan2f rotation, in_range() distance² check, find_best_target() FIRST mode | Tower pool, Creep pool, dist[] for FIRST |
| 10 | Projectile struct + pool, spawn_projectile(), fire rate cooldown timer, apply_damage(), kill_creep() + gold | Tower, Creep, fixed pool, spawn timer |
| 11 | HP bar rendering + CLAMP color, death_timer on creep, Particle struct + float-text, draw_rect_blend | Creep hp/max_hp, draw_rect, GAME_RGBA |
| 12 | WaveDef struct + designated initialisers (levels.c), HP scaling powf, wave state transitions, victory/gameover | GamePhase, change_phase(), spawn_creep, compact |
| 13 | TowerDef data table, tower shop UI hit testing, mouse_in_rect(), selected_tower_type, gold-check feedback | Tower pool, MouseState, draw_text, economy |
| 14 | slow_timer + slow_factor on Creep, Swarm AoE splash, effective_damage() armour bypass | Projectile, apply_damage, Particle (splash circle), Frost pulse |
| 15 | TargetMode enum (all 5), comparator pattern in find_best_target(), tower selection + mode cycling, info box | Tower, Creep, dist[], hp, Euclidean dist |
| 16 | creep_flying_move() straight-line, CreepDef data table, CREEP_FAST/FLYING/ARMOURED types | Creep struct, BFS (skipped by flying), effective_damage |
| 17 | Spawn-on-death (4 children), Boss lives_cost > 1, victory trigger | kill_creep, spawn_creep, change_phase, GamePhase |
| 18 | send_wave_early() + early gold bonus, apply_interest() + INTEREST_RATE, mid-wave sell BFS re-run | wave timer, economy, sell_tower, change_phase |
| 19 | draw_ui_panel() complete layout, sell button interaction, DEBUG_TRAP / ASSERT macros | draw_text, snprintf, MouseState, Tower pool |
| 20 | Particle pool fully wired, spawn_explosion(), draw_rect_blend overlays, stun stars + frost rings | Particle struct, draw_rect_blend, death_timer |
| 21 | SfxId enum + SoundDef table, game_get_audio_samples() PCM loop, phase accumulator, platform audio stream | GameAudioState, delta-time, GamePhase, change_phase |

---

## Course-builder.md rules: followed vs. exceptions

### Followed ✅

- CPU backbuffer pipeline (`Backbuffer`, `uint32_t *pixels`, GPU upload each frame)
- `GAME_RGB(r,g,b)` / `GAME_RGBA(r,g,b,a)` macros, `0xAARRGGBB` byte order
- R↔B channel swap in Raylib backend (Option A: swap before/after `UpdateTexture`)
- 3-layer split: game logic (`game.c`) / `platform.h` contract / `main_*.c` backends
- `MouseState` with `left_pressed`/`left_released` edge-detection (adapted from `GameButtonState` model — DTD is mouse-only)
- `prepare_input_frame` equivalent: mouse `prev_x`/`prev_y` updated, edges reset each frame
- `GAME_PHASE` enum state machine with `change_phase()` helper; introduced before any second screen
- 8×8 ASCII-indexed bitmap font (BIT7-left convention); `draw_char` / `draw_text` / `snprintf`
- Platform contract minimal and stable (4 mandatory functions + audio extensions)
- Data-driven tables isolated (`levels.c` for wave data; `TowerDef`/`CreepDef` in `game.h`)
- `= {0}` aggregate init; `memset` for full reset; `ASSERT(expr)` guards all pool accesses
- `render_*` functions are pure reads of `GameState` — no mutations in render
- `pitch` field on `Backbuffer` (never hardcode `width × 4`)
- `#define CANVAS_W 760` / `CANVAS_H 520` fixed logical canvas
- Both backends in every lesson section with platform-specific code
- Letterbox centering from Lesson 01; mouse coordinate transform from Lesson 03
- VSync + manual frame cap in X11; `SetTargetFPS(60)` in Raylib
- Fixed free-list pools for towers, creeps, projectiles, particles (no `malloc` in game loop)
- Swap-remove pattern (`compact_*`) for pool maintenance
- `DEBUG_TRAP()` / `ASSERT()` macro with `#ifdef DEBUG`; `-DDEBUG` in build script
- `AUDIO_CHUNK_SIZE = 2048` constant for Raylib audio
- `game_audio_init` / `game_init` ordering fix documented (retrigger music after platform audio init)
- `snd_pcm_sw_params_set_start_threshold(pcm, sw, 1)` documented in Lesson 21
- All numeric literals annotated with units/meaning in lesson code
- All 3 exercises per lesson (beginner / intermediate / challenge)

### Intentional exceptions ⚠️

| Rule | Exception | Reason |
|------|-----------|--------|
| Default **Y-up** coordinate system | **Y-down pixel coordinates** throughout | DTD is a grid game; `pixel_y = row * CELL_SIZE` maps directly to screen. Adding a Y-flip adds noise with zero conceptual benefit. Documented in Lesson 02 and in `game.c` header comment. (Ref: course-builder.md §Coordinate and unit systems — grid exception.) |
| `PIXELS_PER_METER` world-unit system | **`CELL_SIZE = 20 px`** cell-unit system | Grid game; no physics, no gravity. Cells are the natural unit. Speed is stated in px/s. (Ref: course-builder.md §Unit system — grid-based exception; research §12 Constants and Formulas.) |
| `GameButtonState` keyboard input | **`MouseState`** struct | DTD gameplay is mouse-only (no keyboard gameplay input). `MouseState` is a direct adaptation of the button-state model for a pointing device. |
| Lesson 1 manual `clang` command | **`build-dev.sh` from Lesson 01** | DTD has two backends from the start; a manual command for both would be confusing. The script is introduced in the first lesson since both backends are compiled simultaneously. (Ref: course-builder.md §Build command progression — says "no later than first lesson with second .c file"; DTD has two .c files from Lesson 01.) |
| `DAS/ARR` auto-repeat for held keys | **Not applicable** | DTD has no keyboard gameplay. Auto-repeat is a Tetris/platformer concern. |
| `RepeatInterval` struct | **Not applicable** | Mouse clicks are single-frame edge events; no held-key repeat needed. |

---

## Key references to research document

> All section numbers refer to `ai-llm-knowledge-dump/prompt/research-desktop-tower-defense.md`

| Course section | Research sections |
|---------------|------------------|
| Grid constants, cell states, coordinate system | §3 Grid System |
| BFS algorithm, flow field, legality check | §4 Pathfinding (full pseudocode) |
| Tower stats, visual design, upgrade system | §5 Tower Catalog |
| Creep types, HP scaling, movement details | §6 Creep Catalog |
| Wave structure, wave order, wave overlap | §7 Wave System |
| Economy: kill gold, sell ratio, interest | §8 Economy System |
| State machine phases and transitions | §9 Game State Machine |
| Range check, targeting modes, AoE | §10 Targeting System |
| MouseState struct, UI regions, placement workflow | §11 Input System |
| Constants and formulas table | §12 Constants and Formulas |
| Procedural rendering, color palette, UI layout | §13 Visual Design and Rendering |
| Sound effects list, background music | §14 Audio Design |
| Minimum viable feature set checklist | §18 Minimum Viable Feature Set |
| All animation timing data (frame counts) | §19 Timing Data |
| course-builder.md alignment (coord system, units, state machine, entity pools, audio pattern) | §20 Alignment with course-builder.md |
| Verification checklist | §21 Verification Checklist |
| Open questions / unverified mechanics | §22 Open Questions |

---

## Notes on open questions (research §22)

The following values from `research-desktop-tower-defense.md §22` are approximations. The course uses the "best guess" values listed in the research document and documents them as tunable:

- Frost slow duration: **60 frames (2.0 s)** — treat as tunable
- Bash stun radius: **2 cells** — treat as tunable
- Boss HP formula: `3.0 × base_hp × 1.10^(wave-1)` — approximate; adjust for feel
- Early-send gold multiplier: **remaining_wave_time × 10 gold/s** — approximate
- Kill gold per creep: use wiki table from research §8; tune per playtest

These are documented in source code comments and in Lesson 12 / 18 where they first appear.
