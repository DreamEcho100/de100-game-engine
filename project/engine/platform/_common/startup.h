#ifndef DE100_PLATFORM__COMMON_STARTUP_H
#define DE100_PLATFORM__COMMON_STARTUP_H

#include "../../game/audio.h"
#include "../../game/backbuffer.h"
#include "../../game/config.h"
#include "../../game/game-loader.h"
#include "../../game/input.h"
#include "../../game/memory.h"

int platform_game_startup(LoadGameCodeConfig *load_game_code_config,
                          GameCode *game, GameConfig *game_config,
                          GameMemory *memory, GameInput *old_game_input,
                          GameInput *new_game_input, GameBackBuffer *buffer,
                          GameAudioOutputBuffer *audio_buffer);

int free_platform_game_startup(LoadGameCodeConfig *load_game_code_config,
                               GameCode *game, GameConfig *game_config,
                               GameMemory *memory, GameInput *old_game_input,
                               GameInput *new_game_input,
                               GameBackBuffer *buffer,
                               GameAudioOutputBuffer *audio_buffer);

#endif /* DE100_PLATFORM__COMMON_STARTUP_H */