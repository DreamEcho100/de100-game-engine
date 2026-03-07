#ifndef DE100_PLATFORMS_RAYLIB_AUDIO_H
#define DE100_PLATFORMS_RAYLIB_AUDIO_H

#include "../../_common/base.h"
#include "../../_common/memory.h"
#include "../../game/audio.h"
#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 RAYLIB SOUND OUTPUT STATE
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  AudioStream stream;
  bool stream_valid;
  bool stream_playing;

  // Buffer configuration
  u32 buffer_size_frames;

  // Sample buffer for game to fill
  De100MemoryBlock sample_buffer;
  u32 sample_buffer_size;

  // Track write statistics
  i64 total_samples_written;
  i32 writes_this_period;
  double last_stats_time;

} RaylibSoundOutput;

extern RaylibSoundOutput g_raylib_audio_output;

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 PUBLIC FUNCTION DECLARATIONS
// ═══════════════════════════════════════════════════════════════════════════

bool raylib_init_audio(GameAudioOutputBuffer *audio_output,
                       i32 samples_per_second, i32 game_update_hz);

void raylib_shutdown_audio(GameAudioOutputBuffer *audio_output);

/**
 * Check if we should generate audio this frame.
 * Returns number of samples to generate, or 0 if buffer is full.
 */
u32 raylib_get_samples_to_write(GameAudioOutputBuffer *audio_output);

/**
 * Send samples to Raylib audio stream.
 */
void raylib_send_samples(GameAudioOutputBuffer *audio_output);

void raylib_clear_audio_buffer(GameAudioOutputBuffer *audio_output);

void raylib_debug_audio_latency(GameAudioOutputBuffer *audio_output);

void raylib_audio_fps_change_handling(GameAudioOutputBuffer *audio_output);
void raylib_debug_audio_overlay(void);

#endif // DE100_PLATFORMS_RAYLIB_AUDIO_H
