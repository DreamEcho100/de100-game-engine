#ifndef HANDMADE_COMMON_INPUT_H
#define HANDMADE_COMMON_INPUT_H
#include "../../base.h"
#include "../../game.h"

void prepare_input_frame(GameInput *old_input, GameInput *new_input);
void process_game_button_state(bool is_down, GameButtonState *old_state, GameButtonState *new_state);

#endif // HANDMADE_COMMON_INPUT_H