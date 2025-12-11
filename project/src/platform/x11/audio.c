/**
 * @fileoverview
 *
 * # Handmade Hero - Day 7 & 8 Audio System Explained
 *
 * ## 🔊 What Day 7 Is Really About
 *
 * Casey introduces **audio output** using DirectSound. Here's the mental model:
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    AUDIO PIPELINE (Day 7)                       │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  Your Game          DirectSound           Sound Card            │
 * │  ┌─────────┐       ┌───────────┐         ┌──────────┐          │
 * │  │ Generate │  →   │ Secondary │    →    │ DAC      │  → 🔊    │
 * │  │ Samples  │      │ Buffer    │         │ (Digital │          │
 * │  │ (48kHz)  │      │ (Ring)    │         │  to      │          │
 * │  └─────────┘       └───────────┘         │ Analog)  │          │
 * │                          ↑               └──────────┘          │
 * │                          │                                      │
 * │                    ┌───────────┐                                │
 * │                    │ Primary   │ ← Sets format (48kHz, 16-bit) │
 * │                    │ Buffer    │                                │
 * │                    └───────────┘                                │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ### Key Concepts
 *
 * | Concept | What It Is | Web Analogy |
 * |---------|------------|-------------|
 * | Sample Rate | 48000 samples/second | Like FPS but for audio |
 * | Buffer Size | Ring buffer for audio data | Like a streaming buffer |
 * | Primary Buffer | Sets the audio format | Like `audioContext.sampleRate` |
 * | Secondary Buffer | Where you write samples | Like `AudioBuffer` in Web
 * Audio | | Cooperative Level | How you share the sound card | Like exclusive
 * fullscreen mode |
 *
 * ### Linux Audio System (ALSA)
 *
 * This is the BIG addition for Day 7. On Linux, we use **ALSA** (Advanced Linux
 * Sound Architecture) instead of DirectSound.
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │              WINDOWS vs LINUX AUDIO                             │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  Windows:                      Linux:                           │
 * │  ┌──────────────┐             ┌──────────────┐                 │
 * │  │ DirectSound  │             │ ALSA         │                 │
 * │  │ dsound.dll   │             │ libasound    │                 │
 * │  └──────────────┘             └──────────────┘                 │
 * │         ↓                            ↓                          │
 * │  LoadLibrary()               dlopen() or link directly          │
 * │  DirectSoundCreate()         snd_pcm_open()                    │
 * │  SetCooperativeLevel()       snd_pcm_set_params()              │
 * │  CreateSoundBuffer()         snd_pcm_writei()                  │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## 🔊 Day 8 - Understanding the Ring Buffer
 *
 * This is **THE core concept** of Day 8. Let me explain it visually:
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                      DIRECTSOUND RING BUFFER                            │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │  The buffer is circular - when you reach the end, it wraps to start!   │
 * │                                                                         │
 * │  ┌────────────────────────────────────────────────────────────┐        │
 * │  │ 0                                              BufferSize  │        │
 * │  │ ├──────────────────────────────────────────────────────────┤        │
 * │  │ │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│        │
 * │  │ │              ↑                    ↑                      │        │
 * │  │ │         PlayCursor           WriteCursor                 │        │
 * │  │ │         (hardware             (safe to                   │        │
 * │  │ │          reading)              write here)               │        │
 * │  │ └──────────────────────────────────────────────────────────┘        │
 * │  │                                                                      │
 * │  │  ▓▓▓ = Audio being played (DON'T TOUCH!)                            │
 * │  │  ░░░ = Safe to write new audio data                                  │
 * │                                                                         │
 * │  WRAP-AROUND CASE:                                                      │
 * │  ┌────────────────────────────────────────────────────────────┐        │
 * │  │░░░░░░░░░░▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░│        │
 * │  │         ↑                                  ↑               │        │
 * │  │    PlayCursor                         ByteToLock           │        │
 * │  │                                                            │        │
 * │  │  Region2 ◄────────────────────────────────► Region1        │        │
 * │  │  (start of buffer)                    (end of buffer)      │        │
 * │  └────────────────────────────────────────────────────────────┘        │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ### Square Wave Generation
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                      SQUARE WAVE (256 Hz)                               │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │  ToneVolume (+3000)                                                     │
 * │      ┌────┐    ┌────┐    ┌────┐    ┌────┐                              │
 * │      │    │    │    │    │    │    │    │                              │
 * │  ────┘    └────┘    └────┘    └────┘    └────                          │
 * │                                                                         │
 * │  -ToneVolume (-3000)                                                    │
 * │                                                                         │
 * │  ◄──────────────────────────────────────────►                          │
 * │           One period = 48000/256 = 187.5 samples                        │
 * │                                                                         │
 * │  Casey's formula:                                                       │
 * │  SampleValue = ((RunningSampleIndex / HalfPeriod) % 2)                 │
 * │                ? ToneVolume : -ToneVolume                               │
 * │                                                                         │
 * │  Sample 0-93:   +3000  (first half)                                    │
 * │  Sample 94-187: -3000  (second half)                                   │
 * │  Sample 188:    +3000  (next period starts)                            │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 */
#include "audio.h"
#include "../base.h"
#include <dlfcn.h> // 🆕 For dlopen, dlsym, dlclose (Casey's LoadLibrary equivalent)
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif // M_PI
#ifndef M_double_PI
#define M_double_PI (2.f * M_PI)
#endif // M_double_PI

// ═══════════════════════════════════════════════════════════════
// 🔊 ALSA AUDIO DYNAMIC LOADING (Casey's DirectSound Pattern)
// ═══════════════════════════════════════════════════════════════
//
// Casey dynamically loads dsound.dll using LoadLibrary/GetProcAddress.
// We do the same with dlopen/dlsym for libasound.so.
//
// WHY DYNAMIC LOADING?
// 1. Program doesn't crash if ALSA isn't installed
// 2. We can gracefully fall back to no audio
// 3. No compile-time dependency on -lasound
// 4. Exactly mirrors Casey's Win32 approach
//
// ═══════════════════════════════════════════════════════════════

// ───────────────────────────────────────────────────────────────
// Stub Implementations (used when ALSA not available)
// ───────────────────────────────────────────────────────────────

ALSA_SND_PCM_OPEN(AlsaSndPcmOpenStub) {
  /*
   * The (void) casts for the parameters 'pcm', 'device', 'stream', and 'mode'
   * are used to explicitly mark them as unused in this stub implementation.
   * This prevents compiler warnings about unused parameters, making it clear
   * that their omission is intentional in this context.
   */
  (void)pcm;
  (void)device;
  (void)stream;
  (void)mode;
  return -1; // Error: no device
}

ALSA_SND_PCM_SET_PARAMS(AlsaSndPcmSetParamsStub) {
  (void)pcm;
  (void)format;
  (void)access;
  (void)channels;
  (void)rate;
  (void)soft_resample;
  (void)latency;
  return -1;
}

ALSA_SND_PCM_WRITEI(AlsaSndPcmWriteiStub) {
  (void)pcm;
  (void)buffer;
  (void)frames;
  return 0; // Pretend we wrote 0 frames
}

ALSA_SND_PCM_PREPARE(AlsaSndPcmPrepareStub) {
  (void)pcm;
  return -1;
}

ALSA_SND_PCM_CLOSE(AlsaSndPcmCloseStub) {
  (void)pcm;
  return 0;
}

ALSA_SND_STRERROR(AlsaSndStrerrorStub) {
  (void)errnum;
  return "ALSA not loaded";
}

ALSA_SND_PCM_AVAIL(AlsaSndPcmAvailStub) {
  (void)pcm;
  return 0;
}

ALSA_SND_PCM_RECOVER(AlsaSndPcmRecoverStub) {
  (void)pcm;
  (void)err;
  (void)silent;
  return -1;
}

// ───────────────────────────────────────────────────────────────
// Global Function Pointers (initially point to stubs)
// ───────────────────────────────────────────────────────────────

alsa_snd_pcm_open *SndPcmOpen_ = AlsaSndPcmOpenStub;
alsa_snd_pcm_set_params *SndPcmSetParams_ = AlsaSndPcmSetParamsStub;
alsa_snd_pcm_writei *SndPcmWritei_ = AlsaSndPcmWriteiStub;
alsa_snd_pcm_prepare *SndPcmPrepare_ = AlsaSndPcmPrepareStub;
alsa_snd_pcm_close *SndPcmClose_ = AlsaSndPcmCloseStub;
alsa_snd_strerror *SndStrerror_ = AlsaSndStrerrorStub;
alsa_snd_pcm_avail *SndPcmAvail_ = AlsaSndPcmAvailStub;
alsa_snd_pcm_recover *SndPcmRecover_ = AlsaSndPcmRecoverStub;

// ───────────────────────────────────────────────────────────────
// Sound Output State
// ───────────────────────────────────────────────────────────────
LinuxSoundOutput g_sound_output = {0};

// ═══════════════════════════════════════════════════════════════
// 🔊 Load ALSA Library (Casey's Win32LoadXInput equivalent)
// ═══════════════════════════════════════════════════════════════
//
// This mirrors Casey's Win32LoadXInput() function exactly:
// 1. Try to load the library
// 2. If found, get function pointers
// 3. If not found, stubs remain in place
//
// ═══════════════════════════════════════════════════════════════

void linux_load_alsa(void) {
  printf("Loading ALSA library...\n");

  // ───────────────────────────────────────────────────────────
  // Try to load libasound.so (Casey's LoadLibrary equivalent)
  // ───────────────────────────────────────────────────────────
  //
  // Try multiple names (like Casey tries xinput1_4.dll, then xinput1_3.dll)
  //
  // RTLD_LAZY = Only resolve symbols when called (faster load)
  // RTLD_NOW  = Resolve all symbols immediately (catches errors early)
  // ───────────────────────────────────────────────────────────

  void *alsa_lib = dlopen("libasound.so.2", RTLD_LAZY);
  if (!alsa_lib) {
    alsa_lib = dlopen("libasound.so", RTLD_LAZY);
  }

  if (!alsa_lib) {
    fprintf(stderr, "❌ ALSA: Could not load libasound.so: %s\n", dlerror());
    fprintf(stderr,
            "   Audio disabled. Install: sudo apt install libasound2\n");
    return; // Stubs remain in place - audio just won't work
  }

  printf("✅ ALSA: Loaded libasound.so\n");
  g_sound_output.alsa_library = alsa_lib;

  // ───────────────────────────────────────────────────────────
  // Get function pointers (Casey's GetProcAddress equivalent)
  // ───────────────────────────────────────────────────────────

// Helper macro to reduce repetition (like Casey does implicitly)
#define LOAD_ALSA_FN(fn_ptr, fn_name, type)                                    \
  do {                                                                         \
    fn_ptr = (type *)dlsym(alsa_lib, fn_name);                                 \
    if (!fn_ptr) {                                                             \
      fprintf(stderr, "❌ ALSA: Could not load %s\n", fn_name);                \
    } else {                                                                   \
      printf("   ✓ Loaded %s\n", fn_name);                                     \
    }                                                                          \
  } while (0)

  LOAD_ALSA_FN(SndPcmOpen_, "snd_pcm_open", alsa_snd_pcm_open);
  LOAD_ALSA_FN(SndPcmSetParams_, "snd_pcm_set_params", alsa_snd_pcm_set_params);
  LOAD_ALSA_FN(SndPcmWritei_, "snd_pcm_writei", alsa_snd_pcm_writei);
  LOAD_ALSA_FN(SndPcmPrepare_, "snd_pcm_prepare", alsa_snd_pcm_prepare);
  LOAD_ALSA_FN(SndPcmClose_, "snd_pcm_close", alsa_snd_pcm_close);
  LOAD_ALSA_FN(SndStrerror_, "snd_strerror", alsa_snd_strerror);
  LOAD_ALSA_FN(SndPcmAvail_, "snd_pcm_avail", alsa_snd_pcm_avail);
  LOAD_ALSA_FN(SndPcmRecover_, "snd_pcm_recover", alsa_snd_pcm_recover);
#undef LOAD_ALSA_FN

  // Verify critical functions loaded
  if (!SndPcmOpen_ || !SndPcmSetParams_ || !SndPcmWritei_) {
    fprintf(stderr, "❌ ALSA: Missing critical functions, audio disabled\n");
    // Reset to stubs
    SndPcmOpen_ = AlsaSndPcmOpenStub;
    SndPcmSetParams_ = AlsaSndPcmSetParamsStub;
    SndPcmWritei_ = AlsaSndPcmWriteiStub;
    dlclose(alsa_lib);
    g_sound_output.alsa_library = NULL;
  }
}

// ═══════════════════════════════════════════════════════════════
// 🔊 Initialize Sound (Casey's Win32InitDSound equivalent)
// ═══════════════════════════════════════════════════════════════
//
// Casey's DirectSound setup:
// 1. DirectSoundCreate()         → SndPcmOpen()
// 2. SetCooperativeLevel()       → (not needed in ALSA)
// 3. Create primary buffer       → (format set via snd_pcm_set_params)
// 4. Set primary format          → snd_pcm_set_params()
// 5. Create secondary buffer     → (ALSA manages internally)
//
// ═══════════════════════════════════════════════════════════════

void linux_init_sound(int32_t samples_per_second, int32_t buffer_size_bytes) {
  printf("Initializing sound output...\n");

  // ───────────────────────────────────────────────────────────
  // STEP 1: Open the PCM device
  // ───────────────────────────────────────────────────────────
  // Casey: DirectSoundCreate()
  // Linux: snd_pcm_open()
  //
  // "default" = system default audio device
  //             (PulseAudio will intercept this on most systems)
  // ───────────────────────────────────────────────────────────

  int err = SndPcmOpen(&g_sound_output.handle,
                       "default",                     // Device
                       LINUX_SND_PCM_STREAM_PLAYBACK, // Output
                       0);                            // Blocking mode

  if (err < 0) {
    fprintf(stderr, "❌ Sound: Cannot open audio device: %s\n",
            SndStrerror(err));
    g_sound_output.is_valid = false;
    return;
  }

  printf("✅ Sound: Opened audio device\n");

  // ───────────────────────────────────────────────────────────
  // STEP 2: Set format parameters
  // ───────────────────────────────────────────────────────────
  // Casey creates WAVEFORMATEX:
  //   wBitsPerSample = 16
  //   nChannels = 2
  //   nSamplesPerSec = 48000
  //   nBlockAlign = 4 (2 channels × 2 bytes)
  //
  // We use snd_pcm_set_params() which does it all in one call
  // ───────────────────────────────────────────────────────────

  // Latency calculation:
  // 🆕 Day 8: Use shorter latency for better responsiveness
  // Casey uses ~66ms (1/15 second), we'll use 50ms
  unsigned int latency_us = 50000; // 50ms in microseconds

  err = SndPcmSetParams(g_sound_output.handle,
                        LINUX_SND_PCM_FORMAT_S16_LE,         // 16-bit signed
                        LINUX_SND_PCM_ACCESS_RW_INTERLEAVED, // L-R-L-R
                        2,                                   // Stereo
                        samples_per_second,                  // 48000 Hz
                        1,                                   // Allow resample
                        latency_us);                         // 100ms buffer

  if (err < 0) {
    fprintf(stderr, "❌ Sound: Cannot set parameters: %s\n", SndStrerror(err));
    SndPcmClose(g_sound_output.handle);
    g_sound_output.is_valid = false;
    return;
  }

  printf("✅ Sound: Format set to %d Hz, 16-bit stereo\n", samples_per_second);

  // ───────────────────────────────────────────────────────────
  // STEP 3: Store parameters for later use
  // ───────────────────────────────────────────────────────────

  g_sound_output.samples_per_second = samples_per_second;
  g_sound_output.bytes_per_sample =
      sizeof(int16_t) * 2; // 16-bit stereo = 4 bytes
  g_sound_output.buffer_size = buffer_size_bytes;

  // ═══════════════════════════════════════════════════════════
  // 🆕 Day 8: Allocate sample buffer (Casey's secondary buffer)
  // ═══════════════════════════════════════════════════════════
  //
  // We need a buffer to generate samples into before writing to ALSA.
  // Size: 1/15 second of audio (like Casey's buffer)
  //
  // samples_per_second / 15 = frames per write
  // × 2 channels × 2 bytes = buffer size
  // ═══════════════════════════════════════════════════════════

  g_sound_output.sample_buffer_size = samples_per_second / 15; // ~3200 frames
  int sample_buffer_bytes =
      g_sound_output.sample_buffer_size * g_sound_output.bytes_per_sample;

  g_sound_output.sample_buffer =
      (int16_t *)mmap(NULL, sample_buffer_bytes, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (g_sound_output.sample_buffer == MAP_FAILED) {
    fprintf(stderr, "❌ Sound: Cannot allocate sample buffer\n");
    SndPcmClose(g_sound_output.handle);
    g_sound_output.is_valid = false;
    return;
  }

  // ═══════════════════════════════════════════════════════════
  // 🆕 Day 8: Initialize sound generation parameters
  // ═══════════════════════════════════════════════════════════
  //
  // Casey's Day 8 values:
  //   ToneHz = 256 (middle C-ish)
  //   ToneVolume = 3000 (amplitude)
  //   SquareWavePeriod = SamplesPerSecond / ToneHz
  // ═══════════════════════════════════════════════════════════
  g_sound_output.running_sample_index = 0;
  g_sound_output.tone_hz = 256;
  g_sound_output.tone_volume = 6000;
  g_sound_output.wave_period = samples_per_second / g_sound_output.tone_hz;
  // g_sound_output.half_wave_period = g_sound_output.wave_period / 2;

  // Latency calculation
  g_sound_output.t_sine = 0;

  // Latency (1/15 second like Casey)
  g_sound_output.latency_sample_count = samples_per_second / 15;

  g_sound_output.pan_position = 0;

  g_sound_output.is_valid = true;

  printf("✅ Sound: Initialized!\n");
  printf("   Sample rate:    %d Hz\n", samples_per_second);
  printf("   Buffer size:    %d frames (~%.1f ms)\n",
         g_sound_output.sample_buffer_size,
         (float)g_sound_output.sample_buffer_size / samples_per_second *
             1000.0f);
  printf("   Tone frequency: %d Hz\n", g_sound_output.tone_hz);
  printf("   Wave period:    %d samples\n", g_sound_output.wave_period);
  printf("   Sample rate:  %d Hz\n", samples_per_second);
  printf("   Buffer size:  %d bytes\n", buffer_size_bytes);
  printf("   Latency:      %.1f ms\n", latency_us / 1000.0f);
}

// ═══════════════════════════════════════════════════════════════
// 🆕 Day 8: Fill Sound Buffer with Square Wave
// ═══════════════════════════════════════════════════════════════
//
// This is the Linux equivalent of Casey's Lock/Write/Unlock pattern.
//
// DIFFERENCE FROM DIRECTSOUND:
// - DirectSound: Lock buffer → write → unlock → hardware plays
// - ALSA: Fill our buffer → snd_pcm_writei() copies to hardware
//
// ALSA is simpler because it manages the ring buffer for us!
// ═══════════════════════════════════════════════════════════════
void linux_fill_sound_buffer(void) {
  if (!g_sound_output.is_valid) {
    return;
  }

  // ───────────────────────────────────────────────────────────
  // Step 1: Check how many frames ALSA wants
  // ───────────────────────────────────────────────────────────
  // snd_pcm_avail() returns how many frames we CAN write
  // This is like GetCurrentPosition() in DirectSound
  // ───────────────────────────────────────────────────────────

  long frames_available = SndPcmAvail(g_sound_output.handle);

  if (frames_available < 0) {
    // Error occurred (probably underrun)
    // Try to recover
    int err = SndPcmRecover(g_sound_output.handle, (int)frames_available, 1);
    if (err < 0) {
      fprintf(stderr, "❌ Sound: Recovery failed: %s\n", SndStrerror(err));
      return;
    }
    frames_available = SndPcmAvail(g_sound_output.handle);
    if (frames_available < 0) {
      return;
    }
  }

  // Don't write more than our buffer can hold
  if (frames_available > (long)g_sound_output.sample_buffer_size) {
    frames_available = g_sound_output.sample_buffer_size;
  }

  if (frames_available <= 0) {
    return; // Buffer full, nothing to write
  }

  // ───────────────────────────────────────────────────────────
  // Step 2: Generate square wave samples
  // ───────────────────────────────────────────────────────────
  //
  // Casey's formula (Day 8):
  //   SampleValue = ((RunningSampleIndex / HalfPeriod) % 2)
  //                 ? ToneVolume : -ToneVolume
  //
  // This creates a square wave:
  //   +3000 for first half of period
  //   -3000 for second half of period
  // ───────────────────────────────────────────────────────────

  int16_t *sample_out = g_sound_output.sample_buffer;

  for (long i = 0; i < frames_available; ++i) {
    //// Calculate sample value (Casey's exact formula)
    // int16_t sample_value = ((g_sound_output.running_sample_index /
    //                         g_sound_output.half_wave_period) %
    //                        2)
    //                           ? g_sound_output.tone_volume
    //                           : -g_sound_output.tone_volume;

    // ═══════════════════════════════════════════════════════════
    // 🆕 Day 9: Generate sine wave sample
    // ═══════════════════════════════════════════════════════════
    // Casey's exact formula:
    //   SineValue = sinf(tSine);
    //   SampleValue = (int16)(SineValue * ToneVolume);
    //   tSine += 2π / WavePeriod;
    // ═══════════════════════════════════════════════════════════
    real32 sine_value = sinf(g_sound_output.t_sine);
    int16_t sample_value = (int16_t)(sine_value * g_sound_output.tone_volume);

    int left_gain = (100 - g_sound_output.pan_position);  // 0 to 200
    int right_gain = (100 + g_sound_output.pan_position); // 0 to 200

    // Write to BOTH channels (stereo)
    *sample_out++ = (sample_value * left_gain) / 200;  // Left channel
    *sample_out++ = (sample_value * right_gain) / 200; // Right channel
    //  Why divide by 200?
    // Because gains range from 0-200, and we want 100% = 200/200 = 1.0

    // Increment phase accumulator
    g_sound_output.t_sine +=
        (M_double_PI * 1.0f) / (float)g_sound_output.wave_period;

    // Wrap to [0, 2π) range
    if (g_sound_output.t_sine >= M_double_PI) {
      g_sound_output.t_sine -= M_double_PI;
    }

    g_sound_output.running_sample_index++;
  }

  // ───────────────────────────────────────────────────────────
  // Step 3: Write samples to ALSA
  // ───────────────────────────────────────────────────────────
  //
  // snd_pcm_writei() copies our samples to ALSA's internal buffer.
  // ALSA then feeds them to the sound card at the right rate.
  //
  // This is simpler than DirectSound's Lock/Unlock pattern!
  // ───────────────────────────────────────────────────────────

  long frames_written = SndPcmWritei(
      g_sound_output.handle, g_sound_output.sample_buffer, frames_available);

  if (frames_written < 0) {
    // Handle errors (underrun, etc.)
    frames_written =
        SndPcmRecover(g_sound_output.handle, (int)frames_written, 1);
    if (frames_written < 0) {
      fprintf(stderr, "❌ Sound: Write failed: %s\n",
              SndStrerror((int)frames_written));
    }
  }
}
