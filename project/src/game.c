#include "game.h"
#include "base.h"
#include <stdio.h>
#include <sys/mman.h>

OffscreenBuffer g_backbuffer = {0};
SoundOutput g_sound_output = {0};
GameState g_game_state = {0};
local_persist_var GradientState g_gradient_state = {0};
local_persist_var PixelState g_pixel_state = {0};

INIT_BACKBUFFER_STATUS init_backbuffer(int width, int height,
                                       int bytes_per_pixel,
                                       pixel_composer_fn composer) {
  g_backbuffer.memory = NULL;
  g_backbuffer.width = width;
  g_backbuffer.height = height;
  g_backbuffer.bytes_per_pixel = bytes_per_pixel;
  g_backbuffer.pitch = g_backbuffer.width * g_backbuffer.bytes_per_pixel;
  int initial_initial_buffer_size = g_backbuffer.pitch * g_backbuffer.height;

  g_backbuffer.memory =
      mmap(NULL, initial_initial_buffer_size, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (g_backbuffer.memory == MAP_FAILED) {
    g_backbuffer.memory = NULL;
    fprintf(stderr, "mmap failed: could not allocate %d bytes\n",
            initial_initial_buffer_size);
    return INIT_BACKBUFFER_STATUS_MMAP_FAILED;
  }

  // Set pixel composer function for Raylib (R8G8B8A8)
  g_backbuffer.compose_pixel = composer;

  return INIT_BACKBUFFER_STATUS_SUCCESS;
}

void init_game_state() {
  g_game_state.controls = (GameControls){0};
  g_gradient_state = (GradientState){0};
  g_pixel_state = (PixelState){0};
  g_game_state.speed = 5;
  g_game_state.is_running = true;
  g_game_state.gamepad_id = -1; // No gamepad yet
}

void render_weird_gradient() {
  uint8_t *row = (uint8_t *)g_backbuffer.memory;

  // The following is correct for X11
  for (int y = 0; y < g_backbuffer.height; ++y) {
    uint32_t *pixels = (uint32_t *)row;
    for (int x = 0; x < g_backbuffer.width; ++x) {

      *pixels++ =
          g_backbuffer.compose_pixel(0, // Default red value (for both backends)
                                     (y + g_gradient_state.offset_y), //
                                     (x + g_gradient_state.offset_x),
                                     255 // Full opacity for Raylib
          );
    }
    row += g_backbuffer.pitch;
  }
}

void testPixelAnimation(int pixelColor) {
  // Test pixel animation
  uint32_t *pixels = (uint32_t *)g_backbuffer.memory;
  int total_pixels = g_backbuffer.width * g_backbuffer.height;

  int test_offset =
      g_pixel_state.offset_y * g_backbuffer.width + g_pixel_state.offset_x;

  if (test_offset < total_pixels) {
    pixels[test_offset] = pixelColor;
  }

  if (g_pixel_state.offset_x + 1 < g_backbuffer.width - 1) {
    g_pixel_state.offset_x += 1;
  } else {
    g_pixel_state.offset_x = 0;
    if (g_pixel_state.offset_y + 75 < g_backbuffer.height - 1) {
      g_pixel_state.offset_y += 75;
    } else {
      g_pixel_state.offset_y = 0;
    }
  }
}

// Handle game controls
inline void handle_controls() {

  if (g_game_state.controls.up) {
    g_gradient_state.offset_y += g_game_state.speed;
  }
  if (g_game_state.controls.left) {
    g_gradient_state.offset_x += g_game_state.speed;
  }
  if (g_game_state.controls.down) {
    g_gradient_state.offset_y -= g_game_state.speed;
  }
  if (g_game_state.controls.right) {
    g_gradient_state.offset_x -= g_game_state.speed;
  }

  // if (g_game_state.gamepad_id >= 0) {
  //   // Same math as Casey! (>> 12 = divide by 4096)
  //   // This converts -32767..+32767 to about -8..+8 pixels/frame
  //   int normalized_left_stick_x = g_game_state.controls.left_stick_x / 4096;
  //   int normalized_left_stick_y = g_game_state.controls.left_stick_y / 4096;

  //   g_gradient_state.offset_x -= normalized_left_stick_x;
  //   g_gradient_state.offset_y -= normalized_left_stick_y;

  //   // set_tone_frequency(
  //   //     512 +
  //   //     (int)(256.0f * ((real32)g_game_state.controls.left_stick_y /
  //   //     30000.0f)));
  //   if (normalized_left_stick_y != 0) {
  //     handle_update_tone_frequency(normalized_left_stick_y);
  //     g_game_state.controls.set_to_defined_tone = DEFINED_TONE_NONE;
  //   }
  //   if (normalized_left_stick_x != 0) {
  //     handle_update_tone_frequency(normalized_left_stick_x);
  //     g_game_state.controls.set_to_defined_tone = DEFINED_TONE_NONE;
  //   }
  //   // Optional: Start button resets
  //   if (g_game_state.controls.start) {
  //     g_gradient_state.offset_x = 0;
  //     g_gradient_state.offset_y = 0;
  //     printf("START pressed - reset offsets\n");
  //   }
  // }

  /*


  // ═══════════════════════════════════════════════════════════
  // 🎮 Analog stick controls (Casey's Day 6 pattern!)
  // ═══════════════════════════════════════════════════════════
  // Raylib gives us -1.0 to +1.0, Casey expects -32767 to +32767
  // So we convert: float * 32767 = int16 range
  // Then apply Casey's >> 12 bit shift

  if (game_state->gamepad_id >= 0) {
    // Convert Raylib's normalized floats to Casey's int16 range
    int16_t stick_x = (int16_t)(game_state->controls.left_stick_x * 32767.0f);
    int16_t stick_y = (int16_t)(game_state->controls.left_stick_y * 32767.0f);

    // Apply Casey's >> 12 math (divide by 4096)
    game_state->gradient.offset_x -= stick_x >> 12;
    game_state->gradient.offset_y -= stick_y >> 12;

    // ═══════════════════════════════════════════════════════════
    // Day 8: Use analog stick for frequency control
    // ═══════════════════════════════════════════════════════════
    // Matches your X11 backend exactly!
    if (stick_y >> 12 != 0) {
      handle_update_tone_frequency(stick_y >> 12);
    }
    if (stick_x >> 12 != 0) {
      handle_update_tone_frequency(stick_x >> 12);
    }

    // Start button resets
    if (game_state->controls.start) {
      game_state->gradient.offset_x = 0;
      game_state->gradient.offset_y = 0;
      printf("START pressed - reset offsets\n");
    }
  }
*/

  // Audio volume/pan controls
  if (g_game_state.controls.increase_sound_volume) {
    handle_increase_volume(500);
    g_game_state.controls.increase_sound_volume = false;
  }
  if (g_game_state.controls.decrease_sound_volume) {
    handle_increase_volume(-500);
    g_game_state.controls.decrease_sound_volume = false;
  }
  if (g_game_state.controls.move_sound_pan_left) {
    handle_increase_pan(-10);
    g_game_state.controls.move_sound_pan_left = false;
  }
  if (g_game_state.controls.move_sound_pan_right) {
    handle_increase_pan(10);
    g_game_state.controls.move_sound_pan_right = false;
  }

  switch (g_game_state.controls.set_to_defined_tone) {

  case DEFINED_TONE_C4:
    set_tone_frequency((int)261.63f);
    printf("🎵 Note: C4 (261.63 Hz)\n");
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_NONE;
    break;
  case DEFINED_TONE_D4:
    set_tone_frequency((int)293.66f);
    printf("🎵 Note: D4 (293.66 Hz)\n");
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_NONE;
    break;
  case DEFINED_TONE_E4:
    set_tone_frequency((int)329.63f);
    printf("🎵 Note: E4 (329.63 Hz)\n");
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_NONE;
    break;
  case DEFINED_TONE_F4:
    set_tone_frequency((int)349.23f);
    printf("🎵 Note: F4 (349.23 Hz)\n");
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_NONE;
    break;
  case DEFINED_TONE_G4:
    set_tone_frequency((int)392.00f);
    printf("🎵 Note: G4 (392.00 Hz)\n");
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_NONE;
    break;
  case DEFINED_TONE_A4:
    set_tone_frequency((int)440.00f);
    printf("🎵 Note: A4 (440.00 Hz) - Concert Pitch\n");
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_NONE;
    break;
  case DEFINED_TONE_B4:
    set_tone_frequency((int)493.88f);
    printf("🎵 Note: B4 (493.88 Hz)\n");
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_NONE;
    break;
  case DEFINED_TONE_C5:
    set_tone_frequency((int)523.25f);
    printf("🎵 Note: C5 (523.25 Hz)\n");
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_NONE;
    break;
  case DEFINED_TONE_NONE:
    break;
  }
  // controls.defined_tone = DEFINED_TONE_NONE;
}

// Audio control handlers

// NEW: Helper function to change frequency
inline void set_tone_frequency(int hz) {
  g_sound_output.tone_hz = (int)hz;
  g_sound_output.wave_period =
      g_sound_output.samples_per_second / g_sound_output.tone_hz;
  g_sound_output.running_sample_index = 0;
}

// Fun test
inline void handle_update_tone_frequency(int hz_to_add) {
  int new_hz = g_sound_output.tone_hz + hz_to_add;
  // Clamp to safe range
  if (new_hz < 60)
    new_hz = 60;
  if (new_hz > 1000)
    new_hz = 1000;

  set_tone_frequency((float)new_hz);

  printf("🎵 Tone frequency: %d Hz (period: %d samples)\n", new_hz,
         g_sound_output.wave_period);
}

inline void handle_increase_volume(int num) {
  int new_volume = g_sound_output.tone_volume + num;

  // Clamp to safe range
  if (new_volume < 0)
    new_volume = 0;
  if (new_volume > 15000)
    new_volume = 15000;

  g_sound_output.tone_volume = new_volume;
  printf("🔊 Volume: %d / %d (%.1f%%)\n", new_volume, 15000,
         (new_volume * 100.0f) / 15000);
}

/**
 * **Linear vs Equal-Power Panning:**
 *
 *
 * ```
 * Linear Panning (simple):
 *
 * ────────────────────────────
 *
 * Pan center: both channels at 50%
 * Problem: Sounds QUIETER in center (50% + 50% ≠ 100% perceived volume)
 *
 * Math: L = (100-pan)/200, R = (100+pan)/200
 *
 *
 * Equal-Power Panning (better):
 *
 * ─────────────────────────────
 *
 * Pan center: both channels at 70.7% (√2/2)
 * Result: Constant perceived loudness across pan range
 *
 *
 * Math: L = cos(angle), R = sin(angle)
 *       where angle = (pan+1) * π/4
 * ```
 *
 *
 * **Why the "center dip" happens with linear:**
 * ```
 * Human hearing perceives power, not amplitude
 * Power ∝ amplitude²
 *
 * Linear at center:
 *     L = 0.5, R = 0.5
 *     Total power = 0.5² + 0.5² = 0.25 + 0.25 = 0.5  ← Only 50%!
 *
 * Equal-power at center:
 *     L = 0.707, R = 0.707
 *     Total power = 0.707² + 0.707² = 0.5 + 0.5 = 1.0  ← Full 100%!
 * ```
 *
 *
 * **Visual comparison:**
 * ```
 * Linear Pan (has dip):
 * Perceived  │     ╱╲
 * Loudness   │   ╱    ╲
 *            │ ╱        ╲
 *            └──────────────
 *            L    C      R
 *
 * Equal-Power (flat):
 * Perceived  │ ──────────
 * Loudness   │
 *            │
 *            └──────────────
 *            L    C      R
 * ```
 *
 */
inline void handle_increase_pan(int num) {
  printf("lol");
  int new_pan = g_sound_output.pan_position + num;
  if (new_pan < -100)
    new_pan = -100;
  if (new_pan > 100)
    new_pan = 100;

  g_sound_output.pan_position = new_pan;

  // Visual indicator (same as X11)
  char indicator[50] = {0};
  int pos = (g_sound_output.pan_position + 100) * 20 / 200;
  for (int i = 0; i < 21; i++) {
    indicator[i] = (i == pos) ? '*' : '-';
  }
  indicator[21] = '\0';

  printf("🎧 Pan: %s %+d\n", indicator, g_sound_output.pan_position);
  printf("    L ◀%s▶ R\n", indicator);
}

void game_update_and_render(int pixel_color) {
  render_weird_gradient();
  testPixelAnimation(pixel_color);
}