#include "main.h"
#include "../utils/backbuffer.h"
#include "../utils/draw-shapes.h"
#include "../utils/draw-text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void draw_cell(Backbuffer *backbuffer, int x, int y, uint32_t color) {
  int py = (y * CELL_SIZE) + (CELL_SIZE * HEADER_ROWS) + 1;
  int px = (x * CELL_SIZE) + 1;
  int size = CELL_SIZE - 2;
  draw_rect(backbuffer, px, py, size, size, color);
}

/* Reset transition counts — they're per-frame */
void prepare_input_frame(GameInput *input) {
  input->turn_left.half_transition_count = 0;
  input->turn_right.half_transition_count = 0;
  input->restart = 0;
}

// /* Calculate pan position based on X coordinate */
// static float calculate_pan(int x) {
//   float center = GRID_WIDTH / 2.0f;
//   float offset = (float)x - center;
//   float pan = (offset / center) * 0.8f;
//   if (pan < -1.0f)
//     pan = -1.0f;
//   if (pan > 1.0f)
//     pan = 1.0f;
//   return pan;
// }

// /* Spawn food at random location not occupied by snake */
// static void spawn_food(GameState *state) {
//   Snake *snake = &state->snake;
//   int ok;
//   do {
//     state->food_x = 1 + rand() % (GRID_WIDTH - 2);
//     state->food_y = rand() % (GRID_HEIGHT - 1);

//     ok = 1;
//     int idx = snake->tail;
//     int rem = snake->length;
//     while (rem > 0) {
//       if (snake->segments[idx].x == state->food_x &&
//           snake->segments[idx].y == state->food_y) {
//         ok = 0;
//         break;
//       }
//       idx = (idx + 1) % MAX_SNAKE;
//       rem--;
//     }
//   } while (!ok);
// }

void game_init(GameState *game_state,
               AudioOutputBuffer *game_audio_output_buffer) {
  int saved_best = game_state->best_score;
  int saved_sps = game_state->audio.samples_per_second;

  memset(game_state, 0, sizeof(GameState));

  game_state->best_score = saved_best;
  game_state->audio.samples_per_second = saved_sps;

  SNAKE_DIR current_dir = SNAKE_DIR_RIGHT;
  game_state->snake = (Snake){
      .head = 9,
      .tail = 0,
      .length = 10,
      .direction = current_dir,
      .next_direction = current_dir,
      .speed = 8,
      .move_repeat = {.timer = 0.0f, .interval = 0.15f},
  };

  game_state->is_game_over = false;
  game_state->score = 0;
  // game_state->grow_pending = 0;

  /* Initialize snake segments */
  for (int i = 0; i < game_state->snake.length; ++i) {
    game_state->snake.segments[i].x = GRID_WIDTH / 2 - 5 + i;
    game_state->snake.segments[i].y = GRID_HEIGHT / 2;
  }

  /* Seed random and spawn food */
  srand((unsigned)time(NULL));
  // spawn_food(game_state);

  /* Initialize audio */
  if (game_audio_output_buffer->is_initialized) {
    game_state->audio.samples_per_second =
        game_audio_output_buffer->samples_per_second;
    game_audio_init(&game_state->audio);
    game_music_play(&game_state->audio);
  }
}

void game_render(GameState *game_state, Backbuffer *backbuffer) {
  /* Clear to black */
  draw_rect(backbuffer, 0, 0, backbuffer->width, backbuffer->height,
            COLOR_BLACK);

  /* Header background */
  draw_rect(backbuffer, 0, 0, backbuffer->width, HEADER_ROWS * CELL_SIZE,
            COLOR_DARK_GRAY);
  /* Header separator line */
  draw_rect(backbuffer, 0, (HEADER_ROWS - 1) * CELL_SIZE, backbuffer->width, 2,
            COLOR_LIME_GREEN);

  char buf[64];
  {
    int title_x = (backbuffer->width - 5 * 6 * 2) / 2;
    int text_y = (HEADER_ROWS * CELL_SIZE - 14) / 2;
    draw_text(backbuffer, title_x, text_y, "SNAKE", COLOR_LIME_GREEN, 2);

    snprintf(buf, sizeof(buf), "SCORE:%d", game_state->score);
    draw_text(backbuffer, 8, text_y, buf, COLOR_WHITE, 2);

    snprintf(buf, sizeof(buf), "BEST:%d", game_state->best_score);
    draw_text(backbuffer, backbuffer->width - (int)(strlen(buf)) * 12 - 8,
              text_y, buf, COLOR_YELLOW, 2);
  }

  // /* Draw food */
  // draw_cell(backbuffer, game_state->food_x, game_state->food_y, COLOR_RED);

  /* Draw snake body */
  Snake *snake = &game_state->snake;
  uint32_t body_color =
      game_state->is_game_over ? COLOR_DARK_RED : COLOR_LIME_GREEN;
  uint32_t head_color = game_state->is_game_over ? COLOR_DARK_RED : COLOR_WHITE;

  int idx = snake->tail;
  int rem = snake->length - 1;
  while (rem > 0) {
    draw_cell(backbuffer, snake->segments[idx].x, snake->segments[idx].y,
              body_color);
    idx = (idx + 1) % MAX_SNAKE;
    rem--;
  }

  /* Draw head last */
  draw_cell(backbuffer, snake->segments[snake->head].x,
            snake->segments[snake->head].y, head_color);

  /* Game over overlay */
  if (game_state->is_game_over) {
    int field_y = HEADER_ROWS * CELL_SIZE;
    int field_w = GRID_WIDTH * CELL_SIZE;
    int field_h = GRID_HEIGHT * CELL_SIZE;
    int cx = field_w / 2;
    int cy = field_y + field_h / 2;

    draw_rect_blend(backbuffer, 0, field_y, field_w, field_h,
                    GAME_RGBA(0, 0, 0, 180));

    draw_rect(backbuffer, 0, field_y, field_w, 4, COLOR_RED);
    draw_rect(backbuffer, 0, field_y + field_h - 4, field_w, 4, COLOR_RED);
    draw_rect(backbuffer, 0, field_y, 4, field_h, COLOR_RED);
    draw_rect(backbuffer, field_w - 4, field_y, 4, field_h, COLOR_RED);

    {
      int scale = 3;
      int str_w = 9 * 6 * scale;
      draw_text(backbuffer, cx - str_w / 2, cy - 36, "GAME OVER", COLOR_RED,
                scale);
    }

    snprintf(buf, sizeof(buf), "SCORE %d", game_state->score);
    {
      int sw = (int)(strlen(buf)) * 12;
      draw_text(backbuffer, cx - sw / 2, cy + 6, buf, COLOR_WHITE, 2);
    }
    {
      int hw = 9 * 12;
      draw_text(backbuffer, cx - hw / 2, cy + 28, "R RESTART", COLOR_WHITE, 2);
    }
    {
      int qw = 6 * 12;
      draw_text(backbuffer, cx - qw / 2, cy + 50, "Q QUIT", COLOR_GRAY, 2);
    }
  }
}

void game_update(GameState *game_state, GameInput *input,
                 AudioOutputBuffer *game_audio_output_buffer,
                 float delta_time) {
  if (game_state->is_game_over) {
    if (input->restart) {
      game_init(game_state, game_audio_output_buffer);
      game_play_sound(&game_state->audio, SOUND_RESTART);
    }
    return;
  }

  Snake *snake = &game_state->snake;

  /* Direction input: turn fires on just-pressed */
  if (input->turn_left.ended_down &&
      input->turn_left.half_transition_count > 0) {
    SNAKE_DIR turned =
        (snake->direction + (DIRECTION_SNAKE_SIZE - 1)) % DIRECTION_SNAKE_SIZE;
    /* Prevent 180-degree turn */
    if (turned != (snake->direction + 2) % DIRECTION_SNAKE_SIZE) {
      snake->next_direction = turned;
    }
  }
  if (input->turn_right.ended_down &&
      input->turn_right.half_transition_count > 0) {
    SNAKE_DIR turned = (snake->direction + 1) % DIRECTION_SNAKE_SIZE;
    if (turned != (snake->direction + 2) % DIRECTION_SNAKE_SIZE) {
      snake->next_direction = turned;
    }
  }

  /* Accumulate time for movement */
  snake->move_repeat.timer += delta_time;
  if (snake->move_repeat.timer < snake->move_repeat.interval) {
    return;
  }
  snake->move_repeat.timer -= snake->move_repeat.interval;

  /* Commit direction */
  snake->direction = snake->next_direction;

  /* Calculate new head position */
  int dx = DIRECTION_SNAKE_X[snake->direction];
  int dy = DIRECTION_SNAKE_Y[snake->direction];
  int new_x = snake->segments[snake->head].x + dx;
  int new_y = snake->segments[snake->head].y + dy;

  /* Wall collision */
  if (new_x < 1 || new_x >= GRID_WIDTH - 1 || new_y < 0 ||
      new_y >= GRID_HEIGHT - 1) {
    if (game_state->score > game_state->best_score) {
      game_state->best_score = game_state->score;
    }
    game_state->is_game_over = true;
    game_music_stop(&game_state->audio);
    game_play_sound(&game_state->audio, SOUND_GAME_OVER);
    return;
  }

  /* Self collision */
  int idx = snake->tail;
  int rem = snake->length;
  while (rem > 0) {
    if (snake->segments[idx].x == new_x && snake->segments[idx].y == new_y) {
      if (game_state->score > game_state->best_score) {
        game_state->best_score = game_state->score;
      }
      game_state->is_game_over = true;
      game_music_stop(&game_state->audio);
      game_play_sound(&game_state->audio, SOUND_GAME_OVER);
      return;
    }
    idx = (idx + 1) % MAX_SNAKE;
    rem--;
  }

  /* Advance head */
  snake->head = (snake->head + 1) % MAX_SNAKE;
  snake->segments[snake->head].x = new_x;
  snake->segments[snake->head].y = new_y;
  snake->length++;

  // /* Food collision */
  // if (new_x == game_state->food_x && new_y == game_state->food_y) {
  //   game_state->score++;
  //   game_state->grow_pending += 5;

  //   /* Speed up every 3 points */
  //   if (game_state->score % 3 == 0 && snake->move_repeat.interval > 0.05f) {
  //     snake->move_repeat.interval -= 0.01f;
  //   }

  //   /* Spatial audio: pan based on food position */
  //   float pan = calculate_pan(game_state->food_x);
  //   game_play_sound_at(&game_state->audio, SOUND_FOOD_EATEN, pan);

  //   spawn_food(game_state);
  // }

  // /* Tail advance (unless growing) */
  // if (game_state->grow_pending > 0) {
  //   game_state->grow_pending--;
  //   game_play_sound(&game_state->audio, SOUND_GROW);
  // } else {
  snake->tail = (snake->tail + 1) % MAX_SNAKE;
  snake->length--;
  // }
}
