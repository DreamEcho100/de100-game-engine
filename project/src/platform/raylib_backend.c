#include "raylib_backend.h"
#include "../game/game.h"
#include <raylib.h>
#include <stdlib.h>

static inline uint32_t rgba_format(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return (a << 24) | (r << 16) | (g << 8) | b;
}

int platform_main() {
  int width = 800, height = 600;

  InitWindow(width, height, "Raylib Backend");
  SetTargetFPS(60);

  uint32_t *pixels = malloc(width * height * sizeof(uint32_t));
  Image img = {.data = pixels,
               .width = width,
               .height = height,
               .mipmaps = 1,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  Texture2D tex = LoadTextureFromImage(img);

  GameState state = {0};

  while (!WindowShouldClose()) {
    game_update(&state, pixels, width, height, rgba_format);
    UpdateTexture(tex, pixels);

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(tex, 0, 0, WHITE);
    EndDrawing();
  }

  return 0;
}
