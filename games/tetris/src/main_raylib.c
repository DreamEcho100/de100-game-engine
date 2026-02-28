#include "game.h"
#include "platform.h"

#include <raylib.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  Texture2D texture;
} RaylibState;

RaylibState g_raylib = {0};

int platform_init(PlatformGameProps *props) {
  InitWindow(props->width, props->height, props->title);
  SetTargetFPS(60);

  /* Create Raylib texture to display backbuffer */
  Image img = {.data = props->backbuffer.pixels,
               .width = props->backbuffer.width,
               .height = props->backbuffer.height,
               .mipmaps = 1,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  g_raylib.texture = LoadTextureFromImage(img);

  return 0;
}

void platform_shutdown(void) {
  UnloadTexture(g_raylib.texture);
  CloseWindow();
}

int main(void) {
  PlatformGameProps platform_game_props = {0};
  if (platform_game_props_init(&platform_game_props) != 0) {
    return 1;
  }

  if (platform_init(&platform_game_props) != 0) {
    return 1;
  }

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
      game_update(&state, &input, delta_time);
    }

    /* ═══════════════════════════════════════════════════════════════════
     * Render - Game draws to backbuffer (platform independent!)
     * ═══════════════════════════════════════════════════════════════════ */
    game_reder(&platform_game_props.backbuffer, &state);

    /* ═══════════════════════════════════════════════════════════════════
     * Display - Platform uploads backbuffer to screen
     * ═════════��═════════════════════════════════════════════════════════ */
    UpdateTexture(g_raylib.texture, platform_game_props.backbuffer.pixels);

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(g_raylib.texture, 0, 0, WHITE);
    EndDrawing();
  }

  platform_game_props_free(&platform_game_props);
  platform_shutdown();
  return 0;
}
