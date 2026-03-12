# Lesson 13 — Music Sequencer

## Overview

| Item | Value |
|------|-------|
| **Backend** | Both |
| **New concepts** | `MusicTone` (persistent-phase oscillator), `MusicSequencer` (MIDI-based, sample-accurate), smoothstep fade, `audio_midi_to_freq` |
| **Observable outcome** | A C-major scale plays automatically in a loop; manually triggered tones layer over it; no click artifacts on note transitions |
| **Files modified** | `src/game/audio_demo.c`, `src/utils/audio.h` |

---

## What you'll build

A step-based music sequencer using MIDI note numbers.  Unlike the SFX voice pool (where each note creates a new `ToneGenerator`), the music uses a **single persistent `MusicTone`** that changes frequency at step boundaries while keeping its phase continuous.  The sequencer advances inside `game_get_audio_samples` (one step per sample) for sample-accurate timing.

---

## Background

### Why not use a ToneGenerator voice for music?

The earlier approach triggered a new `ToneGenerator` voice for each music note.  Each new voice starts with `phase_acc = 0.0f`, which creates a waveform discontinuity at every note boundary → audible click between notes.

**`MusicTone` (new)** solves this differently:
- One persistent oscillator — phase carries over from note to note
- Phase only resets when coming from complete silence (`frequency == 0`)
- Amplitude transitions use a **smoothstep curve** instead of an instant switch

### Smoothstep fade

Smoothstep: `t² × (3 − 2t)` creates an S-shaped curve from 0→1 (or 1→0) that has zero derivative at both endpoints.  This means amplitude transitions begin and end without any sudden change in slope — completely artifact-free even with square waves.

```
1.0 ─────────╮
             │  smoothstep: t²(3-2t)
             │
0.0 ─────────╯
     fade start → fade end  (MUSIC_FADE_SAMPLES = 441 ≈ 10ms)
```

Compare to linear fade:
```
1.0 ─────────╲  ← slope discontinuity at both ends → subtle click
              ╲
0.0 ────────────
```

### Sample-accurate sequencing

The music sequencer now advances **inside `game_get_audio_samples`**, one step per sample.  This means note boundaries happen on the exact sample they are scheduled for — not up to 16.7 ms late as frame-based advancement would produce.

`step_duration_samples = 0.15 × 44100 = 6615 samples` (150 ms per step).

### MIDI note numbers

MIDI encodes pitch as an integer 0–127:
- `0` = REST (silence)
- `60` = C4 (261.63 Hz)
- `69` = A4 (440 Hz)
- `72` = C5 (523.25 Hz)

Convert with `audio_midi_to_freq(note)` from `utils/audio-helpers.h`:

```c
// Formula: 440 × 2^((note - 69) / 12)
float freq = audio_midi_to_freq(69);  // → 440.0f (A4)
float freq = audio_midi_to_freq(72);  // → 523.25f (C5)
float freq = audio_midi_to_freq(0);   // → 0.0f (REST)
```

---

## What to type

### `src/utils/audio.h` — `MusicTone`, `MUSIC_FADE_SAMPLES`, and `MusicSequencer`

```c
/* MusicTone — dedicated music oscillator.  Phase persists across notes. */
typedef struct {
    float phase;                   /* [0,1) — NOT reset between notes */
    float frequency;               /* Hz. 0 = silence. */
    float volume;                  /* Target amplitude (0–1). */
    float current_volume;          /* Smoothstep-faded working amplitude. */
    float pan;
    int   is_playing;              /* 1 = tone active; 0 = fading to silence */
    int   fade_samples_remaining;  /* Countdown for smoothstep transition. */
} MusicTone;

#define MUSIC_FADE_SAMPLES 441     /* 10ms smoothstep fade at 44100 Hz */

/* MusicSequencer — MIDI step sequencer, sample-accurate. */
typedef struct {
    const uint8_t **patterns;      /* Array of MIDI pattern pointers. */
    int             pattern_len;   /* Steps per pattern. */
    int             pattern_count; /* Number of patterns. */
    int             current_pattern;
    int             current_step;
    int             step_duration_samples; /* Samples per step (set at init). */
    int             step_samples_remaining;/* Countdown to next step. */
    MusicTone       tone;          /* The single persistent music oscillator. */
    int             playing;
} MusicSequencer;
```

### `src/game/audio_demo.c` — MIDI patterns

```c
#define DEMO_PATTERN_LEN   16
#define DEMO_NUM_PATTERNS   2

/* Pattern A — C major scale ascending then descending
 * C4=60  D4=62  E4=64  F4=65  G4=67  A4=69  B4=71  C5=72 */
static const uint8_t DEMO_PATTERN_A[DEMO_PATTERN_LEN] = {
    60, 62, 64, 65, 67, 69, 71, 72,   /* ascend */
     0, 72, 71, 69, 67, 65, 64, 62,   /* rest, descend */
};

/* Pattern B — arpeggios */
static const uint8_t DEMO_PATTERN_B[DEMO_PATTERN_LEN] = {
    60, 64, 67, 60, 64, 67, 60,  0,   /* C major arpeggio */
    62, 65, 69, 62, 65, 69, 62,  0,   /* D minor arpeggio */
};

/* (Optional) Swap to these for the snake game melody: */
// static const uint8_t SNAKE_PATTERN_A[DEMO_PATTERN_LEN] = {
//     72, 76, 79, 81, 79, 76, 72, 67,  0, 74, 76, 79, 76, 74, 72,  0,
// };

static const uint8_t *DEMO_PATTERNS[DEMO_NUM_PATTERNS] = {
    DEMO_PATTERN_A,
    DEMO_PATTERN_B,
};
```

### `game_get_audio_samples` — music section (add inside the per-sample loop)

```c
/* ── Music sequencer (sample-accurate) ─────────────────────────── */
if (audio->sequencer.playing) {
    MusicSequencer *seq  = &audio->sequencer;
    MusicTone      *tone = &seq->tone;

    /* Advance to next step when current step expires */
    if (seq->step_samples_remaining <= 0) {
        const uint8_t *pat  = seq->patterns[seq->current_pattern];
        uint8_t        note = pat[seq->current_step];

        if (note > 0) {
            float new_freq = audio_midi_to_freq((int)note);
            /* Only reset phase from silence — preserve across note-to-note */
            if (!tone->is_playing || tone->frequency == 0.0f)
                tone->phase = 0.0f;
            tone->frequency  = new_freq;
            tone->is_playing = 1;
        } else {
            tone->is_playing = 0;   /* REST — fade out */
        }
        tone->fade_samples_remaining = MUSIC_FADE_SAMPLES;

        seq->step_samples_remaining = seq->step_duration_samples;
        seq->current_step++;
        if (seq->current_step >= seq->pattern_len) {
            seq->current_step    = 0;
            seq->current_pattern = (seq->current_pattern + 1) % seq->pattern_count;
        }
    }
    seq->step_samples_remaining--;

    /* Smoothstep fade: t²(3−2t)
     * is_playing=1 → volume rises 0→target (note-on)
     * is_playing=0 → volume falls target→0 (note-off / rest) */
    if (tone->fade_samples_remaining > 0) {
        float t      = 1.0f - (float)tone->fade_samples_remaining
                                   / (float)MUSIC_FADE_SAMPLES;
        float smooth = t * t * (3.0f - 2.0f * t);
        float fade   = tone->is_playing ? smooth : (1.0f - smooth);
        tone->current_volume = tone->volume * fade;
        tone->fade_samples_remaining--;
    } else {
        tone->current_volume = tone->is_playing ? tone->volume : 0.0f;
    }

    if (tone->current_volume > 0.0001f && tone->frequency > 0.0f) {
        float wave   = audio_wave_square(tone->phase);
        float sample = wave * tone->current_volume * audio->music_volume;

        float pan_l, pan_r;
        audio_calculate_pan(tone->pan, &pan_l, &pan_r);
        mix_l += sample * pan_l;
        mix_r += sample * pan_r;

        tone->phase += tone->frequency * inv_sr;
        if (tone->phase >= 1.0f) tone->phase -= 1.0f;
    }
}
```

### `game_audio_update` (simplify — sequencer moved to audio loop)

```c
void game_audio_update(GameAudioState *audio, float dt_ms) {
    /* Only SFX frequency slides remain here.
     * Music sequencer advances sample-accurately in game_get_audio_samples. */
    for (int v = 0; v < MAX_SOUNDS; v++) {
        ToneGenerator *gen = &audio->voices[v];
        if (!gen->active || gen->freq_slide == 0.0f) continue;
        gen->frequency += gen->freq_slide * (dt_ms / 1000.0f);
        if (gen->frequency < 20.0f) gen->frequency = 20.0f;
    }
}
```

### `game_audio_init_demo`

```c
void game_audio_init_demo(GameAudioState *audio) {
    memset(audio, 0, sizeof(*audio));
    audio->samples_per_second = AUDIO_SAMPLE_RATE;
    audio->master_volume      = 0.8f;
    audio->sfx_volume         = 1.0f;
    audio->music_volume       = 0.3f;

    MusicSequencer *seq = &audio->sequencer;
    seq->patterns              = DEMO_PATTERNS;
    seq->pattern_len           = DEMO_PATTERN_LEN;
    seq->pattern_count         = DEMO_NUM_PATTERNS;
    seq->step_duration_samples = (int)(0.15f * AUDIO_SAMPLE_RATE);  /* 150ms/step */
    seq->step_samples_remaining= 0;  /* fire immediately */
    seq->tone.volume           = 0.3f;
    seq->playing               = 1;
}
```

---

## Explanation

| Concept | Detail |
|---------|--------|
| `MusicTone.phase` persistent | Phase carries across notes → no waveform discontinuity → no inter-note click |
| Phase reset only from silence | `if (!tone->is_playing \|\| tone->frequency == 0)` — only reset when truly coming from silence |
| `t²(3−2t)` smoothstep | S-curve with zero slope at endpoints; fade-in and fade-out both click-free |
| MUSIC_FADE_SAMPLES = 441 | 10ms — long enough to eliminate clicks; short enough to not blur note attacks |
| Sample-accurate sequencing | Sequencer in audio loop: note boundaries on exact sample; frame-based would lag by up to 16.7ms |
| `step_duration_samples = 0.15 × 44100 = 6615` | 150ms per step; 16 steps × 150ms = 2.4s per pattern |
| MIDI note 0 = REST | `audio_midi_to_freq(0)` returns 0.0f; `frequency == 0` guard prevents synthesis |
| `music_volume = 0.3f` | Lower than SFX so the melody doesn't overpower manual key presses |
| `master_volume × 16000` | Leaves headroom for simultaneous music + SFX without clipping |

---

## Build and run

```sh
./build-dev.sh --backend=raylib -r
```

The C-major scale + arpeggios start immediately.  Press Space to layer a tone over the music.  Both should be audible without clicks.

---

## Quiz

1. Why does preserving `MusicTone.phase` across note boundaries eliminate inter-note clicks?
2. What happens if you change `master_volume` from 0.8 to 1.5?  What does `audio_clamp_sample` protect against?
3. A MIDI pattern has `step_duration_samples = 6615` (150ms).  How would you change it to make the music play at twice the speed?
4. Why does the sequencer advance inside `game_get_audio_samples` rather than `game_audio_update`?
