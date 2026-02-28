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

void platform_get_input(GameInput *input) {
  /* Quit */
  if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE)) {
    input->quit = 1;
  }

  /* Restart */
  if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_SPACE)) {
    input->restart = 1;
  }

  /* Turn left (CCW): just-pressed maps to half_transition_count=1, ended_down=1
   * Turn keys fire once per press — IsKeyPressed returns true only on the
   * frame the key goes down, which matches the "just pressed" behavior. */
  if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
    UPDATE_BUTTON(input->turn_left, 1);
  } else if (IsKeyReleased(KEY_LEFT) || IsKeyReleased(KEY_A)) {
    UPDATE_BUTTON(input->turn_left, 0);
  }

  if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
    UPDATE_BUTTON(input->turn_right, 1);
  } else if (IsKeyReleased(KEY_RIGHT) || IsKeyReleased(KEY_D)) {
    UPDATE_BUTTON(input->turn_right, 0);
  }
}

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

  GameInput game_input = {};
  GameState game_state = {};
  game_init(&game_state);

  while (!WindowShouldClose()) {
    float delta_time = GetFrameTime();

    /* ═══════════════════════════════════════════════════════════════════
     * Input
     * ═══════════════════════════════════════════════════════════════════ */
    prepare_input_frame(&game_input);
    platform_get_input(&game_input);

    /* Quit */
    if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE)) {
      platform_game_props.is_running = 0;
      WindowShouldClose(); // Signal Raylib to close the window
      break;
    }

    game_update(&game_state, &game_input, delta_time);
    game_render(&game_state, &platform_game_props.backbuffer);

    /* Upload CPU backbuffer to GPU texture, then draw it */
    UpdateTexture(g_raylib.texture, platform_game_props.backbuffer.pixels);

    BeginDrawing();
    ClearBackground(RED);
    DrawTexture(g_raylib.texture, 0, 0, WHITE);
    EndDrawing();
  }

  platform_game_props_free(&platform_game_props);
  platform_shutdown();
  return 0;
}
