#ifndef DE100_PLATFORM_RAYLIB_INPUTS_KEYBOARD_H
#define DE100_PLATFORM_RAYLIB_INPUTS_KEYBOARD_H

#include "../../../game/input.h"
#include "../../_common/config.h"
#include "../../../engine.h"

// Updated to take PlatformAudioConfig (mirrors X11 pattern)
void handle_keyboard_inputs(EnginePlatformState *platform, EngineGameState *game);

#endif // DE100_PLATFORM_RAYLIB_INPUTS_KEYBOARD_H
