/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/raylib/main.c — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 01 — InitWindow, SetTargetFPS, main loop shell.
 *             First window on screen.
 *
 * LESSON 03 — LoadTextureFromImage with PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
 *             UpdateTexture, DrawTexture. First pixel on screen.
 *             R↔B channel swap: our 0xAARRGGBB matches R8G8B8A8 directly.
 *             Both Raylib (PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) and X11 (GL_RGBA)
 *             use the same [R,G,B,A] byte order — no swap needed on either.
 *
 * LESSON 07 — IsKeyPressed / IsKeyReleased with UPDATE_BUTTON.
 *             Double-buffered input via platform_swap_input_buffers.
 *
 * LESSON 08 — GetFrameTime(), letterbox via DrawTexturePro + dest Rectangle.
 *             FPS counter via GetFPS() fed into demo_render.
 *             DE100 frame timing note: Raylib's SetTargetFPS handles the
 *             sleep internally; we replicate the timing pattern for pedagogy
 *             but do not override Raylib's scheduler.
 *
 * LESSON 09 — Minimal audio: InitAudioDevice, LoadAudioStream, first tone.
 *
 * LESSON 11 — Full audio: pre-fill 2 silent buffers, while-IsAudioStreamProcessed
 *             drain loop, UpdateAudioStream.
 *
 * LESSON 14 — game/demo.c replaced by game/main.c.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <raylib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../platform.h"
#include "../../game/base.h"
#include "../../game/demo.h"
#include "../../game/audio_demo.h"
#include "../../utils/math.h"

/* ─────────────────────────────────────────────────────────────────────────
 * Raylib-specific state
 * ─────────────────────────────────────────────────────────────────────────
 */
typedef struct {
  Texture2D   texture;
  AudioStream audio_stream;
  int         buffer_size_frames;
  int         audio_initialized;
} RaylibState;

static RaylibState g_raylib = {0};

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio (LESSON 09 minimal → LESSON 11 full)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 09 — Minimal path students write first:
 *   InitAudioDevice();
 *   SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE);
 *   g_raylib.audio_stream = LoadAudioStream(AUDIO_SAMPLE_RATE, 16, 2);
 *   PlayAudioStream(g_raylib.audio_stream);
 *   // → first audible tone on space press
 *
 * LESSON 11 — Full path (what this file contains): adds pre-fill of two
 *   silent buffers and the while-IsAudioStreamProcessed drain loop.
 *   The contrast between L09 (simple) and L11 (robust) is the teaching moment.
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int init_audio(PlatformAudioConfig *cfg, AudioOutputBuffer *audio_buf) {
  InitAudioDevice();

  if (!IsAudioDeviceReady()) {
    fprintf(stderr, "⚠ Raylib: Audio device not ready\n");
    return -1;
  }

  /* LESSON 11 — SetAudioStreamBufferSizeDefault must be called BEFORE
   * LoadAudioStream.  The default (4096) matches our AUDIO_CHUNK_SIZE.   */
  SetAudioStreamBufferSizeDefault(cfg->chunk_size);

  g_raylib.audio_stream = LoadAudioStream((unsigned)cfg->sample_rate,
                                           16, (unsigned)cfg->channels);
  g_raylib.buffer_size_frames = cfg->chunk_size;

  if (!IsAudioStreamValid(g_raylib.audio_stream)) {
    fprintf(stderr, "⚠ Raylib: Failed to create audio stream\n");
    CloseAudioDevice();
    return -1;
  }

  /* LESSON 11 — Pre-fill two silent buffers.
   * Raylib uses a double-buffer internally; pre-filling both ensures the
   * stream starts playing immediately without a pop.                      */
  memset(audio_buf->samples, 0,
         (size_t)cfg->chunk_size * AUDIO_BYTES_PER_FRAME);
  UpdateAudioStream(g_raylib.audio_stream, audio_buf->samples, cfg->chunk_size);
  UpdateAudioStream(g_raylib.audio_stream, audio_buf->samples, cfg->chunk_size);

  PlayAudioStream(g_raylib.audio_stream);

  /* Store stream pointer in cfg for shutdown (opaque void*). */
  cfg->raylib_stream = &g_raylib.audio_stream;
  g_raylib.audio_initialized = 1;

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 RAYLIB AUDIO\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("  Sample rate:  %d Hz\n", cfg->sample_rate);
  printf("  Buffer:       %d frames (~%.0f ms)\n",
         cfg->chunk_size,
         (float)cfg->chunk_size / (float)cfg->sample_rate * 1000.0f);
  printf("✓ Audio initialized\n");
  printf("═══════════════════════════════════════════════════════════\n\n");
  return 0;
}

static void shutdown_audio(void) {
  if (g_raylib.audio_initialized) {
    StopAudioStream(g_raylib.audio_stream);
    UnloadAudioStream(g_raylib.audio_stream);
    CloseAudioDevice();
    g_raylib.audio_initialized = 0;
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Display (LESSON 03, LESSON 08)
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* LESSON 03 — First version: DrawTexture at (0,0).
 * LESSON 08 — Upgraded to DrawTexturePro with letterbox destination rect.
 *
 * Raylib letterbox: compute a destination Rectangle that fits the canvas
 * inside the current window with correct aspect ratio.
 *
 * JS analogy: CSS `object-fit: contain` on a <canvas> element.           */
static void display_backbuffer(Backbuffer *bb) {
  /* LESSON 03 — Upload CPU pixels → GPU texture. No R↔B swap needed:
   * PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 reads bytes [R,G,B,A] from memory.
   * Our 0xAARRGGBB in LE memory is [R,G,B,A] at bytes [0,1,2,3] — correct. */
  UpdateTexture(g_raylib.texture, bb->pixels);

  int win_w = GetScreenWidth();
  int win_h = GetScreenHeight();

  /* LESSON 08 — Letterbox rectangle */
  float sx = (float)win_w / (float)bb->width;
  float sy = (float)win_h / (float)bb->height;
  float scale = MIN(sx, sy);
  float dst_w = (float)bb->width  * scale;
  float dst_h = (float)bb->height * scale;
  float off_x = ((float)win_w - dst_w) * 0.5f;
  float off_y = ((float)win_h - dst_h) * 0.5f;

  Rectangle src  = { 0.0f, 0.0f, (float)bb->width, (float)bb->height };
  Rectangle dest = { off_x, off_y, dst_w, dst_h };

  BeginDrawing();
  ClearBackground(BLACK);
  DrawTexturePro(g_raylib.texture, src, dest, (Vector2){0, 0}, 0.0f, WHITE);
  EndDrawing();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio drain (LESSON 11)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 11 — while loop: consume ALL processed buffers this frame.
 * IsAudioStreamProcessed returns true when Raylib's internal buffer needs
 * refilling.  Using `while` (not `if`) prevents the stream from draining
 * to silence if two buffers become ready in the same frame.              */
static void process_audio(GameAudioState *audio, AudioOutputBuffer *audio_buf) {
  if (!g_raylib.audio_initialized) return;

  if (!IsAudioStreamPlaying(g_raylib.audio_stream))
    PlayAudioStream(g_raylib.audio_stream);

  while (IsAudioStreamProcessed(g_raylib.audio_stream)) {
    int frames = g_raylib.buffer_size_frames;
    if (frames > audio_buf->max_sample_count)
      frames = audio_buf->max_sample_count;

    game_get_audio_samples(audio, audio_buf, frames);
    UpdateAudioStream(g_raylib.audio_stream, audio_buf->samples, frames);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  /* LESSON 01 — Create window and set target FPS.
   * SetTargetFPS(60) causes Raylib's EndDrawing() to sleep internally —
   * we don't need a manual sleep loop for Raylib.                        */
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(GAME_W, GAME_H, TITLE);
  SetTargetFPS(TARGET_FPS);

  if (!IsWindowReady()) {
    fprintf(stderr, "❌ Raylib: InitWindow failed\n");
    return 1;
  }

  /* Allow window resizing (letterbox handles the rest). */
  SetWindowState(FLAG_WINDOW_RESIZABLE);

  /* ── Backbuffer allocation ─────────────────────────────────────────── */
  Backbuffer bb = {
    .width           = GAME_W,
    .height          = GAME_H,
    .bytes_per_pixel = 4,
    .pitch           = GAME_W * 4,
  };
  bb.pixels = (uint32_t *)malloc((size_t)(GAME_W * GAME_H) * 4);
  if (!bb.pixels) { CloseWindow(); return 1; }
  memset(bb.pixels, 0, (size_t)(GAME_W * GAME_H) * 4);

  /* LESSON 03 — Create GPU texture from the backbuffer.
   * PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 — matches our [R,G,B,A] memory layout.
   * Both Raylib and X11 use the same [R,G,B,A] format; no R↔B swap needed.  */
  Image img = {
    .data    = bb.pixels,
    .width   = GAME_W,
    .height  = GAME_H,
    .mipmaps = 1,
    .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
  };
  g_raylib.texture = LoadTextureFromImage(img);
  SetTextureFilter(g_raylib.texture, TEXTURE_FILTER_POINT);

  if (!IsTextureValid(g_raylib.texture)) {
    fprintf(stderr, "❌ Raylib: LoadTextureFromImage failed\n");
    free(bb.pixels);
    CloseWindow();
    return 1;
  }

  /* ── Audio buffer + config ─────────────────────────────────────────── */
  AudioOutputBuffer audio_buf = {
    .sample_count     = AUDIO_CHUNK_SIZE,
    .max_sample_count = AUDIO_CHUNK_SIZE,
  };
  audio_buf.samples = (int16_t *)malloc((size_t)AUDIO_CHUNK_SIZE * AUDIO_BYTES_PER_FRAME);
  if (!audio_buf.samples) { free(bb.pixels); CloseWindow(); return 1; }
  memset(audio_buf.samples, 0, (size_t)AUDIO_CHUNK_SIZE * AUDIO_BYTES_PER_FRAME);

  PlatformAudioConfig audio_cfg = {
    .sample_rate = AUDIO_SAMPLE_RATE,
    .channels    = AUDIO_CHANNELS,
    .chunk_size  = AUDIO_CHUNK_SIZE,
  };
  if (init_audio(&audio_cfg, &audio_buf) != 0) {
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

  printf("✓ Platform initialized (Raylib backend)\n");

  /* ── Main loop ─────────────────────────────────────────────────────── */
  while (!WindowShouldClose()) {
    /* LESSON 07 — Double-buffered input swap + prepare */
    platform_swap_input_buffers(&curr_input, &prev_input);
    prepare_input_frame(curr_input, prev_input);

    /* LESSON 07 — Poll Raylib input into GameInput */
    if (IsKeyDown(KEY_ESCAPE) || IsKeyDown(KEY_Q)) {
      UPDATE_BUTTON(&curr_input->buttons.quit, 1);
      break;
    }
    if (IsKeyDown(KEY_SPACE)) {
      UPDATE_BUTTON(&curr_input->buttons.play_tone, 1);
    } else {
      UPDATE_BUTTON(&curr_input->buttons.play_tone, 0);
    }

    /* Quit on Q pressed */
    if (curr_input->buttons.quit.ended_down) break;

    /* LESSON 09/10 — Trigger demo tone on space press */
    if (button_just_pressed(curr_input->buttons.play_tone)) {
      game_play_sound_at(&audio_state, SOUND_TONE_MID);
    }

    /* LESSON 13 — Advance game-time audio state */
    float dt_ms = GetFrameTime() * 1000.0f;
    game_audio_update(&audio_state, dt_ms);

    /* LESSON 11 — Fill audio stream buffers */
    process_audio(&audio_state, &audio_buf);

    /* LESSON 03/08 — Render demo frame + display */
    demo_render(&bb, curr_input, GetFPS());
    display_backbuffer(&bb);
  }

  /* ── Cleanup ───────────────────────────────────────────────────────── */
  shutdown_audio();
  if (IsTextureValid(g_raylib.texture))
    UnloadTexture(g_raylib.texture);
  free(audio_buf.samples);
  free(bb.pixels);
  CloseWindow();
  return 0;
}
