#ifndef SNAKE_H
#define SNAKE_H

/* ============================================================
 * snake.h — Core game types, constants, and public API
 *
 * Platform-independent. Included by snake.c, main_x11.c, main_raylib.c.
 * Nothing in here is OS-specific.
 *
 * Port of OneLoneCoder Snake (javidx9) from C++/Windows to C/Linux.
 * ============================================================ */

#include <stdint.h> /* uint32_t */
#include <stdlib.h> /* rand(), srand() */
#include <string.h> /* memset() */
#include <time.h>   /* time() */

/* ─── Grid & Display ─────────────────────────────────────────── */
#define GRID_WIDTH 60
#define GRID_HEIGHT 20
#define CELL_SIZE 20
#define HEADER_ROWS 3
#define MAX_SNAKE 1200 /* GRID_WIDTH * GRID_HEIGHT */

/* Window pixel dimensions */
#define WINDOW_WIDTH (GRID_WIDTH * CELL_SIZE)
#define WINDOW_HEIGHT ((GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE)

/* ─── Backbuffer ─────────────────────────────────────────────── */

/* The game renders into this CPU pixel buffer.
 * Platforms upload it to the GPU each frame — no platform drawing API
 * is ever called from game code. */
typedef struct {
  uint32_t *pixels; /* 0xAARRGGBB packed pixel data */
  int width;
  int height;
} SnakeBackbuffer;

/* ─── Color System ───────────────────────────────────────────── */

/* Pack (r, g, b, a) into a single uint32_t: 0xAARRGGBB.
 * Alpha is the highest byte; blue is the lowest. */
#define GAME_RGBA(r, g, b, a)                                                  \
  (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) |      \
   (uint32_t)(b))

#define GAME_RGB(r, g, b) GAME_RGBA(r, g, b, 0xFF)

/* Predefined colors */
#define COLOR_BLACK GAME_RGB(0, 0, 0)
#define COLOR_WHITE GAME_RGB(255, 255, 255)
#define COLOR_RED GAME_RGB(220, 50, 50)
#define COLOR_DARK_RED GAME_RGB(139, 0, 0)
#define COLOR_GREEN GAME_RGB(50, 205, 50)
#define COLOR_YELLOW GAME_RGB(255, 215, 0)
#define COLOR_DARK_GRAY GAME_RGB(64, 64, 64)
#define COLOR_GRAY GAME_RGB(128, 128, 128)

/* ─── Direction Enum ─────────────────────────────────────────── */

/* Typed enum prevents passing a raw int where a direction is expected.
 * Arithmetic still works: (dir + 1) % 4 = turn right (CW).
 *                         (dir + 3) % 4 = turn left (CCW). */
typedef enum {
  SNAKE_DIR_UP = 0,
  SNAKE_DIR_RIGHT = 1,
  SNAKE_DIR_DOWN = 2,
  SNAKE_DIR_LEFT = 3
} SNAKE_DIR;

/* ─── Segment ────────────────────────────────────────────────── */
typedef struct {
  int x, y;
} Segment;

/* ─── Input System ───────────────────────────────────────────── */

/* GameButtonState tracks both the current state AND transitions within a frame.
 * Origin: Casey Muratori's Handmade Hero.
 *
 * ended_down:            1 = key is pressed now; 0 = key is released.
 * half_transition_count: number of state changes this frame.
 *   0 = no change (held or idle)
 *   1 = normal press or release
 *   2 = full tap (pressed + released within one frame)
 *
 * "Just pressed this frame" check:
 *   button.ended_down && button.half_transition_count > 0 */
typedef struct {
  int half_transition_count;
  int ended_down;
} GameButtonState;

/* Call on KeyPress: UPDATE_BUTTON(btn, 1)
 * Call on KeyRelease: UPDATE_BUTTON(btn, 0)
 * Only increments half_transition_count when state actually changes. */
#define UPDATE_BUTTON(button, is_down)                                         \
  do {                                                                         \
    if ((button).ended_down != (is_down)) {                                    \
      (button).half_transition_count++;                                        \
      (button).ended_down = (is_down);                                         \
    }                                                                          \
  } while (0)

/* Turn actions fire once per press — no auto-repeat needed for snake. */
typedef struct {
  GameButtonState turn_left;  /* CCW: Left arrow or A */
  GameButtonState turn_right; /* CW:  Right arrow or D */
  int restart;                /* R or Space — fires once per press */
  int quit;                   /* Q or Escape */
} GameInput;

/* ─── GameState (ring buffer) ────────────────────────────────── */

typedef struct {
  /* Ring buffer — segments[MAX_SNAKE] holds all body positions.
   * head: index of the most recently written head segment.
   * tail: index of the oldest (tail-tip) segment.
   * length: number of valid segments currently in the buffer. */
  Segment segments[MAX_SNAKE];
  int head;
  int tail;
  int length;

  SNAKE_DIR direction; /* current moving direction */
  SNAKE_DIR
  next_direction; /* direction queued by input (applied on next move) */

  /* Delta-time based movement timing.
   * move_timer accumulates delta_time each frame.
   * When move_timer >= move_interval, the snake moves and move_timer wraps. */
  float move_timer;
  float move_interval; /* seconds per step; decreases as score grows */

  int grow_pending; /* segments still to be added before tail advances */
  int food_x;
  int food_y;
  int score;
  int best_score; /* preserved across snake_init calls */
  int game_over;
} GameState;

/* ─── Public Game API ────────────────────────────────────────── */

/* Reset half_transition_count on all buttons — call once per frame
 * BEFORE platform_get_input so counts accumulate fresh each frame. */
void prepare_input_frame(GameInput *input);

/* Initialize (or reset) the game. Preserves best_score. */
void snake_init(GameState *s);

/* Place food at a random position not occupied by the snake. */
void snake_spawn_food(GameState *s);

/* Advance the game by delta_time seconds.
 * Applies input, accumulates move_timer, moves snake when interval fires. */
void snake_update(GameState *s, const GameInput *input, float delta_time);

/* Render the full game frame into backbuffer. Platform-independent. */
void snake_render(const GameState *s, SnakeBackbuffer *backbuffer);

#endif /* SNAKE_H */
