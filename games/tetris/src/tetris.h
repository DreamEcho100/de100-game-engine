#ifndef TETRIS_H
#define TETRIS_H

#include <stdbool.h>

#define FIELD_WIDTH 12
#define FIELD_HEIGHT 18
#define CELL_SIZE 30

#define TETROMINO_SPAN '.'
#define TETROMINO_BLOCK 'X'
#define TETROMINO_LAYER_COUNT 4
#define TETROMINO_SIZE 16
#define TETROMINOS_COUNT 7

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
} TETRIS_FIELD_CELL;

typedef enum {
  TETROMINO_R_0,
  TETROMINO_R_90,
  TETROMINO_R_180,
  TETROMINO_R_270,
} TETROMINO_R_DIR;

typedef struct {
  int col;                    /* starting column */
  int row;                    /* starting row    */
  TETROMINO_BY_IDX index;     /* which tetromino */
  TETROMINO_BY_IDX nextIndex; /* next tetromino */
  TETROMINO_R_DIR rotation;   /* current rotation: 0, 1, 2, or 3 */
} CurrentPiece;
typedef struct {
  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT]; /* the play field */
  CurrentPiece current_piece;
  int score;
  int pieces_count; /* total pieces locked — used for difficulty scaling */
  bool game_over;

  // Replace speed/speed_count with time-based fields
  float drop_timer;    // Accumulates delta time (in seconds)
  float drop_interval; // Time between drops (e.g., 1.0 = 1 second)
} GameState;

/* All the inputs the game cares about.
   platform_get_input() fills this in each frame. */
typedef struct {
  int move_left;  /* 1 if left arrow pressed this frame */
  int move_right; /* 1 if right arrow pressed this frame */
  int move_down;  /* 1 if down arrow pressed this frame */
  int rotate_x;   /* 1 if X/Z pressed this frame */
  // int quit;               /* 1 if window closed or Escape pressed */
} GameInput;

/* ── Piece data ─────────────────────────────────────── */

/* 7 tetrominoes, each a 4×4 grid of '.' (empty) and 'X' (solid).
   Defined in tetris.c — 'extern' tells the compiler
   "trust me, it exists somewhere else." */
extern const char *TETROMINOES[7];

void game_init(GameState *state);

/* ── Function declarations ──────────────────────────── */

/* Returns the index into a 4×4 piece string for rotation r.
   px = column (0–3), py = row (0–3), r = rotation counter. */
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r);

int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                             int pos_x, int pos_y);
void tetris_update(GameState *state, GameInput *input, float delta_time);

#endif // TETRIS_H
