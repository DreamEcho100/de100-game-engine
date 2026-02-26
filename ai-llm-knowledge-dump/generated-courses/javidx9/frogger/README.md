# Frogger — Javidx9 Course Port

## What is this?

A port of [OneLoneCoder / javidx9's Frogger](https://github.com/OneLoneCoder) from C++/Windows console to C/Linux. The frog must cross five lanes of traffic and three lanes of river to reach the homes at the top. Reach 3 homes to win.

## How to play

| Key | Action |
|-----|--------|
| `↑` / `W` | Hop forward (one tile per press) |
| `↓` / `S` | Hop backward (one tile per press) |
| `←` / `A` | Hop left (one tile per press) |
| `→` / `D` | Hop right (one tile per press) |
| `Esc` / `Q` | Quit |

Hops fire **once per press** — holding a key does not repeat. On river rows the frog is carried sideways by the log beneath it.

## How to build

From the `course/` directory:

```sh
# X11 backend (default Linux)
./build_x11.sh

# Raylib backend
./build_raylib.sh
```

Both scripts compile `src/frogger.c` together with the chosen platform layer and link against the platform's libraries.

## Architecture

| Layer | File | Responsibility |
|-------|------|----------------|
| Game logic | `src/frogger.c` + `src/frogger.h` | All update and render logic; no OS calls |
| Platform contract | `src/platform.h` | Declares `platform_get_input`, timing, and backbuffer upload |
| X11 platform | `src/platform_x11.c` | Window creation, event loop, XImage upload |
| Raylib platform | `src/platform_raylib.c` | Window creation, event loop, `UpdateTexture` upload |

**Backbuffer pipeline** — `frogger_render` writes pixels into a `FroggerBackbuffer` (`uint32_t pixels[1024 × 640]`). The platform uploads that buffer to the GPU once per frame; no platform drawing API is called from game code.

**Delta-time loop** — `frogger_tick(state, input, dt)` advances the simulation by `dt` seconds (capped at 100 ms). Lane scroll positions, river-carry physics, and the death timer are all driven by `state->time`.

**Data-Oriented Design** — lane speeds (`float lane_speeds[10]`) and lane patterns (`char lane_patterns[10][64]`) are separate arrays so the hot path (danger buffer rebuild) touches only the speed floats (40 bytes, one cache line).

## Lessons

| # | Title | What you learn |
|---|-------|----------------|
| 1 | Screen & cell math | `SCREEN_CELLS_W/H`, `CELL_PX`, `TILE_PX`; coordinate systems |
| 2 | Sprite loader | `.spr` binary format; flat `SpriteBank` pool; no heap in game loop |
| 3 | Lane scroll | `lane_scroll()` — positive-modulo pixel math; fixing the C-truncation jump bug |
| 4 | Input system | `GameButtonState`; `half_transition_count`; "just-pressed" vs held |
| 5 | Game tick | `frogger_tick`: hop, river carry, danger buffer rebuild, collision |
| 6 | Danger buffer | `uint8_t danger[W×H]` rebuilt every tick; center-cell collision |
| 7 | Rendering | Painter's algorithm; `draw_sprite_partial`; death flash; HUD |
| 8 | Platform split | `platform.h` contract; same game.c compiles under X11 and Raylib |
| 9 | DOD layout | Separating hot (speeds) from cold (patterns) data |

## Original source

Based on **javidx9 / OneLoneCoder** — *"Code-It-Yourself! Frogger"* (Windows console, C++).  
Original repository: <https://github.com/OneLoneCoder/Javidx9>  
YouTube series: *One Lone Coder* — used here for educational study only.
