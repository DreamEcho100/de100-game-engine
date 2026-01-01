#include "game.h"
#include "base.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>

// ═══════════════════════════════════════════════════════════════
// PLATFORM-SHARED STATE (declared extern in game.h)
// ═══════════════════════════════════════════════════════════════
OffscreenBuffer g_backbuffer = {0};
SoundOutput g_sound_output = {0};
bool is_game_running = true;

// CONFIGURATION (could be const)
int KEYBOARD_CONTROLLER_INDEX = 0;

// ═══════════════════════════════════════════════════════════════
// GAME-PRIVATE STATE (not visible to platform)
// ═══════════════════════════════════════════════════════════════
file_scoped_global_var GameState g_game_state = {0};

file_scoped_global_var inline real32 apply_deadzone(real32 value) {
  if (fabsf(value) < CONTROLLER_DEADZONE) {
    return 0.0f;
  }
  return value;
}

file_scoped_global_var inline bool
controller_has_input(GameControllerInput *controller) {
  return (fabsf(controller->end_x) > CONTROLLER_DEADZONE ||
          fabsf(controller->end_y) > CONTROLLER_DEADZONE ||
          controller->up.ended_down || controller->down.ended_down ||
          controller->left.ended_down || controller->right.ended_down);
}

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
  g_game_state = (GameState){0};
  g_game_state.speed = 5;
  is_game_running = true;
}

void game_shutdown() {
  is_game_running = false;
  // Equivalent to c++ `delete g_game_state`
  g_game_state = (GameState){0};
}

void render_weird_gradient() {
  uint8_t *row = (uint8_t *)g_backbuffer.memory;

  // The following is correct for X11
  for (int y = 0; y < g_backbuffer.height; ++y) {
    uint32_t *pixels = (uint32_t *)row;
    for (int x = 0; x < g_backbuffer.width; ++x) {

      *pixels++ = g_backbuffer.compose_pixel(
          0, // Default red value (for both backends)
          (y + g_game_state.gradient_state.offset_y), //
          (x + g_game_state.gradient_state.offset_x),
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

  int test_offset = g_game_state.pixel_state.offset_y * g_backbuffer.width +
                    g_game_state.pixel_state.offset_x;

  if (test_offset < total_pixels) {
    pixels[test_offset] = pixelColor;
  }

  if (g_game_state.pixel_state.offset_x + 1 < g_backbuffer.width - 1) {
    g_game_state.pixel_state.offset_x += 1;
  } else {
    g_game_state.pixel_state.offset_x = 0;
    if (g_game_state.pixel_state.offset_y + 75 < g_backbuffer.height - 1) {
      g_game_state.pixel_state.offset_y += 75;
    } else {
      g_game_state.pixel_state.offset_y = 0;
    }
  }
}

// Handle game controls
inline void handle_controls(GameControllerInput *controller) {
  if (controller->is_analog) {
    // ═════════════════════════════════════════════════════════
    // ANALOG MOVEMENT (Joystick)
    // ═════════════════════════════════════════════════════════
    // Casey's Day 13 formula:
    //   BlueOffset += (int)(4.0f * Input0->EndX);
    //   ToneHz = 256 + (int)(128.0f * Input0->EndY);
    //
    // end_x/end_y are NORMALIZED (-1.0 to +1.0)!
    // ═════════════════════════════════════════════════════════

    real32 x = apply_deadzone(controller->end_x);
    real32 y = apply_deadzone(controller->end_y);

    // Horizontal stick controls blue offset
    g_game_state.gradient_state.offset_x -= (int)(4.0f * x);
    g_game_state.gradient_state.offset_y -= (int)(4.0f * y);

    // Vertical stick controls tone frequency
    // NOTE: `tone_hz` is moved to the game layer, but initilized by the
    // platform layer
    g_sound_output.tone_hz = 256 + (int)(128.0f * y);

  } else {
    // ═════════════════════════════════════════════════════════
    // DIGITAL MOVEMENT (Keyboard only)
    // ═════════════════════════════════════════════════════════
    // Option A: Use button states (discrete, snappy)
    if (controller->up.ended_down) {
      g_game_state.gradient_state.offset_y += g_game_state.speed;
    }
    if (controller->down.ended_down) {
      g_game_state.gradient_state.offset_y -= g_game_state.speed;
    }
    if (controller->left.ended_down) {
      g_game_state.gradient_state.offset_x += g_game_state.speed;
    }
    if (controller->right.ended_down) {
      g_game_state.gradient_state.offset_x -= g_game_state.speed;
    }

    // OR Option B: Use analog values (same formula as joystick)
    // g_game_state.gradient_state.offset_x += (int)(4.0f * controller->end_x);
    // g_game_state.gradient_state.offset_y += (int)(4.0f * controller->end_y);

    // Pick ONE, not both!

    // g_game_state.gradient_state.offset_x += (int)(4.0f * controller->end_x);
    // g_game_state.gradient_state.offset_y += (int)(4.0f * controller->end_y);
    // g_sound_output.tone_hz = 256 + (int)(128.0f * controller->end_y);
  }

  // Clamp tone
  if (g_sound_output.tone_hz < 20)
    g_sound_output.tone_hz = 20;
  if (g_sound_output.tone_hz > 2000)
    g_sound_output.tone_hz = 2000;

  // // Audio volume/pan controls
  // if (controller->increase_sound_volume) {
  //   handle_increase_volume(500);
  //   controller->increase_sound_volume = false;
  // }
  // if (controller->decrease_sound_volume) {
  //   handle_increase_volume(-500);
  //   controller->decrease_sound_volume = false;
  // }
  // if (controller->move_sound_pan_left) {
  //   handle_increase_pan(-10);
  //   controller->move_sound_pan_left = false;
  // }
  // if (controller->move_sound_pan_right) {
  //   handle_increase_pan(10);
  //   controller->move_sound_pan_right = false;
  // }

  // switch (controller->set_to_defined_tone) {

  // case DEFINED_TONE_C4:
  //   set_tone_frequency((int)261.63f);
  //   printf("🎵 Note: C4 (261.63 Hz)\n");
  //   controller->set_to_defined_tone = DEFINED_TONE_NONE;
  //   break;
  // case DEFINED_TONE_D4:
  //   set_tone_frequency((int)293.66f);
  //   printf("🎵 Note: D4 (293.66 Hz)\n");
  //   controller->set_to_defined_tone = DEFINED_TONE_NONE;
  //   break;
  // case DEFINED_TONE_E4:
  //   set_tone_frequency((int)329.63f);
  //   printf("🎵 Note: E4 (329.63 Hz)\n");
  //   controller->set_to_defined_tone = DEFINED_TONE_NONE;
  //   break;
  // case DEFINED_TONE_F4:
  //   set_tone_frequency((int)349.23f);
  //   printf("🎵 Note: F4 (349.23 Hz)\n");
  //   controller->set_to_defined_tone = DEFINED_TONE_NONE;
  //   break;
  // case DEFINED_TONE_G4:
  //   set_tone_frequency((int)392.00f);
  //   printf("🎵 Note: G4 (392.00 Hz)\n");
  //   controller->set_to_defined_tone = DEFINED_TONE_NONE;
  //   break;
  // case DEFINED_TONE_A4:
  //   set_tone_frequency((int)440.00f);
  //   printf("🎵 Note: A4 (440.00 Hz) - Concert Pitch\n");
  //   controller->set_to_defined_tone = DEFINED_TONE_NONE;
  //   break;
  // case DEFINED_TONE_B4:
  //   set_tone_frequency((int)493.88f);
  //   printf("🎵 Note: B4 (493.88 Hz)\n");
  //   controller->set_to_defined_tone = DEFINED_TONE_NONE;
  //   break;
  // case DEFINED_TONE_C5:
  //   set_tone_frequency((int)523.25f);
  //   printf("🎵 Note: C5 (523.25 Hz)\n");
  //   controller->set_to_defined_tone = DEFINED_TONE_NONE;
  //   break;
  // case DEFINED_TONE_NONE:
  //   break;
  // }
  // // controls.defined_tone = DEFINED_TONE_NONE;
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

inline void process_game_button_state(bool is_down, GameButtonState *old_state,
                                      GameButtonState *new_state) {
  new_state->ended_down = is_down;
  if (old_state->ended_down != new_state->ended_down) {
    new_state->half_transition_count++;
  }
  // new_state->half_transition_count +=
  //     ((old_state->ended_down != new_state->ended_down && 1) || 0);
}

void game_update_and_render(GameInput *input) {

  static int frame = 0;
  frame++;

  // Find active controller
  GameControllerInput *active_controller = NULL;

  // Priority 1: Check for active joystick input (controllers 1-4)
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    if (i == KEYBOARD_CONTROLLER_INDEX)
      continue;

    GameControllerInput *c = &input->controllers[i];
    if (c->is_connected) {
      // Use helper function instead of hardcoded 0.15!
      if (controller_has_input(c)) {
        active_controller = c;
        break;
      }
    }
  }

  // Priority 2: Check keyboard if no joystick input
  if (!active_controller) {
    GameControllerInput *keyboard =
        &input->controllers[KEYBOARD_CONTROLLER_INDEX];
    bool has_input = (keyboard->up.ended_down || keyboard->down.ended_down ||
                      keyboard->left.ended_down || keyboard->right.ended_down);
    if (has_input) {
      active_controller = keyboard;
    }
  }

  // Fallback: Always use keyboard (controller[0]) if nothing else
  if (!active_controller) {
    active_controller = &input->controllers[KEYBOARD_CONTROLLER_INDEX];
  }

  if (frame % 60 == 0) {
    printf("Frame %d: active_controller=%p\n", frame,
           (void *)active_controller);
    if (active_controller) {
      printf("  is_analog=%d end_x=%.2f end_y=%.2f\n",
             active_controller->is_analog, active_controller->end_x,
             active_controller->end_y);
      printf("  up=%d down=%d left=%d right=%d\n",
             active_controller->up.ended_down,
             active_controller->down.ended_down,
             active_controller->left.ended_down,
             active_controller->right.ended_down);
      printf("Frame %d: is_analog=%d end_x=%.2f end_y=%.2f "
             "up=%d down=%d left=%d right=%d\n",
             frame, active_controller->is_analog, active_controller->end_x,
             active_controller->end_y, active_controller->up.ended_down,
             active_controller->down.ended_down,
             active_controller->left.ended_down,
             active_controller->right.ended_down);
    }
  }

  handle_controls(active_controller);

  render_weird_gradient();

  int testPixelAnimationColor = g_backbuffer.compose_pixel(255, 0, 0, 255);
  testPixelAnimation(testPixelAnimationColor);
}

// #ifdef PLATFORM_X11
// // On X11, define colors in BGRA directly
// #define GAME_RED 0x0000FFFF
// #else
// // On Raylib/other, use RGBA
// #define GAME_RED 0xFF0000FF
// #endif