#include "game.h"
#include "utils/draw-shapes.h"
#include "utils/draw-text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Tetromino Data
 * ═══════════════════════════════════════════════════════════════════════════
 */

const char TETROMINO_I[TETROMINO_SIZE] =
    "..X...X...X...X."; /* 0: I — vertical bar       */
const char TETROMINO_J[TETROMINO_SIZE] =
    "..X...X..XX....."; /* 5: J — J-shape (mirror L) */
const char TETROMINO_L[TETROMINO_SIZE] =
    ".X...X...XX....."; /* 4: L — L-shape            */
const char TETROMINO_O[TETROMINO_SIZE] =
    ".....XX..XX....."; /* 6: O — square (uses 3×3) */
const char TETROMINO_S[TETROMINO_SIZE] =
    ".X...XX...X....."; /* 1: S — S-shape            */
const char TETROMINO_T[TETROMINO_SIZE] =
    "..X..XX...X....."; /* 3: T — T-shape            */
const char TETROMINO_Z[TETROMINO_SIZE] =
    "..X..XX..X......"; /* 2: Z — Z-shape            */

const char *TETROMINOES[TETROMINOS_COUNT] = {
    TETROMINO_I, //
    TETROMINO_J, //
    TETROMINO_L, //
    TETROMINO_O, //
    TETROMINO_S, //
    TETROMINO_T, //
    TETROMINO_Z, //
};
/* ═══════════════════════════════════════════════════════════════════════════
 * Game-Specific Drawing
 * ═══════════════════════════════════════════════════════════════════════════
 */

static uint32_t get_tetromino_color(TETROMINO_BY_IDX index) {
  switch (index) {
  case TETROMINO_I_IDX:
    return COLOR_CYAN;
  case TETROMINO_J_IDX:
    return COLOR_BLUE;
  case TETROMINO_L_IDX:
    return COLOR_ORANGE;
  case TETROMINO_O_IDX:
    return COLOR_YELLOW;
  case TETROMINO_S_IDX:
    return COLOR_GREEN;
  case TETROMINO_T_IDX:
    return COLOR_MAGENTA;
  case TETROMINO_Z_IDX:
    return COLOR_RED;
  default:
    return COLOR_GRAY;
  }
}

static void draw_cell(Backbuffer *bb, int col, int row, uint32_t color) {
  int x = col * CELL_SIZE + 1;
  int y = row * CELL_SIZE + 1;
  int w = CELL_SIZE - 2;
  int h = CELL_SIZE - 2;
  draw_rect(bb, x, y, w, h, color);
}

static void draw_piece(Backbuffer *bb, int piece_index, int field_col,
                       int field_row, uint32_t color,
                       TETROMINO_R_DIR rotation) {
  for (int py = 0; py < TETROMINO_LAYER_COUNT; py++) {
    for (int px = 0; px < TETROMINO_LAYER_COUNT; px++) {
      int pi = tetromino_pos_value(px, py, rotation);
      if (TETROMINOES[piece_index][pi] == TETROMINO_BLOCK) {
        draw_cell(bb, field_col + px, field_row + py, color);
      }
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main Render Function - Called by Platform Layer
 * ═══════════════════════════════════════════════════════════════════════════
 */

void game_render(Backbuffer *backbuffer, GameState *state) {
  /* Clear to black */
  for (int i = 0; i < backbuffer->width * backbuffer->height; i++) {
    backbuffer->pixels[i] = COLOR_BLACK;
  }

  /* ═══════════════════════════════════════════════════════════════════
   * Draw the field
   * ═══════════════════════════════════════════════════════════════════ */
  for (int row = 0; row < FIELD_HEIGHT; row++) {
    for (int col = 0; col < FIELD_WIDTH; col++) {
      unsigned char cell_value = state->field[row * FIELD_WIDTH + col];

      switch (cell_value) {
      case TETRIS_FIELD_EMPTY:
        continue;
      case TETRIS_FIELD_I:
      case TETRIS_FIELD_J:
      case TETRIS_FIELD_L:
      case TETRIS_FIELD_O:
      case TETRIS_FIELD_S:
      case TETRIS_FIELD_T:
      case TETRIS_FIELD_Z:
        draw_cell(backbuffer, col, row, get_tetromino_color(cell_value - 1));
        break;
      case TETRIS_FIELD_WALL:
        draw_cell(backbuffer, col, row, COLOR_GRAY);
        break;
      case TETRIS_FIELD_TMP_FLASH:
        draw_cell(backbuffer, col, row, COLOR_WHITE);
        break;
      }
    }
  }

  /* ═══════════════════════════════════════════════════════════════════
   * Draw the current falling piece
   * ═══════════════════════════════════════════════════════════════════ */
  draw_piece(backbuffer, state->current_piece.index, state->current_piece.x,
             state->current_piece.y,
             get_tetromino_color(state->current_piece.index),
             state->current_piece.rotate_x_value);

  /* ═══════════════════════════════════════════════════════════════════
   * Draw the HUD sidebar
   * ═══════════════════════════════════════════════════════════════════ */
  int sx = FIELD_WIDTH * CELL_SIZE + 10;
  int sy = 10;
  char buf[32];

  /* Score */
  draw_text(backbuffer, sx, sy, "SCORE", COLOR_WHITE, 2);
  snprintf(buf, sizeof(buf), "%d", state->score);
  draw_text(backbuffer, sx, sy + 16, buf, COLOR_YELLOW, 2);

  /* Level */
  draw_text(backbuffer, sx, sy + 40, "LEVEL", COLOR_WHITE, 2);
  snprintf(buf, sizeof(buf), "%d", state->level);
  draw_text(backbuffer, sx, sy + 56, buf, COLOR_CYAN, 2);

  /* Pieces */
  draw_text(backbuffer, sx + 80, sy + 40, "PIECES", COLOR_WHITE, 2);
  snprintf(buf, sizeof(buf), "%d", state->pieces_count);
  draw_text(backbuffer, sx + 80, sy + 56, buf, COLOR_CYAN, 2);

  /* Next piece label */
  draw_text(backbuffer, sx, sy + 85, "NEXT", COLOR_WHITE, 2);

  /* Next piece preview */
  int preview_x = sx;
  int preview_y = sy + 105;
  int preview_cell_size = CELL_SIZE / 2;

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (TETROMINOES[state->current_piece.next_index]
                     [tetromino_pos_value(px, py, 0)] == TETROMINO_BLOCK) {
        uint32_t color = get_tetromino_color(state->current_piece.next_index);
        draw_rect(backbuffer, preview_x + px * preview_cell_size + 1,
                  preview_y + py * preview_cell_size + 1, preview_cell_size - 2,
                  preview_cell_size - 2, color);
      }
    }
  }

  /* Controls hint */
  int hint_y = FIELD_HEIGHT * CELL_SIZE - 90;
  draw_text(backbuffer, sx, hint_y, "CONTROLS", COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 18, "{} MOVE LEFT/RIGHT", COLOR_DARK_GRAY,
            1);
  draw_text(backbuffer, sx, hint_y + 28, "v  SOFT DROP", COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 38, "Z  ROTATE LEFT", COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 48, "X  ROTATE RIGHT", COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 58, "R  RESTART", COLOR_DARK_GRAY, 1);
  draw_text(backbuffer, sx, hint_y + 68, "Q  QUIT", COLOR_DARK_GRAY, 1);
  /* ═══════════════════════════════════════════════════════════════════
   * Game over overlay
   * ═══════════════════════════════════════════════════════════════════ */
  if (state->is_game_over) {
    int cx = FIELD_WIDTH * CELL_SIZE / 2;
    int cy = FIELD_HEIGHT * CELL_SIZE / 2;

    /* Semi-transparent overlay */
    draw_rect_blend(backbuffer, cx - 80, cy - 50, 160, 100,
                    GAME_RGBA(0, 0, 0, 200));

    /* Red border */
    draw_rect(backbuffer, cx - 80, cy - 50, 160, 3, COLOR_RED);
    draw_rect(backbuffer, cx - 80, cy + 47, 160, 3, COLOR_RED);
    draw_rect(backbuffer, cx - 80, cy - 50, 3, 100, COLOR_RED);
    draw_rect(backbuffer, cx + 77, cy - 50, 3, 100, COLOR_RED);

    /* Game over text */
    draw_text(backbuffer, cx - 54, cy - 30, "GAME OVER", COLOR_RED, 2);
    draw_text(backbuffer, cx - 60, cy, "R RESTART", COLOR_WHITE, 2);
    draw_text(backbuffer, cx - 45, cy + 20, "Q QUIT", COLOR_WHITE, 2);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Game Logic (unchanged from before)
 * ═══════════════════════════════════════════════════════════════════════════
 */

void game_init(GameState *state) {
  state->score = 0;
  state->is_game_over = false;
  state->pieces_count = 1;
  state->level = 0;

  state->completed_lines.count = 0;
  state->completed_lines.flash_timer.timer = 0;
  state->completed_lines.flash_timer.interval = 0.4f;
  memset(&state->completed_lines.indexes, 0,
         sizeof(int) * TETROMINO_LAYER_COUNT);

  // Time-based dropping
  state->input_values.rotate_direction.timer = 0.0f;
  state->input_values.rotate_direction.interval =
      0.8f; // 1 second between drops initially

  // DAS = Delayed Auto Shift (initial delay before repeating starts)
  // Using `initial_delay`
  // ARR = Auto Repeat Rate (speed of repeating after DAS)
  // Using `interval`

  /* Configure auto-repeat intervals */
  state->input_repeat.move_left.initial_delay = 0.01f;
  state->input_repeat.move_left.interval = 0.05f;

  state->input_repeat.move_right.initial_delay = 0.01f;
  state->input_repeat.move_right.interval = 0.05f;

  state->input_repeat.move_down.initial_delay = 0.0f;
  state->input_repeat.move_down.interval = 0.03f;

  /* Build the boundary walls. */
  for (int y = 0; y < FIELD_HEIGHT; y++) {
    for (int x = 0; x < FIELD_WIDTH; x++) {
      /* Left wall, right wall, or floor → value 9 */
      if (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) {
        state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_WALL;
      } else {
        /* Everything else stays 0 (empty) */
        state->field[y * FIELD_WIDTH + x] = TETRIS_FIELD_EMPTY;
      }
    }
  }

  /* Pick a random starting piece. */
  srand((unsigned int)time(NULL));
  state->current_piece = (CurrentPiece){
      /*  center-ish, start in the middle of the field */
      .x = (FIELD_WIDTH * 0.5) - (TETROMINO_LAYER_COUNT * 0.5),
      .y = 0,
      .index = rand() % TETROMINOS_COUNT,
      .next_index = rand() % TETROMINOS_COUNT,
      .rotate_x_value = TETROMINO_R_0,
  };
}

int tetromino_pos_value(int px, int py, TETROMINO_R_DIR r) {
  switch (r) {
  case TETROMINO_R_0:
    return py * TETROMINO_LAYER_COUNT + px;
  case TETROMINO_R_90:
    return 12 + py - (px * TETROMINO_LAYER_COUNT);
  case TETROMINO_R_180:
    return 15 - (py * TETROMINO_LAYER_COUNT) - px;
  case TETROMINO_R_270:
    return 3 - py + (px * TETROMINO_LAYER_COUNT);
  }

  // TODO: better to use `ASSERT`
  printf("Invalid rotation: %d\n", r);
  return 0; /* Should never happen */
}

int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                             int pos_x, int pos_y) {
  // TODO: use `ASSERT`
  if (piece < 0 || piece >= TETROMINOS_COUNT)
    return 0;

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      /* Which character in the piece string? */
      int pi = tetromino_pos_value(px, py, rotation);

      /* Skip empty cells in the piece */
      if (TETROMINOES[piece][pi] == TETROMINO_SPAN)
        continue;

      /* Where would this cell land in the field? */
      int field_x = pos_x + px;
      int field_y = pos_y + py;

      /* Skip if outside the field bounds
         (piece's 4×4 box can hang outside on the sides) */
      if (field_x < 0 || field_x >= FIELD_WIDTH)
        continue;
      if (field_y < 0 || field_y >= FIELD_HEIGHT)
        continue;

      /* Field index for this position */
      int fi = field_y * FIELD_WIDTH + field_x;

      /* If the field cell is not empty → doesn't fit */
      if (state->field[fi] != 0)
        return 0;
    }
  }
  return 1; /* all solid cells checked — it fits */
}

/* Reset transition counts — they're per-frame */
void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
  printf("Before\n");
  // DEBUG: Print what was copied
  printf("[PREPARE] current_input->current: left=%d right=%d down=%d\n",
         current_input->move_left.ended_down,
         current_input->move_right.ended_down,
         current_input->move_down.half_transition_count);
  printf("[PREPARE] old_input->current: left=%d right=%d down=%d\n",
         old_input->move_left.ended_down, old_input->move_right.ended_down,
         old_input->move_down.half_transition_count);
  // Copy button state, reset transitions
  for (int btn = 0; btn < BUTTON_COUNT; btn++) {
    current_input->buttons[btn].ended_down = old_input->buttons[btn].ended_down;
    current_input->buttons[btn].half_transition_count = 0;
  }
  printf("After\n");
  // DEBUG: Print what was copied
  printf("[PREPARE] current_input->current: left=%d right=%d down=%d\n",
         current_input->move_left.ended_down,
         current_input->move_right.ended_down,
         current_input->move_down.half_transition_count);
  printf("[PREPARE] old_input->current: left=%d right=%d down=%d\n",
         old_input->move_left.ended_down, old_input->move_right.ended_down,
         old_input->move_down.half_transition_count);
};

/**
 * Process an action with auto-repeat behavior.
 *
 * @param button     The button state to process
 * @param repeat     The repeat interval for this action
 * @param delta_time Seconds since last frame
 * @param should_trigger Output: set to 1 if action should fire this frame
 */
static void handle_action_with_repeat(GameButtonState *button,
                                      RepeatInterval *repeat, float delta_time,
                                      int *should_trigger) {
  *should_trigger = 0;

  // Button released - reset everything
  if (!button->ended_down) {
    repeat->timer = 0.0f;
    repeat->is_active = false;
    return;
  }

  // Just pressed this frame
  if (button->half_transition_count > 0) {
    repeat->timer = 0.0f;
    repeat->is_active = true;

    // Trigger immediately ONLY if no initial delay
    if (repeat->initial_delay <= 0.0f) {
      *should_trigger = 1;
    }
    return;
  }

  // Button held from previous frame
  if (!repeat->is_active) {
    return;
  }

  repeat->timer += delta_time;

  // Has initial_delay?
  if (repeat->initial_delay > 0.0f) {
    // Crossed the initial_delay threshold?
    if (repeat->timer >= repeat->initial_delay) {
      *should_trigger = 1;
      repeat->timer = 0.0f;         // Reset for interval phase
      repeat->initial_delay = 0.0f; // Switch to interval mode
      // Actually, don't mutate initial_delay! Use a flag instead.
    }
  } else {
    // No initial_delay - just use interval
    if (repeat->timer >= repeat->interval) {
      *should_trigger = 1;
      repeat->timer -= repeat->interval; // Keep remainder
    }
  }
}

/* Calculate pan position based on piece X position */
/* Returns -1.0 (left) to 1.0 (right) */
static float calculate_piece_pan(int piece_x) {
  /* Field is 12 wide (including walls)
   * Playable area: columns 1-10 (walls at 0 and 11)
   * Center column: ~5.5
   *
   * piece_x = 1 (far left)  → pan = -0.8
   * piece_x = 5 (center)    → pan = 0.0
   * piece_x = 10 (far right) → pan = 0.8
   */
  float center = (FIELD_WIDTH - 2) / 2.0f + 1.0f; /* ~6.0 */
  float max_offset = (FIELD_WIDTH - 2) / 2.0f;    /* ~5.0 */

  float offset = (float)piece_x - center;
  float pan = (offset / max_offset) * 0.8f; /* Scale to ±0.8, not full ±1.0 */

  /* Clamp to valid range */
  if (pan < -1.0f)
    pan = -1.0f;
  if (pan > 1.0f)
    pan = 1.0f;

  return pan;
}

void tetris_apply_input(GameState *state, GameInput *input, float delta_time) {
  /* Calculate current pan position based on piece location */
  float pan = calculate_piece_pan(state->current_piece.x);

  /* Rotate clockwise: try rotation + 1. (% 4 wraps 3 back to 0.) */
  if (input->rotate_x.ended_down &&
      input->rotate_x.half_transition_count != 0 &&
      state->current_piece.next_rotate_x_value != TETROMINO_ROTATE_X_NONE) {

    TETROMINO_R_DIR new_rotation;
    if (state->current_piece.next_rotate_x_value ==
        TETROMINO_ROTATE_X_GO_RIGHT) {
      switch (state->current_piece.rotate_x_value) {
      case TETROMINO_R_0:
        new_rotation = TETROMINO_R_90;
        break;
      case TETROMINO_R_90:
        new_rotation = TETROMINO_R_180;
        break;
      case TETROMINO_R_180:
        new_rotation = TETROMINO_R_270;
        break;
      case TETROMINO_R_270:
        new_rotation = TETROMINO_R_0;
        break;
      }
    } else {
      switch (state->current_piece.rotate_x_value) {
      case TETROMINO_R_0:
        new_rotation = TETROMINO_R_270;
        break;
      case TETROMINO_R_90:
        new_rotation = TETROMINO_R_0;
        break;
      case TETROMINO_R_180:
        new_rotation = TETROMINO_R_90;
        break;
      case TETROMINO_R_270:
        new_rotation = TETROMINO_R_180;
        break;
      }
    }
    int does_piece_fit = tetromino_does_piece_fit(
        state, state->current_piece.index, new_rotation, state->current_piece.x,
        state->current_piece.y);

    if (does_piece_fit) {
      state->current_piece.rotate_x_value = new_rotation;
      game_play_sound_at(&state->audio, SOUND_ROTATE, pan);
    }

    // input->rotate_x.value = 0;
  }

  /* ── Horizontal movement: independent auto-repeat ── */
  int should_move_left = 0;
  int should_move_right = 0;

  handle_action_with_repeat(&input->move_left, &state->input_repeat.move_left,
                            delta_time, &should_move_left);
  handle_action_with_repeat(&input->move_right, &state->input_repeat.move_right,
                            delta_time, &should_move_right);

  /* Move left: try current_piece.col - 1 */
  if (should_move_left &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x - 1,
                               state->current_piece.y)) {
    state->current_piece.x--;
    game_play_sound_at(&state->audio, SOUND_MOVE, pan);
  }

  /* Move right: try current_piece.col + 1 */
  if (should_move_right &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x + 1,
                               state->current_piece.y)) {
    state->current_piece.x++;
    game_play_sound_at(&state->audio, SOUND_MOVE, pan);
  }

  /* ── Soft drop: independent auto-repeat (faster interval) ── */
  int should_move_down = 0;

  handle_action_with_repeat(&input->move_down, &state->input_repeat.move_down,
                            delta_time, &should_move_down);
  /* Soft drop: try current_piece.row + 1 (down = positive Y) */
  if (should_move_down &&
      tetromino_does_piece_fit(state, state->current_piece.index,
                               state->current_piece.rotate_x_value,
                               state->current_piece.x,
                               state->current_piece.y + 1)) {
    state->current_piece.y++;
    game_play_sound_at(&state->audio, SOUND_MOVE, pan);
  }
}

void game_update(GameState *state, GameInput *input, float delta_time) {
  /* Update music sequencer with real delta_time (consistent across platforms)
   */
  game_audio_update(&state->audio, delta_time);

  if (state->is_game_over) {
    if (state->should_restart) {
      game_init(state);
      if (state->audio.samples_per_second > 0) {
        game_music_play(&state->audio);
        game_play_sound(&state->audio, SOUND_RESTART);
      }
    }
    return;
  }

  if (state->completed_lines.flash_timer.timer > 0) {
    state->completed_lines.flash_timer.timer -= delta_time;

    if (state->completed_lines.flash_timer.timer <= 0) {
      /* timer hit zero: collapse all completed rows now */

      /* IMPORTANT: Process lines from bottom to top (highest row index first).
       * Why? When we collapse a row, everything ABOVE it shifts down by 1.
       * If we processed top-to-bottom, the stored indices would become stale.
       *
       * Example: lines[0]=14, lines[1]=15 (two adjacent complete rows)
       * If we collapse row 14 first, row 15 shifts to row 14.
       * But lines[1] still says 15 — wrong!
       *
       * By processing bottom-to-top (row 15 first), we avoid this problem.
       * After collapsing row 15, row 14 is still at row 14.
       */
      for (int i = state->completed_lines.count - 1; i >= 0; i--) {
        int line_index = state->completed_lines.indexes[i];

        /* Collapse this row: copy each row above it down by one.
         * Start at the completed row, work upward to row 1.
         * We skip row 0 in the copy (nothing above it to copy from).
         */
        for (int py = line_index; py > 0; --py) {
          for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
            state->field[py * FIELD_WIDTH + px] =
                state->field[(py - 1) * FIELD_WIDTH + px];
          }
        }

        /* Clear the top row (row 0) — it has no row above to copy from */
        for (int px = 1; px < FIELD_WIDTH - 1; px++) {
          state->field[px] = TETRIS_FIELD_EMPTY;
        }

        /* Adjust remaining line indices.
         * All lines we haven't processed yet (indices 0 to i-1) are
         * ABOVE the line we just collapsed. They've all shifted down
         * by 1 row, so increment their stored indices.
         */
        for (int j = i - 1; j >= 0; --j) {
          if (state->completed_lines.indexes[j] < line_index) {
            state->completed_lines.indexes[j]++;
          }
        }
      }

      if (state->completed_lines.count == 4) {
        game_play_sound(&state->audio, SOUND_TETRIS);
      } else if (state->completed_lines.count > 0) {
        game_play_sound(&state->audio, SOUND_LINE_CLEAR);
      }

      state->completed_lines.count = 0;
    }
    return; /* freeze all game logic while flashing */
  }

  /* ── Handle player input (always responsive) ── */
  tetris_apply_input(state, input, delta_time);

  /* ── Accumulate time for gravity ── */
  state->input_values.rotate_direction.timer += delta_time;

  int game_speed = state->level - 1 < 0 ? 0 : state->level;
  float tetromino_drop_interval =
      state->input_values.rotate_direction.interval +
      (0.01f + game_speed * 0.01f);
  if (tetromino_drop_interval < 0.1f) {
    tetromino_drop_interval = 0.1f; /* cap at 100ms per drop */
  }

  /* ── Check if it's time to drop the piece ── */
  if (state->input_values.rotate_direction.timer >= tetromino_drop_interval) {
    // Reset timer, keeping remainder for precision
    // float drop_interval = 0.1f + state->speed * 0.01f;
    // if (drop_interval > 0.8f) {
    //   drop_interval = 0.8f;
    // }
    state->input_values.rotate_direction.timer -= tetromino_drop_interval;

    /* Try to move the piece down one row */
    if (tetromino_does_piece_fit(state, state->current_piece.index,
                                 state->current_piece.rotate_x_value,
                                 state->current_piece.x,
                                 state->current_piece.y + 1)) {
      state->current_piece.y++;
    } else {
      /* If it doesn't fit, piece has landed - locking in lesson 09 */
      for (int px = 0; px < TETROMINO_LAYER_COUNT; ++px) {
        for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
          int pi =
              tetromino_pos_value(px, py, state->current_piece.rotate_x_value);
          if (TETROMINOES[state->current_piece.index][pi] != TETROMINO_SPAN) {
            int fx = state->current_piece.x + px;
            int fy = state->current_piece.y + py;
            if (fx >= 0 && fx < FIELD_WIDTH && fy >= 0 && fy < FIELD_HEIGHT) {
              state->field[fy * FIELD_WIDTH + fx] =
                  state->current_piece.index +
                  1; // Store piece index + 1 (0 is empty) for rendering
            }
          }
        }
      }
      float pan = calculate_piece_pan(state->current_piece.x);
      game_play_sound_at(&state->audio, SOUND_DROP, pan);

      state->completed_lines.count = 0;

      for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
        int row_y = state->current_piece.y + py;

        /* Skip if outside playable area (above top or at/below floor) */
        if (row_y < 0 || row_y >= FIELD_HEIGHT - 1) {
          continue;
        }

        bool completed = 1;
        for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
          /* Tag this row: value `TETRIS_FIELD_EMPTY` = white in the renderer */
          if (state->field[(row_y)*FIELD_WIDTH + px] == TETRIS_FIELD_EMPTY) {
            completed = false;
            break;
          }
        }

        if (completed) {
          for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
            state->field[(row_y)*FIELD_WIDTH + px] = TETRIS_FIELD_TMP_FLASH;
          }

          state->completed_lines.indexes[state->completed_lines.count++] =
              row_y;
        }
      }

      if (state->completed_lines.count > 0) {
        state->completed_lines.flash_timer.timer =
            state->completed_lines.flash_timer.interval;
      }

      /* Score and difficulty */
      state->pieces_count++;
      state->score += 25; /* 25 points for every piece locked */
      if (state->completed_lines.count > 0) {
        state->score += (1 << state->completed_lines.count) * 100;
      }

      if (state->pieces_count % 25 == 0) {
        state->level++; // Increment for display purposes
        game_play_sound(&state->audio, SOUND_LEVEL_UP);
      }

      // reset drop-timer and current piece for next round
      state->input_values.rotate_direction.timer = 0.0f;
      state->current_piece = (CurrentPiece){
          .x = (FIELD_WIDTH * 0.5) - (TETROMINO_LAYER_COUNT * 0.5),
          .y = 0,
          .index = state->current_piece.next_index,
          .next_index = rand() % TETROMINOS_COUNT,
          .rotate_x_value = TETROMINO_R_0,
      };

      if (!tetromino_does_piece_fit(state, state->current_piece.index,
                                    state->current_piece.rotate_x_value,
                                    state->current_piece.x,
                                    state->current_piece.y)) {
        state->is_game_over = true;
        game_play_sound(&state->audio, SOUND_GAME_OVER);
        game_music_stop(&state->audio); /* Stop music on game over */
      }
    }
    //
  }
}

// 1. Precision
// ```c
// // Old: Loses precision
// state->tetromino_drop.timer -= state->tetromino_drop.interval;  // Keeps
// remainder!

// // vs tick-based
// state->speed_count = 0; // Loses any "extra" time
// ```

// 2. Easy Difficulty Scaling
// ```c
// // Make game faster as player progresses
// state->tetromino_drop.interval = 1.0f / (1.0f + state->piece_count * 0.05f);
// // piece 0:  1.0 second
// // piece 10: 0.67 seconds
// // piece 20: 0.5 seconds
// ```

// 3. Smooth Slow-Motion
// ```c
// // Just multiply delta_time
// float slow_mo_factor = 0.5f;  // Half speed
// game_update(&game_state, &input, delta_time * slow_mo_factor);
// ```

// 4. Pause is Trivial
// ```c
// if (!paused) {
//   game_update(&game_state, &input, delta_time);
// }
// // When paused, just don't call update - timer doesn't advance
// ```