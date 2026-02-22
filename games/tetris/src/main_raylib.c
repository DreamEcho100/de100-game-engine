#include "base.h"

#include <raylib.h>

static void draw_cell(int col, int row, Color color) {
  int x = col * CELL_SIZE + 1;
  int y = row * CELL_SIZE + 1;
  int w = CELL_SIZE - 1;
  int h = CELL_SIZE - 1;

  DrawRectangle(x, y, w, h, color);
}

static void draw_field_boundary(Color wall_color) {
  int col, row;

  /* Left wall: column 0, all rows */
  for (row = 0; row < FIELD_HEIGHT; ++row) {
    draw_cell(0, row, wall_color);
  }

  /* Right wall: column 11, all rows */
  for (row = 0; row < FIELD_HEIGHT; ++row) {
    draw_cell(FIELD_WIDTH - 1, row, wall_color);
  }

  /* Floor: row 17, all columns */
  for (col = 0; col < FIELD_WIDTH; ++col) {
    draw_cell(col, FIELD_HEIGHT - 1, wall_color);
  }
}

static void draw_piece(int piece_index, int field_col, int field_row,
                       Color color) {
  int px, py;

  for (py = 0; py < TETROMINO_LAYER_COUNT; py++) {
    for (px = 0; px < TETROMINO_LAYER_COUNT; px++) {
      int pi = py * TETROMINO_LAYER_COUNT + px;

      if (TETROMINOES[piece_index][pi] == TETROMINO_BLOCK) {
        draw_cell(field_col + px, field_row + py, color);
      }
    }
  }
}

int main(void) {
  InitWindow(FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE, "Tetris");
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);

    draw_field_boundary(GRAY);
    // TETROMINO_I_IDX,
    draw_piece(0, 0, 0, (Color){0, 255, 255, .a = 255}); // Cyan color
    // TETROMINO_J_IDX,
    draw_piece(TETROMINO_J_IDX, 1, 2, BLUE);
    // TETROMINO_L_IDX,
    draw_piece(TETROMINO_L_IDX, 3, 0, ORANGE);
    // TETROMINO_O_IDX,
    draw_piece(TETROMINO_O_IDX, 3, 2, YELLOW);
    // TETROMINO_S_IDX,
    draw_piece(TETROMINO_S_IDX, 4, 0, GREEN);
    // TETROMINO_T_IDX,
    draw_piece(TETROMINO_T_IDX, 5, 2, MAGENTA);
    // TETROMINO_Z_IDX,
    draw_piece(TETROMINO_Z_IDX, 7, 0, RED);

    EndDrawing();
  }

  CloseWindow();
  return 0;
}
