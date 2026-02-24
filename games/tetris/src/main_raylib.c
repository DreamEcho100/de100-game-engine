#include "platform.h"
#include "tetris.h"

#include <raylib.h>
#include <stdio.h>

static void draw_cell(int col, int row, Color color) {
  int x = col * CELL_SIZE + 1;
  int y = row * CELL_SIZE + 1;
  int w = CELL_SIZE - 1;
  int h = CELL_SIZE - 1;

  DrawRectangle(x, y, w, h, color);
}

static Color get_tetromino_color_by_index(TETROMINO_BY_IDX piece_index) {
  switch (piece_index) {
  case TETROMINO_I_IDX: {
    return (Color){0, 255, 255, 255}; // Cyan color for I pieces
  }
  case TETROMINO_J_IDX: {
    return (Color){0, 0, 255, 255}; // Blue color for J pieces
  }
  case TETROMINO_L_IDX: {
    return (Color){255, 165, 0, 255}; // Orange color for L pieces
  }
  case TETROMINO_O_IDX: {
    return (Color){255, 255, 0, 255}; // Yellow color for O pieces
  }
  case TETROMINO_S_IDX: {
    return (Color){0, 255, 0, 255}; // Green color for S pieces
  }
  case TETROMINO_T_IDX: {
    return (Color){128, 0, 128, 255}; // Purple color for T pieces
  }
  case TETROMINO_Z_IDX: {
    return (Color){255, 0, 0, 255}; // Red color for Z pieces
  }
  default: {
    // Impossible, but just in case, return gray for invalid piece index
    printf(
        "Warning: Invalid piece index %d in get_tetromino_color_by_index()\n",
        piece_index);
    return (Color){128, 128, 128, 255};
  }
  }
}

static void draw_piece(int piece_index, int field_col, int field_row,
                       Color color, TETROMINO_R_DIR rotation) {
  int px, py;

  for (py = 0; py < TETROMINO_LAYER_COUNT; py++) {
    for (px = 0; px < TETROMINO_LAYER_COUNT; px++) {
      int pi = tetromino_pos_value(px, py, rotation);

      if (TETROMINOES[piece_index][pi] == TETROMINO_BLOCK) {
        draw_cell(field_col + px, field_row + py, color);
      }
    }
  }
}

int platform_init(const char *title, int width, int height) {
  InitWindow(width, height, title);
  SetTargetFPS(60);
  return 0;
}
void platform_get_input(GameState *state, GameInput *input) {
  input->restart = IsKeyPressed(KEY_R);
  input->quit = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q);

  /* Rotation — set value based on which key */
  if (IsKeyDown(KEY_X)) {
    UPDATE_BUTTON(input->rotate_x.button, 1);
    input->rotate_x.value = TETROMINO_ROTATE_X_GO_RIGHT; /* Clockwise */
  } else if (IsKeyDown(KEY_Z)) {
    UPDATE_BUTTON(input->rotate_x.button, 1);
    input->rotate_x.value = TETROMINO_ROTATE_X_GO_LEFT; /* Counter-clockwise */
  } else {
    UPDATE_BUTTON(input->rotate_x.button, 0);
    input->rotate_x.value = TETROMINO_ROTATE_X_NONE; /* No rotation */
  }

  /* Movement — can be held */
  UPDATE_BUTTON(input->move_left.button,
                IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
  UPDATE_BUTTON(input->move_right.button,
                IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT));
  UPDATE_BUTTON(input->move_down.button,
                IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN));
}
void platform_render(GameState *state) {
  BeginDrawing();
  ClearBackground(BLACK);

  for (int row = 0; row < FIELD_HEIGHT; row++) {
    for (int col = 0; col < FIELD_WIDTH; col++) {
      unsigned char cell_value = state->field[row * FIELD_WIDTH + col];

      switch (cell_value) {
      case TETRIS_FIELD_EMPTY: {
        continue; // Skip empty cells
      }

      case TETRIS_FIELD_I:
      case TETRIS_FIELD_J:
      case TETRIS_FIELD_L:
      case TETRIS_FIELD_O:
      case TETRIS_FIELD_S:
      case TETRIS_FIELD_T:
      case TETRIS_FIELD_Z: {
        draw_cell(
            col, row,
            get_tetromino_color_by_index(
                cell_value - 1)); // Draw pieces based on their index (subtract
                                  // 1 because field values are piece index + 1)
        break;
      }

      case TETRIS_FIELD_WALL: {
        draw_cell(col, row, (Color){128, 128, 128, 255}); // Draw walls in gray
        break;
      }

      case TETRIS_FIELD_TMP_FLASH: {
        draw_cell(col, row, (Color){255, 255, 255, 255});
        break;
      }
      }
    }
  }

  draw_piece(state->current_piece.index, state->current_piece.x,
             state->current_piece.y,
             get_tetromino_color_by_index(state->current_piece.index),
             state->current_piece.rotation);

  /* ── HUD SIDEBAR ─────────────────────────────────────────────────── */
  int sx = FIELD_WIDTH * CELL_SIZE + 10;
  char buf[64];

  DrawText("SCORE", sx, 10, 16, WHITE);
  snprintf(buf, sizeof(buf), "%d", state->score);
  DrawText(buf, sx, 30, 20, YELLOW);

  int level = state->level;
  DrawText("LEVEL", sx, 60, 16, WHITE);
  snprintf(buf, sizeof(buf), "%d", level);
  DrawText(buf, sx, 80, 20, GREEN);

  int pieces_count = state->pieces_count;
  DrawText("PIECES", sx + 100, 60, 16, WHITE);
  snprintf(buf, sizeof(buf), "%d", pieces_count);
  DrawText(buf, sx + 100, 80, 20, GREEN);

  DrawText("NEXT", sx, 115, 16, WHITE);
  int px, py;
  int prev_col = FIELD_WIDTH + 1;
  int prev_row = 5;
  for (px = 0; px < 4; px++) {
    for (py = 0; py < 4; py++) {
      int pi = tetromino_pos_value(px, py, 0);
      if (TETROMINOES[state->current_piece.next_index][pi] != TETROMINO_SPAN)
        draw_cell(
            prev_col + px, prev_row + py,
            get_tetromino_color_by_index(state->current_piece.next_index));
    }
  }

  /* Controls hint */
  int WINDOW_HEIGHT = FIELD_HEIGHT * CELL_SIZE;
  DrawText("Controls:", sx, WINDOW_HEIGHT - 100, 12, GRAY);
  DrawText("← →    Move", sx, WINDOW_HEIGHT - 84, 12, GRAY);
  DrawText("↓      Drop", sx, WINDOW_HEIGHT - 68, 12, GRAY);
  DrawText("Z/X  Rotate", sx, WINDOW_HEIGHT - 52, 12, GRAY);
  DrawText("R   Restart", sx, WINDOW_HEIGHT - 36, 12, GRAY);
  DrawText("Q/Esc  Quit", sx, WINDOW_HEIGHT - 20, 12, GRAY);

  /* ── Game over overlay ── */
  if (state->game_over) {
    int cx = FIELD_WIDTH * CELL_SIZE / 2;
    int cy = FIELD_HEIGHT * CELL_SIZE / 2;
    DrawRectangle(cx - 70, cy - 36, 140, 72, (Color){0, 0, 0, 200});
    DrawText("GAME OVER", cx - 52, cy - 22, 24, RED);
    DrawText("R = Restart", cx - 46, cy + 4, 14, WHITE);
    DrawText("Q/Esc = Quit", cx - 46, cy + 22, 14, WHITE);
  }

  EndDrawing();
}
void platform_shutdown(void) { CloseWindow(); }

int main(void) {

  int screen_width = (FIELD_WIDTH * CELL_SIZE) + SIDEBAR_WIDTH;
  int screen_height = FIELD_HEIGHT * CELL_SIZE;
  if (platform_init("Tetris", screen_width, screen_height) != 0) {
    return 1;
  }

  GameInput game_input = {0};
  GameState game_state = {0};
  game_init(&game_state, &game_input);

  while (!WindowShouldClose()) {
    float delta_time = GetFrameTime();

    prepare_input_frame(&game_input);
    platform_get_input(&game_state, &game_input);

    if (game_input.quit) {
      break;
    }

    if (game_state.game_over && game_input.restart) {
      game_init(&game_state, &game_input);
    } else {
      tetris_update(&game_state, &game_input, delta_time);
    }

    platform_render(&game_state);
  }

  platform_shutdown();
  return 0;
}
