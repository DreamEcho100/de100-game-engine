#ifndef PLATFORM_H
#define PLATFORM_H
#include "game.h"

/* ── Platform functions ──────────────────────────────── */
/* These are declared here and implemented in main_x11.c / main_raylib.c */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  const char *title;
  int width;
  int height;
  Backbuffer backbuffer;
  bool is_running;
} PlatformGameProps;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define TITLE ("Snake (" STRINGIFY(BACKEND) ") - yt/@javidx9")

static int platform_game_props_init(PlatformGameProps *platform_game_props) {
  printf("Initializing platform game properties...\n");
  platform_game_props->title = TITLE;
  platform_game_props->width = SCREEN_INITIAL_WIDTH;
  platform_game_props->height = SCREEN_INITIAL_HEIGHT;

  platform_game_props->is_running = true;

  printf("Initializing backbuffer...\n");

  /* Allocate backbuffer */
  platform_game_props->backbuffer.width = platform_game_props->width;
  platform_game_props->backbuffer.height = platform_game_props->height;
  platform_game_props->backbuffer.pitch =
      platform_game_props->width * sizeof(uint32_t);
  printf("Allocating backbuffer pixels...\n");
  platform_game_props->backbuffer.pixels =
      (uint32_t *)malloc(platform_game_props->width *
                         platform_game_props->height * sizeof(uint32_t));

  printf("Backbuffer initialized successfully.\n");

  if (!platform_game_props->backbuffer.pixels) {
    fprintf(stderr, "Failed to allocate backbuffer\n");
    return 1;
  }

  printf("✓ Platform game properties initialized successfully.\n");

  return 0;
}

static void platform_game_props_free(PlatformGameProps *platform_game_props) {
  free(platform_game_props->backbuffer.pixels);
}

int platform_init(PlatformGameProps *props);
void platform_render(GameState *state);
// void platform_sleep_ms(int ms);
// int platform_should_quit(void);
/* Get current time in seconds since program start.
 * Used for delta time calculations. */
double platform_get_time(void);
void platform_shutdown(void);

#endif /* PLATFORM_H */