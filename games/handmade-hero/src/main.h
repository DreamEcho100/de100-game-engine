#ifndef DE100_HERO_GAME_H
#define DE100_HERO_GAME_H
#include "./inputs.h"

#include "../../../engine/_common/base.h"
#include "../../../engine/_common/memory.h"
#include "../../../engine/game/audio.h"
#include "../../../engine/game/backbuffer.h"
#include "../../../engine/game/base.h"
#include "../../../engine/game/game-loader.h"
#include "../../../engine/game/inputs.h"
#include "../../../engine/game/memory.h"

GameControllerInput *GetController(GameInput *Input,
                                   unsigned int ControllerIndex);

typedef struct {
  uint32 map[9][17];
  real32 upper_left_x;
  real32 upper_left_y;
  real32 width;
  real32 height;
} TileState;

typedef struct {
  int32 x;
  int32 y;
  real32 width;
  real32 height;
  real32 t_jump;
  real32 color_r;
  real32 color_g;
  real32 color_b;
  real32 color_a;
} PlayerState;
typedef struct {
  // Audio state (embedded struct, not pointer)
  GameAudioState audio;
  // Future: More game-specific state
  // PlayerState player;
  // WorldState world;

  // // Audio state (matches Casey's game_state)
  // int32 tone_hz;
  // real32 t_sine; // Phase accumulator

  TileState tile_state;
  PlayerState player_state;
  // Your other state
  int32 speed;
} HandMadeHeroGameState;

#endif // DE100_HERO_GAME_H
