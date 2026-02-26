# Snake — Javidx9 Course Port

## What is this?

A C port of [javidx9's (OneLoneCoder) Snake tutorial](https://github.com/OneLoneCoder/videos/blob/master/OneLoneCoder_Snake.cpp) from Windows/C++/console to Linux/C with two rendering backends: X11 and Raylib. The port follows the Handmade Hero philosophy — all OS-specific code lives in thin platform backends (`main_x11.c`, `main_raylib.c`), while `snake.c` / `snake.h` are pure C with no platform dependencies.

## How to play

| Key | Action |
|-----|--------|
| `←` / `A` | Turn counter-clockwise (relative left) |
| `→` / `D` | Turn clockwise (relative right) |
| `R` / `Space` | Restart after game over |
| `Q` / `Esc` | Quit |

The snake moves continuously. Eat red food to grow (+5 segments). Hit a wall or yourself and it's game over. Speed increases every 3 points.

## How to build

**X11 backend**
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install libx11-dev libxkbcommon-dev

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
snake.h / snake.c            — pure game logic, zero OS dependencies
platform.h                   — platform API contract
main_x11.c                   — X11 + OpenGL/GLX backend
main_raylib.c                — Raylib backend
```

The game renders into a `SnakeBackbuffer` — a CPU-side `uint32_t *pixels` array in `0xAARRGGBB` format. Each frame, the platform layer uploads that buffer to the GPU. No platform drawing API is ever called from game code.

The snake body is stored as a **ring buffer** (`segments[MAX_SNAKE]` with `head`, `tail`, and `length`), replacing the original C++ `std::list`. New head positions are written at `(head+1) % MAX_SNAKE`; the tail advances unless `grow_pending > 0`.

The main loop is **delta-time driven**: `snake_update(state, input, delta_time)` accumulates `move_timer` each frame and fires a move step when `move_timer >= move_interval`. Subtracting (not zeroing) `move_interval` preserves sub-frame overshoot for consistent speed at any framerate.

Input uses `GameButtonState` (from Casey Muratori's Handmade Hero): direction turns fire only on "just pressed" (`ended_down && half_transition_count > 0`), preventing held-key drift.

## Lessons

| # | Title | What you learn |
|---|-------|----------------|
| 1 | Open a window (X11 + Raylib) | Black window, Q to quit; platform loop skeleton |
| 2 | Draw the arena | `SnakeBackbuffer`, `draw_rect`, header rows, border walls |
| 3 | Snake ring buffer — draw static snake | `segments[]` ring buffer, `head`/`tail`/`length`, drawing body cells |
| 4 | Movement — snake advances each tick | Delta-time `move_timer`, advancing head, advancing tail |
| 5 | Input — turn left/right | `GameButtonState`, `UPDATE_BUTTON`, CW/CCW turn math `(dir±1)%4` |
| 6 | Collision — walls + self | Wall bounds check, ring-buffer self-collision walk, `game_over` flag |
| 7 | Food — spawn, eat, grow | `snake_spawn_food()`, `grow_pending`, food-not-on-snake placement loop |
| 8 | Score + death screen + restart | Score counter, `best_score` preservation, game over overlay, restart key |
| 9 | File split: snake.h/c + platform.h | Clean header/impl separation, same behavior, `prepare_input_frame` |
| 10 | Speed scaling + polish | `move_interval` decrease every 3 points, 0.05s floor, final Valgrind clean |

## Original source

Original C++ Windows console implementation by **javidx9 / OneLoneCoder**:
- Video: [Snake, but Simple! (javidx9)](https://www.youtube.com/watch?v=d1F_1ADp9iY)
- Code: [`OneLoneCoder_Snake.cpp`](https://github.com/OneLoneCoder/videos/blob/master/OneLoneCoder_Snake.cpp)
- License: OLC-3 (see original repo)
