#ifndef TETRIS_H
#define TETRIS_H

#include <stdbool.h>
#include <stdint.h>

#define FIELD_WIDTH 12
#define FIELD_HEIGHT 18
#define CELL_SIZE 30

#define SIDEBAR_WIDTH 200 /* extra pixels to the right of the field */

#define TETROMINO_SPAN '.'
#define TETROMINO_BLOCK 'X'
#define TETROMINO_LAYER_COUNT 4
#define TETROMINO_SIZE 16
#define TETROMINOS_COUNT 7

/* ═══════════════════════════════════════════════════════════════════════════
 * Backbuffer - Platform Independent Rendering Target
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  uint32_t *pixels; /* RGBA pixel data (0xAARRGGBB format) */
  int width;
  int height;
  int pitch; /* Bytes per row (usually width * 4) */
} TetrisBackbuffer;
/* Color helper - pack RGBA into uint32 */
#define TETRIS_RGBA(r, g, b, a)                                                \
  (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) |      \
   (uint32_t)(b))

#define TETRIS_RGB(r, g, b) TETRIS_RGBA(r, g, b, 255)

/* Predefined colors */
#define COLOR_BLACK TETRIS_RGB(0, 0, 0)
#define COLOR_WHITE TETRIS_RGB(255, 255, 255)
#define COLOR_GRAY TETRIS_RGB(128, 128, 128)
#define COLOR_DARK_GRAY TETRIS_RGB(64, 64, 64)
#define COLOR_CYAN TETRIS_RGB(0, 255, 255)
#define COLOR_BLUE TETRIS_RGB(0, 0, 255)
#define COLOR_ORANGE TETRIS_RGB(255, 165, 0)
#define COLOR_YELLOW TETRIS_RGB(255, 255, 0)
#define COLOR_GREEN TETRIS_RGB(0, 255, 0)
#define COLOR_MAGENTA TETRIS_RGB(255, 0, 255)
#define COLOR_RED TETRIS_RGB(255, 0, 0)

/* ═══════════════════════════════════════════════════════════════════════════
 * Tetromino Types
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef enum {
  TETROMINO_I_IDX,
  TETROMINO_J_IDX,
  TETROMINO_L_IDX,
  TETROMINO_O_IDX,
  TETROMINO_S_IDX,
  TETROMINO_T_IDX,
  TETROMINO_Z_IDX,
} TETROMINO_BY_IDX;

typedef enum {
  TETRIS_FIELD_EMPTY,
  TETRIS_FIELD_I,
  TETRIS_FIELD_J,
  TETRIS_FIELD_L,
  TETRIS_FIELD_O,
  TETRIS_FIELD_S,
  TETRIS_FIELD_T,
  TETRIS_FIELD_Z,
  TETRIS_FIELD_WALL,
  TETRIS_FIELD_TMP_FLASH, /* Used to mark completed lines for flashing effect */
} TETRIS_FIELD_CELL;

typedef enum {
  TETROMINO_R_0,
  TETROMINO_R_90,
  TETROMINO_R_180,
  TETROMINO_R_270,
} TETROMINO_R_DIR;

typedef struct {
  /** Accumulates delta time while button is held */
  float timer;

  /** Time between auto-repeats (e.g., 0.1 = 100ms) */
  float interval;
} GameActionRepeat;

typedef struct {
  int x;                       /* starting column */
  int y;                       /* starting row    */
  TETROMINO_BY_IDX index;      /* which tetromino */
  TETROMINO_BY_IDX next_index; /* next tetromino */
  TETROMINO_R_DIR rotation;    /* current rotation: 0, 1, 2, or 3 */
} CurrentPiece;
typedef struct {
  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT]; /* the play field */
  CurrentPiece current_piece;
  int score;
  int pieces_count; /* total pieces locked — used for difficulty scaling */
  bool game_over;
  int level;

  struct {
    int indexes[TETROMINO_LAYER_COUNT]; /* row indices of completed lines this
                                           lock */
    int count;                    /* how many entries in lines[] are valid */
    GameActionRepeat flash_timer; /* countdown: while > 0, game is paused
                                        showing white flash */
  } completed_lines;

  // Replace speed/speed_count with time-based fields
  GameActionRepeat tetromino_drop;
} GameState;

/* ── Input System ─────────────────────────────────────── */

typedef struct {
  /**
   * Number of state changes this frame.
   * 0 = No change (button held or released entire frame)
   * 1 = Changed once (normal press or release)
   * 2 = Changed twice (pressed then released, or vice versa)
   *
   * Why track this? Consider a 30fps game where the user taps a button
   * for 20ms. At 30fps, each frame is 33ms. The button went down AND up
   * within one frame — half_transition_count = 2.
   *
   * For gameplay, we usually care about: "Did the button GO DOWN this frame?"
   * That's true if: ended_down == 1 AND half_transition_count > 0
   */
  int half_transition_count;

  /**
   * Final state at end of frame.
   * 1 = button is currently pressed
   * 0 = button is currently released
   */
  int ended_down;
} GameButtonState;

/**
 * Action with auto-repeat capability.
 * Used for: move_left, move_right, move_down
 *
 * Behavior:
 * - First press: triggers immediately
 * - Hold: triggers again every repeat_interval seconds
 * - Release: stops triggering, resets timer
 */
typedef struct {
  GameButtonState button;
  GameActionRepeat repeat;
} GameActionWithRepeat;

// /**
//  * Action with a directional value.
//  * Used for: rotation
//  *
//  * Behavior:
//  * - Press X: value = 1 (clockwise)
//  * - Press Z: value = -1 (counter-clockwise)
//  * - No auto-repeat — only triggers on initial press
//  */
// typedef struct {
//   GameButtonState button;

//   /**
//    * Direction or magnitude for this action.
//    * For rotation: 1 = clockwise, -1 = counter-clockwise, 0 = no rotation
//    */
//   int value;
// } GameActionWithValue;

typedef enum {
  TETROMINO_ROTATE_X_NONE,
  TETROMINO_ROTATE_X_GO_LEFT,
  TETROMINO_ROTATE_X_GO_RIGHT,
} TETROMINO_ROTATE_X_VALUE;

/* All the inputs the game cares about.
   platform_get_input() fills this in each frame. */
typedef struct {
  GameActionWithRepeat move_left;  /* 1 if left arrow pressed this frame */
  GameActionWithRepeat move_right; /* 1 if right arrow pressed this frame */
  GameActionWithRepeat move_down;  /* 1 if down arrow pressed this frame */
  struct {
    GameButtonState button;

    /**
     * Direction or magnitude for this action.
     * For rotation: 1 = clockwise, -1 = counter-clockwise, 0 = no rotation
     */
    TETROMINO_ROTATE_X_VALUE value;
  } rotate_x;  /* 1 if X/Z pressed this frame */
  int quit;    /* 1 if window closed or Escape pressed */
  int restart; /* 1 if R pressed this frame */
} GameInput;

/* Helper macro to update button state.
 * Tracks transitions and final state.
 *
 * Usage: UPDATE_BUTTON(input->move_left.button, is_key_down);
 */
#define UPDATE_BUTTON(button, is_down)                                         \
  do {                                                                         \
    if ((button).ended_down != (is_down)) {                                    \
      (button).half_transition_count++;                                        \
      (button).ended_down = (is_down);                                         \
    }                                                                          \
  } while (0)

void prepare_input_frame(GameInput *input);

/* ── Piece data ─────────────────────────────────────── */

/* 7 tetrominoes, each a 4×4 grid of '.' (empty) and 'X' (solid).
   Defined in tetris.c — 'extern' tells the compiler
   "trust me, it exists somewhere else." */
extern const char *TETROMINOES[7];

/* Initialization */
void game_init(GameState *state, GameInput *input);
void prepare_input_frame(GameInput *input);

/* Game Logic */
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r);
int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                             int pos_x, int pos_y);
void tetris_update(GameState *state, GameInput *input, float delta_time);

/* Rendering - Platform Independent! */
void tetris_render(TetrisBackbuffer *backbuffer, GameState *state);

/* Drawing Primitives (used by tetris_render, but can be used by platform too)
 */
void draw_rect(TetrisBackbuffer *bb, int x, int y, int w, int h,
               uint32_t color);
void draw_text(TetrisBackbuffer *bb, int x, int y, const char *text,
               uint32_t color, int scale);

#endif // TETRIS_H
