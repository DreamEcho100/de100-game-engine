#define _POSIX_C_SOURCE 200809L

/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/x11/main.c — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 02 — XOpenDisplay, XCreateWindow, GLXCreateContext, event loop.
 *             XSizeHints to set a fixed initial window size (temporary!).
 *
 * LESSON 03 — glTexSubImage2D with GL_RGBA, glOrtho, full-screen quad.
 *             First pixel pushed to screen.
 *
 * LESSON 07 — XkbKeycodeToKeysym, UPDATE_BUTTON, XPending event drain.
 *             Input fed into GameInput double buffer.
 *
 * LESSON 08 — Letterbox math: scale factor + x/y offset so the canvas
 *             fills the window without distortion on resize.
 *             XSizeHints size-lock removed; ConfigureNotify handled.
 *             snprintf + draw_text for FPS counter.
 *             DE100 frame timing: coarse nanosleep + spin-wait.
 *             FrameStats under #ifdef DEBUG (printed on shutdown).
 *
 * LESSON 09 — Minimal audio: snd_pcm_set_params, first tone audible.
 *             (L09 code commented / replaced by L12 full init.)
 *
 * LESSON 12 — Full ALSA init via platform_audio_init (snd_pcm_hw_params).
 *
 * LESSON 14 — game/demo.c + game/audio_demo.c removed; game/main.c added.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>   /* XSizeHints — introduced L02, removed L08 */
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "./audio.h"
#include "./base.h"
#include "../../platform.h"
#include "../../game/base.h"
#include "../../game/demo.h"
#include "../../game/audio_demo.h"
#include "../../utils/math.h"

typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display *, GLXDrawable, int);

/* ═══════════════════════════════════════════════════════════════════════════
 * Frame timing (DE100 pattern — LESSON 08)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * FrameTiming holds three timespec checkpoints per frame:
 *   frame_start — captured at top of loop
 *   work_end    — captured after game logic + render
 *   frame_end   — captured after sleep
 *
 * Two-phase sleep:
 *   Phase 1: coarse nanosleep(1ms) loop until 3ms before deadline.
 *   Phase 2: spin-wait to exact deadline (consumes one CPU core briefly
 *            but eliminates scheduler jitter in the critical window).
 *
 * FrameStats (DEBUG only): min/max/avg frame time, missed frames.
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  struct timespec frame_start;
  struct timespec work_end;
  struct timespec frame_end;
  float work_seconds;
  float total_seconds;
  float sleep_seconds;
} FrameTiming;

#ifdef DEBUG
typedef struct {
  unsigned int frame_count;
  unsigned int missed_frames;
  float min_frame_ms;
  float max_frame_ms;
  float total_frame_ms;
} FrameStats;
static FrameStats g_stats = {0};
#endif

static FrameTiming g_timing = {0};
static const float TARGET_SECONDS = 1.0f / TARGET_FPS;

static inline double timespec_to_seconds(const struct timespec *ts) {
  return (double)ts->tv_sec + (double)ts->tv_nsec * 1e-9;
}

static inline float timespec_diff_seconds(const struct timespec *a,
                                           const struct timespec *b) {
  return (float)(timespec_to_seconds(b) - timespec_to_seconds(a));
}

static inline void get_timespec(struct timespec *ts) {
  clock_gettime(CLOCK_MONOTONIC, ts);
}

/* LESSON 08 — Begin frame: capture frame_start. */
static inline void timing_begin(void) {
  get_timespec(&g_timing.frame_start);
}

/* LESSON 08 — Mark work done: capture work_end, compute work_seconds. */
static inline void timing_mark_work_done(void) {
  get_timespec(&g_timing.work_end);
  g_timing.work_seconds = timespec_diff_seconds(&g_timing.frame_start,
                                                  &g_timing.work_end);
}

/* LESSON 08 — Sleep until target using coarse sleep + spin-wait. */
static void timing_sleep_until_target(void) {
  float elapsed = g_timing.work_seconds;

  if (elapsed < TARGET_SECONDS) {
    float threshold = TARGET_SECONDS - 0.003f; /* 3ms spin budget */

    /* Phase 1: coarse 1ms nanosleep */
    while (elapsed < threshold) {
      struct timespec sleep_ts = { .tv_sec = 0, .tv_nsec = 1000000L };
      nanosleep(&sleep_ts, NULL);
      struct timespec now;
      get_timespec(&now);
      elapsed = timespec_diff_seconds(&g_timing.frame_start, &now);
    }

    /* Phase 2: spin-wait to exact target */
    while (elapsed < TARGET_SECONDS) {
      struct timespec now;
      get_timespec(&now);
      elapsed = timespec_diff_seconds(&g_timing.frame_start, &now);
    }
  }
}

/* LESSON 08 — End frame: capture frame_end, compute total + sleep seconds. */
static inline void timing_end(void) {
  get_timespec(&g_timing.frame_end);
  g_timing.total_seconds  = timespec_diff_seconds(&g_timing.frame_start,
                                                    &g_timing.frame_end);
  g_timing.sleep_seconds  = g_timing.total_seconds - g_timing.work_seconds;

#ifdef DEBUG
  float ms = g_timing.total_seconds * 1000.0f;
  g_stats.frame_count++;
  if (g_stats.frame_count == 1 || ms < g_stats.min_frame_ms)
    g_stats.min_frame_ms = ms;
  if (ms > g_stats.max_frame_ms)
    g_stats.max_frame_ms = ms;
  g_stats.total_frame_ms += ms;
  if (g_timing.total_seconds > TARGET_SECONDS + 0.002f)
    g_stats.missed_frames++;
#endif
}

#ifdef DEBUG
static void print_frame_stats(void) {
  if (g_stats.frame_count == 0) return;
  printf("\n═══════════════════════════════════════════════════════════\n");
  printf("📊 FRAME TIME STATISTICS\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("Total frames:   %u\n",  g_stats.frame_count);
  printf("Missed frames:  %u (%.2f%%)\n", g_stats.missed_frames,
         (float)g_stats.missed_frames / g_stats.frame_count * 100.0f);
  printf("Min frame time: %.2f ms\n", g_stats.min_frame_ms);
  printf("Max frame time: %.2f ms\n", g_stats.max_frame_ms);
  printf("Avg frame time: %.2f ms\n",
         g_stats.total_frame_ms / (float)g_stats.frame_count);
  printf("═══════════════════════════════════════════════════════════\n");
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * VSync detection (LESSON 08)
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void setup_vsync(void) {
  const char *ext = glXQueryExtensionsString(g_x11.display, g_x11.screen);

  if (ext && strstr(ext, "GLX_EXT_swap_control")) {
    PFNGLXSWAPINTERVALEXTPROC fn =
        (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXSwapIntervalEXT");
    if (fn) {
      fn(g_x11.display, g_x11.window, 1);
      g_x11.vsync_enabled = true;
      printf("✓ VSync enabled (GLX_EXT)\n");
      return;
    }
  }

  if (ext && strstr(ext, "GLX_MESA_swap_control")) {
    PFNGLXSWAPINTERVALMESAPROC fn =
        (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXSwapIntervalMESA");
    if (fn) {
      fn(1);
      g_x11.vsync_enabled = true;
      printf("✓ VSync enabled (MESA)\n");
      return;
    }
  }

  g_x11.vsync_enabled = false;
  printf("⚠ VSync not available — software frame limiter active\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Window + GL init (LESSON 02)
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int init_window(void) {
  g_x11.display = XOpenDisplay(NULL);
  if (!g_x11.display) {
    fprintf(stderr, "❌ XOpenDisplay failed\n");
    return -1;
  }

  /* Suppress auto-repeat key events (gives clean press/release pairs). */
  Bool ok;
  XkbSetDetectableAutoRepeat(g_x11.display, True, &ok);

  g_x11.screen  = DefaultScreen(g_x11.display);
  g_x11.window_w = GAME_W;
  g_x11.window_h = GAME_H;

  int attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
  XVisualInfo *visual = glXChooseVisual(g_x11.display, g_x11.screen, attribs);
  if (!visual) {
    fprintf(stderr, "❌ glXChooseVisual: no matching visual\n");
    return -1;
  }

  Colormap cmap = XCreateColormap(g_x11.display,
                                   RootWindow(g_x11.display, g_x11.screen),
                                   visual->visual, AllocNone);

  XSetWindowAttributes wa = {
    .colormap   = cmap,
    .event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                  StructureNotifyMask,
  };

  /* LESSON 02 — XCreateWindow: position (100,100), initial size GAME_W×GAME_H. */
  g_x11.window = XCreateWindow(
      g_x11.display,
      RootWindow(g_x11.display, g_x11.screen),
      100, 100, GAME_W, GAME_H, 0,
      visual->depth, InputOutput, visual->visual,
      CWColormap | CWEventMask, &wa);

  g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
  XFree(visual);

  if (!g_x11.gl_context) {
    fprintf(stderr, "❌ glXCreateContext failed\n");
    return -1;
  }

  XStoreName(g_x11.display, g_x11.window, TITLE);
  XMapWindow(g_x11.display, g_x11.window);

  /* WM_DELETE_WINDOW atom — lets us detect the close button. */
  g_x11.wm_delete_window = XInternAtom(g_x11.display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(g_x11.display, g_x11.window, &g_x11.wm_delete_window, 1);

  /* LESSON 02 — XSizeHints: temporarily fix window to GAME_W × GAME_H so
   * early lessons (03–07) don't need letterbox math yet.
   * LESSON 08 — This block is REMOVED when we add real letterbox scaling.
   *             Students delete these five lines in lesson 08. */
  /* [L02 temporary — removed in L08]
  XSizeHints *hints = XAllocSizeHints();
  hints->flags = PMinSize | PMaxSize;
  hints->min_width  = hints->max_width  = GAME_W;
  hints->min_height = hints->max_height = GAME_H;
  XSetWMNormalHints(g_x11.display, g_x11.window, hints);
  XFree(hints);
  */

  glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

  /* Fixed ortho projection — the letterbox code replaces this in L08.
   * LESSON 03 — glOrtho maps canvas (0,0)→(GAME_W,GAME_H) to NDC.      */
  glViewport(0, 0, GAME_W, GAME_H);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, GAME_W, GAME_H, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);

  glGenTextures(1, &g_x11.texture_id);
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  /* LESSON 03 — Allocate the texture once at init (NULL pixels).
   * Subsequent frames use glTexSubImage2D to update pixels in-place,
   * which avoids re-allocating GPU memory every frame.                    */
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
               GAME_W, GAME_H, 0,
               GL_RGBA, GL_UNSIGNED_BYTE,
               NULL);

  return 0;
}

static void shutdown_window(void) {
  if (g_x11.texture_id)
    glDeleteTextures(1, &g_x11.texture_id);
  if (g_x11.gl_context) {
    glXMakeCurrent(g_x11.display, None, NULL);
    glXDestroyContext(g_x11.display, g_x11.gl_context);
  }
  if (g_x11.window)
    XDestroyWindow(g_x11.display, g_x11.window);
  if (g_x11.display)
    XCloseDisplay(g_x11.display);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Input processing (LESSON 07)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 07 — XPending drain loop: process ALL pending events each frame
 *             (not just one).  Prevents event-queue backup under load.
 *
 * LESSON 07 — XkbKeycodeToKeysym: hardware keycode → X11 KeySym.
 *             XKB variant avoids classic XLookupKeysym multi-step lookup.
 *
 * LESSON 07 — UPDATE_BUTTON macro (from game/base.h) records state + count.
 */

static void process_events(GameInput *curr, int *is_running) {
  /* LESSON 07 — XPending drain loop */
  while (XPending(g_x11.display) > 0) {
    XEvent ev;
    XNextEvent(g_x11.display, &ev);

    switch (ev.type) {
    case KeyPress: {
      KeySym sym = XkbKeycodeToKeysym(g_x11.display, ev.xkey.keycode, 0, 0);
      switch (sym) {
      case XK_Escape:
      case XK_q:
      case XK_Q:
        UPDATE_BUTTON(&curr->buttons.quit, 1);
        *is_running = 0;
        break;
      case XK_space:
        UPDATE_BUTTON(&curr->buttons.play_tone, 1);
        break;
      }
      break;
    }
    case KeyRelease: {
      KeySym sym = XkbKeycodeToKeysym(g_x11.display, ev.xkey.keycode, 0, 0);
      switch (sym) {
      case XK_Escape:
      case XK_q:
      case XK_Q:
        UPDATE_BUTTON(&curr->buttons.quit, 0);
        break;
      case XK_space:
        UPDATE_BUTTON(&curr->buttons.play_tone, 0);
        break;
      }
      break;
    }
    case ClientMessage:
      if ((Atom)ev.xclient.data.l[0] == g_x11.wm_delete_window) {
        UPDATE_BUTTON(&curr->buttons.quit, 1);
        *is_running = 0;
      }
      break;

    /* LESSON 08 — ConfigureNotify: window was resized.
     * Update g_x11.window_w/h so letterbox math recalculates.           */
    case ConfigureNotify:
      g_x11.window_w = ev.xconfigure.width;
      g_x11.window_h = ev.xconfigure.height;
      break;
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Backbuffer → screen (LESSON 03, LESSON 08)
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* LESSON 08 — compute_letterbox: scale + offset to fit canvas into window
 * preserving aspect ratio (no distortion).
 *
 * Algorithm:
 *   scale = min(window_w / canvas_w, window_h / canvas_h)   (fit inside)
 *   offset_x = (window_w - canvas_w * scale) / 2             (center x)
 *   offset_y = (window_h - canvas_h * scale) / 2             (center y)
 *
 * JS analogy: CSS `object-fit: contain` on a <canvas> element.           */
static void compute_letterbox(int win_w, int win_h, int canvas_w, int canvas_h,
                              float *scale, int *off_x, int *off_y) {
  float sx = (float)win_w / (float)canvas_w;
  float sy = (float)win_h / (float)canvas_h;
  *scale = (sx < sy) ? sx : sy;
  *off_x = (int)((win_w  - canvas_w  * *scale) * 0.5f);
  *off_y = (int)((win_h  - canvas_h  * *scale) * 0.5f);
}

/* LESSON 03 — Upload backbuffer pixels to GPU, draw a full-screen quad.
 * LESSON 08 — Quad uses letterbox offsets instead of (0,0)→(GAME_W,GAME_H).
 *
 * PIXEL FORMAT — GL_RGBA:
 * Our GAME_RGB(r,g,b) macro stores pixels in memory as [R][G][B][A] bytes
 * (lowest address = R) because the macro packs R into bits 0-7 of uint32_t
 * which on little-endian puts R at byte[0].
 *
 * GL_RGBA tells the driver: "read bytes as [Red][Green][Blue][Alpha]"
 * which exactly matches our layout.  GL_BGRA would swap R↔B and is WRONG
 * for our memory layout.  (Some engines pack pixels as 0xAABBGGRR, which
 * puts Blue at byte[0] and DOES need GL_BGRA — that is not this course.) */
static void display_backbuffer(Backbuffer *bb) {
  float scale;
  int off_x, off_y;
  compute_letterbox(g_x11.window_w, g_x11.window_h,
                    bb->width, bb->height,
                    &scale, &off_x, &off_y);

  int dst_w = (int)(bb->width  * scale);
  int dst_h = (int)(bb->height * scale);

  /* LESSON 08 — Reset viewport + projection to current window size each frame
   * so the letterbox quad maps to real pixels correctly.                   */
  glViewport(0, 0, g_x11.window_w, g_x11.window_h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, g_x11.window_w, g_x11.window_h, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  /* LESSON 03 — glTexSubImage2D: uploads new pixel data into the already-
   * allocated texture.  Texture was allocated once in init_gl (NULL pixels).
   * GL_RGBA: reads bytes as [R][G][B][A] — matches our GAME_RGB layout.   */
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                  bb->width, bb->height,
                  GL_RGBA, GL_UNSIGNED_BYTE,
                  bb->pixels);

  float x0 = (float)off_x;
  float y0 = (float)off_y;
  float x1 = (float)(off_x + dst_w);
  float y1 = (float)(off_y + dst_h);

  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x0, y0);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x1, y0);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x1, y1);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x0, y1);
  glEnd();

  glXSwapBuffers(g_x11.display, g_x11.window);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio drain loop (LESSON 12)
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void process_audio(GameAudioState *audio, AudioOutputBuffer *audio_buf,
                           PlatformAudioConfig *cfg) {
  if (!cfg->alsa_pcm) return;

  int frames = platform_audio_get_samples_to_write(cfg);
  if (frames <= 0) return;

  /* Clamp to buffer capacity (defence in depth). */
  if (frames > audio_buf->max_sample_count)
    frames = audio_buf->max_sample_count;

  game_get_audio_samples(audio, audio_buf, frames);
  platform_audio_write(audio_buf, frames, cfg);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  /* ── Window ────────────────────────────────────────────────────────── */
  if (init_window() != 0) return 1;
  setup_vsync();

  /* ── Backbuffer allocation ─────────────────────────────────────────── */
  Backbuffer bb = {
    .width            = GAME_W,
    .height           = GAME_H,
    .bytes_per_pixel  = 4,
    .pitch            = GAME_W * 4,
  };
  bb.pixels = (uint32_t *)malloc((size_t)(GAME_W * GAME_H) * 4);
  if (!bb.pixels) { fprintf(stderr, "❌ Out of memory\n"); return 1; }
  memset(bb.pixels, 0, (size_t)(GAME_W * GAME_H) * 4);

  /* ── Audio buffer + config ─────────────────────────────────────────── */
  AudioOutputBuffer audio_buf = {
    .sample_count     = AUDIO_CHUNK_SIZE,
    .max_sample_count = AUDIO_CHUNK_SIZE,
  };
  audio_buf.samples = (int16_t *)malloc((size_t)AUDIO_CHUNK_SIZE * AUDIO_BYTES_PER_FRAME);
  if (!audio_buf.samples) { fprintf(stderr, "❌ Out of memory (audio)\n"); return 1; }
  memset(audio_buf.samples, 0, (size_t)AUDIO_CHUNK_SIZE * AUDIO_BYTES_PER_FRAME);

  PlatformAudioConfig audio_cfg = {
    .sample_rate = AUDIO_SAMPLE_RATE,
    .channels    = AUDIO_CHANNELS,
    .chunk_size  = AUDIO_CHUNK_SIZE,
  };
  if (platform_audio_init(&audio_cfg) != 0) {
    fprintf(stderr, "⚠ Audio init failed — continuing without audio\n");
  }

  /* ── Game state ────────────────────────────────────────────────────── */
  GameAudioState audio_state = {0};
  game_audio_init_demo(&audio_state);

  /* ── Input double buffer ───────────────────────────────────────────── */
  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameInput *curr_input = &inputs[0];
  GameInput *prev_input = &inputs[1];

  /* ── Main loop ─────────────────────────────────────────────────────── */
  int is_running = 1;

#ifdef DEBUG
  printf("\n[DEBUG build — ASan + UBSan + FrameStats active]\n\n");
#endif

  while (is_running) {
    /* LESSON 08 — Begin frame timing */
    timing_begin();

    /* LESSON 07 — Swap + prepare input buffers */
    platform_swap_input_buffers(&curr_input, &prev_input);
    prepare_input_frame(curr_input, prev_input);

    /* LESSON 07 — Drain X11 event queue */
    process_events(curr_input, &is_running);
    if (curr_input->buttons.quit.ended_down) break;

    /* LESSON 09/10 — React to play_tone button */
    if (button_just_pressed(curr_input->buttons.play_tone)) {
      game_play_sound_at(&audio_state, SOUND_TONE_MID);
    }

    /* LESSON 13 — Advance game-time audio state */
    float dt_ms = g_timing.total_seconds > 0.0f
                  ? g_timing.total_seconds * 1000.0f
                  : (1000.0f / TARGET_FPS);
    game_audio_update(&audio_state, dt_ms);

    /* LESSON 12 — Write audio samples to ALSA */
    process_audio(&audio_state, &audio_buf, &audio_cfg);

    /* LESSON 03/08 — Render demo frame.
     * FPS is computed as a rolling 60-frame average to avoid integer jitter.
     * A raw (1/total_seconds) truncation oscillates ±2 fps every frame.   */
    static float frame_time_accum  = 0.0f;
    static int   frame_time_count  = 0;
    static int   fps_display       = TARGET_FPS;
    frame_time_accum += g_timing.total_seconds;
    frame_time_count++;
    if (frame_time_count >= 60) {
      fps_display      = (frame_time_accum > 0.0f)
                         ? (int)(60.0f / frame_time_accum + 0.5f)
                         : TARGET_FPS;
      frame_time_accum = 0.0f;
      frame_time_count = 0;
    }
    demo_render(&bb, curr_input, fps_display);

    /* LESSON 03 — Push backbuffer to GPU */
    display_backbuffer(&bb);

    /* LESSON 08 — Frame timing: mark work done, sleep, end */
    timing_mark_work_done();
    if (!g_x11.vsync_enabled) {
      timing_sleep_until_target();
    }
    timing_end();
  }

  /* ── Cleanup ───────────────────────────────────────────────────────── */
#ifdef DEBUG
  print_frame_stats();
#endif

  /* Drain ALSA and shut down */
  platform_audio_shutdown_cfg(&audio_cfg);

  free(audio_buf.samples);
  free(bb.pixels);
  shutdown_window();
  return 0;
}
