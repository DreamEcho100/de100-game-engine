#include "tetris.h"

#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  int screen_width = (FIELD_WIDTH * CELL_SIZE) + SIDEBAR_WIDTH;
  int screen_height = FIELD_HEIGHT * CELL_SIZE;

  InitWindow(screen_width, screen_height, "Tetris");
  SetTargetFPS(60);

  /* Allocate backbuffer */
  TetrisBackbuffer backbuffer;
  backbuffer.width = screen_width;
  backbuffer.height = screen_height;
  backbuffer.pitch = screen_width * sizeof(uint32_t);
  backbuffer.pixels =
      (uint32_t *)malloc(screen_width * screen_height * sizeof(uint32_t));

  if (!backbuffer.pixels) {
    fprintf(stderr, "Failed to allocate backbuffer\n");
    CloseWindow();
    return 1;
  }

  /* Create Raylib texture to display backbuffer */
  Image img = {.data = backbuffer.pixels,
               .width = screen_width,
               .height = screen_height,
               .mipmaps = 1,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  Texture2D texture = LoadTextureFromImage(img);

  GameInput input = {0};
  GameState state = {0};
  game_init(&state, &input);

  while (!WindowShouldClose()) {
    float delta_time = GetFrameTime();

    /* ═══════════════════════════════════════════════════════════════════
     * Input
     * ═══════════════════════════════════════════════════════════════════ */
    prepare_input_frame(&input);

    /* Quit */
    if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE)) {
      input.quit = 1;
      break;
    }

    /* Restart */
    input.restart = IsKeyPressed(KEY_R);

    /* Rotation */
    if (IsKeyPressed(KEY_X)) {
      UPDATE_BUTTON(input.rotate_x.button, 1);
      input.rotate_x.value = TETROMINO_ROTATE_X_GO_RIGHT;
    } else if (IsKeyPressed(KEY_Z)) {
      UPDATE_BUTTON(input.rotate_x.button, 1);
      input.rotate_x.value = TETROMINO_ROTATE_X_GO_LEFT;
    } else if (IsKeyReleased(KEY_X) || IsKeyReleased(KEY_Z)) {
      UPDATE_BUTTON(input.rotate_x.button, 0);
      input.rotate_x.value = TETROMINO_ROTATE_X_NONE;
    }

    /* Movement */
    UPDATE_BUTTON(input.move_left.button,
                  IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
    UPDATE_BUTTON(input.move_right.button,
                  IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT));
    UPDATE_BUTTON(input.move_down.button,
                  IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN));

    /* ═══════════════════════════════════════════════════════════════════
     * Update
     * ═══════════════════════════════════════════════════════════════════ */
    if (state.game_over && input.restart) {
      game_init(&state, &input);
    } else {
      tetris_update(&state, &input, delta_time);
    }

    /* ═══════════════════════════════════════════════════════════════════
     * Render - Game draws to backbuffer (platform independent!)
     * ═══════════════════════════════════════════════════════════════════ */
    tetris_render(&backbuffer, &state);

    /* ═══════════════════════════════════════════════════════════════════
     * Display - Platform uploads backbuffer to screen
     * ═══════════════════════════════════════════════════════════════════ */
    UpdateTexture(texture, backbuffer.pixels);

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(texture, 0, 0, WHITE);
    EndDrawing();
  }

  UnloadTexture(texture);
  free(backbuffer.pixels);
  CloseWindow();
  return 0;
}
