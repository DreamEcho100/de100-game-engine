#include "../../../game.h"
#include "../../_common/input.h"
#include "../audio.h"
#include <stdio.h>

void handle_keyboard_inputs(GameSoundOutput *sound_output,
                            GameInput *new_game_input) {
  GameControllerInput *new_controller1 =
      &new_game_input->controllers[KEYBOARD_CONTROLLER_INDEX];

  int upDown = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
  if (upDown) {
    // W/Up = Move up = positive Y
    new_controller1->end_y = +1.0f;
    new_controller1->min_y = new_controller1->max_y = new_controller1->end_y;
    new_controller1->is_analog = false;

    // Also set button state for backward compatibility
    process_game_button_state(true, &new_controller1->up);
  }
  int upReleased = IsKeyReleased(KEY_W) || IsKeyReleased(KEY_UP);
  if (upReleased) {
    new_controller1->end_y = 0.0f;
    new_controller1->min_y = new_controller1->max_y = 0.0f;
    new_controller1->is_analog = false;
    process_game_button_state(false, &new_controller1->up);
  }

  int left = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
  if (left) {
    // A/Left = Move left = negative X
    new_controller1->end_x = -1.0f;
    new_controller1->min_x = new_controller1->max_x = new_controller1->end_x;
    new_controller1->is_analog = false;

    process_game_button_state(true, &new_controller1->left);
  }
  int leftReleased = IsKeyReleased(KEY_A) || IsKeyReleased(KEY_LEFT);
  if (leftReleased) {
    new_controller1->end_x = 0.0f;
    new_controller1->min_x = new_controller1->max_x = 0.0f;
    new_controller1->is_analog = false;
    process_game_button_state(false, &new_controller1->left);
  }

  int down = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
  if (down) {
    // S/Down = Move down = negative Y
    new_controller1->end_y = -1.0f;
    new_controller1->min_y = new_controller1->max_y = new_controller1->end_y;
    new_controller1->is_analog = false;

    process_game_button_state(true, &new_controller1->down);
  }
  int downReleased = IsKeyReleased(KEY_S) || IsKeyReleased(KEY_DOWN);
  if (downReleased) {
    new_controller1->end_y = 0.0f;
    new_controller1->min_y = new_controller1->max_y = 0.0f;
    new_controller1->is_analog = false;
    process_game_button_state(false, &new_controller1->down);
  }

  int right = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
  if (right) {
    // D/Right = Move right = positive X
    new_controller1->end_x = +1.0f;
    new_controller1->min_x = new_controller1->max_x = new_controller1->end_x;
    new_controller1->is_analog = false;

    process_game_button_state(true, &new_controller1->right);
  }
  int rightReleased = IsKeyReleased(KEY_D) || IsKeyReleased(KEY_RIGHT);
  if (rightReleased) {
    new_controller1->end_x = 0.0f;
    new_controller1->min_x = new_controller1->max_x = 0.0f;
    new_controller1->is_analog = false;
    process_game_button_state(false, &new_controller1->right);
  }

  if (IsKeyPressed(KEY_ESCAPE)) {
    printf("ESCAPE pressed - exiting\n");
    is_game_running = false;
  }

  if (IsKeyPressed(KEY_F1)) {
    printf("F1 pressed - showing audio debug\n");
    raylib_debug_audio(sound_output);
  }
}
