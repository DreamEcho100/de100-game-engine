#ifndef GAME_SNAKE_H
#define GAME_SNAKE_H

#include "utils/backbuffer.h"
#include <stdbool.h>
#include <stdint.h>

#define GRID_WIDTH 60
#define GRID_HEIGHT 20
#define CELL_SIZE 14
#define HEADER_ROWS 3

#define SCREEN_INITIAL_WIDTH (GRID_WIDTH * CELL_SIZE)
#define SCREEN_INITIAL_HEIGHT ((GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE)
#define MAX_SNAKE (GRID_WIDTH * GRID_HEIGHT)

/* Typed enum prevents passing a raw int where a direction is expected.
 * Arithmetic still works: (dir + 1) % 4 = turn right (CW).
 *                         (dir + 3) % 4 = turn left (CCW). */
typedef enum {
  SNAKE_DIR_UP = 0,
  SNAKE_DIR_RIGHT = 1,
  SNAKE_DIR_DOWN = 2,
  SNAKE_DIR_LEFT = 3
} SNAKE_DIR;

/* direction: 0=UP, 1=RIGHT, 2=DOWN, 3=LEFT */
#define DIRECTION_SNAKE_SIZE 4
static const int DIRECTION_SNAKE_X[DIRECTION_SNAKE_SIZE] = {0, 1, 0, -1};
static const int DIRECTION_SNAKE_Y[DIRECTION_SNAKE_SIZE] = {-1, 0, 1, 0};

typedef struct {
  /** Accumulates delta time while button is held */
  float timer;

  /** Time between auto-repeats (e.g., 0.1 = 100ms) */
  float interval;
} GameActionRepeat;

#define COLOR_BLACK GAME_RGB(0, 0, 0)
#define COLOR_WHITE GAME_RGB(255, 255, 255)
#define COLOR_RED GAME_RGB(220, 50, 50)
#define COLOR_DARK_RED GAME_RGB(139, 0, 0)
#define COLOR_LIME_GREEN GAME_RGB(50, 205, 50)
#define COLOR_DARK_GREEN GAME_RGB(0, 128, 0)
#define COLOR_YELLOW GAME_RGB(255, 215, 0)
#define COLOR_DARK_GRAY GAME_RGB(64, 64, 64)
#define COLOR_GRAY GAME_RGB(128, 128, 128)

typedef struct {
  int x;
  int y;
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

typedef struct {
  GameButtonState turn_left;  /* CCW: Left arrow or A */
  GameButtonState turn_right; /* CW:  Right arrow or D */
  int restart;                /* R or Space — fires once per press */
  int quit;                   /* Q or Escape */
} GameInput;

typedef struct {
  Segment segments[MAX_SNAKE];
  int head;   /* index of the head segment */
  int tail;   /* index of the tail segment */
  int length; /* number of active segments */

  SNAKE_DIR direction;
  SNAKE_DIR next_direction;
  int speed; // TODO: is it needed?!!

  GameActionRepeat
      move_repeat; /* auto-repeat for moving forward (always active) */
} Snake;

typedef struct {
  Snake snake;
  bool is_game_over;
  int score;
  int best_score;
} GameState;

void prepare_input_frame(GameInput *input);

void game_init(GameState *game_state);
void game_update(GameState *game_state, GameInput *input, float delta_time);
void game_render(GameState *game_state, Backbuffer *backbuffer);

#endif // GAME_SNAKE_H
