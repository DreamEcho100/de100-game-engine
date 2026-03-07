#ifndef DE100_GAME_AUDIO_H
#define DE100_GAME_AUDIO_H

#include "../_common/base.h"
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * SOUND SOURCE (Individual audio generator)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * A simple oscillator with smooth volume transitions.
 * Used for continuous tones, music notes, etc.
 *
 * Games can use this directly or define their own structures.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  f32 phase;            /* Phase accumulator (0 to 1) */
  f32 frequency;        /* Current frequency (Hz) */
  f32 target_frequency; /* For smooth frequency transitions */
  f32 volume;           /* Target volume (0.0 to 1.0) */
  f32 current_volume;   /* Smoothed volume for click-free transitions */
  f32 pan_position;     /* -1.0 (left) to 1.0 (right) */
  bool is_playing;
} SoundSource;

/* ═══════════════════════════════════════════════════════════════════════════
 * GAME AUDIO STATE (Minimal - games extend this)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * This is a MINIMAL audio state. Games with more complex audio should
 * define their own GameAudioState in their game code.
 *
 * Example: Snake game defines its own with MusicSequencer, SoundInstance[],
 * etc.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  SoundSource tone;  /* Simple oscillator for basic games */
  f32 master_volume; /* Global volume multiplier */
} GameAudioState;

/* ═══════════════════════════════════════════════════════════════════════════
 * SOUND OUTPUT BUFFER (Platform → Game → Platform)
 *
 * This is the ONLY shared audio contract between the platform layer and
 * game code. Platform fills the metadata and allocates `samples`; the game
 * writes raw PCM into it via game_get_audio_samples().
 *
 * Deliberately minimal — backends own all backend-specific state privately.
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  i32 samples_per_second; /* Sample rate (e.g., 48000) — set once at init */
  i32 sample_count;     /* Samples to generate THIS call (≤ max_sample_count) */
  i32 max_sample_count; /* Buffer capacity — never write more than this */
  void *samples;        /* Pointer to sample buffer (i16 interleaved stereo) */
  bool is_initialized;  /* Platform audio subsystem is up; safe to generate */
} GameAudioOutputBuffer;

#endif /* DE100_GAME_AUDIO_H */
