# Lesson 11 — Raylib Audio: Full Integration

## Overview

| Item | Value |
|------|-------|
| **Backend** | Raylib only |
| **New concepts** | `SetAudioStreamBufferSizeDefault`, pre-fill silence, `while (IsAudioStreamProcessed)` drain loop |
| **Observable outcome** | No audio stutter on Raylib; clean startup without pop; sustained tone plays smoothly |
| **Files modified** | `src/platforms/raylib/main.c` |

---

## What you'll build

A production-quality Raylib audio integration, replacing the minimal L09 version.  The key change: pre-fill two silent buffers on init, and drain with `while` (not `if`) in the audio loop.

---

## Background

### What was wrong with the L09 minimal version?

The L09 version used:
```c
if (IsAudioStreamProcessed(audio_stream)) {
    game_get_audio_samples(...);
    UpdateAudioStream(...);
}
```

Two problems:
1. **`if` vs `while`**: If two buffers become ready in the same frame (happens when a game frame takes longer than usual), only one is refilled.  The stream starves → click / silence.
2. **No pre-fill**: The stream starts empty.  Raylib requests audio immediately on `PlayAudioStream`, before the first `process_audio()` call.  Result: a brief pop or silence at startup.

### The fix

```c
// Init: pre-fill two silent buffers
memset(audio_buf.samples, 0, chunk_size * AUDIO_BYTES_PER_FRAME);
UpdateAudioStream(audio_stream, audio_buf.samples, chunk_size);
UpdateAudioStream(audio_stream, audio_buf.samples, chunk_size);
PlayAudioStream(audio_stream);

// Per-frame: drain ALL processed buffers
while (IsAudioStreamProcessed(audio_stream)) {
    game_get_audio_samples(&audio_state, &audio_buf, chunk_size);
    UpdateAudioStream(audio_stream, audio_buf.samples, chunk_size);
}
```

### `SetAudioStreamBufferSizeDefault(4096)`

Must be called **before** `LoadAudioStream`.  It sets the internal ring-buffer size.  4096 frames ≈ 93ms — large enough to absorb a late game frame, small enough not to add perceptible latency.

### Why `while` (drain all buffers)?

Raylib's audio system uses double-buffering internally.  `IsAudioStreamProcessed` returns true when a buffer slot is free.  If both slots free up in one frame, `if` only fills one — leaving a gap.  `while` fills all available slots.

### JS analogy

```js
// AudioWorkletProcessor.process() — runs every 128 samples regardless of game frame rate
// while-loop equivalent: process() drains as many 128-sample chunks as needed per game frame
```

---

## What to type

### Full `init_audio` (replaces L09 version)

```c
static int init_audio(PlatformAudioConfig *cfg, AudioOutputBuffer *audio_buf) {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return -1;

    // MUST be before LoadAudioStream
    SetAudioStreamBufferSizeDefault(cfg->chunk_size);

    g_raylib.audio_stream = LoadAudioStream(
        (unsigned)cfg->sample_rate, 16, (unsigned)cfg->channels);
    g_raylib.buffer_size_frames = cfg->chunk_size;

    if (!IsAudioStreamValid(g_raylib.audio_stream)) {
        CloseAudioDevice(); return -1;
    }

    // Pre-fill two silent buffers — prevents pop at startup
    memset(audio_buf->samples, 0, cfg->chunk_size * AUDIO_BYTES_PER_FRAME);
    UpdateAudioStream(g_raylib.audio_stream, audio_buf->samples, cfg->chunk_size);
    UpdateAudioStream(g_raylib.audio_stream, audio_buf->samples, cfg->chunk_size);

    PlayAudioStream(g_raylib.audio_stream);
    g_raylib.audio_initialized = 1;
    return 0;
}
```

### Full `process_audio` (replaces L09 `if` version)

```c
static void process_audio(GameAudioState *audio, AudioOutputBuffer *audio_buf) {
    if (!g_raylib.audio_initialized) return;
    if (!IsAudioStreamPlaying(g_raylib.audio_stream))
        PlayAudioStream(g_raylib.audio_stream);

    while (IsAudioStreamProcessed(g_raylib.audio_stream)) {
        int frames = g_raylib.buffer_size_frames;
        if (frames > audio_buf->max_sample_count)
            frames = audio_buf->max_sample_count;
        game_get_audio_samples(audio, audio_buf, frames);
        UpdateAudioStream(g_raylib.audio_stream, audio_buf->samples, frames);
    }
}
```

---

## Explanation

| L09 minimal | L11 production | Why it matters |
|-------------|---------------|----------------|
| `LoadAudioStream` (default buffer) | `SetAudioStreamBufferSizeDefault(4096)` first | Gives us control over latency |
| No pre-fill | 2× silent buffer pre-fill | Eliminates startup pop |
| `if (IsAudioStreamProcessed)` | `while (IsAudioStreamProcessed)` | Prevents starvation when two buffers free in one frame |

---

## Build and run

```sh
./build-dev.sh --backend=raylib -r
```

Hold Space for several seconds.  Audio should play without clicks or stutter.  The tone continues smoothly even while resizing the window.

---

## Quiz

1. Why does `SetAudioStreamBufferSizeDefault` need to be called before `LoadAudioStream` (not after)?
2. What happens if both Raylib ring-buffer slots free up in a single frame and you use `if` instead of `while`?
3. Pre-filling two silent buffers prevents a pop at startup.  Why does an empty buffer cause a pop?
