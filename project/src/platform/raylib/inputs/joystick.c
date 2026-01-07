#include "../../../base.h"
#include "../../../game.h"
#include "../../_common/input.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  int gamepad_id;        // Which Raylib gamepad ID (0-3)
  char device_name[128]; // For debugging
} RaylibJoystickState;

file_scoped_global_var RaylibJoystickState g_joysticks[MAX_JOYSTICK_COUNT] = {
    0};

void raylib_init_gamepad(GameControllerInput *controller_old_input,
                         GameControllerInput *controller_new_input) {

  // Initialize ALL controllers FIRST
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    if (i == KEYBOARD_CONTROLLER_INDEX)
      continue;

    controller_old_input[i].controller_index = i;
    controller_old_input[i].is_connected = false;

    controller_new_input[i].controller_index = i;
    controller_new_input[i].is_connected = false;

    int g_joysticks_index = i - 1; // Adjust for keyboard at index 0
    if (g_joysticks_index >= 0 && g_joysticks_index < MAX_JOYSTICK_COUNT) {
      g_joysticks[g_joysticks_index].gamepad_id = -1;
      memset(g_joysticks[g_joysticks_index].device_name, 0,
             sizeof(g_joysticks[g_joysticks_index].device_name));
    }
  }

  // Mark keyboard (slot 0) as connected AFTER the loop
  controller_old_input[KEYBOARD_CONTROLLER_INDEX].is_connected = true;
  controller_old_input[KEYBOARD_CONTROLLER_INDEX].is_analog = false;

  controller_new_input[KEYBOARD_CONTROLLER_INDEX].is_connected = true;
  controller_new_input[KEYBOARD_CONTROLLER_INDEX].is_analog = false;

  printf("Searching for gamepad...\n");

  // Try gamepads 0-3 (Raylib supports up to 4 controllers)
  for (int i = MAX_KEYBOARD_COUNT; i < MAX_JOYSTICK_COUNT; i++) {

    if (IsGamepadAvailable(i - MAX_KEYBOARD_COUNT)) {
      const char *name = GetGamepadName(i);
      printf("✅ Gamepad %d connected: %s\n", i, name);

      int raylib_gamepad_id = i - MAX_KEYBOARD_COUNT;

      // Store controller index AND file descriptor separately
      controller_old_input[i].controller_index = i;
      controller_old_input[i].is_connected = true;
      controller_old_input[i].is_analog = true; // Joysticks are analog

      controller_new_input[i].controller_index = i;
      controller_new_input[i].is_connected = true;
      controller_new_input[i].is_analog = true; // Joysticks are analog

      int g_joysticks_index =
          i - MAX_KEYBOARD_COUNT; // Adjust for keyboard at index 0

      if (g_joysticks_index >= 0 && g_joysticks_index < MAX_JOYSTICK_COUNT) {
        g_joysticks[g_joysticks_index].gamepad_id = raylib_gamepad_id;
        strncpy(g_joysticks[g_joysticks_index].device_name, name,
                sizeof(g_joysticks[g_joysticks_index].device_name) - 1);
      }
    }
  }
}

void raylib_poll_gamepad(GameInput *old_input, GameInput *new_input) {

  for (int controller_index = MAX_KEYBOARD_COUNT;
       controller_index < MAX_CONTROLLER_COUNT; controller_index++) {

    // Get controller state
    GameControllerInput *old_controller =
        &old_input->controllers[controller_index];
    GameControllerInput *new_controller =
        &new_input->controllers[controller_index];

    int g_joysticks_index = controller_index - MAX_KEYBOARD_COUNT;
    if (g_joysticks_index < 0 || g_joysticks_index >= MAX_JOYSTICK_COUNT) {
      // Skip keyboard (index 0)
      continue;
    }
    RaylibJoystickState *joystick_state = &g_joysticks[g_joysticks_index];

    // Check if gamepad is still connected
    if (joystick_state->gamepad_id < 0 ||
        !IsGamepadAvailable(joystick_state->gamepad_id)) {
      continue;
    }

    int gamepad = joystick_state->gamepad_id;

    // ═══════════════════════════════════════════════════════════
    // 🎮 BUTTON EVENTS (Raylib maps to standard layout)
    // ═══════════════════════════════════════════════════════════
    // Raylib uses SDL2 button mapping (works across all controllers!)
    // - PlayStation: X=A, O=B, □=X, △=Y
    // - Xbox: A=A, B=B, X=X, Y=Y
    // - Nintendo: B=A, A=B, Y=X, X=Y

    // // Face buttons
    // game_state->controls.a_button =
    //     IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    // game_state->controls.b_button =
    //     IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    // game_state->controls.x_button =
    //     IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
    // game_state->controls.y_button =
    //     IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_UP);

    // // Shoulder buttons
    // game_state->controls.left_shoulder =
    //     IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
    // game_state->controls.right_shoulder =
    //     IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);

    // // Menu buttons
    // game_state->controls.back =
    //     IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_LEFT); //
    //     Share/Back
    // game_state->controls.start = IsGamepadButtonDown(
    //     gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT); // Options/Start

    // ═══════════════════════════════════════════════════════════
    // 🎮 D-PAD (works on ALL controllers!)
    // ═══════════════════════════════════════════════════════════
    // Raylib treats D-pad as buttons (digital), not axes (analog).
    // We must convert button states to analog values for game layer.
    // ═══════════════════════════════════════════════════════════

    // ✅ Update start position BEFORE reading buttons
    new_controller->start_x = old_controller->end_x;
    new_controller->start_y = old_controller->end_y;

    // ─────────────────────────────────────────────────────────
    // D-PAD UP/DOWN (Y-axis)
    // ─────────────────────────────────────────────────────────
    bool dpad_up = IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP);
    bool dpad_down =
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN);

    if (dpad_up && !dpad_down) {
      // Only UP pressed
      new_controller->end_y = +1.0f;
      process_game_button_state(true, &new_controller->up);
      process_game_button_state(false, &new_controller->down);

    } else if (dpad_down && !dpad_up) {
      // Only DOWN pressed
      new_controller->end_y = -1.0f;
      process_game_button_state(true, &new_controller->down);
      process_game_button_state(false, &new_controller->up);

    } else if (dpad_up && dpad_down) {
      // Both pressed (cancel out)
      new_controller->end_y = 0.0f;
      process_game_button_state(true, &new_controller->up);
      process_game_button_state(true, &new_controller->down);

    } else {
      // Neither pressed
      new_controller->end_y = 0.0f;
      process_game_button_state(false, &new_controller->up);
      process_game_button_state(false, &new_controller->down);
    }

    // ─────────────────────────────────────────────────────────
    // D-PAD LEFT/RIGHT (X-axis)
    // ─────────────────────────────────────────────────────────
    bool dpad_left =
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    bool dpad_right =
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);

    if (dpad_left && !dpad_right) {
      // Only LEFT pressed
      new_controller->end_x = -1.0f;
      process_game_button_state(true, &new_controller->left);
      process_game_button_state(false, &new_controller->right);

    } else if (dpad_right && !dpad_left) {
      // Only RIGHT pressed
      new_controller->end_x = +1.0f;
      process_game_button_state(true, &new_controller->right);
      process_game_button_state(false, &new_controller->left);

    } else if (dpad_left && dpad_right) {
      // Both pressed (cancel out)
      new_controller->end_x = 0.0f;
      process_game_button_state(true, &new_controller->left);
      process_game_button_state(true, &new_controller->right);

    } else {
      // Neither pressed
      new_controller->end_x = 0.0f;
      process_game_button_state(false, &new_controller->left);
      process_game_button_state(false, &new_controller->right);
    }

    // Update min/max (Day 13 pattern)
    new_controller->min_x = new_controller->max_x = new_controller->end_x;
    new_controller->min_y = new_controller->max_y = new_controller->end_y;

    // ═══════════════════════════════════════════════════════════
    // 🎮 ANALOG STICKS (normalized -1.0 to +1.0)
    // ═══════════════════════════════════════════════════════════
    // Platform layer reports RAW values (no filtering!)
    //
    // Raylib automatically:
    // - Normalizes to -1.0 to +1.0 range
    // - Applies small internal deadzone (~0.05)
    // - Handles platform differences (Windows/Linux/Mac)
    //
    // We just pass the values through!
    // ═══════════════════════════════════════════════════════════

    real32 left_stick_x = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X);
    real32 left_stick_y = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y);

    // Must set is_analog EVERY FRAME because:
    // 1. Raylib always returns a value (even 0.0)
    // 2. prepare_input_frame() preserves old value
    // 3. We need to mark "this frame used joystick, not keyboard"
    new_controller->is_analog = true;

    // Update start position (for movement tracking)
    new_controller->start_x = old_controller->end_x;
    new_controller->start_y = old_controller->end_y;

    // Store RAW values (game layer will apply deadzone!)
    new_controller->end_x = left_stick_x;
    new_controller->end_y = left_stick_y;

    // Day 13: Just set min/max to current value
    // Day 14+: These will track actual min/max during frame
    new_controller->min_x = left_stick_x;
    new_controller->max_x = left_stick_x;
    new_controller->min_y = left_stick_y;
    new_controller->max_y = left_stick_y;

    // game_state->controls.right_stick_x =
    //     (int16_t)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_X) *
    //               32767.0f);
    // game_state->controls.right_stick_y =
    //     (int16_t)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_Y) *
    //               32767.0f);

    // // Triggers (also normalized 0.0 to +1.0)
    // game_state->controls.left_trigger =
    //     (int16_t)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_TRIGGER)
    //     *
    //               32767.0f);
    // game_state->controls.right_trigger =
    //     (int16_t)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_TRIGGER)
    //     *
    //               32767.0f);

    // Debug output for button presses (only print on state change)
    if (IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
      printf("Button A pressed\n");
    }
    if (IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT)) {
      printf("Button Start pressed\n");
    }
  }
}

void debug_joystick_state(GameInput *old_game_input) {
  printf("\n🎮 Controller States:\n");
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    GameControllerInput *c = &old_game_input->controllers[i];
    // LinuxJoystickState *joystick_state = NULL;
    // if (i > 0 && i - 1 < MAX_JOYSTICK_COUNT) {
    //   joystick_state = &g_joysticks[i - 1];
    // }
    RaylibJoystickState *joystick_state = NULL;
    if (i > 0 && i - 1 < MAX_JOYSTICK_COUNT) {
      joystick_state = &g_joysticks[i - 1];
    }
    printf("  [%d] connected=%d analog=%d gamepad_id=%d end_x=%.2f "
           "end_y=%.2f\n",
           i, c->is_connected, c->is_analog,
           joystick_state ? joystick_state->gamepad_id : -1, c->end_x,
           c->end_y);
  }
}