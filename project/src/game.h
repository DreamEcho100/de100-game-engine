#ifndef GAME_H
#define GAME_H

#include "platform/platform.h"
#include "base.h"

typedef struct {
  int offset_x;
  int offset_y;
} GradientState;

typedef struct {
  int offset_x;
  int offset_y;
} PixelState;

typedef enum {
  INIT_BACKBUFFER_STATUS_SUCCESS = 0,
  INIT_BACKBUFFER_STATUS_MMAP_FAILED = 1,
} INIT_BACKBUFFER_STATUS;

typedef struct {
  void *memory; // Raw pixel memory (our canvas!)
  int width;    // Current backbuffer dimensions
  int height;
  int pitch;
  int bytes_per_pixel;
  pixel_composer_fn compose_pixel;
} OffscreenBuffer;


/**
 * GLOBAL STATE
 *
 * In C, we often use global variables for things that need to persist
 * across function calls. Think of these like module-level variables in JS.
 *
 * Casey uses globals to avoid passing everything as parameters.
 * This is a design choice - simpler but less "pure".
 */

// Button states (mirrors Casey's XInput button layout)

typedef enum {
  DEFINED_TONE_NONE,
  DEFINED_TONE_C4,
  DEFINED_TONE_D4,
  DEFINED_TONE_E4,
  DEFINED_TONE_F4,
  DEFINED_TONE_G4,
  DEFINED_TONE_A4,
  DEFINED_TONE_B4,
  DEFINED_TONE_C5,
} DefinedTone;

typedef struct {
  bool up;
  bool down;
  bool left;
  bool right;

  bool start;
  bool back;
  bool a_button;
  bool b_button;
  bool x_button;
  bool y_button;
  bool left_shoulder;
  bool right_shoulder;

  // Analog sticks (16-bit signed, like XInput)
  int16_t left_stick_x;
  int16_t left_stick_y;
  int16_t right_stick_x;
  int16_t right_stick_y;
  int16_t left_trigger;
  int16_t right_trigger;

  // // Analog sticks (normalized -1.0 to +1.0, Raylib style)
  // float left_stick_x;
  // float left_stick_y;
  // float right_stick_x;
  // float right_stick_y;
  // float left_trigger;
  // float right_trigger;

  //
  bool increase_sound_volume;
  bool decrease_sound_volume;
  bool move_sound_pan_left;
  bool move_sound_pan_right;

  DefinedTone set_to_defined_tone;
} GameControls;

typedef struct {
  bool is_initialized;

  // Audio format parameters (Casey's Day 7)
  int32_t samples_per_second; // 48000 Hz
  int32_t bytes_per_sample;   // 4 (16-bit stereo)


  // Day 8: Sound generation state
  uint32_t running_sample_index;
  int tone_hz;
  int16_t tone_volume;
  int wave_period;

  // Day 9
  real32 t_sine;            // Phase accumulator (0 to 2π)
  int latency_sample_count; // How many samples to backbuffer ahead

  //
  int pan_position; // -100 (left) to +100 (right)
} SoundOutput;


typedef struct {
  GameControls controls;
  // GradientState gradient;
  // PixelState pixel;
  int speed;
  bool is_running;
  int gamepad_id; // Which gamepad to use (0-3)
} GameState;

extern OffscreenBuffer g_backbuffer;
extern SoundOutput g_sound_output;
extern GameState g_game_state;

INIT_BACKBUFFER_STATUS init_backbuffer(int width, int height,
                                       int bytes_per_pixel,
                                       pixel_composer_fn composer);
void init_game_state();
void render_weird_gradient();
void testPixelAnimation(
    
    int pixel_color);
void game_update_and_render(int pixel_color);

//
void handle_controls();

//
void set_tone_frequency(int hz);
void handle_update_tone_frequency(int hz_to_add);
void handle_increase_volume(int num);
void handle_increase_pan(int num);

#endif // GAME_H
