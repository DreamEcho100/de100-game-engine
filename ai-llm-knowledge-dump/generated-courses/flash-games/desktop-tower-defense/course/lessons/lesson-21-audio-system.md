# Lesson 21: Audio System

## What we're building

A complete, self-contained audio system built entirely from sine-wave
synthesis — no audio files, no library decoding, just C maths producing PCM
samples that the platform streams to the speaker.  Each tower type has a
distinct fire sound.  Creep deaths, life losses, wave completions, game-over,
and victory all have unique tones.  Background music loops quietly throughout.
The system works on both Raylib (`UpdateAudioStream`) and X11/ALSA
(`snd_pcm_writei`).

## What you'll learn

- The **phase accumulator** pattern: how a single float drives a sine-wave
  oscillator with no drift
- How to mix multiple simultaneous sounds by summing floats and clamping at
  the end
- Linear attack/release envelopes that prevent the audible click caused by
  abrupt waveform starts and stops
- How `game_get_audio_samples()` decouples sound logic from platform audio
  APIs
- Raylib `IsAudioStreamProcessed` / `UpdateAudioStream` integration
- ALSA `snd_pcm_writei` with a low-latency start threshold

## Prerequisites

- Lessons 01–20 complete: all `game_play_sound()` call sites (tower fire,
  creep death, life lost, etc.) can be stubbed until this lesson
- `platform_audio_init`, `platform_audio_update`, `platform_audio_shutdown`
  stubs exist in both backends (added in Lesson 01)
- `<math.h>` available

---

## Step 1: `SfxId` enum and `SoundDef` table

```c
/* src/game.h — SfxId enum */
typedef enum {
    SFX_TOWER_FIRE_PELLET  =  0,
    SFX_TOWER_FIRE_SQUIRT  =  1,
    SFX_TOWER_FIRE_DART    =  2,
    SFX_TOWER_FIRE_SNAP    =  3,
    SFX_TOWER_FIRE_SWARM   =  4,
    SFX_TOWER_FIRE_FROST   =  5,
    SFX_TOWER_FIRE_BASH    =  6,
    SFX_CREEP_DEATH        =  7,
    SFX_BOSS_DEATH         =  8,
    SFX_LIFE_LOST          =  9,
    SFX_TOWER_PLACE        = 10,
    SFX_TOWER_SELL         = 11,
    SFX_WAVE_COMPLETE      = 12,
    SFX_GAME_OVER          = 13,
    SFX_VICTORY            = 14,
    SFX_COUNT              = 15
} SfxId;
```

Add the `SoundDef` struct and table to `audio.c`:

```c
/* src/audio.c — SoundDef: frequency and duration for each sound effect */
typedef struct {
    float frequency;   /* Hz — fundamental pitch                    */
    float duration;    /* seconds                                    */
    float volume;      /* 0..1, mixed at this level                  */
} SoundDef;

static const SoundDef SOUND_DEFS[SFX_COUNT] = {
/*  id                      freq     dur    vol  */
  [SFX_TOWER_FIRE_PELLET] = { 880.0f, 0.05f, 0.25f },
  [SFX_TOWER_FIRE_SQUIRT] = { 440.0f, 0.08f, 0.30f },
  [SFX_TOWER_FIRE_DART]   = { 660.0f, 0.06f, 0.28f },
  [SFX_TOWER_FIRE_SNAP]   = {1100.0f, 0.04f, 0.35f },
  [SFX_TOWER_FIRE_SWARM]  = { 320.0f, 0.10f, 0.20f },
  [SFX_TOWER_FIRE_FROST]  = { 220.0f, 0.12f, 0.30f },
  [SFX_TOWER_FIRE_BASH]   = { 150.0f, 0.14f, 0.40f },
  [SFX_CREEP_DEATH]       = { 500.0f, 0.08f, 0.40f },
  [SFX_BOSS_DEATH]        = {  80.0f, 0.60f, 0.60f },
  [SFX_LIFE_LOST]         = { 200.0f, 0.30f, 0.70f },
  [SFX_TOWER_PLACE]       = { 740.0f, 0.07f, 0.35f },
  [SFX_TOWER_SELL]        = { 370.0f, 0.10f, 0.30f },
  [SFX_WAVE_COMPLETE]     = { 660.0f, 0.40f, 0.50f },
  [SFX_GAME_OVER]         = { 100.0f, 1.20f, 0.60f },
  [SFX_VICTORY]           = { 880.0f, 1.50f, 0.60f },
};
```

**What's happening:**

- Tower fire sounds are short (40–140 ms) to avoid muddy overlapping; lower
  frequencies are assigned to heavier towers (Bash at 150 Hz feels like an
  impact).
- `SFX_BOSS_DEATH` at 80 Hz with 0.6 s is a deep, satisfying boom.
- `SFX_LIFE_LOST` at 200 Hz is loud (vol 0.70) so the player immediately
  notices it.

---

## Step 2: `SoundInstance` pool in `GameAudio`

```c
/* src/game.h — SoundInstance and GameAudio */
#define MAX_SOUND_INSTANCES 32
#define AUDIO_SAMPLE_RATE   44100
#define AUDIO_CHUNK_SIZE    2048   /* frames per platform update          */

typedef struct {
    int   active;
    float frequency;
    float volume;
    float phase;           /* 0..1, normalized oscillator phase    */
    float time_remaining;  /* seconds left                         */
    float duration;        /* total duration (used for envelope)   */
    float pan_position;    /* -1 = full left, 0 = centre, +1 = right */
} SoundInstance;

typedef struct {
    SoundInstance instances[MAX_SOUND_INSTANCES];
    /* Background music tone generator */
    float    music_phase;
    float    music_volume;   /* ramped 0 → 0.15 over 2 s at game start */
    int      sample_rate;
} GameAudio;
```

---

## Step 3: `game_play_sound()`

```c
/* src/audio.c — game_play_sound(): activate a free SoundInstance */
void game_play_sound(GameAudio *audio, SfxId id) {
    ASSERT(id >= 0 && id < SFX_COUNT);
    const SoundDef *def = &SOUND_DEFS[id];

    /* Find an inactive slot; if all are busy, steal the quietest one */
    SoundInstance *slot = NULL;
    float min_vol = 1e9f;
    for (int i = 0; i < MAX_SOUND_INSTANCES; i++) {
        if (!audio->instances[i].active) {
            slot = &audio->instances[i];
            break;
        }
        if (audio->instances[i].volume < min_vol) {
            min_vol = audio->instances[i].volume;
            slot    = &audio->instances[i];
        }
    }

    slot->active         = 1;
    slot->frequency      = def->frequency;
    slot->volume         = def->volume;
    slot->phase          = 0.0f;
    slot->time_remaining = def->duration;
    slot->duration       = def->duration;
    slot->pan_position   = 0.0f;  /* centre; caller may override */
}
```

---

## Step 4: Wire all audio call sites

```c
/* src/towers.c — tower_update(): fire sounds per tower type */
static const SfxId TOWER_FIRE_SFX[TOWER_COUNT] = {
    [TOWER_PELLET] = SFX_TOWER_FIRE_PELLET,
    [TOWER_SQUIRT] = SFX_TOWER_FIRE_SQUIRT,
    [TOWER_DART]   = SFX_TOWER_FIRE_DART,
    [TOWER_SNAP]   = SFX_TOWER_FIRE_SNAP,
    [TOWER_SWARM]  = SFX_TOWER_FIRE_SWARM,
    [TOWER_FROST]  = SFX_TOWER_FIRE_FROST,
    [TOWER_BASH]   = SFX_TOWER_FIRE_BASH,
};

/* Inside the fire section of tower_update(): */
game_play_sound(state->audio, TOWER_FIRE_SFX[t->type]);
```

```c
/* src/creeps.c — kill_creep(): death sounds */
game_play_sound(state->audio,
                (c->type == CREEP_BOSS) ? SFX_BOSS_DEATH : SFX_CREEP_DEATH);

/* src/game.c — exit handler: life-lost sound */
game_play_sound(state->audio, SFX_LIFE_LOST);

/* src/game.c — place_tower(): placement sound */
game_play_sound(state->audio, SFX_TOWER_PLACE);

/* src/game.c — sell_tower(): sell sound */
game_play_sound(state->audio, SFX_TOWER_SELL);

/* src/game.c — check_wave_complete() → WAVE_CLEAR: wave complete sound */
game_play_sound(state->audio, SFX_WAVE_COMPLETE);

/* src/game.c — change_phase() → GAME_OVER: game-over sound */
if (new_phase == GAME_PHASE_GAME_OVER)
    game_play_sound(state->audio, SFX_GAME_OVER);

/* src/game.c — change_phase() → VICTORY: victory sound */
if (new_phase == GAME_PHASE_VICTORY)
    game_play_sound(state->audio, SFX_VICTORY);
```

---

## Step 5: `game_get_audio_samples()` — PCM generation loop

```c
/* src/audio.c — game_get_audio_samples(): fill one chunk of PCM */
#include <stdint.h>
#include <math.h>

#define FADE_SAMPLES 176   /* 4 ms at 44100 Hz = 176.4 → 176 frames */

typedef struct {
    int16_t samples[AUDIO_CHUNK_SIZE * 2]; /* interleaved L / R */
} AudioChunk;

void game_get_audio_samples(GameAudio *audio, AudioChunk *out) {
    for (int i = 0; i < AUDIO_CHUNK_SIZE; i++) {
        float left  = 0.0f;
        float right = 0.0f;

        /* --- Active sound instances --- */
        for (int s = 0; s < MAX_SOUND_INSTANCES; s++) {
            SoundInstance *inst = &audio->instances[s];
            if (!inst->active) continue;

            /* Sine sample */
            float sample = sinf(inst->phase * 6.283185f) * inst->volume;

            /* 4 ms linear fade-in (click prevention at attack) */
            float elapsed   = inst->duration - inst->time_remaining;
            float fade_secs = (float)FADE_SAMPLES / (float)AUDIO_SAMPLE_RATE;
            if (elapsed < fade_secs)
                sample *= elapsed / fade_secs;

            /* 4 ms linear fade-out (click prevention at release) */
            if (inst->time_remaining < fade_secs)
                sample *= inst->time_remaining / fade_secs;

            /* Pan: pan_position in [-1..1]
             * Positive pan = right channel louder  */
            float pan    = inst->pan_position;
            float l_gain = 1.0f - (pan >  0.0f ?  pan : 0.0f);
            float r_gain = 1.0f - (pan < -0.0f ? -pan : 0.0f);

            left  += sample * l_gain;
            right += sample * r_gain;

            /* Advance phase accumulator (wrap at 1.0) */
            inst->phase += inst->frequency / (float)AUDIO_SAMPLE_RATE;
            if (inst->phase >= 1.0f) inst->phase -= 1.0f;

            /* Countdown and deactivate */
            inst->time_remaining -= 1.0f / (float)AUDIO_SAMPLE_RATE;
            if (inst->time_remaining <= 0.0f) inst->active = 0;
        }

        /* --- Background music (simple drone) --- */
        float music_sample = sinf(audio->music_phase * 6.283185f)
                           * audio->music_volume;
        audio->music_phase += 110.0f / (float)AUDIO_SAMPLE_RATE; /* A2 note */
        if (audio->music_phase >= 1.0f) audio->music_phase -= 1.0f;

        left  += music_sample;
        right += music_sample;

        /* --- Clamp and convert to 16-bit --- */
#define CLAMP_F(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
        out->samples[i * 2]     = (int16_t)(CLAMP_F(left,  -1.0f, 1.0f) * 32767.0f);
        out->samples[i * 2 + 1] = (int16_t)(CLAMP_F(right, -1.0f, 1.0f) * 32767.0f);
#undef CLAMP_F
    }
}
```

**What's happening:**

- `inst->phase` is a normalized oscillator phase in `[0, 1)`.  One full sine
  cycle corresponds to `phase` going from 0 to 1.  Incrementing by
  `frequency / sample_rate` each sample gives exactly `frequency` cycles per
  second, regardless of the actual value — this is the **phase accumulator**
  pattern.
- The 4 ms fade-in and fade-out ramp sample amplitude from zero to full and
  back to zero over `FADE_SAMPLES` frames.  Without this ramp, an abrupt jump
  from 0 to full amplitude produces a high-frequency click audible even at low
  volumes.
- Pan is a simple linear cross-fade: `l_gain + r_gain = 2 - |pan|`.  A centre
  pan (`pan = 0`) gives both channels full gain; hard-right gives the right
  channel full gain and the left channel zero.
- Summing all instances before the final clamp is intentional — the clamp at
  `±1.0f` acts as a simple limiter.

---

## Step 6: Background music volume ramp

```c
/* src/audio.c — game_audio_update(): ramp music volume at game start */
void game_audio_update(GameAudio *audio, float dt) {
    /* Ramp from 0 → 0.15 over the first 2 seconds of play */
    if (audio->music_volume < 0.15f) {
        audio->music_volume += dt * (0.15f / 2.0f);
        if (audio->music_volume > 0.15f) audio->music_volume = 0.15f;
    }
}
```

Call `game_audio_update(state->audio, dt)` each frame from `game_update()`.

---

## Step 7: Raylib platform backend integration

```c
/* src/main_raylib.c — platform_audio_init() */
static AudioStream g_audio_stream;
static AudioChunk  g_audio_chunk;

void platform_audio_init(GameState *state, int samples_per_second) {
    InitAudioDevice();
    g_audio_stream = LoadAudioStream(samples_per_second, 16, 2);
    PlayAudioStream(g_audio_stream);
    state->audio->sample_rate = samples_per_second;
}

/* src/main_raylib.c — platform_audio_update() */
void platform_audio_update(GameState *state) {
    /* Only fill the buffer when Raylib has consumed the previous chunk */
    if (IsAudioStreamProcessed(g_audio_stream)) {
        game_get_audio_samples(state->audio, &g_audio_chunk);
        UpdateAudioStream(g_audio_stream,
                          g_audio_chunk.samples,
                          AUDIO_CHUNK_SIZE);
    }
}

/* src/main_raylib.c — platform_audio_shutdown() */
void platform_audio_shutdown(void) {
    StopAudioStream(g_audio_stream);
    UnloadAudioStream(g_audio_stream);
    CloseAudioDevice();
}
```

**What's happening:**

- `IsAudioStreamProcessed` returns true when Raylib's internal buffer has
  been fully consumed — this is the correct gate to avoid overwriting samples
  that are still playing.
- `AUDIO_CHUNK_SIZE = 2048` frames at 44 100 Hz ≈ 46 ms of audio per chunk.
  This is a good trade-off: small enough to keep latency below a frame, large
  enough to avoid the overhead of very frequent updates.

---

## Step 8: X11/ALSA platform backend integration

```c
/* src/main_x11.c — ALSA audio: compile with -lasound */
#ifdef USE_ALSA
#include <alsa/asoundlib.h>

static snd_pcm_t *g_pcm;
static AudioChunk  g_audio_chunk;

void platform_audio_init(GameState *state, int samples_per_second) {
    state->audio->sample_rate = samples_per_second;

    snd_pcm_open(&g_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    snd_pcm_set_params(g_pcm,
                       SND_PCM_FORMAT_S16_LE,     /* signed 16-bit little-endian */
                       SND_PCM_ACCESS_RW_INTERLEAVED,
                       2,                          /* stereo                      */
                       samples_per_second,
                       1,                          /* allow resampling            */
                       50000);                     /* 50 ms latency target        */

    /* Set start threshold to 1 frame so ALSA starts immediately on the
     * first write rather than waiting for the buffer to fill. */
    snd_pcm_sw_params_t *sw;
    snd_pcm_sw_params_alloca(&sw);
    snd_pcm_sw_params_current(g_pcm, sw);
    snd_pcm_sw_params_set_start_threshold(g_pcm, sw, 1);
    snd_pcm_sw_params(g_pcm, sw);
}

void platform_audio_update(GameState *state) {
    game_get_audio_samples(state->audio, &g_audio_chunk);

    snd_pcm_sframes_t written =
        snd_pcm_writei(g_pcm, g_audio_chunk.samples, AUDIO_CHUNK_SIZE);

    /* Recover from underruns automatically */
    if (written == -EPIPE) {
        snd_pcm_prepare(g_pcm);
    }
}

void platform_audio_shutdown(void) {
    snd_pcm_drain(g_pcm);
    snd_pcm_close(g_pcm);
}
#else
/* Stub when ALSA is not available */
void platform_audio_init(GameState *s, int hz) { (void)s; (void)hz; }
void platform_audio_update(GameState *s)        { (void)s; }
void platform_audio_shutdown(void)              {}
#endif /* USE_ALSA */
```

**What's happening:**

- `snd_pcm_sw_params_set_start_threshold(pcm, sw, 1)` starts the ALSA stream
  the moment the first sample frame is written.  The default threshold is
  equal to the full hardware buffer, which adds a full buffer's worth of
  latency (~100 ms) before the first sound plays.
- `snd_pcm_writei` blocks briefly if the ALSA ring buffer is full — this acts
  as backpressure that keeps `platform_audio_update` running at the correct
  rate.
- `-EPIPE` is the ALSA "underrun" error code.  It occurs when the application
  writes too slowly and the hardware runs out of samples.  `snd_pcm_prepare`
  resets the stream so the next write succeeds.

---

## Build and run

```bash
mkdir -p build

# Raylib (audio enabled automatically)
clang -Wall -Wextra -std=c99 -O2 \
      -o build/dtd \
      src/main_raylib.c src/game.c src/render.c \
      src/creeps.c src/towers.c src/levels.c src/bfs.c src/audio.c \
      -lraylib -lm
./build/dtd

# X11 without ALSA (silent)
clang -Wall -Wextra -std=c99 -O2 \
      -o build/dtd \
      src/main_x11.c src/game.c src/render.c \
      src/creeps.c src/towers.c src/levels.c src/bfs.c src/audio.c \
      -lX11 -lm
./build/dtd

# X11 with ALSA
clang -Wall -Wextra -std=c99 -O2 -DUSE_ALSA \
      -o build/dtd \
      src/main_x11.c src/game.c src/render.c \
      src/creeps.c src/towers.c src/levels.c src/bfs.c src/audio.c \
      -lX11 -lasound -lm
./build/dtd
```

**Expected:** Each tower type produces a distinct pitch when firing.  Creep
deaths produce a short pop; boss death produces a deep extended boom.  Losing a
life plays a loud low-frequency warning.  Wave-complete, game-over, and victory
each play a clearly recognisable tone.  A quiet 110 Hz drone fades in over the
first two seconds of gameplay.  No audio glitches or clicks on either platform.

---

## Exercises

1. **Beginner** — Add a second harmonic to the `SFX_VICTORY` sound by calling
   `game_play_sound` twice in rapid succession: once at 880 Hz and once at
   1320 Hz (a perfect fifth above).  Verify the chord is audible at game
   completion.

2. **Intermediate** — Spatial audio: when a tower fires, set
   `inst->pan_position` based on the tower's horizontal grid position:
   `pan = (tower_col / (float)GRID_COLS) * 2.0f - 1.0f`
   so towers on the left pan left and towers on the right pan right.
   Verify with headphones that a left-column tower sounds different from a
   right-column tower.

3. **Challenge** — Replace the fixed 110 Hz background drone with a simple
   4-note melody that loops.  Store a `float melody_notes[4]` array and a
   `melody_index` counter in `GameAudio`.  Advance `melody_index` every
   `melody_note_timer` seconds (e.g., 0.5 s per note) and set
   `audio->music_phase = 0.0f` with the new frequency each time.

---

## What's next

Congratulations — you have built a complete Desktop Tower Defense clone in C
from scratch: BFS pathfinding, seven tower types, seven creep types, a full
economy system, polished visuals, and synthesised audio.  All of it runs on a
plain pixel backbuffer with no game-engine dependencies.

From here you could explore:

- **Serialisation**: save/load game state to a binary file using `fwrite` /
  `fread` so a session can be resumed
- **Replay system**: record all random seeds and input events; replay them
  deterministically for debugging or sharing
- **WASM port**: compile with `emcc` and replace the platform backends with
  Canvas 2D + Web Audio API calls
- **Level editor**: render the grid to a `GameBackbuffer` in a separate tool
  and write `WaveDef` arrays to `levels.c` from a GUI
