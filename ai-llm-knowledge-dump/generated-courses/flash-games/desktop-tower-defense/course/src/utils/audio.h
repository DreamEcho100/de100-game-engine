/* src/utils/audio.h  —  Desktop Tower Defense | Audio Types */
#ifndef DTD_AUDIO_H
#define DTD_AUDIO_H

#include <stdint.h>

#define MAX_SIMULTANEOUS_SOUNDS  16
#define AUDIO_CHUNK_SIZE         2048   /* samples per Raylib audio stream fill */
#define AUDIO_SAMPLE_RATE        44100

/* A single synthesized sound definition (no WAV files — all PCM synthesis). */
typedef struct {
    float frequency;        /* start frequency in Hz  */
    float frequency_end;    /* end frequency (0 = constant) */
    float duration_ms;      /* duration in milliseconds */
    float volume;           /* 0.0–1.0 */
} SoundDef;

/* A live playing instance of a sound. */
typedef struct {
    float  phase;           /* oscillator phase 0.0–1.0 */
    float  frequency;       /* current frequency (Hz) */
    float  frequency_slide; /* Hz/sample slide toward frequency_end */
    float  time_remaining;  /* seconds until silence */
    float  volume;          /* 0.0–1.0 */
    float  pan_position;    /* -1.0 = full left, 0.0 = center, 1.0 = full right */
    int    active;
} SoundInstance;

/* A simple square/sine oscillator for background music. */
typedef struct {
    float frequency;
    float current_volume;
    float target_volume;
    int   is_playing;
} ToneGenerator;

/* All runtime audio state (stored inside GameState). */
typedef struct {
    SoundInstance active_sounds[MAX_SIMULTANEOUS_SOUNDS];
    ToneGenerator music_tone;
    float         master_volume; /* 0.0–1.0 */
    float         sfx_volume;
    float         music_volume;
} GameAudioState;

/* Output buffer filled by game_get_audio_samples(). */
typedef struct {
    int16_t *samples;           /* interleaved stereo: L R L R ... */
    int      samples_per_second;
    int      sample_count;      /* total samples (not frames) */
} AudioOutputBuffer;

#endif /* DTD_AUDIO_H */
