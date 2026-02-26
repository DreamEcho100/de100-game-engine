# Asteroids — Javidx9 Course Port

## What is this?

A port of [OneLoneCoder / javidx9's Asteroids](https://github.com/OneLoneCoder) from C++/Windows console to C/Linux. The player pilots a triangle ship through a field of tumbling asteroids, shooting them apart for points. Large asteroids split into two medium ones; medium ones split into two small ones; small ones vanish. Clear all asteroids to advance to the next wave.

## How to play

| Key | Action |
|-----|--------|
| `←` / `A` | Rotate counter-clockwise (held = continuous) |
| `→` / `D` | Rotate clockwise (held = continuous) |
| `↑` / `W` | Thrust forward (held = acceleration) |
| `Space` | Fire bullet (one shot per press — holding does not rapid-fire) |
| `Esc` / `Q` | Quit |

The ship wraps around screen edges. Bullets expire after 2.5 seconds. After death the scene freezes for 1.5 seconds then resets.

## How to build

From the `course/` directory:

```sh
# X11 backend (default Linux)
./build_x11.sh

# Raylib backend
./build_raylib.sh
```

Both scripts compile `src/asteroids.c` together with the chosen platform layer.

## Architecture

| Layer | File | Responsibility |
|-------|------|----------------|
| Game logic | `src/asteroids.c` + `src/asteroids.h` | All update and render logic; no OS calls |
| Platform contract | `src/platform.h` | Declares `platform_get_input`, timing, and backbuffer upload |
| X11 platform | `src/platform_x11.c` | Window creation, event loop, XImage upload |
| Raylib platform | `src/platform_raylib.c` | Window creation, event loop, `UpdateTexture` upload |

**Backbuffer pipeline** — `asteroids_render` writes pixels into an `AsteroidsBackbuffer` (`uint32_t pixels[800 × 600]`). The platform uploads that buffer to the GPU once per frame.

**Delta-time loop** — `asteroids_update(state, input, dt)` advances physics by `dt` seconds. Rotation, thrust, bullet movement, and the death timer are all `× dt` so the game runs at the same speed regardless of frame rate.

**Bresenham line + toroidal wrapping** — `draw_line` uses integer Bresenham; each pixel is written via `draw_pixel_w` which wraps coordinates modulo screen dimensions. Wireframe objects crossing the screen edge render seamlessly without any special case.

**Entity pools** — asteroids and bullets live in fixed C arrays (`SpaceObject asteroids[64]`, `SpaceObject bullets[32]`). Dead objects are marked `active = 0` and removed by `compact_pool` (swap-with-last, O(1) amortized). No heap allocation occurs in the game loop.

## Lessons

| # | Title | What you learn |
|---|-------|----------------|
| 1 | Screen & backbuffer | `SCREEN_W/H`, pixel format, clearing with `draw_rect` |
| 2 | Bresenham's line | Integer line drawing; why no floating-point needed |
| 3 | 2D rotation matrix | `x' = x·cos θ − y·sin θ`; `draw_wireframe` model pipeline |
| 4 | Toroidal wrapping | `draw_pixel_w` modulo wrap; seamless edge crossing |
| 5 | Input system | `GameButtonState`; held vs just-pressed; `prepare_input_frame` |
| 6 | Euler integration | `dx += sin(angle)·accel·dt`; `x += dx·dt` |
| 7 | Entity pools | Fixed arrays + `compact_pool` (swap-with-last); no heap in loop |
| 8 | Circle collision | Squared-distance test; no `sqrt` needed |
| 9 | Asteroid splitting | `size/2`; two random-angle children spawned at hit position |
| 10 | Level progression | Wave clear detection; bonus score; safe respawn placement |
| 11 | State machine | `GAME_PHASE` enum; `PHASE_DEAD` freezes update, blinks ship |

## Original source

Based on **javidx9 / OneLoneCoder** — *"Code-It-Yourself! Asteroids"* (Windows console, C++).  
Original repository: <https://github.com/OneLoneCoder/Javidx9>  
YouTube series: *One Lone Coder* — used here for educational study only.
