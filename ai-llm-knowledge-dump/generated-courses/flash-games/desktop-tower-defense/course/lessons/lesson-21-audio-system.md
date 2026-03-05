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
    SFX_TOWER_FIRE_PELLET  =  0,  /* 5 per-tower fire sounds */
    SFX_TOWER_FIRE_SQUIRT  =  1,
    SFX_TOWER_FIRE_DART    =  2,
    SFX_TOWER_FIRE_SNAP    =  3,
    SFX_TOWER_FIRE_SWARM   =  4,
    SFX_FROST_PULSE        =  5,  /* AoE pulse — not a "fire" sound */
    SFX_BASH_HIT           =  6,  /* AoE stun impact */
    SFX_CREEP_DEATH        =  7,
    SFX_BOSS_DEATH         =  8,
    SFX_LIFE_LOST          =  9,
    SFX_TOWER_PLACE        = 10,
    SFX_TOWER_SELL         = 11,
    SFX_WAVE_COMPLETE      = 12,
    SFX_GAME_OVER          = 13,
    SFX_VICTORY            = 14,
    SFX_WAVE_START         = 15,  /* plays when each wave begins */
    SFX_INTEREST_EARN      = 16,  /* soft clink when interest gold is added */
    SFX_EARLY_SEND         = 17,  /* chirp when player sends wave early */
    SFX_COUNT              = 18
} SfxId;
```

> **Design note:** Frost and Bash do not use "fire" SFX because they trigger
> an AoE pulse instead of launching a projectile.  They get distinct names
> (`SFX_FROST_PULSE`, `SFX_BASH_HIT`) that reflect what actually happens.
> `SFX_WAVE_START`, `SFX_INTEREST_EARN`, and `SFX_EARLY_SEND` were added to
> provide audio feedback for every economy event — without them the game feels
> unresponsive during the between-wave phase.

Add the `SoundDef` struct and table to `audio.c`.  The struct has **four**
fields — a frequency *slide* (start → end) allows descending or ascending
tones without a second oscillator:

```c
/* src/utils/audio.h — SoundDef */
typedef struct {
    float frequency;      /* start Hz                                */
    float frequency_end;  /* end Hz (0 = constant pitch)            */
    float duration_ms;    /* duration in milliseconds                */
    float volume;         /* 0.0–1.0                                 */
} SoundDef;

/* src/audio.c — one entry per SfxId */
static const SoundDef SOUND_DEFS[SFX_COUNT] = {
    /* SFX_TOWER_FIRE_PELLET  */ { 800.0f,  400.0f,  50.0f,  0.3f },
    /* SFX_TOWER_FIRE_SQUIRT  */ { 300.0f,  200.0f,  80.0f,  0.4f },
    /* SFX_TOWER_FIRE_DART    */ { 600.0f,  900.0f,  40.0f,  0.4f },
    /* SFX_TOWER_FIRE_SNAP    */ { 100.0f,   50.0f, 150.0f,  0.7f },
    /* SFX_TOWER_FIRE_SWARM   */ { 400.0f,  200.0f, 100.0f,  0.5f },
    /* SFX_FROST_PULSE        */ {1200.0f,  600.0f, 120.0f,  0.3f },
    /* SFX_BASH_HIT           */ { 150.0f,   80.0f, 100.0f,  0.6f },
    /* SFX_CREEP_DEATH        */ { 500.0f,  200.0f,  80.0f,  0.4f },
    /* SFX_BOSS_DEATH         */ {  80.0f,   40.0f, 400.0f,  0.8f },
    /* SFX_LIFE_LOST          */ { 200.0f,  100.0f, 250.0f,  0.7f },
    /* SFX_TOWER_PLACE        */ { 900.0f,  700.0f,  60.0f,  0.3f },
    /* SFX_TOWER_SELL         */ { 700.0f,  900.0f,  80.0f,  0.3f },
    /* SFX_WAVE_COMPLETE      */ { 440.0f,  880.0f, 200.0f,  0.5f },
    /* SFX_GAME_OVER          */ { 300.0f,  100.0f, 500.0f,  0.6f },
    /* SFX_VICTORY            */ { 440.0f,  880.0f, 600.0f,  0.7f },
    /* SFX_WAVE_START         */ { 350.0f,  550.0f, 120.0f,  0.4f },
    /* SFX_INTEREST_EARN      */ { 660.0f,  880.0f,  80.0f,  0.25f},
    /* SFX_EARLY_SEND         */ { 500.0f,  700.0f, 160.0f,  0.45f},
};
```

**What's happening:**

- Tower fire sounds slide downward (e.g., Pellet 800→400 Hz) for a "pew"
  feel; Dart slides *up* (600→900 Hz) for a sharper effect.
- `SFX_BOSS_DEATH` at 80 Hz with 0.4 s is a deep, satisfying boom.
- `SFX_LIFE_LOST` at 200 Hz is loud (vol 0.70) so the player immediately
  notices it.
- `SFX_INTEREST_EARN` is quiet (vol 0.25) — a soft metallic clink so it
  doesn't interrupt gameplay concentration.
- `SFX_WAVE_START` (350→550 Hz rising) signals the wave opening; `SFX_EARLY_SEND`
  (500→700 Hz) is slightly higher-pitched to convey "bonus earned".

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

In `game.c`, every game event that should produce audio needs a single
`game_play_sound()` call.  Here is the complete inventory:

```c
/* src/game.c — tower fire (inside update_towers(), after projectile spawn) */
SfxId sfx = SFX_TOWER_FIRE_PELLET;  /* default */
switch (t->type) {
    case TOWER_SQUIRT: sfx = SFX_TOWER_FIRE_SQUIRT; break;
    case TOWER_DART:   sfx = SFX_TOWER_FIRE_DART;   break;
    case TOWER_SNAP:   sfx = SFX_TOWER_FIRE_SNAP;   break;
    case TOWER_SWARM:  sfx = SFX_TOWER_FIRE_SWARM;  break;
    default: break;
}
game_play_sound(&s->audio, sfx);
/* NOTE: Frost and Bash are AoE — they play SFX_FROST_PULSE / SFX_BASH_HIT
 * in their own update branches, NOT via this table. */

/* src/game.c — Frost AoE pulse */
game_play_sound(&s->audio, SFX_FROST_PULSE);

/* src/game.c — Bash AoE stun */
game_play_sound(&s->audio, SFX_BASH_HIT);

/* src/game.c — creep death (kill_creep equivalent) */
game_play_sound(&s->audio,
    c->type == CREEP_BOSS ? SFX_BOSS_DEATH : SFX_CREEP_DEATH);

/* src/game.c — creep exits the grid (life lost) */
game_play_sound(&s->audio, SFX_LIFE_LOST);

/* src/game.c — tower placement and sell */
game_play_sound(&s->audio, SFX_TOWER_PLACE);
game_play_sound(&s->audio, SFX_TOWER_SELL);

/* src/game.c — change_phase(): phase-triggered sounds */
case GAME_PHASE_WAVE:       game_play_sound(&s->audio, SFX_WAVE_START);    break;
case GAME_PHASE_WAVE_CLEAR: game_play_sound(&s->audio, SFX_WAVE_COMPLETE); break;
case GAME_PHASE_GAME_OVER:  game_play_sound(&s->audio, SFX_GAME_OVER);     break;
case GAME_PHASE_VICTORY:    game_play_sound(&s->audio, SFX_VICTORY);       break;

/* src/game.c — start_wave(): interest earned */
if (interest > 0)
    game_play_sound(&s->audio, SFX_INTEREST_EARN);

/* src/game.c — send_wave_early(): early send bonus */
game_play_sound(&s->audio, SFX_EARLY_SEND);
```

> **Coverage check:** every distinct economy and combat event now has an
> audio call.  The full set covers: 5 projectile towers, 2 AoE towers,
> 2 creep deaths (normal + boss), 1 life-lost, 2 shop actions (place/sell),
> 4 phase transitions, 2 economy events (interest + early send).
> Total: **18 SFX** → matches `SFX_COUNT = 18`.

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

## ⚠️ Critical wiring: two calls you must not forget

These two omissions produce **complete audio silence** — no error messages,
no assertions, just nothing playing.  They are the most common audio bugs
when first building this system.

### Pitfall 1 — `game_audio_init()` must be called inside `game_init()`

`game_init()` starts with `memset(s, 0, sizeof(*s))` which zeros *all* fields
including `audio.master_volume` and `audio.music_tone`.  If you never call
`game_audio_init()`, `master_volume` stays `0.0f` and every sample is
multiplied by zero → silence.

```c
/* src/game.c — game_init(): MUST include this call */
void game_init(GameState *s) {
    memset(s, 0, sizeof(*s));
    /* ... grid / economy setup ... */

    /* CRITICAL: initialise audio AFTER memset or all volumes stay 0 */
    game_audio_init(&s->audio);
}
```

### Pitfall 2 — `game_audio_update()` must be called inside `game_update()`

`game_audio_update()` advances the music sequencer and ramps
`music_tone.current_volume` toward `target_volume`.  If it is never called
the ambient drone's `current_volume` stays `0.0f` → no music.

```c
/* src/game.c — game_update(): call at the very top, before phase switch */
void game_update(GameState *s, float dt) {
    if (dt > 0.1f) dt = 0.1f;

    /* CRITICAL: must be called every frame so the music volume ramps up */
    game_audio_update(&s->audio, dt);

    /* ... rest of update ... */
}
```

> **How to diagnose silent audio:** add a temporary `printf` inside
> `game_get_audio_samples()` to print `audio->master_volume` and
> `audio->music_tone.current_volume`.  If either is `0.0f`, trace back to
> whichever init/update call is missing.

---

## Step 6: Background music volume ramp

`game_audio_init()` sets `music_tone.target_volume = 0.3f` and
`music_tone.is_playing = 1` but leaves `current_volume = 0.0f`.
`game_audio_update()` ramps `current_volume` toward `target_volume` each
frame so the drone fades in smoothly instead of clicking to full volume:

```c
/* src/audio.c — game_audio_update(): ramp music volume + drift frequency */
void game_audio_update(GameAudioState *audio, float dt) {
    static float freq_timer = 0.0f;
    ToneGenerator *t = &audio->music_tone;

    /* Smooth volume ramp toward target (avoids mute clicks) */
    float target = t->is_playing ? t->target_volume : 0.0f;
    float step   = 0.1f * dt;
    if      (t->current_volume < target) t->current_volume = MIN(t->current_volume + step, target);
    else if (t->current_volume > target) t->current_volume = MAX(t->current_volume - step, target);

    /* Drift frequency every ~2 s for a living, shifting drone */
    if (t->is_playing) {
        freq_timer += dt;
        if (freq_timer >= 2.0f) {
            freq_timer -= 2.0f;
            static const float FREQ_OFFSETS[] = { 110.0f, 120.0f, 100.0f, 130.0f, 110.0f };
            static int freq_step = 0;
            freq_step   = (freq_step + 1) % 5;
            t->frequency = FREQ_OFFSETS[freq_step];
        }
    }
}
```

> **Signature:** takes `GameAudioState *` (not `GameAudio *`).  The struct
> is defined in `src/utils/audio.h`.  `game_audio_update()` is called at
> the top of `game_update()` each frame — see the Critical Wiring section
> above for why this must not be skipped.

---

## Step 7: Raylib platform backend integration

```c
/* src/main_raylib.c — platform_audio_init() */
static AudioStream g_audio_stream;
static int16_t     g_sample_buffer[AUDIO_CHUNK_SIZE * 4]; /* stereo headroom */
static int         g_audio_init = 0;

void platform_audio_init(GameState *state, int samples_per_second) {
    (void)state; /* game_audio_init() is called inside game_init() */

    InitAudioDevice();
    if (!IsAudioDeviceReady()) { g_audio_init = 0; return; }

    /* CRITICAL: set buffer size BEFORE LoadAudioStream */
    SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE);

    g_audio_stream = LoadAudioStream((unsigned int)samples_per_second, 16, 2);
    if (!IsAudioStreamValid(g_audio_stream)) {
        CloseAudioDevice();
        g_audio_init = 0;
        return;
    }
    PlayAudioStream(g_audio_stream);
    g_audio_init = 1;
}

/* src/main_raylib.c — platform_audio_update() */
void platform_audio_update(GameState *state) {
    if (!g_audio_init) return;
    if (!IsAudioStreamProcessed(g_audio_stream)) return;

    AudioOutputBuffer out = {
        .samples            = g_sample_buffer,
        .samples_per_second = AUDIO_SAMPLE_RATE,
        .sample_count       = AUDIO_CHUNK_SIZE,
    };
    game_get_audio_samples(state, &out);
    /* MUST pass exactly AUDIO_CHUNK_SIZE — a mismatch drifts the ring buffer */
    UpdateAudioStream(g_audio_stream, g_sample_buffer, AUDIO_CHUNK_SIZE);
}

/* src/main_raylib.c — platform_audio_shutdown() */
void platform_audio_shutdown(void) {
    if (!g_audio_init) return;
    StopAudioStream(g_audio_stream);
    UnloadAudioStream(g_audio_stream);
    CloseAudioDevice();
    g_audio_init = 0;
}
```
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
    game_get_audio_samples(state, &out);  /* takes full GameState* — not audio sub-struct */

    snd_pcm_sframes_t written =
        snd_pcm_writei(g_pcm, out.samples, AUDIO_CHUNK_SIZE);

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

Use `build-dev.sh` (introduced in Lesson 01):

```bash
# Raylib (audio enabled automatically)
./build-dev.sh --backend=raylib -r

# X11 without ALSA (silent, no audio)
./build-dev.sh --backend=x11 -r

# X11 with ALSA (audio enabled)
# Edit build-dev.sh to add -DALSA_AVAILABLE -lasound to the x11 branch,
# or compile manually:
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG -DALSA_AVAILABLE \
      -o build/game \
      src/main_x11.c src/game.c src/levels.c src/audio.c \
      src/utils/draw-shapes.c src/utils/draw-text.c \
      -lX11 -lasound -lm
./build/game
```

**Expected:** Each tower type produces a distinct pitch when firing.  Creep
deaths produce a short pop; boss death produces a deep extended boom.  Losing a
life plays a loud low-frequency warning.  A rising chirp plays when each new
wave begins; a soft metallic clink plays when interest is added; a brighter
chirp plays when you send a wave early for a gold bonus.  Wave-complete,
game-over, and victory each play a clearly recognisable tone.  A quiet 110 Hz
drone fades in over the first two seconds of gameplay (because
`game_audio_init` is called and `game_audio_update` advances the volume ramp).
No audio glitches or clicks on either platform.

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
