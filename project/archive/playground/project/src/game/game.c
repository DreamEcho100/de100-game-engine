#include "game.h"

void game_update(GameState *state, uint32_t *pixels, int width, int height,
                 PixelFormatFunc format_pixel) {
  state->t += 0.01f;
  if (state->t > 1.0f)
    state->t -= 1.0f;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int i = y * width + x;

      float v = (float)x / width + state->t;
      if (v > 1.0f)
        v -= 1.0f;

      uint8_t r = (uint8_t)(255 * v);
      uint8_t g = (uint8_t)((255 * y) / height);
      uint8_t b = 128;
      uint8_t a = 255;

      pixels[i] = format_pixel(r, g, b, a);
    }
  }
}
