#ifndef TETRIS_BASE_H
#define TETRIS_BASE_H

#define FIELD_WIDTH 12
#define FIELD_HEIGHT 18
#define CELL_SIZE 30

#define TETROMINO_SPAN '.'
#define TETROMINO_BLOCK 'X'
#define TETROMINO_LAYER_COUNT 4
#define TETROMINO_SIZE 16
#define TETROMINOS_COUNT 7

const char TETROMINO_I[TETROMINO_SIZE] =
    "..X...X...X...X."; /* 0: I — vertical bar       */
const char TETROMINO_J[TETROMINO_SIZE] =
    "..X...X..XX....."; /* 5: J — J-shape (mirror L) */
const char TETROMINO_L[TETROMINO_SIZE] =
    ".X...X...XX....."; /* 4: L — L-shape            */
const char TETROMINO_O[TETROMINO_SIZE] =
    ".....XX..XX....."; /* 6: O — square (uses 3×3) */
const char TETROMINO_S[TETROMINO_SIZE] =
    ".X...XX...X....."; /* 1: S — S-shape            */
const char TETROMINO_T[TETROMINO_SIZE] =
    "..X..XX...X....."; /* 3: T — T-shape            */
const char TETROMINO_Z[TETROMINO_SIZE] =
    "..X..XX..X......"; /* 2: Z — Z-shape            */

const char *TETROMINOES[TETROMINOS_COUNT] = {
    TETROMINO_I, //
    TETROMINO_J, //
    TETROMINO_L, //
    TETROMINO_O, //
    TETROMINO_S, //
    TETROMINO_T, //
    TETROMINO_Z, //
};

typedef enum {
  TETROMINO_I_IDX,
  TETROMINO_J_IDX,
  TETROMINO_L_IDX,
  TETROMINO_O_IDX,
  TETROMINO_S_IDX,
  TETROMINO_T_IDX,
  TETROMINO_Z_IDX,
} TETROMINO_BY_IDX;

#endif // TETRIS_BASE_H