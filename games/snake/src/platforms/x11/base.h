#define _POSIX_C_SOURCE 200809L

#ifndef PLATFORMS_X11_BASE_H
#define PLATFORMS_X11_BASE_H

#include <stdbool.h>
#include <stdint.h>

#include <GL/glx.h>
#include <X11/Xlib.h>
#include <alsa/asoundlib.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * ALSA-specific audio configuration
 * These fields are meaningless to other backends (Raylib manages its own
 * buffer internally).  Owned entirely by the X11 platform layer.
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct {
  int hz;                /* Game update rate (60 Hz) */
  int frames_of_latency; /* Target buffered frames (2) */
  int samples_per_frame; /* Derived: samples_per_second / hz */
  int latency_samples;   /* Derived: samples_per_frame * frames_of_latency */
  int safety_samples;    /* Derived: samples_per_frame / 3 */
  int64_t running_sample_index; /* Total frames written to ALSA */
} X11AudioConfig;

typedef struct {
  snd_pcm_t *pcm_handle;
  X11AudioConfig config;
} X11Audio;

typedef struct {
  Display *display;      /* X11 server connection       */
  Window window;         /* The application window      */
  GLXContext gl_context; /* OpenGL context              */
  int screen;            /* Default screen number        */
  int width;
  int height;
  Atom wm_delete_window; /* WM_DELETE_WINDOW atom       */
  bool vsync_enabled;
  GLuint texture_id; /* GPU texture for the backbuf */
  int is_running;    /* 0 → exit the game loop      */

  X11Audio audio;
} X11State;

extern X11State g_x11;

#endif // PLATFORMS_X11_BASE_H