#ifndef GAME_H
#define GAME_H

#include "utils/audio.h"
#include "utils/backbuffer.h"
#include <stdbool.h>
#include <stdint.h>

#define FIELD_WIDTH 12
#define FIELD_HEIGHT 18
#define CELL_SIZE 30

#define SIDEBAR_WIDTH 200 /* extra pixels to the right of the field */

#define TETROMINO_SPAN '.'
#define TETROMINO_BLOCK 'X'
#define TETROMINO_LAYER_COUNT 4
#define TETROMINO_SIZE 16
#define TETROMINOS_COUNT 7

/* Predefined colors */
#define COLOR_BLACK GAME_RGB(0, 0, 0)
#define COLOR_WHITE GAME_RGB(255, 255, 255)
#define COLOR_GRAY GAME_RGB(128, 128, 128)
#define COLOR_DARK_GRAY GAME_RGB(64, 64, 64)
#define COLOR_CYAN GAME_RGB(0, 255, 255)
#define COLOR_BLUE GAME_RGB(0, 0, 255)
#define COLOR_ORANGE GAME_RGB(255, 165, 0)
#define COLOR_YELLOW GAME_RGB(255, 255, 0)
#define COLOR_GREEN GAME_RGB(0, 255, 0)
#define COLOR_MAGENTA GAME_RGB(255, 0, 255)
#define COLOR_RED GAME_RGB(255, 0, 0)

/* ═══════════════════════════════════════════════════════════════════════════
 * Tetromino Types
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef enum {
  TETROMINO_I_IDX,
  TETROMINO_J_IDX,
  TETROMINO_L_IDX,
  TETROMINO_O_IDX,
  TETROMINO_S_IDX,
  TETROMINO_T_IDX,
  TETROMINO_Z_IDX,
} TETROMINO_BY_IDX;

typedef enum {
  TETRIS_FIELD_EMPTY,
  TETRIS_FIELD_I,
  TETRIS_FIELD_J,
  TETRIS_FIELD_L,
  TETRIS_FIELD_O,
  TETRIS_FIELD_S,
  TETRIS_FIELD_T,
  TETRIS_FIELD_Z,
  TETRIS_FIELD_WALL,
  TETRIS_FIELD_TMP_FLASH, /* Used to mark completed lines for flashing effect */
} TETRIS_FIELD_CELL;

typedef enum {
  TETROMINO_R_0,
  TETROMINO_R_90,
  TETROMINO_R_180,
  TETROMINO_R_270,
} TETROMINO_R_DIR;

typedef struct {
  /** Accumulates delta time while button is held */
  float timer;

  /** Time before first auto-repeat (e.g., 0.2 = 200ms) */
  float initial_delay;

  /** Time between auto-repeats (e.g., 0.1 = 100ms) */
  float interval;

  bool is_active; /* Whether the button is currently active (held down) */
} RepeatInterval;

typedef enum {
  TETROMINO_ROTATE_X_NONE,
  TETROMINO_ROTATE_X_GO_LEFT,
  TETROMINO_ROTATE_X_GO_RIGHT,
} TETROMINO_ROTATE_X_VALUE;
typedef struct {
  int x;                          /* starting column */
  int y;                          /* starting row    */
  TETROMINO_BY_IDX index;         /* which tetromino */
  TETROMINO_BY_IDX next_index;    /* next tetromino */
  TETROMINO_R_DIR rotate_x_value; /* current rotation: 0, 1, 2, or 3 */
  TETROMINO_ROTATE_X_VALUE next_rotate_x_value;
} CurrentPiece;

/* ── Input System ─────────────────────────────────────── */

typedef struct {
  /**
   * Number of state changes this frame.
   * 0 = No change (button held or released entire frame)
   * 1 = Changed once (normal press or release)
   * 2 = Changed twice (pressed then released, or vice versa)
   *
   * Why track this? Consider a 30fps game where the user taps a button
   * for 20ms. At 30fps, each frame is 33ms. The button went down AND up
   * within one frame — half_transition_count = 2.
   *
   * For gameplay, we usually care about: "Did the button GO DOWN this frame?"
   * That's true if: ended_down == 1 AND half_transition_count > 0
   */
  int half_transition_count;

  /**
   * Final state at end of frame.
   * 1 = button is currently pressed
   * 0 = button is currently released
   */
  int ended_down;
} GameButtonState;

#define BUTTON_COUNT 4
// typedef struct {
// union
//   GameButtonState buttons[BUTTON_COUNT]; // Iterate
//   struct {
//   };
// }
// } GameInput;
typedef struct {
  union {
    GameButtonState buttons[BUTTON_COUNT];
    struct {
      GameButtonState move_left;  /* 1 if left arrow pressed this frame */
      GameButtonState move_right; /* 1 if right arrow pressed this frame */
      GameButtonState move_down;  /* 1 if down arrow pressed this frame */
      GameButtonState rotate_x;   /* 1 if X/Z pressed this frame */
    };
  };
} GameInput;

/* Helper macro to update button state.
 * Tracks transitions and final state.
 *
 * Usage: UPDATE_BUTTON(input->move_left.button, is_key_down);
 */
#define UPDATE_BUTTON(button, is_down)                                         \
  do {                                                                         \
    if ((button).ended_down != (is_down)) {                                    \
      (button).half_transition_count++;                                        \
      (button).ended_down = (is_down);                                         \
    }                                                                          \
  } while (0)

typedef struct {
  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT]; /* the play field */
  CurrentPiece current_piece;
  int score;
  int pieces_count; /* total pieces locked — used for difficulty scaling */
  bool is_game_over;
  int level;

  struct {
    int indexes[TETROMINO_LAYER_COUNT]; /* row indices of completed lines this
                                           lock */
    int count;                  /* how many entries in lines[] are valid */
    RepeatInterval flash_timer; /* countdown: while > 0, game is paused
                                        showing white flash */
  } completed_lines;

  struct {
    RepeatInterval move_left;  /* 1 if left arrow pressed this frame */
    RepeatInterval move_right; /* 1 if right arrow pressed this frame */
    RepeatInterval move_down;  /* 1 if down arrow pressed this frame */
    /* rotate doesn't need auto-repeat */
  } input_repeat;
  // Replace speed/speed_count with time-based fields
  struct {
    RepeatInterval rotate_direction;
  } input_values;

  // Audio state
  GameAudioState audio;
  int should_quit;    /* 1 if window closed or Escape pressed */
  int should_restart; /* 1 if R pressed this frame */
} GameState;

/* ── Piece data ─────────────────────────────────────── */

/* 7 tetrominoes, each a 4×4 grid of '.' (empty) and 'X' (solid).
   Defined in tetris.c — 'extern' tells the compiler
   "trust me, it exists somewhere else." */
extern const char *TETROMINOES[7];

/* Initialization */
void game_init(GameState *state);
void prepare_input_frame(GameInput *old_input, GameInput *new_input);

/* Game Logic */
int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r);
int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                             int pos_x, int pos_y);
void game_update(GameState *state, GameInput *input, float delta_time);

/* Rendering - Platform Independent! */
void game_render(Backbuffer *backbuffer, GameState *state);

/* Initialize audio state (call from game_init) */
void game_audio_init(GameAudioState *audio, int samples_per_second);

/* Queue a sound effect to play */
void game_play_sound(GameAudioState *audio, SOUND_ID sound);
/* Queue a sound effect with specific pan position */
/* pan: -1.0 = full left, 0.0 = center, 1.0 = full right */
void game_play_sound_at(GameAudioState *audio, SOUND_ID sound,
                        float pan_position);

/* Start/stop background music */
void game_music_play(GameAudioState *audio);
void game_music_stop(GameAudioState *audio);

/* Platform calls this to get audio samples */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer);
void game_audio_update(GameAudioState *audio, float delta_time);

#endif // GAMEH
