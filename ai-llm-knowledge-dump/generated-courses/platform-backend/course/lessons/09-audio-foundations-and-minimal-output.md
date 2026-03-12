# Lesson 09 — Audio Foundations and Minimal Output

## Overview

| Item | Value |
|------|-------|
| **Backend** | Both (minimal init on both so students hear output immediately) |
| **New concepts** | `AudioOutputBuffer`, interleaved stereo PCM, `GameAudioState`, phase accumulator, `audio_write_sample` |
| **Observable outcome** | Press Space → audible tone on BOTH backends by end of lesson |
| **Files created** | `src/utils/audio.h`, `src/game/audio_demo.c`, `src/game/audio_demo.h` |
| **Files modified** | `src/platforms/raylib/main.c`, `src/platforms/x11/main.c` |

---

## What you'll build

A minimal audio pipeline: a CPU-side buffer filled with synthesized PCM samples, delivered to the platform's audio device.  Both backends connect to their audio device in this lesson so you can hear the result immediately.

---

## Background

### JS analogy

```js
// Web Audio API analogy
const ctx = new AudioContext();                   // platform_audio_init
const worklet = await ctx.audioWorklet.addModule(...);
// worklet.process() fills Float32Arrays →  game_get_audio_samples
// AudioWorkletNode connects to ctx.destination → platform_audio_write
```

`AudioOutputBuffer` is the `Float32Array` buffer.  `game_get_audio_samples` is the `process()` callback.

### Interleaved stereo

PCM audio for stereo uses **interleaved** layout:

```
[L0, R0, L1, R1, L2, R2, ...]
```

One pair (Li, Ri) is one "sample frame".  At 44100 Hz, 60 fps = 735 sample frames per game frame.

The `AudioOutputBuffer` stores `int16_t` (16-bit signed):
```c
typedef struct {
    int16_t *samples;        // Allocated by platform
    int      sample_count;   // Capacity (frames)
    int      max_sample_count; // Safety cap
} AudioOutputBuffer;
```

### Phase accumulator

The simplest synthesis technique: advance a phase variable by `freq / sample_rate` each sample.  When phase crosses 0.5, the square wave flips:

```c
gen->phase_acc += gen->frequency / (float)AUDIO_SAMPLE_RATE;
if (gen->phase_acc >= 1.0f) gen->phase_acc -= 1.0f;
float wave = (gen->phase_acc < 0.5f) ? 1.0f : -1.0f;
```

At 440 Hz (A4), phase advances by `440/44100 ≈ 0.01` per sample — completing one full cycle every 100 samples (2.27 ms).

### `audio_write_sample` helper

```c
static inline void audio_write_sample(AudioOutputBuffer *buf,
                                      int frame_index, float l, float r) {
    if (l >  1.0f) l =  1.0f;
    if (l < -1.0f) l = -1.0f;
    if (r >  1.0f) r =  1.0f;
    if (r < -1.0f) r = -1.0f;
    buf->samples[frame_index * AUDIO_CHANNELS + 0] = (int16_t)(l * 32767.0f);
    buf->samples[frame_index * AUDIO_CHANNELS + 1] = (int16_t)(r * 32767.0f);
}
```

All format-specific details (int16_t cast, channel count, frame stride) live here.  If a future course upgrades to float32 PCM, only this function changes.

### Two-loop audio architecture

```
GAME LOOP (≈60 Hz):
  game_audio_update(state, dt_ms)  → advances sequencer, slides, removes expired voices
  process_audio(...)               → calls platform_audio_get_samples_to_write + game_get_audio_samples

AUDIO LOOP (callback / drain):
  game_get_audio_samples(state, buf, n) → synthesizes PCM; reads state read-mostly
```

Separating "advance state" from "synthesize samples" prevents the audio thread from stalling on game logic.

---

## What to type

### Minimal Raylib audio (what to add in L09)

```c
// In init_audio():
InitAudioDevice();
SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE);
audio_stream = LoadAudioStream(AUDIO_SAMPLE_RATE, 16, AUDIO_CHANNELS);
PlayAudioStream(audio_stream);
// → you can hear a tone when Space is pressed!

// In process_audio():
if (IsAudioStreamProcessed(audio_stream)) {
    int frames = AUDIO_CHUNK_SIZE;
    game_get_audio_samples(&audio_state, &audio_buf, frames);
    UpdateAudioStream(audio_stream, audio_buf.samples, frames);
}
```

This minimal version works but may stutter if `IsAudioStreamProcessed` returns true for two buffers in the same frame.  **Lesson 11** replaces the `if` with `while` and adds pre-fill of two silent buffers.

### Minimal X11 audio (what to add in L09)

```c
// In platform_audio_init():
snd_pcm_t *pcm;
snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
snd_pcm_set_params(pcm,
    SND_PCM_FORMAT_S16_LE,
    SND_PCM_ACCESS_RW_INTERLEAVED,
    AUDIO_CHANNELS,
    AUDIO_SAMPLE_RATE,
    1,       /* allow resample */
    50000);  /* 50ms latency */
cfg->alsa_pcm = pcm;
// → you can hear a tone when Space is pressed!
```

Simple but gives ALSA full control of buffer sizing — can stutter under load.  **Lesson 12** replaces this with `snd_pcm_hw_params` for precise control.

### `src/utils/audio.h` (key excerpts)

```c
#define AUDIO_SAMPLE_RATE  44100
#define AUDIO_CHANNELS     2
#define AUDIO_CHUNK_SIZE   4096
#define AUDIO_BYTES_PER_FRAME (AUDIO_CHANNELS * sizeof(int16_t))

typedef struct {
    int16_t *samples;
    int      sample_count;
    int      max_sample_count;
} AudioOutputBuffer;

/* ToneGenerator — one SFX voice in the pool */
typedef struct {
    float phase_acc;
    float frequency;
    float freq_slide;       /* Hz/sec pitch sweep — introduced L10 */
    float volume;
    float pan;
    int   samples_remaining;
    int   total_samples;    /* set = samples_remaining on trigger; for decay env */
    int   fade_in_samples;  /* 88 = 2ms attack ramp; 0 = instant */
    int   active;
    int   age;
} ToneGenerator;

/* MusicTone — dedicated music oscillator with persistent phase */
typedef struct {
    float phase;
    float frequency;
    float volume;
    float current_volume;         /* smoothstep-faded working volume */
    float pan;
    int   is_playing;
    int   fade_samples_remaining;
} MusicTone;
#define MUSIC_FADE_SAMPLES 441    /* 10ms smoothstep fade */

#define MAX_SOUNDS 8
typedef struct {
    ToneGenerator  voices[MAX_SOUNDS];
    int            next_age;
    MusicSequencer sequencer;
    int            samples_per_second; /* set at init from AUDIO_SAMPLE_RATE */
    float          master_volume;
    float          sfx_volume;
    float          music_volume;
} GameAudioState;

/* audio_write_sample — introduced in L09 as a clean format-agnostic helper */
static inline void audio_write_sample(AudioOutputBuffer *buf,
                                      int idx, float l, float r) { ... }
```

> **audio-helpers.h (new in L10):** `src/utils/audio-helpers.h` provides stateless waveform generators (`audio_wave_square`, `audio_wave_sine`, etc.), `audio_midi_to_freq`, `audio_calculate_pan`, and `audio_clamp_sample`.  Copy it verbatim into any game course.

### `src/game/audio_demo.c` — `game_get_audio_samples` (fully annotated)

```c
void game_get_audio_samples(GameAudioState *audio, AudioOutputBuffer *buf,
                             int num_frames) {
    int frames  = (num_frames < buf->max_sample_count) ? num_frames : buf->max_sample_count;
    float inv_sr = 1.0f / (float)audio->samples_per_second;

    for (int i = 0; i < frames; i++) {
        float mix_l = 0.0f, mix_r = 0.0f;

        /* SFX voices ─────────────────────────────────────────────── */
        for (int v = 0; v < MAX_SOUNDS; v++) {
            ToneGenerator *gen = &audio->voices[v];
            if (!gen->active) continue;
            if (gen->samples_remaining == 0) { gen->active = 0; continue; }

            gen->phase_acc += gen->frequency * inv_sr;
            if (gen->phase_acc >= 1.0f) gen->phase_acc -= 1.0f;
            float wave = audio_wave_square(gen->phase_acc);

            // Full decay: amplitude tracks from 1.0→0.0 across entire note.
            // This eliminates the end-of-note click without a separate fade window.
            float env = 1.0f;
            if (gen->total_samples > 0) {
                env = (float)gen->samples_remaining / (float)gen->total_samples;
                // Fade-in: ramp from 0→1 over first fade_in_samples (≈2ms).
                // Prevents the waveform from jumping from silence to full
                // amplitude at note-start, which would cause a start-of-note click.
                int played = gen->total_samples - gen->samples_remaining;
                if (gen->fade_in_samples > 0 && played < gen->fade_in_samples)
                    env *= (float)played / (float)gen->fade_in_samples;
            }

            float sample = wave * gen->volume * env * audio->sfx_volume;
            float pan_l, pan_r;
            audio_calculate_pan(gen->pan, &pan_l, &pan_r);
            mix_l += sample * pan_l;
            mix_r += sample * pan_r;
            if (gen->samples_remaining > 0) gen->samples_remaining--;
        }

        /* Music + Final mix: see Lesson 13. */

        // Scale: master_volume × 16000 gives headroom for 4 simultaneous voices.
        float scale = audio->master_volume * 16000.0f;
        buf->samples[i * AUDIO_CHANNELS + 0] = audio_clamp_sample(mix_l * scale);
        buf->samples[i * AUDIO_CHANNELS + 1] = audio_clamp_sample(mix_r * scale);
    }
}
```

**Why `master_volume × 16000` instead of `32767`?**  With multiple voices summing, `mix_l` can exceed ±1.0.  Using 16000 (roughly half of 32767) gives headroom for 2× peak simultaneous sum before clipping.  `audio_clamp_sample` guards against any remaining overflow.

**Why two click-elimination mechanisms?**

| Mechanism | Eliminates | How |
|-----------|-----------|-----|
| `fade_in_samples = 88` (2ms) | Start-of-note click | Ramps amplitude 0→1 at note start |
| Full decay envelope (`remaining/total`) | End-of-note click | Amplitude smoothly reaches 0 at note end |

An end-only fade window (the older approach) only helped with note endings.  Combined fade-in + full decay ensures no discontinuity at either boundary.

---

## Explanation

| Concept | Detail |
|---------|--------|
| `AUDIO_CHUNK_SIZE = 4096` | ~93ms at 44100 Hz.  Matches Raylib's default buffer; gives ALSA enough headroom. |
| `phase_acc -= 1.0f` | Wrap by subtraction (not `fmod`) — avoids float precision drift over long sessions |
| `samples_remaining = ms * rate / 1000` | Convert duration in milliseconds to sample frame count |
| `max_sample_count` safety cap | `platform_audio_get_samples_to_write` already caps; `max_sample_count` is defence-in-depth |
| `AUDIO_MASTER_VOLUME = 0.6f` | Global output gain applied after mixing — normalises loudness across backends |
| `AUDIO_FADE_FRAMES = 441` | 10ms linear fade-out before voice deactivation — prevents end-of-note click artifacts |
| Minimal init (this lesson) | Simple, works, may stutter.  L11 (Raylib) and L12 (ALSA) fix the stutter. |

---

## Build and run

```sh
./build-dev.sh --backend=raylib -r
# → press Space → hear a 440 Hz square wave tone
./build-dev.sh --backend=x11 -r
# → press Space → same tone from ALSA
```

Both backends should produce audible output this lesson.

---

## Quiz

1. Why is PCM audio stored as `int16_t` (16-bit signed) instead of `uint8_t`?
2. What does `phase_acc >= 1.0f → phase_acc -= 1.0f` compute?  What pitch does `frequency = 440` produce?
3. Why does the two-loop architecture separate `game_audio_update` from `game_get_audio_samples`?
