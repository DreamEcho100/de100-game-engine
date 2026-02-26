# Snake Course — Upgrade Plan

Reference implementation pattern: `ai-llm-knowledge-dump/Javidx9-courses/tetris/course/`

---

## Overview

The current snake course was built before the tetris upgrade established the professional
architecture. Every pattern from that upgrade must now be applied to snake:

| Area | Current (old) | Target (upgraded) |
|------|---------------|-------------------|
| Platform contract | 6 functions: init, get_input, render, sleep_ms, should_quit, shutdown | 4 functions: init, get_time, get_input, display_backbuffer |
| Rendering | Platform draws cells (X11/Raylib API calls in platform) | Game renders to `SnakeBackbuffer`; platform uploads pixels |
| Color system | X11 named colors / Raylib `Color` struct | `uint32_t` + `SNAKE_RGB`/`SNAKE_RGBA` macros (`0xAARRGGBB`) |
| Input | Simple booleans (`turn_left`, `turn_right`) | `GameButtonState` (`half_transition_count` + `ended_down`) |
| Direction | `#define DIR_UP 0` etc. | `typedef enum { SNAKE_DIR_UP, ... } SNAKE_DIR` |
| Game timing | `platform_sleep_ms(BASE_TICK_MS)` + `tick_count/speed` | Delta-time accumulator (`move_timer += dt`; trigger when ≥ interval) |
| Build tool | `gcc` | `clang` |
| X11 backend | `XCreateSimpleWindow`, no OpenGL, nanosleep | GLX/OpenGL backbuffer upload, VSync, `XkbSetDetectableAutoRepeat` |
| Raylib backend | Native `DrawRectangle` calls | Upload `SnakeBackbuffer` as `Texture2D` each frame |
| HUD rendering | Done in platform (X11/Raylib draw calls) | Done in `snake.c` via bitmap font + draw primitives |
| Lessons | 10 lessons, old API patterns | 12 lessons, new architecture taught incrementally |

---

## Target Architecture

```
┌──────────────────────────────────────────────────────┐
│             main_x11.c / main_raylib.c               │
│  platform_init(title, w, h)                          │
│  loop:                                               │
│    dt = platform_get_time() delta                    │
│    prepare_input_frame(&input)                       │
│    platform_get_input(&input)                        │
│    snake_update(&state, &input, dt)                  │
│    snake_render(&state, &backbuffer)                 │
│    platform_display_backbuffer(&backbuffer)  ← VSync │
└──────────────────────────────────────────────────────┘
                    ↓ SnakeBackbuffer *
┌──────────────────────────────────────────────────────┐
│                     snake.c                          │
│  snake_init, snake_update (delta-time)               │
│  snake_render (draws everything to backbuffer)       │
│  draw_rect, draw_rect_blend                          │
│  draw_char, draw_text (5×7 bitmap font)              │
└──────────────────────────────────────────────────────┘
                    ↓ types / API
┌──────────────────────────────────────────────────────┐
│                     snake.h                          │
│  SnakeBackbuffer, SNAKE_RGB/A, color constants       │
│  SNAKE_DIR enum, GameButtonState, GameInput          │
│  GameState, public function declarations             │
└──────────────────────────────────────────────────────┘
```

---

## Phase 1 — Backup

Move all existing lesson files to `lessons/old/` (preserve original work).

Files to back up:
- `lessons/lesson-01.md` through `lessons/lesson-10.md` (10 files)

---

## Phase 2 — Source File Rewrites

### `snake.h` — Complete rewrite

**Add:**

```c
/* Backbuffer */
typedef struct {
    uint32_t *pixels;
    int       width;
    int       height;
} SnakeBackbuffer;

/* Color macros — 0xAARRGGBB */
#define SNAKE_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) << 8)  |  (uint32_t)(b))
#define SNAKE_RGB(r, g, b) SNAKE_RGBA(r, g, b, 0xFF)

/* Named colors */
#define COLOR_BLACK      SNAKE_RGB(  0,   0,   0)
#define COLOR_WHITE      SNAKE_RGB(255, 255, 255)
#define COLOR_RED        SNAKE_RGB(220,  50,  50)
#define COLOR_DARK_RED   SNAKE_RGB(139,   0,   0)
#define COLOR_GREEN      SNAKE_RGB( 50, 205,  50)
#define COLOR_YELLOW     SNAKE_RGB(255, 215,   0)
#define COLOR_DARK_GRAY  SNAKE_RGB( 80,  80,  80)
#define COLOR_GRAY       SNAKE_RGB(128, 128, 128)

/* Direction enum */
typedef enum {
    SNAKE_DIR_UP    = 0,
    SNAKE_DIR_RIGHT = 1,
    SNAKE_DIR_DOWN  = 2,
    SNAKE_DIR_LEFT  = 3
} SNAKE_DIR;

/* Input */
typedef struct {
    int half_transition_count;
    int ended_down;
} GameButtonState;

#define UPDATE_BUTTON(button, is_down)                    \
    do {                                                   \
        if ((button).ended_down != (is_down)) {            \
            (button).half_transition_count++;              \
            (button).ended_down = (is_down);               \
        }                                                  \
    } while (0)

typedef struct {
    GameButtonState turn_left;
    GameButtonState turn_right;
    int restart;   /* simple flag — fires on just-pressed only */
    int quit;
} GameInput;

/* GameState */
typedef struct {
    Segment  segments[MAX_SNAKE];
    int      head;
    int      tail;
    int      length;

    SNAKE_DIR direction;
    SNAKE_DIR next_direction;

    float    move_timer;      /* accumulates delta_time */
    float    move_interval;   /* seconds per move; decreases as score grows */

    int      grow_pending;
    int      food_x;
    int      food_y;
    int      score;
    int      best_score;
    int      game_over;
} GameState;

/* Public API */
void snake_init(GameState *s);
void snake_spawn_food(GameState *s);
void snake_update(GameState *s, const GameInput *input, float delta_time);
void snake_render(const GameState *s, SnakeBackbuffer *backbuffer);
void prepare_input_frame(GameInput *input);
```

**Remove:** `PlatformInput`, `#define DIR_*`, `BASE_TICK_MS`, `speed`, `tick_count`

**Update:** `CELL_SIZE` from 14 → 20 (matching PLAN.md design)

---

### `snake.c` — Major rewrite

**Add:**

1. **`draw_rect`** — solid rectangle fill into `SnakeBackbuffer`
2. **`draw_rect_blend`** — alpha-composited rectangle fill
3. **5×7 bitmap font** — same 96-character glyph table as tetris course
4. **`draw_char` / `draw_text`** — render glyphs at scale
5. **`draw_cell`** — render one grid cell (inset by 1px) at grid coordinates

**Rewrite:**
- `snake_init` — use `SNAKE_DIR_RIGHT`, `move_interval` instead of `speed`
- `snake_update` — delta-time accumulator replaces `tick_count`; typed `SNAKE_DIR`
- Add `snake_render` — move ALL drawing out of platform into this function

**`snake_render` draws (in order):**
1. Clear backbuffer to `COLOR_BLACK`
2. Header background (`COLOR_DARK_GRAY`)
3. Header separator line (`COLOR_GREEN`)
4. Score and best score text (via `draw_text`)
5. Border walls (`COLOR_GREEN`)
6. Food (`COLOR_RED`)
7. Snake body segments (`COLOR_YELLOW`, or `COLOR_DARK_RED` if game over)
8. Snake head (`COLOR_WHITE`, or `COLOR_DARK_RED` if game over)
9. Game over overlay (`draw_rect_blend` + border + text)

**`snake_update` — delta-time pattern:**
```c
void snake_update(GameState *s, const GameInput *input, float delta_time) {
    if (s->game_over) {
        if (input->restart) snake_init(s);
        return;
    }

    /* Direction input — turn fires on just-pressed */
    if (input->turn_right.ended_down && input->turn_right.half_transition_count > 0)
        s->next_direction = (SNAKE_DIR)((s->direction + 1) % 4);
    if (input->turn_left.ended_down && input->turn_left.half_transition_count > 0)
        s->next_direction = (SNAKE_DIR)((s->direction + 3) % 4);

    /* Accumulate time — only move when interval is reached */
    s->move_timer += delta_time;
    if (s->move_timer < s->move_interval) return;
    s->move_timer -= s->move_interval;   /* keep remainder */

    /* ... rest of movement/collision/food logic unchanged ... */
}
```

---

### `platform.h` — Rewrite to 4-function contract

```c
void   platform_init(const char *title, int width, int height);
double platform_get_time(void);
void   platform_get_input(GameInput *input);
void   platform_display_backbuffer(const SnakeBackbuffer *backbuffer);
```

**Remove:** `platform_render`, `platform_sleep_ms`, `platform_should_quit`, `platform_shutdown`

---

### `main_x11.c` — Full rewrite

Pattern: identical to tetris `main_x11.c` but for snake.

Key changes:
- `XCreateWindow` (not `XCreateSimpleWindow`) — required for GLX visual
- `XkbSetDetectableAutoRepeat` — suppress X11 key-repeat synthetic events
- `XPending` + non-blocking drain (not `XNextEvent`)
- GLX context creation (`glXCreateContext`)
- `setup_vsync` — try `GLX_EXT_swap_control`, fall back to `GLX_MESA_swap_control`
- `platform_display_backbuffer` — `glTexImage2D` + `glBegin(GL_QUADS)` fullscreen quad
- `platform_get_time` — `clock_gettime(CLOCK_MONOTONIC)`
- `ConfigureNotify` → `glViewport` + `glOrtho` on resize
- Turn keys: `UPDATE_BUTTON` on `KeyPress`/`KeyRelease`
- `main()` — delta-time loop, `malloc`/`free` backbuffer, no `platform_sleep_ms`
- `quit` set from `ClientMessage` or Q/Escape

---

### `main_raylib.c` — Full rewrite

Pattern: identical to tetris `main_raylib.c` but for snake.

Key changes:
- `platform_init`: `InitWindow` + `SetTargetFPS(60)`
- `platform_display_backbuffer`: `UpdateTexture` + `DrawTextureEx`
- `platform_get_time`: `GetTime()`
- `platform_get_input`: `IsKeyPressed`/`IsKeyDown` → `UPDATE_BUTTON`
  - Turn keys use `IsKeyPressed` (single-frame) → set `ended_down=1, htc=1` then reset
- `main()`: delta-time loop, `malloc`/`free` backbuffer, `WindowShouldClose()` for quit

---

### `build_x11.sh` — Update

```sh
#!/bin/bash
set -e
mkdir -p build
echo "Building snake_x11..."
clang src/snake.c src/main_x11.c \
    -o build/snake_x11 \
    -Wall -Wextra -O2 \
    -lX11 -lxkbcommon -lGL -lGLX
echo "Done! Run with: ./build/snake_x11"
```

### `build_raylib.sh` — Update

```sh
#!/bin/bash
set -e
mkdir -p build
echo "Building snake_raylib..."
clang src/snake.c src/main_raylib.c \
    -o build/snake_raylib \
    -Wall -Wextra -O2 \
    -lraylib -lm
echo "Done! Run with: ./build/snake_raylib"
```

---

## Phase 3 — Lesson Rewrites (12 lessons)

| # | Title | Key content |
|---|-------|-------------|
| 01 | Window + GLX/OpenGL Context | `XCreateWindow`, GLX init, `glXSwapBuffers`, VSync setup |
| 02 | SnakeBackbuffer: Platform-Independent Canvas | `uint32_t *pixels`, `malloc`/`free`, `glTexImage2D` upload |
| 03 | Color System: `uint32_t`, `SNAKE_RGB`, Named Constants | `0xAARRGGBB` bit-packing, pre-defined color names |
| 04 | Typed Enums: `SNAKE_DIR` | `typedef enum`, why not `#define` ints, direction math `(dir+1)%4` |
| 05 | GameState + Ring Buffer + `snake_init` | `Segment[]`, head/tail/length, `move_interval`, `memset` |
| 06 | Drawing Primitives: `draw_rect`, `draw_cell` | Pixel math, bounds clamping, grid→pixel formula |
| 07 | `snake_render`: The Drawing Function | Rendering order, draw_rect for all game elements |
| 08 | Delta-Time Game Loop + Move Timer | `platform_get_time`, `move_timer += dt`, `timer -= interval` trick |
| 09 | Input System: `GameButtonState` + `UPDATE_BUTTON` | half_transition_count, `prepare_input_frame`, turn-once behavior |
| 10 | Collision Detection: Walls + Self | Ring buffer iteration, wall bounds, self-overlap check |
| 11 | Food, Growth, Score, Speed Scaling | `snake_spawn_food`, `grow_pending`, `move_interval` reduction |
| 12 | Bitmap Font + HUD + Game Over Overlay | 5×7 glyphs, `draw_text`, `draw_rect_blend`, alpha compositing, final integration |

---

## Invariants to Preserve

- Ring buffer logic (`head`, `tail`, `length`, `grow_pending`) — correct, keep as-is
- `snake_spawn_food` collision check — correct, keep as-is
- Direction arithmetic: `(dir+1)%4` CW, `(dir+3)%4` CCW — keep, just type as `SNAKE_DIR`
- No reversing: already handled by `next_direction` (only overwritten on turn press, not each frame)
- `best_score` preserved across `snake_init` — keep `saved_best` pattern

---

## Window Dimensions

`CELL_SIZE` changes from 14 → 20 to match PLAN.md design:
- `WINDOW_WIDTH  = GRID_WIDTH  * CELL_SIZE = 60 * 20 = 1200px`
- `WINDOW_HEIGHT = (GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE = 23 * 20 = 460px`
