#include <raylib.h>

#define GAME_W 800
#define GAME_H 600
#define TARGET_FPS 60
#define TITLE "Asteroids"

int main(void) {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(GAME_W, GAME_H, TITLE);
  SetTargetFPS(TARGET_FPS);
  SetWindowState(FLAG_WINDOW_RESIZABLE);

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground((Color){20, 20, 30, 255});
    EndDrawing();
  }

  CloseWindow();

  return 0;
}