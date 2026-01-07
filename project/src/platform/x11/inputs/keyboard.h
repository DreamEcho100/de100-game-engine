#ifndef X11_INPUTS_KEYBOARD_H
#define X11_INPUTS_KEYBOARD_H

#include "../../_common/input.h"
#include <X11/Xlib.h>

void handleEventKeyPress(XEvent *event, GameInput *new_game_input,
                         GameSoundOutput *sound_output);
void handleEventKeyRelease(XEvent *event, GameInput *new_game_input);

#endif // X11_INPUTS_KEYBOARD_H