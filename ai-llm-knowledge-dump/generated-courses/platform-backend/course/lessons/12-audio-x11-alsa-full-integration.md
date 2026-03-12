# Lesson 12 — Audio X11/ALSA Full Integration

## Overview

| Item | Value |
|------|-------|
| **Backend** | X11 only |
| **New concepts** | `snd_pcm_hw_params`, `snd_pcm_sw_params`, ALSA latency model, `snd_pcm_avail_update`, `snd_pcm_recover` |
| **Observable outcome** | No audio stutter under X11; clean startup; underrun recovery without crash |
| **Files modified** | `src/platforms/x11/audio.c` |

---

## What you'll build

Replace the minimal `snd_pcm_set_params` from Lesson 09 with a production ALSA setup that gives explicit control over ring-buffer sizing, start threshold, and underrun recovery.

---

## Background

### Why `snd_pcm_hw_params` instead of `snd_pcm_set_params`?

`snd_pcm_set_params` is a convenience wrapper — it lets ALSA pick buffer sizes.  For this course, we use `snd_pcm_hw_params` explicitly so you understand **the latency model** and can tune it.

### `alloca.h` include order (IMPORTANT for clang)

```c
// MUST come before asoundlib.h!
#include <alloca.h>
#include <alsa/asoundlib.h>
```

`asoundlib.h` uses `alloca()` internally via the `*_alloca()` macro helpers.  With clang, if `alloca.h` hasn't been seen first, clang emits an implicit-declaration error.  GCC is more forgiving but including `alloca.h` first is always correct.

### ALSA latency model

```
samples_per_frame  = sample_rate / TARGET_FPS      ≈ 735  (44100 / 60)
latency_samples    = samples_per_frame × 2         ≈ 1470  (~33 ms)
safety_samples     = samples_per_frame / 3         ≈ 245
ring_buffer_size   = (latency + safety) × 4        ≈ 6860  (ALSA rounds up to power-of-2)
```

The ring buffer is deliberately oversized × 4 so that a single frame of game-loop latency never drains the buffer.  ALSA always rounds up to the nearest hardware-supported power-of-two, so the real buffer is typically 8192 frames.

### `hw_buffer_size` ≠ "samples to write per frame"

This confuses beginners.  They are **different things**:

| Variable | What it means |
|----------|---------------|
| `hw_buffer_size` (8192) | Total ring-buffer capacity ALSA allocates |
| `snd_pcm_avail_update()` | How many frames ALSA can accept right now (varies each call) |
| `samples_per_frame` (735) | How many frames you *target* writing per game frame |

You query `avail_update` each frame, not `buffer_size`.

### `sw_params` start threshold

Default start threshold = full buffer → ALSA waits until the buffer is completely full before starting playback.  For a 8192-frame buffer at 44100 Hz this is **185ms of silence** at startup.

```c
// Fix: start playback as soon as the first sample arrives
snd_pcm_sw_params_set_start_threshold(pcm, sw, 1);
```

### Underrun recovery

ALSA calls an underrun (`EPIPE`) when `snd_pcm_writei` can't keep up.  `snd_pcm_recover` handles `EPIPE`, `ESTRPIPE` (suspend) and other transient errors:

```c
snd_pcm_sframes_t n = snd_pcm_writei(pcm, buf, frames);
if (n < 0) snd_pcm_recover(pcm, (int)n, 0);
```

After recovery, pre-fill one frame of silence to prevent a click artifact.

---

## What to type

### `#include` order

```c
/* MUST come before asoundlib.h */
#include <alloca.h>
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <string.h>
#include "platforms/x11/audio.h"
#include "platform.h"   /* TARGET_FPS */
```

### Module-level latency state

```c
/* Static — ALSA-only details that don't belong in PlatformAudioConfig */
static int g_samples_per_frame    = 0;
static int g_latency_samples      = 0;
static int g_safety_samples       = 0;
```

### `platform_audio_init` — hw_params setup

```c
int platform_audio_init(PlatformAudioConfig *cfg) {
    g_samples_per_frame = cfg->sample_rate / TARGET_FPS;   /* ≈ 735 */
    g_latency_samples   = g_samples_per_frame * 2;
    g_safety_samples    = g_samples_per_frame / 3;

    snd_pcm_t *pcm;
    snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    cfg->alsa_pcm = pcm;

    snd_pcm_hw_params_t *hw;
    snd_pcm_hw_params_alloca(&hw);         /* stack-alloc via alloca internally */
    snd_pcm_hw_params_any(pcm, hw);
    snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm, hw, (unsigned)cfg->channels);

    unsigned int rate = (unsigned)cfg->sample_rate;
    snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);
    cfg->sample_rate = (int)rate;   /* ALSA may adjust to nearest supported rate */

    /* Ring buffer = (latency + safety) × 4 */
    snd_pcm_uframes_t hw_buf = (snd_pcm_uframes_t)(g_latency_samples + g_safety_samples) * 4;
    snd_pcm_hw_params_set_buffer_size_near(pcm, hw, &hw_buf);

    snd_pcm_uframes_t period = hw_buf / 4;
    snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, 0);
    snd_pcm_hw_params(pcm, hw);

    snd_pcm_hw_params_get_buffer_size(hw, &hw_buf);
    cfg->alsa_buffer_size = (int)hw_buf;

    /* sw_params — start on first sample */
    snd_pcm_sw_params_t *sw;
    snd_pcm_sw_params_alloca(&sw);
    snd_pcm_sw_params_current(pcm, sw);
    snd_pcm_sw_params_set_start_threshold(pcm, sw, 1);
    snd_pcm_sw_params(pcm, sw);

    /* Pre-fill silence (prevents click on first write) */
    snd_pcm_prepare(pcm);
    int16_t *silence = (int16_t *)alloca((size_t)hw_buf * 2 * sizeof(int16_t));
    memset(silence, 0, (size_t)hw_buf * 2 * sizeof(int16_t));
    snd_pcm_writei(pcm, silence, hw_buf);

    return 0;
}
```

### `platform_audio_get_samples_to_write`

```c
int platform_audio_get_samples_to_write(PlatformAudioConfig *cfg) {
    snd_pcm_t *pcm = (snd_pcm_t *)cfg->alsa_pcm;
    snd_pcm_sframes_t avail = snd_pcm_avail_update(pcm);
    if (avail < 0) {
        snd_pcm_recover(pcm, (int)avail, 0);
        /* Pre-fill silence after recovery */
        int16_t *sil = (int16_t *)alloca((size_t)g_samples_per_frame * 2 * sizeof(int16_t));
        memset(sil, 0, (size_t)g_samples_per_frame * 2 * sizeof(int16_t));
        snd_pcm_writei(pcm, sil, (snd_pcm_uframes_t)g_samples_per_frame);
        return g_samples_per_frame;
    }
    int n = (int)avail;
    /* Cap at 4× samples_per_frame */
    if (n > g_samples_per_frame * 4) n = g_samples_per_frame * 4;
    /* Safety cap — never exceed AudioOutputBuffer capacity */
    if (cfg->chunk_size > 0 && n > cfg->chunk_size) n = cfg->chunk_size;
    return n;
}
```

### `platform_audio_write`

```c
void platform_audio_write(AudioOutputBuffer *buf, int num_frames,
                          PlatformAudioConfig *cfg) {
    snd_pcm_t *pcm = (snd_pcm_t *)cfg->alsa_pcm;
    snd_pcm_sframes_t n = snd_pcm_writei(pcm, buf->samples,
                                         (snd_pcm_uframes_t)num_frames);
    if (n < 0) snd_pcm_recover(pcm, (int)n, 0);
}
```

---

## Explanation

| L09 minimal | L12 production | Why |
|-------------|----------------|-----|
| `snd_pcm_set_params` (ALSA picks buffer) | `snd_pcm_hw_params` (explicit) | We control latency |
| No start threshold change | `sw_params start_threshold = 1` | Eliminates 185ms silence at startup |
| No underrun handling | `snd_pcm_recover` | Prevents crash on xrun |
| No pre-fill | Silence pre-fill on init + recovery | Prevents click artifacts |
| No `avail_update` cap | `min(avail, 4×spf, chunk_size)` | Prevents huge mixer chunks and OOB writes |

---

## Build and run

```sh
./build-dev.sh --backend=x11 -r
```

Hold Space for several seconds — audio should be continuous.  Try loading the system (run another program) to trigger underrun recovery; the tone should resume without crash.

---

## Quiz

1. Why does `alloca.h` need to come before `asoundlib.h` specifically with clang?
2. What is the difference between `cfg->alsa_buffer_size` (8192) and the value returned by `snd_pcm_avail_update`?
3. The default `sw_params` start threshold is the full buffer size.  At 44100 Hz with a 8192-frame buffer, how many milliseconds of silence would you hear at startup without the fix?
