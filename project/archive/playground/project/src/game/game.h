#ifndef GAME_H
#define GAME_H

#include <stdint.h>

typedef struct {
  float t;
} GameState;

typedef uint32_t (*PixelFormatFunc)(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void game_update(GameState *state, uint32_t *pixels, int width, int height, PixelFormatFunc format_pixel);

#endif
