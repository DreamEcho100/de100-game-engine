#ifndef PLATFORM_H
#define PLATFORM_H
#include "game.h"

/* ── Platform functions ──────────────────────────────────── */
/* These are declared here and implemented in main_x11.c / main_raylib.c */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int samples_per_second;  /* 44100 or 48000 */
  int buffer_size_samples; /* How many samples per buffer */
  int is_initialized;

  /* Casey's Latency Model */
  int latency_samples;          /* Target latency (~2 frames worth) */
  int safety_samples;           /* Safety margin (~1/3 frame) */
  int samples_per_frame;        /* Samples per game frame */
  int64_t running_sample_index; /* Total samples written (ever) */
  int frames_of_latency;        /* Target ~[number] frames of audio buffered */
  int hz;                       /* Game logic update rate (e.g., 30 Hz) */
} PlatformAudioConfig;

typedef struct {
  const char *title;
  int width;
  int height;
  Backbuffer backbuffer;
  PlatformAudioConfig audio;
  bool is_running;
} PlatformGameProps;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define TITLE ("Tetris (" TOSTRING(BACKEND) ") - yt/@javidx9")

static int platform_game_props_init(PlatformGameProps *platform_game_props) {
  int screen_width = (FIELD_WIDTH * CELL_SIZE) + SIDEBAR_WIDTH;
  int screen_height = FIELD_HEIGHT * CELL_SIZE;

  platform_game_props->is_running = true;

  platform_game_props->title = TITLE;
  platform_game_props->width = screen_width;
  platform_game_props->height = screen_height;

  /* Allocate backbuffer */
  platform_game_props->backbuffer.width = platform_game_props->width;
  platform_game_props->backbuffer.height = platform_game_props->height;
  platform_game_props->backbuffer.bytes_per_pixel = 4;
  platform_game_props->backbuffer.pitch =
      platform_game_props->width *
      platform_game_props->backbuffer.bytes_per_pixel;
  platform_game_props->backbuffer.pixels =
      (uint32_t *)malloc(platform_game_props->width *
                         platform_game_props->height * sizeof(uint32_t));

  if (!platform_game_props->backbuffer.pixels) {
    fprintf(stderr, "Failed to allocate backbuffer\n");
    return 1;
  }

  /* Audio config - set defaults, platform will fill the rest */
  memset(&platform_game_props->audio, 0, sizeof(PlatformAudioConfig));
  platform_game_props->audio.hz = 60; /* Game runs at 60 Hz */
  platform_game_props->audio.frames_of_latency =
      2; /* Target 2 frames of latency */

  return 0;
}

static void platform_game_props_free(PlatformGameProps *platform_game_props) {
  free(platform_game_props->backbuffer.pixels);
}

int platform_init(PlatformGameProps *props);
void platform_render(GameState *state);
double platform_get_time(void);
void platform_shutdown(void);
int platform_audio_init(PlatformAudioConfig *config);
void platform_audio_shutdown(void);

static inline void platform_swap_input_buffers(GameInput *old_input,
                                               GameInput *current_input) {
  // Swap each frame
  GameInput temp = *current_input;
  *current_input = *old_input;
  *old_input = temp;
  return;
}

#endif /* PLATFORM_H */
