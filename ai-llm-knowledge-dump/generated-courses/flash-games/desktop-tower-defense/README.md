# Desktop Tower Defense — C Game Course

A hands-on course that re-implements the classic Flash tower-defense game
**Desktop Tower Defense** (Paul Preece, 2007) in C, using the same CPU
back-buffer + platform-abstraction pattern established in the Tetris, Snake,
Asteroids, Frogger, and Sugar Sugar courses.

---

## What you will build

| Feature | What it teaches |
|---------|----------------|
| 30 × 20 tile grid, Y-down pixel coordinates | Flat-array grid, cell-state enum, coordinate system (grid exception) |
| **BFS distance field** (the heart of DTD) | Backwards BFS from exit, flow field, O(600) pathfinding |
| Tower placement legality check | Double-BFS-per-click, reachability enforcement |
| 7 tower types (Pellet → Bash) | Data-driven `TowerDef` table, fire rate + cooldown |
| **Tower upgrade system** (0→1→2) | `upgrade_cost[]`, `upgrade_damage_mult[]`, sell-value recalculation |
| 5 targeting modes (First / Last / Strongest / Weakest / Closest) | Comparator pattern, type-driven `switch` |
| 7 creep types including flying, armoured, spawn, boss | `CreepDef` table, straight-line movement for flyers, effective_damage armour |
| 40-wave sequence with HP scaling | Designated initialisers in `levels.c`, `powf` HP formula |
| Economy: kill gold, 70% sell, early-send bonus, interest | Data-driven economy, float-up text particles |
| **Sprite placeholder system** | `SpriteAtlas`, placeholder→real-asset swap workflow, free asset sources |
| **5 game mods** (Default / Terrain / Weather / Night / Boss) | Runtime behaviour switching via `GameMod` enum, BFS extensions, overlay effects |
| Start menu, mod select, game-over, victory screens | `GAME_PHASE_MOD_SELECT`, FSM navigation, button hover states |
| PCM audio mixer, 18 SFX, ambient music | `AudioOutputBuffer`, phase accumulator, click prevention |
| Both X11 and Raylib backends | Letterbox centering, R↔B channel swap, ALSA + Raylib audio |

---

## Target audience

JavaScript / web developers learning **C** and **low-level game programming** through a complete, real game project. No prior C knowledge assumed — every new concept is anchored to its nearest JS equivalent before the C version is introduced.

**Prerequisite knowledge:**
- Completed the **Tetris** or **Sugar Sugar** course (or equivalent familiarity with the `platform.h` contract, `Backbuffer` pipeline, and `GAME_PHASE` state machine)
- C99 compiler (`gcc` or `clang`)
- Raylib 4.x installed (for the Raylib backend); X11 + ALSA dev libraries for the X11 backend

---

## What makes this course different from the others

Every other course in this series teaches a game with a **fixed path** or **fixed physics**. Desktop Tower Defense is the first game where **the path is computed dynamically every frame** based on player decisions. The entire game is built around one algorithm — a backwards BFS from the exit — which serves as both the pathfinding engine and the placement legality enforcer. Teaching this algorithm is the central goal of the course.

---

## Build instructions

```bash
cd course

# Default build (Raylib, debug)
./build-dev.sh

# Build and run — Raylib
./build-dev.sh --backend=raylib -r

# Build and run — X11
./build-dev.sh --backend=x11 -r

# Debug mode (adds -fsanitize=address,undefined)
./build-dev.sh --backend=raylib -d -r
```

Binary output: `./build/game`

---

## Lesson list

| # | Title | Key concept |
|---|-------|-------------|
| 01 | Window, Canvas, and Build Script | `Backbuffer` pipeline, `build-dev.sh`, both backends |
| 02 | Grid System and Cell States | `CellState` enum, flat-array `grid[]`, **Y-down pixel coordinates** |
| 03 | Mouse Input and Cell Hit Testing | `MouseState`, letterbox mouse transform, pixel-to-cell math |
| 04 | **BFS Distance Field** | Backwards BFS from exit, flow field, flat-array queue |
| 05 | Tower Placement and Legality | Tower pool, double-BFS legality check, `sell_tower()` |
| 06 | Single Creep and Flow Navigation | `Creep` struct, float position, BFS-driven smooth movement |
| 07 | Creep Pool and Wave Spawning | Fixed free-list pool, spawn timer, `compact_creeps()` |
| 08 | State Machine and Economy | `GAME_PHASE`, `change_phase()`, lives + gold |
| 09 | Tower Barrel and Targeting | `atan2f` barrel rotation, `in_range()`, FIRST targeting mode |
| 10 | Tower Firing and Damage | Projectile pool, fire rate cooldown, `apply_damage()`, kill → gold |
| 11 | HP Bars and Death Effects | HP bar colour, `death_timer`, floating gold text particles |
| 12 | Wave System and Wave Table | `WaveDef` + `levels.c`, HP scaling `powf`, wave transitions |
| 13 | Five Single-Target Tower Types | `TowerDef` data table, tower shop UI, gold-check feedback |
| 14 | Frost Slow and Swarm AoE | `slow_timer`, AoE splash, `effective_damage()` armour bypass |
| 15 | All Five Targeting Modes | Comparator pattern, mode cycling, tower selection UI |
| 16 | Fast, Flying, and Armoured Creeps | `creep_flying_move()`, `CreepDef` table, ARMOURED DR |
| 17 | Spawn Creep and Boss | Spawn-on-death children, boss lives cost, victory trigger |
| 18 | Send Wave Early and Interest | Early gold bonus, DTD 1.5 interest mechanic |
| 19 | Side Panel UI and HUD | `draw_ui_panel()`, sell button, `ASSERT` macros |
| 20 | Visual Polish and Particles | Particle pool, `spawn_explosion()`, `draw_rect_blend` overlays |
| 21 | Audio System | `SfxId`/`SoundDef`, PCM mixer, 18 SFX, critical wiring pitfalls |
| 22 | Tower Upgrade System | `TowerDef.upgrade_cost[]`, upgrade button UI, sell-value recalculation |
| 23 | Menus and Screens | `GAME_PHASE_MOD_SELECT`, start screen, game-over restart, victory |
| 24 | Sprite System | `SpriteAtlas` placeholder pattern, `draw_sprite()`, asset swap workflow |
| 25 | Mod Architecture | `GameMod` enum, `active_mod` runtime branching, default mod baseline |
| 26 | Terrain Mod | `CELL_WATER`/`CELL_MOUNTAIN`, terrain-aware BFS, speed multipliers |
| 27 | Weather Mod | Time-based event cycles, speed modifiers, weather particles |
| 28 | Night Mod | Global range reduction, per-tower bonuses, dark grid overlay |
| 29 | Boss Mod | Wave frequency override, damage-immune shield, mid-HP spawn trigger |

---

## Architecture overview

```
     main_x11.c                 main_raylib.c
         │                           │
         └───────────┬───────────────┘
                     │ platform.h contract
                     │  platform_init()
                     │  platform_get_input()   → MouseState
                     │  platform_display_backbuffer()
                     │  platform_audio_update()
                     ▼
              game_update(state, mouse, dt)
                     │
                     ├── change_phase()
                     ├── bfs_fill_distance()
                     ├── tower_update() → find_best_target() → spawn_projectile()
                     ├── creep_move_toward_exit() / creep_flying_move()
                     ├── projectile_update() → apply_damage() → kill_creep()
                     ├── update_wave()
                     └── game_audio_update()
                     │
              game_render(backbuffer, state)
                     │
                     ├── draw_grid()
                     ├── draw_towers()        (barrel rotation via atan2f)
                     ├── draw_creeps()        (HP bars, death flash, frost tint)
                     ├── draw_projectiles()
                     ├── draw_particles()     (float text, explosions, stun stars)
                     ├── draw_ui_panel()      (shop, wave, lives, gold, sell btn)
                     └── draw_phase_overlay() (title, game over, victory)
                     │
              Backbuffer (uint32_t* pixels array)
                     │
                     └── uploaded to GPU each frame (glTexSubImage2D / UpdateTexture)
```

---

## JS → C mental model (key mappings for this course)

| JavaScript / Web | C equivalent | Key difference |
|-----------------|-------------|----------------|
| `const grid = Array(600).fill(0)` | `uint8_t grid[GRID_ROWS * GRID_COLS]` | Fixed size; no GC; `idx = row * GRID_COLS + col` |
| BFS with a `Set` for visited | `int dist[GRID_ROWS * GRID_COLS]`; `memset(-1)` for unvisited | Flat array, no hash set; O(600) — fits on stack |
| `towers.push(new Tower(...))` | `Tower towers[MAX_TOWERS]; tower_count++` | Pre-allocated fixed array; no heap |
| `addEventListener('mousemove')` | `platform_get_input()` fills `MouseState` every frame | No event system; poll once per frame |
| `canvas.getBoundingClientRect()` | `(mouse_raw - g_offset) / g_scale` — letterbox math | Must transform from window pixels to canvas pixels manually |
| `Math.atan2(dy, dx)` | `atan2f(dy, dx)` from `<math.h>` | Same formula; C needs `f` suffix for `float` version |
| `enemy.speed *= 0.5` | `creep.speed *= creep.slow_factor` (when `slow_timer > 0`) | Plain float multiply; no object system |
| `enemies.filter(e => e.hp > 0)` | `compact_creeps(state)` — swap-remove after every frame | Manual compaction; no built-in filter |
| `ctx.fillText(score, 10, 30)` | `draw_text(bb, 10, 30, buf, COLOR_WHITE, 2)` | Bitmap font drawn pixel-by-pixel into the backbuffer |

---

## File structure

```
course/
├── build-dev.sh
├── src/
│   ├── platform.h
│   ├── game.h
│   ├── game.c
│   ├── audio.c
│   ├── levels.c
│   ├── main_x11.c
│   ├── main_raylib.c
│   └── utils/
│       ├── backbuffer.h
│       ├── draw-shapes.c/.h
│       ├── draw-text.c/.h
│       ├── math.h
│       └── audio.h
└── lessons/
    ├── lesson-01-window-canvas-and-build-script.md
    ├── lesson-02-grid-system-and-cell-states.md
    ├── ...
    └── lesson-21-audio-system.md
```

---

## Additional resources

- **Research document:** `ai-llm-knowledge-dump/prompt/research-desktop-tower-defense.md` — full game mechanics, BFS pseudocode, tower/creep stats, wave order, timing data, verification checklist
- **Course-builder principles:** `ai-llm-knowledge-dump/prompt/course-builder.md`
- **Tetris reference implementation:** `games/tetris/` — canonical patterns for backbuffer, platform.h, audio
- **Original game:** Desktop Tower Defense by Paul Preece (2007); playable in Flashpoint archive for study
- **Community wiki (archived):** `dtd.wikia.com` via Wayback Machine — tower stats, wave tables
- **HTML5 remakes:** GitHub search "desktop tower defense html5" — community reverse-engineered implementations for cross-reference
