#ifndef PLATFORMS_X11_AUDIO_H
#define PLATFORMS_X11_AUDIO_H

#include "../../utils/audio.h" /* AudioOutputBuffer */
#include "base.h"              /* X11AudioConfig */
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform Audio API  (X11 / ALSA)
 *
 * All three functions take X11AudioConfig* — this struct lives inside
 * g_x11.audio.config and is invisible to the shared platform.h layer.
 * ═══════════════════════════════════════════════════════════════════════════
 */

int platform_audio_init(X11AudioConfig *config,
                        AudioOutputBuffer *audio_buffer);
void platform_audio_shutdown(void);
int platform_audio_get_samples_to_write(X11AudioConfig *config,
                                        AudioOutputBuffer *audio_buffer);
void platform_audio_write(X11AudioConfig *config,
                          AudioOutputBuffer *audio_buffer,
                          int samples_to_write);

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio Helper Functions (used by game)
 * ═══════════════════════════════════════════════════════════════════════════
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Waveform generators */
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

/* MIDI to frequency */
static inline float audio_midi_to_freq(int note) {
  if (note == 0)
    return 0.0f;
  return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

/* Volume ramping (click-free transitions) */
static inline float audio_ramp_volume(float current, float target,
                                      float speed) {
  if (current < target) {
    current += speed;
    if (current > target)
      current = target;
  } else if (current > target) {
    current -= speed;
    if (current < 0.0f)
      current = 0.0f;
  }
  return current;
}

#endif // PLATFORMS_X11_AUDIO_H
