// ═══════════════════════════════════════════════════════════════
// 🔊 HANDMADE HERO - AUDIO SYSTEM (Days 7-19)
// ═══════════════════════════════════════════════════════════════
//
// WHAT THIS FILE DOES:
// - Dynamically loads ALSA (Linux audio library)
// - Initializes audio output at 48kHz, 16-bit stereo
// - Generates sine wave test audio
// - Handles audio buffer synchronization (Day 19)
// - Provides debug visualization of audio timing
//
// WHY WE USE ALSA:
// - Linux equivalent of DirectSound (Windows audio API)
// - We load it dynamically (dlopen) so the game doesn't crash if audio fails
// - ALSA manages the ring buffer for us (simpler than DirectSound)
//
// KEY CONCEPTS:
// - Sample Rate: 48000 samples/second (like FPS but for audio)
// - Ring Buffer: Circular buffer that wraps around when full
// - Latency: Delay between writing audio and hearing it (~66ms)
// - Frame-Aligned: Audio writes sync with game logic (30 FPS)
//
// CASEY'S PATTERN:
// - Audio runs at FIXED rate (30 FPS game logic)
// - Rendering can adapt (30-120 FPS)
// - Humans tolerate visual stutter, but hate audio clicks!
// ═══════════════════════════════════════════════════════════════
#include "audio.h"
#include "../../base.h"
#include "../../game.h"
#include <dlfcn.h> // For dlopen, dlsym, dlclose (Casey's LoadLibrary equivalent)
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if HANDMADE_INTERNAL
// ═══════════════════════════════════════════════════════════════
// 📊 DEBUG AUDIO MARKER TRACKING
// ═══════════════════════════════════════════════════════════════
// Casey stores 15 markers (0.5 seconds at 30 FPS)
// We'll do the same
// ═══════════════════════════════════════════════════════════════

#define MAX_DEBUG_AUDIO_MARKERS_30                                             \
  30 // 1 second at 30 FPS (or 0.5 sec at 60 FPS)
#define MAX_DEBUG_AUDIO_MARKERS_60                                             \
  60 // 1 second at 60 FPS (or 0.5 sec at 120 FPS)
#define MAX_DEBUG_AUDIO_MARKERS_120                                            \
  120 // 1 second at 120 FPS (or 0.5 sec at 240 FPS)

LinuxDebugAudioMarker g_debug_audio_markers[MAX_DEBUG_AUDIO_MARKERS] = {0};
int g_debug_marker_index = 0;

#endif // HANDMADE_INTERNAL

// ═══════════════════════════════════════════════════════════════
// ALSA DYNAMIC LOADING (Casey's Pattern)
// ═══════════════════════════════════════════════════════════════
// WHY: Game runs even if audio fails (graceful degradation)
// HOW: dlopen() to load library, dlsym() to get function pointers
// WHEN: At startup, before trying to play audio
// ═══════════════════════════════════════════════════════════════

// Stub functions return errors when ALSA isn't loaded

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
  (void)backbuffer;
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

ALSA_SND_PCM_DELAY(AlsaSndPcmDelayStub) {
  (void)pcm;
  (void)delayp;
  return -1; // Error: not available
}

// Global function pointers (start as stubs, replaced if ALSA loads
// successfully)

alsa_snd_pcm_open *SndPcmOpen_ = AlsaSndPcmOpenStub;
alsa_snd_pcm_set_params *SndPcmSetParams_ = AlsaSndPcmSetParamsStub;
alsa_snd_pcm_writei *SndPcmWritei_ = AlsaSndPcmWriteiStub;
alsa_snd_pcm_prepare *SndPcmPrepare_ = AlsaSndPcmPrepareStub;
alsa_snd_pcm_close *SndPcmClose_ = AlsaSndPcmCloseStub;
alsa_snd_strerror *SndStrerror_ = AlsaSndStrerrorStub;
alsa_snd_pcm_avail *SndPcmAvail_ = AlsaSndPcmAvailStub;
alsa_snd_pcm_recover *SndPcmRecover_ = AlsaSndPcmRecoverStub;
alsa_snd_pcm_delay *SndPcmDelay_ = AlsaSndPcmDelayStub;

// Global audio state (ALSA handle, buffers, latency params)
LinuxSoundOutput g_linux_sound_output = {0};

// ═══════════════════════════════════════════════════════════════
// Load ALSA Library (Casey's Win32LoadXInput pattern)
// ═══════════════════════════════════════════════════════════════
// WHY: Graceful degradation if ALSA missing
// HOW: Try loading libasound.so.2, fallback to libasound.so
// WHEN: Called once at startup
// ═══════════════════════════════════════════════════════════════

void linux_load_alsa(void) {
  printf("Loading ALSA library...\n");

  // Try versioned library first (more specific), then fallback
  // RTLD_LAZY = resolve symbols when first called (faster startup)

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
  g_linux_sound_output.alsa_library = alsa_lib;

  // Get function pointers (like Casey's GetProcAddress)
  // Macro reduces repetition - each function loaded the same way
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
  LOAD_ALSA_FN(SndPcmDelay_, "snd_pcm_delay", alsa_snd_pcm_delay);
#undef LOAD_ALSA_FN

  // Sanity check: did we get the core functions?
  if (SndPcmOpen_ == AlsaSndPcmOpenStub ||
      SndPcmSetParams_ == AlsaSndPcmSetParamsStub
      // || SndPcmWritei_ == AlsaSndPcmWriteiStub
  ) {
    fprintf(stderr, "❌ ALSA: Missing critical functions, audio disabled\n");
    // Reset to stubs
    SndPcmOpen_ = AlsaSndPcmOpenStub;
    SndPcmSetParams_ = AlsaSndPcmSetParamsStub;
    SndPcmWritei_ = AlsaSndPcmWriteiStub;
    dlclose(alsa_lib);
    g_linux_sound_output.alsa_library = NULL;
  }

  // Day 10: Optional latency measurement (not all ALSA versions have it)
  if (SndPcmDelay_ == AlsaSndPcmDelayStub) {
    printf("⚠️  ALSA: snd_pcm_delay not available\n");
    printf("    Day 10 latency measurement disabled\n");
    printf("    Falling back to Day 9 behavior\n");
  } else {
    printf("✓ ALSA: Day 10 latency measurement available\n");
  }

  printf("✓ ALSA library loaded successfully\n");
}

// ═══════════════════════════════════════════════════════════════
// Initialize Sound System
// ═══════════════════════════════════════════════════════════════
// WHAT: Opens ALSA device, sets format, calculates latency
// WHY: Game needs audio output synchronized to game logic
// WHEN: Called once at startup after loading ALSA library
// ═══════════════════════════════════════════════════════════════

bool linux_init_sound(GameSoundOutput *sound_output, int32_t samples_per_second,
                      int32_t buffer_size_bytes, int32_t game_update_hz) {
  // Store fixed game update rate (audio never adapts, only rendering does)
  sound_output->game_update_hz = game_update_hz;
  // ─────────────────────────────────────────────────────────────
  // Day 19: Frame-Based Latency Calculation
  // ─────────────────────────────────────────────────────────────
  // WHY: Latency should match game timing (not arbitrary 100ms)
  // HOW: 3 frames of audio ahead = (3 * samples_per_frame)
  // EXAMPLE: At 30 FPS: 3 * (48000/30) = 4800 samples = 100ms
  // ─────────────────────────────────────────────────────────────

  int32_t samples_per_frame = samples_per_second / sound_output->game_update_hz;
  int32_t latency_sample_count = FRAMES_OF_AUDIO_LATENCY * samples_per_frame;
  int64_t latency_us =
      (((int64_t)latency_sample_count * 1000000) / samples_per_second);
#if HANDMADE_INTERNAL
  printf("[AUDIO] Latency: %d samples (%.1f ms) at %d FPS\n",
         latency_sample_count, latency_us / 1000.0f, game_update_hz);
#endif

  // Store calculated latency
  g_linux_sound_output.latency_sample_count = latency_sample_count;
  g_linux_sound_output.latency_microseconds = latency_us;

  // Open audio device ("default" = system default, usually PulseAudio)
  // SND_PCM_STREAM_PLAYBACK = output audio (not recording)

  int err = SndPcmOpen(&g_linux_sound_output.handle,
                       "default",                     // Device
                       LINUX_SND_PCM_STREAM_PLAYBACK, // Output
                       SND_PCM_NONBLOCK);             // Blocking mode

  if (err < 0) {
    fprintf(stderr, "❌ Sound: Cannot open audio device: %s\n",
            SndStrerror(err));
    sound_output->is_initialized = false;
    return false;
  }

  printf("✅ Sound: Opened audio device\n");

  // Set audio format: 48kHz, 16-bit signed, stereo, interleaved
  // Latency passed in microseconds (ALSA manages buffering)

  err = SndPcmSetParams(g_linux_sound_output.handle,
                        LINUX_SND_PCM_FORMAT_S16_LE,         // 16-bit signed
                        LINUX_SND_PCM_ACCESS_RW_INTERLEAVED, // L-R-L-R
                        2,                                   // Stereo
                        samples_per_second,                  // 48000 Hz
                        1, // soft_resample (allow ALSA to resample if needed)
                        latency_us); // 100ms backbuffer

  if (err < 0) {
    fprintf(stderr, "❌ Sound: Cannot set parameters: %s\n", SndStrerror(err));
    SndPcmClose(g_linux_sound_output.handle);
    sound_output->is_initialized = false;
    return false;
  }

  printf("✅ Sound: Format set to %d Hz, 16-bit stereo\n", samples_per_second);

  // Store audio parameters
  sound_output->samples_per_second = samples_per_second;
  sound_output->bytes_per_sample = sizeof(int16_t) * 2; // L+R channels
  g_linux_sound_output.buffer_size = buffer_size_bytes;

  // Allocate temporary buffer for generating audio before writing to ALSA
  // Size: 1/15 second (Casey's pattern for manageable chunk size)
  g_linux_sound_output.sample_buffer_size = samples_per_second / 15;
  int sample_buffer_bytes =
      g_linux_sound_output.sample_buffer_size * sound_output->bytes_per_sample;

  g_linux_sound_output.sample_buffer = platform_allocate_memory(
      NULL, sample_buffer_bytes, PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE);

  if (!g_linux_sound_output.sample_buffer.base) {
    fprintf(stderr, "❌ Sound: Cannot allocate sample backbuffer\n");
    SndPcmClose(g_linux_sound_output.handle);
    sound_output->is_initialized = false;
    return false;
  }

  // Initialize test tone parameters (256 Hz sine wave for debugging)
  sound_output->running_sample_index = 0;
  sound_output->tone_hz = 256;
  sound_output->tone_volume = 6000;
  sound_output->wave_period = samples_per_second / sound_output->tone_hz;
  sound_output->t_sine = 0;
  sound_output->latency_sample_count = samples_per_second / 15;
  sound_output->pan_position = 0;
  sound_output->is_initialized = true;

#if HANDMADE_INTERNAL
  printf("✅ Sound: Initialized!\n");
  printf("   Sample rate:    %d Hz\n", samples_per_second);
  printf("   Buffer size:    %d frames (~%.1f ms)\n",
         g_linux_sound_output.sample_buffer_size,
         (float)g_linux_sound_output.sample_buffer_size / samples_per_second *
             1000.0f);
  printf("   Tone frequency: %d Hz\n", sound_output->tone_hz);
  printf("   Wave period:    %d samples\n", sound_output->wave_period);
  printf("   Sample rate:  %d Hz\n", samples_per_second);
  printf("   Buffer size:  %d bytes\n", buffer_size_bytes);
  printf("   Latency:      %.1f ms\n", latency_us / 1000.0f);
#endif

  return true;
}

// Check if ALSA latency measurement is available (not all versions have it)
file_scoped_fn inline bool linux_audio_has_latency_measurement(void) {
  return SndPcmDelay_ != AlsaSndPcmDelayStub;
}

#if HANDMADE_INTERNAL
// ═══════════════════════════════════════════════════════════════
// Day 19: Audio Debug Visualization
// ═══════════════════════════════════════════════════════════════
// WHAT: Draw vertical lines showing where audio is playing/writing
// WHY: Visualize audio timing to debug clicks and underruns
// HOW TO READ:
//   - White lines = PlayCursor (where hardware is reading)
//   - Red lines = WriteCursor (safe write boundary)
//   - Lines jumping = audio glitch
//   - Smooth scrolling = healthy audio
// ═══════════════════════════════════════════════════════════════

// Draw single vertical line on backbuffer
file_scoped_fn void linux_debug_draw_vertical(GameOffscreenBuffer *buffer,
                                              int x, int top, int bottom,
                                              uint32_t color) {

  static int call_count = 0;
  if (++call_count == 1) {
    printf("[DEBUG] linux_debug_draw_vertical called! x=%d color=0x%08X\n", x,
           color);
  }

  // Bounds check
  if (x < 0 || x >= buffer->width)
    return;
  if (top < 0)
    top = 0;
  if (bottom > buffer->height)
    bottom = buffer->height;

  // Get pixel pointer for this column
  uint8_t *pixel = (uint8_t *)buffer->memory.base +
                   x * buffer->bytes_per_pixel + top * buffer->pitch;

  // Draw vertical line
  for (int y = top; y < bottom; ++y) {
    *(uint32_t *)pixel = color;
    pixel += buffer->pitch;
  }
}

// Draw all audio debug markers (white = play cursor, red = write cursor)
void linux_debug_sync_display(GameOffscreenBuffer *buffer,
                              GameSoundOutput *sound_output,
                              LinuxDebugAudioMarker *markers,
                              int marker_count) {
  int pad_x = 16;
  int pad_y = 16;
  int top = pad_y;
  int bottom = buffer->height - pad_y;

  // Visualization window: 10× latency (gives good resolution without wrapping
  // too fast)
  int64_t visualization_window = sound_output->latency_sample_count * 10;
  real32 scale_factor =
      (real32)(buffer->width - 2 * pad_x) / (real32)visualization_window;

#if HANDMADE_INTERNAL
  // Print stats every 5 seconds (300 frames at 60fps)
  static int debug_counter = 0;
  if (++debug_counter % 300 == 5) {
    printf("[DEBUG MARKERS] Drawing %d markers:\n", marker_count);
    for (int i = 0; i < 5 && i < marker_count; ++i) { // Print first 5
      printf("  [%d] delay=%ld avail=%ld play=%ld write=%ld\n", i,
             (long)markers[i].delay_frames, (long)markers[i].avail_frames,
             (long)markers[i].play_cursor_sample,
             (long)markers[i].write_cursor_sample);
    }
  }
#endif

  // Draw all markers (skip uninitialized ones)
  for (int i = 0; i < marker_count; ++i) {
    LinuxDebugAudioMarker *marker = &markers[i];

    if (marker->play_cursor_sample == 0 && marker->write_cursor_sample == 0) {
      continue;
    }

    // Modulo wrap to visualization window
    int64_t play_wrapped = marker->play_cursor_sample % visualization_window;
    int64_t write_wrapped = marker->write_cursor_sample % visualization_window;

    // Draw
    int play_x = pad_x + (int)(scale_factor * play_wrapped);
    int write_x = pad_x + (int)(scale_factor * write_wrapped);

    linux_debug_draw_vertical(buffer, play_x, top, bottom, 0xFFFFFFFF); // White
    linux_debug_draw_vertical(buffer, write_x, top, bottom, 0xFF0000FF); // Red
  }
}

#endif // HANDMADE_INTERNAL

// ═══════════════════════════════════════════════════════════════
// Fill Sound Buffer (Day 19 Pattern)
// ═══════════════════════════════════════════════════════════════
// WHAT: Generate audio samples and write to ALSA
// HOW: Write exactly (samples_per_second / game_update_hz) per frame
// WHY: Fixed write amount keeps audio in sync with game logic
// WHEN: Called every frame AFTER game update
// ═══════════════════════════════════════════════════════════════
void linux_fill_sound_buffer(GameSoundOutput *sound_output) {
  if (!sound_output->is_initialized || !g_linux_sound_output.handle) {
    return;
  }

  // ─────────────────────────────────────────────────────────────
  // Query ALSA buffer state (delay = buffered audio, avail = free space)
  // ─────────────────────────────────────────────────────────────
  static int log_counter = 0;
  snd_pcm_sframes_t delay_frames = 0;
  snd_pcm_sframes_t avail_frames = 0;

  // How much audio is queued for playback?
  int delay_result = SndPcmDelay_(g_linux_sound_output.handle, &delay_frames);
  if (delay_result < 0) {
    fprintf(stderr, "[AUDIO] snd_pcm_delay error: %s\n",
            SndStrerror_(delay_result));
    delay_frames = 0;
  }

  // How much space is available for writing?
  avail_frames = SndPcmAvail_(g_linux_sound_output.handle);
  if (avail_frames < 0) {
    fprintf(stderr, "[AUDIO] snd_pcm_avail error: %s\n",
            SndStrerror_((int)avail_frames));
    avail_frames = 0;
  }

  // Calculate virtual cursors (like DirectSound's PlayCursor/WriteCursor)
  int64_t play_cursor_sample =
      sound_output->running_sample_index - delay_frames;
  int64_t write_cursor_sample =
      sound_output->running_sample_index + avail_frames;

#if HANDMADE_INTERNAL
  // Save marker for debug visualization
  LinuxDebugAudioMarker *marker = &g_debug_audio_markers[g_debug_marker_index];
  marker->delay_frames = delay_frames;
  marker->avail_frames = avail_frames;
  marker->play_cursor_sample = play_cursor_sample;
  marker->write_cursor_sample = write_cursor_sample;

  g_debug_marker_index++;
  if (g_debug_marker_index >= MAX_DEBUG_AUDIO_MARKERS) { // Ring buffer wrap
    g_debug_marker_index = 0;
  }
#endif

  // Debug logging
  if (++log_counter >= 60) {
    log_counter = 0;
    printf("[AUDIO] RSI:%ld  Delay:%ld  Avail:%ld  PlayCursor:%ld  "
           "WriteCursor:%ld\n",
           (long)sound_output->running_sample_index, (long)delay_frames,
           (long)avail_frames, (long)play_cursor_sample,
           (long)write_cursor_sample);
  }

  // ═══════════════════════════════════════════════════════════════
  // 🔊 DAY 19: CALCULATE HOW MUCH TO WRITE
  // ═══════════════════════════════════════════════════════════════
  // ✅ USE STORED GAME UPDATE HZ (always 30)
  int samples_per_frame =
      sound_output->samples_per_second / sound_output->game_update_hz;
  int frames_to_write = samples_per_frame;

  // Safety check
  if (frames_to_write > avail_frames) {
    frames_to_write = avail_frames;
  }

  if (frames_to_write <= 0) {
    return;
  }

  // ─────────────────────────────────────────────────────────────
  // Generate sine wave test audio with panning
  // ─────────────────────────────────────────────────────────────
  int16_t *sample_out = g_linux_sound_output.sample_buffer.base;

  for (long i = 0; i < frames_to_write; ++i) {
    real32 sine_value = sinf(sound_output->t_sine);
    int16_t sample_value = (int16_t)(sine_value * sound_output->tone_volume);

    int left_gain = (100 - sound_output->pan_position);
    int right_gain = (100 + sound_output->pan_position);

    *sample_out++ = (sample_value * left_gain) / 200;
    *sample_out++ = (sample_value * right_gain) / 200;

    sound_output->t_sine += M_double_PI / (float)sound_output->wave_period;

    if (sound_output->t_sine >= M_double_PI) {
      sound_output->t_sine -= M_double_PI;
    }

    sound_output->running_sample_index++;
  }

  // ─────────────────────────────────────────────────────────────
  // Write generated samples to ALSA (hardware buffer)
  // ─────────────────────────────────────────────────────────────
  long frames_written =
      SndPcmWritei_(g_linux_sound_output.handle,
                    g_linux_sound_output.sample_buffer.base, frames_to_write);

  if (frames_written < 0) {
    if (frames_written == -EAGAIN || frames_written == -EWOULDBLOCK) {
      return;
    }

    frames_written =
        SndPcmRecover_(g_linux_sound_output.handle, (int)frames_written, 1);
    if (frames_written < 0) {
      fprintf(stderr, "❌ Sound: Write failed: %s\n",
              SndStrerror_((int)frames_written));
      return;
    }
  }

  if (frames_written > 0 && frames_written != frames_to_write) {
    printf("⚠️ Partial write: wanted %d, wrote %ld\n", frames_to_write,
           frames_written);
  }
}

// Debug helper: Print current audio latency (Day 10)
void linux_debug_audio_latency(GameSoundOutput *sound_output) {
  if (!sound_output->is_initialized) {
    printf("❌ Audio: Not initialized\n");
    return;
  }

  printf("┌─────────────────────────────────────────────────────────┐\n");
  printf("│ 🔊 Audio Debug Info                                     │\n");
  printf("├─────────────────────────────────────────────────────────┤\n");

  // Check if Day 10 features available
  if (!linux_audio_has_latency_measurement()) {
    printf("│ ⚠️  Mode: Day 9 (Availability-Based)                    │\n");
    printf("│                                                         │\n");
    printf("│ snd_pcm_delay not available                             │\n");
    printf("│ Latency measurement disabled                            │\n");
    printf("│                                                         │\n");

    long frames_available = SndPcmAvail(g_linux_sound_output.handle);

    printf("│ Frames available: %ld                                   │\n",
           frames_available);
    printf("│ Sample rate:     %d Hz                                 │\n",
           sound_output->samples_per_second);
    printf("│ Frequency:       %d Hz                                 │\n",
           sound_output->tone_hz);
    printf("│ Volume:          %d / 15000                            │\n",
           sound_output->tone_volume);
    printf("│ Pan:             %+d (L=%d, R=%d)                      │\n",
           sound_output->pan_position, 100 - sound_output->pan_position,
           100 + sound_output->pan_position);
    printf("└─────────────────────────────────────────────────────────┘\n");
    return;
  }

  // Day 10 mode - full latency stats
  printf("│ ✅ Mode: Day 10 (Latency-Aware)                          │\n");
  printf("│                                                         │\n");

  snd_pcm_sframes_t delay_frames = 0;
  int err = SndPcmDelay(g_linux_sound_output.handle, &delay_frames);

  if (err < 0) {
    printf("│ ❌ Can't measure delay: %s                              │\n",
           SndStrerror(err));
    printf("└─────────────────────────────────────────────────────────┘\n");
    return;
  }

  long frames_available = SndPcmAvail(g_linux_sound_output.handle);

  float actual_latency_ms =
      (float)delay_frames / sound_output->samples_per_second * 1000.0f;
  float target_latency_ms = (float)sound_output->latency_sample_count /
                            sound_output->samples_per_second * 1000.0f;

  printf("│ Target latency:  %.1f ms (%d frames)                 │\n",
         target_latency_ms, sound_output->latency_sample_count);
  printf("│ Actual latency:  %.1f ms (%ld frames)                │\n",
         actual_latency_ms, (long)delay_frames);

  // Color-code latency status
  float latency_diff = actual_latency_ms - target_latency_ms;
  if (fabs(latency_diff) < 5.0f) {
    printf("│ Status:          ✅ GOOD (±%.1fms)                       │\n",
           latency_diff);
  } else if (fabs(latency_diff) < 10.0f) {
    printf("│ Status:          ⚠️  OK (±%.1fms)                         │\n",
           latency_diff);
  } else {
    printf("│ Status:          ❌ BAD (±%.1fms)                         │\n",
           latency_diff);
  }

  printf("│                                                         │\n");
  printf("│ Frames available: %ld                                   │\n",
         frames_available);
  printf("│ Sample rate:     %d Hz                                 │\n",
         sound_output->samples_per_second);
  printf("│ Frequency:       %d Hz                                 │\n",
         sound_output->tone_hz);
  printf("│ Volume:          %d / 15000                            │\n",
         sound_output->tone_volume);
  printf("│ Pan:             %+d (L=%d, R=%d)                      │\n",
         sound_output->pan_position, 100 - sound_output->pan_position,
         100 + sound_output->pan_position);
  printf("└─────────────────────────────────────────────────────────┘\n");
}

void linux_unload_alsa(GameSoundOutput *sound_output) {
  printf("Unloading ALSA audio...\n");

  // Free sample backbuffer
  platform_free_memory(&g_linux_sound_output.sample_buffer);

  // Close PCM device
  if (g_linux_sound_output.handle) {
    SndPcmClose(g_linux_sound_output.handle);
    g_linux_sound_output.handle = NULL;
  }

  // Unload ALSA library
  if (g_linux_sound_output.alsa_library) {
    dlclose(g_linux_sound_output.alsa_library);
    g_linux_sound_output.alsa_library = NULL;
  }

  sound_output->is_initialized = false;

  printf("✅ ALSA audio unloaded.\n");
}