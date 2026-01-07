
#ifndef RAYLIB_INPUTS_JOYSTICK_H
#define RAYLIB_INPUTS_JOYSTICK_H

#include "../../_common/input.h"

void raylib_init_gamepad(GameControllerInput *controller_old_input, GameControllerInput *controller_new_input);
void raylib_poll_gamepad(GameInput *old_input, GameInput *new_input);
void debug_joystick_state(GameInput *old_game_input);

#endif // RAYLIB_INPUTS_JOYSTICK_H