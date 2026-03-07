#ifndef UTILS_BASE_GAME_H
#define UTILS_BASE_GAME_H

#include "audio.h"
#include "backbuffer.h"

typedef struct {
  Backbuffer backbuffer;
  AudioOutputBuffer audio;
} GameProps;

#endif // UTILS_BASE_GAME_H