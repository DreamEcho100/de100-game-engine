# Asteroids Course — PLAN.md

Reference source: `OneLoneCoder_Asteroids.cpp`
Course output: `course/`

---

## What the program does

Classic Asteroids arcade game with **wireframe vector graphics** — no sprites.

- A triangular ship sits in the center of the screen
- Left/Right arrows rotate the ship; Up arrow fires thrust (acceleration → velocity)
- Spacebar fires a bullet in the direction the ship faces
- 2–3 large asteroids drift across the screen, wrapping edge-to-edge (toroidal space)
- Shooting an asteroid splits it into two smaller ones; small asteroids vanish when shot
- If the ship touches any asteroid: game over → reset
- Clearing all asteroids advances to the next level (new asteroids spawn)
- Score displayed in top-left corner via bitmap font

**What makes this game new vs tetris/snake/frogger:**
1. **Wireframe rendering** — Bresenham's line algorithm draws lines into the backbuffer pixel-by-pixel; no sprites, no tile grids
2. **2D rotation matrix** — `x' = x·cos(θ) - y·sin(θ)`, `y' = x·sin(θ) + y·cos(θ)` transforms model vertices each frame
3. **Newtonian physics** — acceleration changes velocity; velocity changes position (both scaled by `dt`)
4. **Entity pools** — fixed-size C arrays with `active` flags replace C++ `vector<sSpaceObject>`; no heap allocation in the hot path
5. **Circle collision** — `(dx*dx + dy*dy) < r*r` (squared distance avoids `sqrt`)
6. **Asteroid splitting** — when hit, one asteroid becomes two at random angles
7. **Toroidal space** — objects wrap from one edge to the opposite

---

## Architecture decisions (vs original C++ code)

| Original (C++ / olcConsoleGameEngine) | This course (C / backbuffer) |
|--------------------------------------|------------------------------|
| `vector<sSpaceObject> vecAsteroids`  | `SpaceObject asteroids[MAX_ASTEROIDS]` + `int asteroid_count` |
| `vector<sSpaceObject> vecBullets`    | `SpaceObject bullets[MAX_BULLETS]` + `int bullet_count` |
| `vector<pair<float,float>> vecModelShip` | `Vec2 ship_model[SHIP_VERTS]` static array |
| `vector<pair<float,float>> vecModelAsteroid` | `Vec2 asteroid_model[ASTEROID_VERTS]` static array |
| `DrawWireFrameModel(...)` — draws to console | `draw_wireframe(bb, model, n, x, y, angle, scale, color)` — draws to backbuffer |
| `DrawLine(x1,y1,x2,y2)` — console char | `draw_line(bb, x1,y1,x2,y2, color)` — Bresenham into pixel array |
| `OnUserCreate` / `OnUserUpdate` | `asteroids_init` / `asteroids_update` / `asteroids_render` |
| C++ class inheritance | Plain C structs, free functions |
| `ScreenWidth()` / `ScreenHeight()` | `SCREEN_W` / `SCREEN_H` constants |
| `bDead = true` on next frame → `ResetGame()` | `state.phase = PHASE_DEAD` → timer → `PHASE_PLAYING` |
| `remove_if` + `erase` (invalidates iterators) | `compact_pool()` — swap dead entry with last, decrement count |

### Entity pool pattern
```
asteroids[MAX_ASTEROIDS]:  [ A | A | A | . | . | . ]
asteroid_count = 3          ^active^      ^unused^
```
No gaps — when an asteroid dies, swap it with the last active one.
`compact_pool` is O(1) per removal (amortized).

### Color system
Bytes in memory: `[RR, GG, BB, AA]` — uint32_t value is `0xAABBGGRR` on little-endian.
Matches both `GL_RGBA` (X11/OpenGL) and `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` (Raylib).
```c
#define ASTEROIDS_RGBA(r,g,b,a) \
    (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(g)<<8)|(uint32_t)(r))
#define ASTEROIDS_RGB(r,g,b) ASTEROIDS_RGBA(r,g,b,0xFF)
```
NOTE: This is `0xAABBGGRR` (not `0xAARRGGBB`). The course-builder.md prompt has an outdated macro — this correct version is used here and in tetris/snake/frogger.

---

## Key types

```c
typedef struct { float x, y; } Vec2;

typedef struct {
    float x, y;     /* position */
    float dx, dy;   /* velocity */
    float angle;    /* rotation (radians) */
    int   size;     /* radius in pixels; 0 = bullet */
    int   active;   /* 1 = alive, 0 = dead */
} SpaceObject;

typedef enum { PHASE_PLAYING, PHASE_DEAD } GAME_PHASE;

typedef struct {
    SpaceObject player;
    SpaceObject asteroids[MAX_ASTEROIDS];
    int         asteroid_count;
    SpaceObject bullets[MAX_BULLETS];
    int         bullet_count;
    int         score;
    GAME_PHASE  phase;
    float       dead_timer;
    Vec2        ship_model[SHIP_VERTS];
    Vec2        asteroid_model[ASTEROID_VERTS];
} GameState;
```

---

## Proposed lesson sequence

| # | What gets built | What the student sees |
|---|----------------|-----------------------|
| 01 | Window + GLX/OpenGL context | Black window, resizable, closeable |
| 02 | `AsteroidsBackbuffer` + `draw_pixel` + clear | Black window; toggling a white pixel with `draw_pixel(bb, 100, 100, white)` |
| 03 | Bresenham's line algorithm (`draw_line`) | A diagonal white line across the window |
| 04 | `draw_wireframe` — rotate + scale + translate + draw polygon | A white triangle and a jagged circle in the center |
| 05 | `Vec2`, 2D rotation matrix — full math derivation | Triangle spins when you press Left/Right arrow |
| 06 | `GameInput` + `GameButtonState` + `UPDATE_BUTTON` | Input prints to console; no visual yet |
| 07 | `SpaceObject` + entity pools (`asteroids[]`, `bullets[]`) | Two asteroids drift across screen (no collision yet) |
| 08 | Ship physics — rotation, thrust, `dx/dy/x/y` update | Ship moves, wraps around edges |
| 09 | Toroidal space — `wrap_coordinates` | All objects (ship + asteroids) wrap correctly |
| 10 | Bullets — fire on Space (just-pressed), pool add/remove | Bullets fire from ship nose, disappear at edges |
| 11 | Circle collision — `IsPointInsideCircle` (squared), asteroid splitting | Shooting asteroids splits them; small ones vanish |
| 12 | Death + reset state machine (`GAME_PHASE` enum) | Ship-asteroid contact → brief flash → reset |
| 13 | Level progression + score + bitmap font HUD | Score counts up; new wave spawns when screen is cleared |
| 14 | Final integration — both backends, zero-warning build, valgrind | Full working game on X11 and Raylib |

Total: **14 lessons**

---

## Planned file structure

```
course/
├── build_x11.sh
├── build_raylib.sh
├── src/
│   ├── asteroids.h       — types, enums, ASTEROIDS_RGB, GameButtonState, GameInput, GameState, function declarations
│   ├── asteroids.c       — all game logic + rendering (draw_line, draw_wireframe, asteroids_init, asteroids_update, asteroids_render)
│   ├── platform.h        — 4-function platform contract
│   ├── main_x11.c        — GLX/OpenGL backbuffer, XkbSetDetectableAutoRepeat, delta-time loop
│   └── main_raylib.c     — Texture2D backbuffer pipeline, delta-time loop
└── lessons/
    ├── lesson-01.md  …  lesson-14.md
```

---

## New C / math concepts introduced (teach inline)

| Concept | Where introduced | Teaching approach |
|---------|-----------------|-------------------|
| `Vec2` struct (2-component float vector) | Lesson 04 | JS analogy: `{x: number, y: number}` |
| Fixed-size arrays as entity pools | Lesson 07 | "Like a pre-allocated JS array with a length tracker" |
| Bresenham's line algorithm | Lesson 03 | Worked pixel-by-pixel example first |
| 2D rotation matrix | Lesson 05 | Unit circle diagram → `cos/sin` values at 0°,90° → formula |
| Squared-distance collision | Lesson 11 | "sqrt is slow; compare squared values instead" with numbers |
| Euler integration (v += a*dt, p += v*dt) | Lesson 08 | Concrete example: a=20, dt=0.016 → v=0.32 after 1 frame |
| Toroidal modulo wrapping | Lesson 09 | "Like Pac-Man — left wall connects to right wall" |
| Compact pool removal (swap-with-last) | Lesson 11 | ASCII diagram of the array before and after |
| `sinf`/`cosf` (C math library) | Lesson 05 | Show `#include <math.h>` and `-lm` linker flag |
| `rand()` / `RAND_MAX` | Lesson 07 | "Like Math.random() in JS, scaled to 0..1 by dividing" |

---

## Important implementation notes

- **No `sqrt` in collision**: `(dx*dx + dy*dy) < (r*r)` — avoid `sqrt` in the hot path
- **`srand(time(NULL))` in `main`** — seed once at startup, not in `asteroids_init` (reset doesn't re-seed)
- **Bullet pool**: `MAX_BULLETS 32` is sufficient; fire rate limited by "just pressed" (not held)
- **Asteroid pool**: `MAX_ASTEROIDS 64` covers large(16) → medium(8) × 2 → small(4) × 4 splits = 1+2+4 = 7 per initial asteroid × 2 = 14 max realistically; 64 is safe
- **Asteroid model**: generated once in `asteroids_init` with `rand()` — 20 verts, each scaled by `noise = rand()/RAND_MAX * 0.4 + 0.8` (jagged circle)
- **Ship model**: 3 verts — `{0,-5}`, `{-2.5,+2.5}`, `{+2.5,+2.5}` (isoceles triangle pointing up)
- **Bullet angle**: the `angle` field on a bullet is reused as a **lifetime counter** (counts down); when ≤ 0, bullet is dead — this is the original code's trick
- **New asteroids on level clear**: spawned 90° left and right of player direction, at radius 30 from player
- **`draw_line` wraps coordinates**: the overridden `Draw()` in the original wraps per-pixel — in our version `draw_line` calls `draw_pixel` with wrapped coords, so wireframe objects seamlessly cross screen edges

---

_Created: 2026-02-25 — Analysis phase complete. Execute course build next._
