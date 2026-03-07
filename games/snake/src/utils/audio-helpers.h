#ifndef AUDIO_HELPERS_H
#define AUDIO_HELPERS_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * MUSIC SEQUENCER PRIMITIVES
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * The engine provides:
 *   - Note/step timing helpers
 *   - MIDI conversion
 *   - Tone generation with smooth transitions
 *
 * The GAME defines:
 *   - Pattern data (what notes to play)
 *   - Pattern count and length
 *   - How patterns are arranged
 *
 * This allows any game to implement its own music style:
 *   - Snake: Simple 4-pattern loop
 *   - Tetris: Multiple tracks with bass line
 *   - RPG: Dynamic music that changes with game state
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── MIDI Conversion ───────────────────────────────────────────────────────
 */

/* Convert MIDI note number to frequency (A4 = 69 = 440Hz) */
static inline float audio_midi_to_freq(int32_t note) {
  if (note == 0)
    return 0.0f; /* 0 = rest/silence */
  return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

/* Common MIDI note constants */
#define DE100_MIDI_C4 60
#define DE100_MIDI_D4 62
#define DE100_MIDI_E4 64
#define DE100_MIDI_F4 65
#define DE100_MIDI_G4 67
#define DE100_MIDI_A4 69
#define DE100_MIDI_B4 71
#define DE100_MIDI_C5 72
#define DE100_MIDI_D5 74
#define DE100_MIDI_E5 76
#define DE100_MIDI_G5 79
#define DE100_MIDI_A5 81
#define DE100_MIDI_REST 0

/* ─── Waveform Generators ───────────────────────────────────────────────────
 */

static inline float audio_wave_square(float phase) {
  return (phase < 0.5f) ? 1.0f : -1.0f;
}

static inline float audio_wave_sine(float phase) {
  return sinf(phase * 2.0f * (float)M_PI);
}

static inline float audio_wave_triangle(float phase) {
  if (phase < 0.25f)
    return phase * 4.0f;
  if (phase < 0.75f)
    return 2.0f - phase * 4.0f;
  return phase * 4.0f - 4.0f;
}

static inline float audio_wave_sawtooth(float phase) {
  return 2.0f * phase - 1.0f;
}

/* Pulse wave with configurable duty cycle (0.0 to 1.0) */
static inline float audio_wave_pulse(float phase, float duty) {
  return (phase < duty) ? 1.0f : -1.0f;
}

/* ─── Volume/Envelope Helpers ───────────────────────────────────────────────
 */

/* Smooth volume ramping (click-free transitions) */
static inline float audio_ramp_volume(float current, float target,
                                            float ramp_speed) {
  if (current < target) {
    current += ramp_speed;
    if (current > target)
      current = target;
  } else if (current > target) {
    current -= ramp_speed;
    if (current < 0.0f)
      current = 0.0f;
  }
  return current;
}

/* Default ramp speed (~2ms at 48kHz) */
#define DE100_AUDIO_RAMP_SPEED 0.002f

/* ─── Sequencer Step Timing ─────────────────────────────────────────────────
 */

/* Returns true when step should advance, handles timer internally */
static inline bool sequencer_should_advance(float *step_timer,
                                                  float step_duration,
                                                  float delta_time) {
  *step_timer += delta_time;
  if (*step_timer >= step_duration) {
    *step_timer -= step_duration;
    return true;
  }
  return false;
}

/* Calculate step duration from BPM and steps per beat */
static inline float
sequencer_bpm_to_step_duration(float bpm, int32_t steps_per_beat) {
  /* BPM = beats per minute
   * steps_per_beat = how many sequencer steps per beat (typically 4 for 16th
   * notes) step_duration = seconds per step */
  return 60.0f / (bpm * (float)steps_per_beat);
}

/* ─── Oscillator State ──────────────────────────────────────────────────────
 */

/* Generic oscillator that games can embed in their own structures */
typedef struct {
  float phase;          /* 0.0 to 1.0, wraps */
  float frequency;      /* Hz */
  float volume;         /* Target volume */
  float current_volume; /* Smoothed volume (for click-free) */
} De100Oscillator;

/* Advance oscillator phase, returns current phase before advance */
static inline float oscillator_advance(De100Oscillator *osc,
                                             float inv_sample_rate) {
  float current_phase = osc->phase;
  osc->phase += osc->frequency * inv_sample_rate;
  if (osc->phase >= 1.0f)
    osc->phase -= 1.0f;
  return current_phase;
}

/* Update oscillator volume with ramping */
static inline void oscillator_update_volume(De100Oscillator *osc,
                                                  bool is_playing,
                                                  float ramp_speed) {
  float target = is_playing ? osc->volume : 0.0f;
  osc->current_volume =
      audio_ramp_volume(osc->current_volume, target, ramp_speed);
}

/* ─── Sample Clamping ───────────────────────────────────────────────────────
 */

static inline int16_t audio_clamp_sample(float sample) {
  if (sample > 32767.0f)
    return 32767;
  if (sample < -32768.0f)
    return -32768;
  return (int16_t)sample;
}

/* ─── Stereo Panning ────────────────────────────────────────────────────────
 */

static inline void audio_calculate_pan(float pan, float *left_vol,
                                             float *right_vol) {
  /* Linear panning: pan -1.0 = full left, 0.0 = center, 1.0 = full right */
  if (pan <= 0.0f) {
    *left_vol = 1.0f;
    *right_vol = 1.0f + pan;
  } else {
    *left_vol = 1.0f - pan;
    *right_vol = 1.0f;
  }
}

#endif // AUDIO_HELPERS_H
