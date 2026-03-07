/* ═══════════════════════════════════════════════════════════════════════════
 * audio.h  —  Snake Game Audio
 * ═══════════════════════════════════════════════════════════════════════════
 */

#ifndef GAME_AUDIO_H
#define GAME_AUDIO_H

#include "../utils/audio.h"
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Sound Effect Identifiers
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef enum {
  SOUND_NONE = 0,
  SOUND_FOOD_EATEN,
  SOUND_GROW,
  SOUND_RESTART,
  SOUND_GAME_OVER,
  SOUND_COUNT
} SOUND_ID;

/* ═══════════════════════════════════════════════════════════════════════════
 * Sound Instance
 * ═══════════════════════════════════════════════════════════════════════════
 */
#define MAX_SIMULTANEOUS_SOUNDS 4

typedef struct {
  SOUND_ID sound_id;
  float phase;
  int samples_remaining;
  float volume;
  float frequency;
  float frequency_slide;
  int total_samples;
  int fade_in_samples;
  float pan_position;
} SoundInstance;

/* ═══════════════════════════════════════════════════════════════════════════
 * Tone Generator (for background music)
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct {
  float phase;
  float frequency;
  float volume;
  float pan_position;
  float current_volume;
  int is_playing;
  int fade_samples_remaining; /* NEW: Track fade-in/out progress */
} ToneGenerator;

#define MUSIC_FADE_SAMPLES 512 /* ~10ms at 48kHz - smooth transition */

/* ═══════════════════════════════════════════════════════════════════════════
 * Music Sequencer
 * ═══════════════════════════════════════════════════════════════════════════
 */
#define MUSIC_PATTERN_LENGTH 16
#define MUSIC_NUM_PATTERNS 4

typedef struct {
  uint8_t patterns[MUSIC_NUM_PATTERNS][MUSIC_PATTERN_LENGTH];
  int current_pattern;
  int current_step;
  int step_duration_samples;  /* NEW: step duration in samples */
  int step_samples_remaining; /* NEW: samples until next step */
  // float step_timer;             /* DEPRECATED: kept for compatibility */
  // float step_duration;          /* DEPRECATED: kept for compatibility */
  ToneGenerator tone;
  int is_playing;
} MusicSequencer;

/* ═══════════════════════════════════════════════════════════════════════════
 * Game Audio State
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct {
  SoundInstance active_sounds[MAX_SIMULTANEOUS_SOUNDS];
  int active_sound_count;

  MusicSequencer music;

  float master_volume;
  float sfx_volume;
  float music_volume;

  int samples_per_second;
} GameAudioState;

/* ═══════════════════════════════════════════════════════════════════════════
 * Function Declarations
 * ═══════════════════════════════════════════════════════════════════════════
 */

void game_audio_init(GameAudioState *audio);
void game_play_sound(GameAudioState *audio, SOUND_ID sound);
void game_play_sound_at(GameAudioState *audio, SOUND_ID sound,
                        float pan_position);
void game_music_play(GameAudioState *audio);
void game_music_stop(GameAudioState *audio);
void game_get_audio_samples(GameAudioState *audio, AudioOutputBuffer *buffer);
void game_audio_update(GameAudioState *audio, float delta_time);

#endif /* GAME_AUDIO_H */
