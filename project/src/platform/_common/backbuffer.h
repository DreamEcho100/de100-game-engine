#ifndef HANDMADE_COMMON_BACKBUFFER_H
#define HANDMADE_COMMON_BACKBUFFER_H
#include "../../base.h"
#include "../../game.h"

typedef enum {
  INIT_BACKBUFFER_STATUS_SUCCESS = 0,
  INIT_BACKBUFFER_STATUS_MMAP_FAILED = 1,
} INIT_BACKBUFFER_STATUS;
INIT_BACKBUFFER_STATUS init_backbuffer(GameOffscreenBuffer *buffer, int width, int height, int bytes_per_pixel, pixel_composer_fn composer);

#endif // HANDMADE_COMMON_BACKBUFFER_H