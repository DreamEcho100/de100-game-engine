#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
// #include "../../../base.h"
#include "../../../game.h"
// #include "../../_common/backbuffer.h"
#include "../../_common/input.h"
#include "../audio.h"

void handleEventKeyPress(XEvent *event, GameInput *new_game_input,
                         GameSoundOutput *sound_output) {
  KeySym key = XLookupKeysym(&event->xkey, 0);
  // printf("pressed\n");

  GameControllerInput *new_controller1 =
      &new_game_input->controllers[KEYBOARD_CONTROLLER_INDEX];

  switch (key) {
  case XK_F1: {
    printf("F1 pressed - showing audio debug\n");
    linux_debug_audio_latency(sound_output);
    break;
  }
  case (XK_w):
  case (XK_W):
  case (XK_Up): {
    // W/Up = Move up = positive Y
    new_controller1->end_y = +1.0f;
    new_controller1->min_y = new_controller1->max_y = new_controller1->end_y;
    new_controller1->is_analog = false;

    // Also set button state for backward compatibility
    process_game_button_state(true, &new_controller1->up);
    break;
  }
  case (XK_a):
  case (XK_A):
  case (XK_Left): {
    // A/Left = Move left = negative X
    new_controller1->end_x = -1.0f;
    new_controller1->min_x = new_controller1->max_x = new_controller1->end_x;
    new_controller1->is_analog = false;

    process_game_button_state(true, &new_controller1->left);
    break;
  }
  case (XK_s):
  case (XK_S):
  case (XK_Down): {
    // S/Down = Move down = negative Y
    new_controller1->end_y = -1.0f;
    new_controller1->min_y = new_controller1->max_y = new_controller1->end_y;
    new_controller1->is_analog = false;

    process_game_button_state(true, &new_controller1->down);
    break;
  }
  case (XK_d):
  case (XK_D):
  case (XK_Right): {
    // D/Right = Move right = positive X
    new_controller1->end_x = +1.0f;
    new_controller1->min_x = new_controller1->max_x = new_controller1->end_x;
    new_controller1->is_analog = false;

    process_game_button_state(true, &new_controller1->right);
    break;
  }
  case (XK_space): {
    printf("SPACE pressed\n");
    break;
  }
  case (XK_Escape): {
    printf("ESCAPE pressed - exiting\n");
    is_game_running = false;
    break;
  }
  }
}

void handleEventKeyRelease(XEvent *event, GameInput *new_game_input) {
  KeySym key = XLookupKeysym(&event->xkey, 0);

  GameControllerInput *new_controller1 =
      &new_game_input->controllers[KEYBOARD_CONTROLLER_INDEX];

  switch (key) {
  case (XK_w):
  case (XK_W):
  case (XK_Up): {
    new_controller1->end_y = 0.0f;
    new_controller1->min_y = new_controller1->max_y = 0.0f;
    new_controller1->is_analog = false;
    process_game_button_state(false, &new_controller1->up);
    break;
  }
  case (XK_a):
  case (XK_A):
  case (XK_Left): {
    new_controller1->end_x = 0.0f;
    new_controller1->min_x = new_controller1->max_x = 0.0f;
    new_controller1->is_analog = false;
    process_game_button_state(false, &new_controller1->left);
    break;
  }
  case (XK_s):
  case (XK_S):
  case (XK_Down): {
    new_controller1->end_y = 0.0f;
    new_controller1->min_y = new_controller1->max_y = 0.0f;
    new_controller1->is_analog = false;
    process_game_button_state(false, &new_controller1->down);
    break;
  }
  case (XK_d):
  case (XK_D):
  case (XK_Right): {
    new_controller1->end_x = 0.0f;
    new_controller1->min_x = new_controller1->max_x = 0.0f;
    new_controller1->is_analog = false;
    process_game_button_state(false, &new_controller1->right);
    break;
  }
  case (XK_space): {
    printf("SPACE released\n");
    break;
  }
  case (XK_Escape): {
    printf("ESCAPE released - exiting\n");
    is_game_running = false;
    break;
  }
  }
}