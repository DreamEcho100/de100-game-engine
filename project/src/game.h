#ifndef GAME_H
#define GAME_H

#include "platform/platform.h"
#include "base.h"

#define CONTROLLER_DEADZONE 0.10f

typedef struct {
  int offset_x;
  int offset_y;
} GradientState;

typedef struct {
  int offset_x;
  int offset_y;
} PixelState;

typedef struct {
  GradientState gradient_state;
  PixelState pixel_state;
  int speed;
} GameState;

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

// typedef struct {
//   bool up;
//   bool down;
//   bool left;
//   bool right;

//   bool start;
//   bool back;
//   bool a_button;
//   bool b_button;
//   bool x_button;
//   bool y_button;
//   bool left_shoulder;
//   bool right_shoulder;

//   // Analog sticks (16-bit signed, like XInput)
//   int16_t left_stick_x;
//   int16_t left_stick_y;
//   int16_t right_stick_x;
//   int16_t right_stick_y;
//   int16_t left_trigger;
//   int16_t right_trigger;

//   // // Analog sticks (normalized -1.0 to +1.0, Raylib style)
//   // float left_stick_x;
//   // float left_stick_y;
//   // float right_stick_x;
//   // float right_stick_y;
//   // float left_trigger;
//   // float right_trigger;

//   //
//   bool increase_sound_volume;
//   bool decrease_sound_volume;
//   bool move_sound_pan_left;
//   bool move_sound_pan_right;

//   DefinedTone set_to_defined_tone;
// } GameControls;

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

/**
  * 🎮 DAY 13: PLATFORM-INDEPENDENT INPUT ABSTRACTION
  * ═══════════════════════════════════════════════════════════════
  *
  * These structures replace ALL platform-specific input handling.
  * Game layer ONLY sees these - no X11 KeySym, no joystick events!
  *
  * Casey's Day 13 pattern: Abstract button state + analog sticks
  * ═══════════════════════════════════════════════════════════════
  *
  * BUTTON STATE (replaces raw bool flags)
  * ───────────────────────────────────────────────────────────────
  *
  * Tracks BOTH current state AND transitions (press/release events).
  *
  * Casey's pattern:
  *   EndedDown = final state this frame
  *   HalfTransitionCount = how many times it changed
  *
  * Examples:
  *   HalfTransitionCount=0, EndedDown=false → Button not pressed, no change
  *   HalfTransitionCount=1, EndedDown=true  → Button JUST pressed!
  *   HalfTransitionCount=0, EndedDown=true  → Button held down
  *   HalfTransitionCount=1, EndedDown=false → Button JUST released!
  *   HalfTransitionCount=2, EndedDown=true  → Pressed THEN released THEN pressed
  *   (same frame!)
  * ───────────────────────────────────────────────────────────────
  */
typedef struct {
  /**  Number of state changes this frame */
  int half_transition_count;
  /** Final state (true = pressed, false = released) */
  bool32 ended_down;
} GameButtonState;

/**
* CONTROLLER INPUT (replaces your GameControls struct)
* ───────────────────────────────────────────────────────────────
*
* Platform-agnostic controller abstraction.
* Works with:
*   - Xbox controllers (via joystick API)
*   - PlayStation controllers (via joystick API)
*   - Keyboard (converted to analog values)
*   - Future: Steam Deck, Switch Pro, etc.
*
* Casey's design: Analog sticks normalized to -1.0 to +1.0
*
* ────────────────────
*/
typedef struct {
  /** `true` = joystick, `false` = keyboard (digital) */
  bool32 is_analog;

  /**
   * ─────────────────────────────────────────────────────────────
   * Analog stick tracking (all values -1.0 to +1.0)
   * ─────────────────────────────────────────────────────────────
   * start_x/y: Stick position at beginning of frame
   * min_x/y:   Minimum position seen this frame (for deadzone)
   * max_x/y:   Maximum position seen this frame (for deadzone)
   * end_x/y:   Final position this frame (what game uses)
   *
   * NOTE: Day 13: Just use end_x/y (min/max for future Day 14+)
   * ─────────────────────────────────────────────────────────────
   */
  //
  real32 start_x, start_y;
  real32 min_x, min_y;
  real32 max_x, max_y;
  real32 end_x, end_y;


  /**
  * Can access as:
  *   - Array: for(int i=0; i<6; i++) buttons[i]...
  *   - Named: if(controller->up.ended_down) ...
  *
  * SAME MEMORY, TWO ACCESS PATTERNS! ✨
  * ─────────────────────────────────────────────────────────────
  */
  union {
    GameButtonState buttons[6];
    struct {
      GameButtonState up;
      GameButtonState down;
      GameButtonState left;
      GameButtonState right;

      // GameButtonState start;
      // GameButtonState back;
      // GameButtonState a_button;
      // GameButtonState b_button;
      // GameButtonState x_button;
      // GameButtonState y_button;
      GameButtonState left_shoulder;
      GameButtonState right_shoulder;
    };
  };

  // id: useful for debugging multiple controllers and for joystick file
  // descriptor
  // int id;
  int controller_index; // Which controller slot (0-3)
  // int fd;               // File descriptor for /dev/input/jsX
  bool is_connected;    // Is this controller active?
} GameControllerInput;

/**
* GAME INPUT (replaces your GameControls struct)
* ───────────────────────────────────────────────────────────────
*
* Supports up to 4 controllers (local multiplayer ready!)
*
* Casey's pattern:
*   Controllers[0] = Player 1
*   Controllers[1] = Player 2 (future)
*   Controllers[2] = Player 3 (future)
*   Controllers[3] = Player 4 (future)
*
* ───────────────────────────────────────────────────────────────
*/
typedef struct {
  GameControllerInput controllers[5];
} GameInput;

// typedef struct {
//   int speed;
//   bool is_running;
//   // int gamepad_id; // Which gamepad to use (0-3)
// } GameState;

#define MAX_CONTROLLER_COUNT 5
#define MAX_KEYBOARD_COUNT 1
#define MAX_JOYSTICK_COUNT (MAX_CONTROLLER_COUNT - MAX_KEYBOARD_COUNT)


extern OffscreenBuffer g_backbuffer;
extern SoundOutput g_sound_output;
extern int KEYBOARD_CONTROLLER_INDEX;
// extern GameState g_game_state;
extern bool is_game_running;

INIT_BACKBUFFER_STATUS init_backbuffer(int width, int height,
                                       int bytes_per_pixel,
                                       pixel_composer_fn composer);
void init_game_state();
void render_weird_gradient();
void testPixelAnimation(int pixel_color);

/**
* 🎮 DAY 13: Updated Game Entry Point
* ═══════════════════════════════════════════════════════════════
*
* New signature takes abstract input (not pixel_color parameter).
*
* Casey's Day 13 change:
*   OLD: GameUpdateAndRender(Buffer, XOffset, YOffset, Sound, ToneHz)
*   NEW: GameUpdateAndRender(Input, Buffer, Sound)
*
* We add:
*   game_input *input  ← Platform-agnostic input state
*
* Game state (offsets, tone_hz, etc.) now lives in game.c as
* local_persist variables (hidden from platform!).
*
* ═══════════════════════════════════════════════════════════════
*/
void game_update_and_render(GameInput *input);


//
void set_tone_frequency(int hz);
void handle_update_tone_frequency(int hz_to_add);
void handle_increase_volume(int num);
void handle_increase_pan(int num);
void handle_controls(GameControllerInput *controller);

#endif // GAME_H
