# Tetris — Javidx9 Course Port

## What is this?

A C port of [javidx9's (OneLoneCoder) Tetris tutorial](https://github.com/OneLoneCoder/videos/blob/master/OneLoneCoder_Tetris.cpp) from Windows/C++/console to Linux/C with two rendering backends: X11 and Raylib. The port follows the Handmade Hero philosophy — a thin platform abstraction layer separates all OS-specific code from pure game logic, so `tetris.c` / `tetris.h` compile identically under both backends.

## How to play

| Key | Action |
|-----|--------|
| `←` / `→` | Move piece left / right (hold for DAS auto-repeat) |
| `↓` | Soft drop (hold for fast auto-repeat) |
| `X` | Rotate clockwise |
| `Z` | Rotate counter-clockwise |
| `R` | Restart (game over screen) |
| `Q` / `Esc` | Quit |

## How to build

**X11 backend**
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install libx11-dev libxkbcommon-dev libgl-dev

cd course
chmod +x build_x11.sh
./build_x11.sh
./build/x11
```

**Raylib backend**
```bash
# Install dependencies (Ubuntu 22.04+)
sudo apt install libraylib-dev

cd course
chmod +x build_raylib.sh
./build_raylib.sh
./build/raylib
```

Both binaries are written to `course/build/`.

## Architecture

```
tetris.h / tetris.c          — pure game logic, zero OS dependencies
platform.h                   — platform API contract (6 functions)
main_x11.c                   — X11 + OpenGL/GLX backend
main_raylib.c                — Raylib backend
```

The game renders into a `TetrisBackbuffer` — a CPU-side `uint32_t *pixels` array in `0xAARRGGBB` format. Each frame, the platform layer uploads that buffer to the GPU (X11 via GLX texture blit; Raylib via `UpdateTexture`). No platform drawing API is ever called from game code.

The main loop is **delta-time driven**: `tetris_update(state, input, delta_time)` receives fractional seconds since the last frame. All timers (`tetromino_drop`, `completed_lines.flash_timer`, `move_left/right/down` auto-repeat) accumulate delta-time and subtract their interval, preserving sub-frame remainders for framerate-independent accuracy.

Input uses `GameButtonState` (from Casey Muratori's Handmade Hero): `ended_down` holds the current key state, `half_transition_count` counts state changes within the frame, enabling reliable "just pressed" detection even at low framerates.

## Lessons

| # | Title | What you learn |
|---|-------|----------------|
| 1 | Project Setup & C vs C++ Differences | Build scripts, C vs STL, replacing `vector`/`wstring` with fixed arrays |
| 2 | Tetromino Representation & Rotation Algorithm | 4×4 character grids, `tetromino_pos_value()` index math for 0°/90°/180°/270° |
| 3 | Collision Detection: `DoesPieceFit()` | Iterating the 4×4 piece grid, mapping to field coords, wall/floor checks |
| 4 | Platform Abstraction Layer Design | `platform.h` contract, why separating game from OS pays off |
| 5 | X11 Backend: Window Creation & Rendering | Xlib window, GLX context, uploading the backbuffer as a texture |
| 6 | Raylib Backend: Window Creation & Rendering | `InitWindow`, `UpdateTexture`, Raylib render loop |
| 7 | Input Handling: X11 Events vs Raylib Polling | `GameButtonState`, `UPDATE_BUTTON` macro, XKB keycodes vs Raylib `IsKeyDown` |
| 8 | Game Loop & Tick-Based → Delta-Time Timing | Why tick-counting breaks on variable framerates; delta-time fix |
| 9 | Piece Movement, Locking & Spawning | Soft drop, gravity timer, locking piece to field, spawning next piece, game over |
| 10 | Line Detection, Clearing & Gravity | Detecting full rows, `TETRIS_FIELD_TMP_FLASH` white-flash pause, collapsing rows bottom-to-top |
| 11 | Scoring, Speed Progression & HUD | 25 pts/piece + bonus for multi-line clears, level-up every 25 pieces, sidebar HUD |
| 12 | Game Over, Restart & Final Integration | `game_over` flag, restart key, semi-transparent overlay, full integration |

## Original source

Original C++ Windows console implementation by **javidx9 / OneLoneCoder**:
- Video: [Tetris, but Simple! (javidx9)](https://www.youtube.com/watch?v=8OK8_tHeCIA)
- Code: [`OneLoneCoder_Tetris.cpp`](https://github.com/OneLoneCoder/videos/blob/master/OneLoneCoder_Tetris.cpp)
- License: OLC-3 (see original repo)
