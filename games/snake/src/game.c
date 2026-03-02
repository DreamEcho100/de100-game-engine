#include "game.h"
#include "utils/backbuffer.h"
#include "utils/draw-shapes.h"
#include "utils/draw-text.h"
#include <stdio.h>
#include <string.h>

static void draw_cell(Backbuffer *backbuffer, int x, int y, uint32_t color) {
  int py = (y * CELL_SIZE) + (CELL_SIZE * HEADER_ROWS) + 1;
  int px = (x * CELL_SIZE) + 1;
  int size = CELL_SIZE - 2;
  draw_rect(backbuffer, px, py, size, size, color);
}

/* Reset transition counts — they're per-frame */
void prepare_input_frame(GameInput *input) {
  /* Reset per-frame counters — NOT ended_down (that persists).
   * NOT move_timer or any GameState field (those are in snake.c). */
  input->turn_left.half_transition_count = 0;
  input->turn_right.half_transition_count = 0;
  input->restart = 0;
};

void game_init(GameState *game_state) {
  SNAKE_DIR current_dir = SNAKE_DIR_RIGHT;
  game_state->snake = (Snake){
      .head = 9,                /* index of the head segment */
      .tail = 0,                /* index of the tail segment */
      .length = 10,             /* number of active segments */
      .direction = current_dir, /* start moving RIGHT */
      .next_direction =
          current_dir, /* next_direction starts the same as direction */
      .speed = 8,      // TODO: is it needed?!!
      .move_repeat = {.timer = 0.0f, .interval = 0.15f, /* 150ms per step */},
  };

  game_state->is_game_over = false;

  /* Initialize 10 segments in a horizontal line at the center */
  int i;
  for (i = 0; i < game_state->snake.length; ++i) {
    game_state->snake.segments[i].x =
        GRID_WIDTH * 0.5 - 5 + i;                        /* columns 25..34 */
    game_state->snake.segments[i].y = GRID_HEIGHT * 0.5; /* row 10         */
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
    int title_x =
        (backbuffer->width - 5 * 6 * 2) / 2; /* center "SNAKE" at scale 2 */
    int text_y = (HEADER_ROWS * CELL_SIZE - 14) / 2; /* vertically centered */
    draw_text(backbuffer, title_x, text_y, "SNAKE", COLOR_LIME_GREEN, 2);

    snprintf(buf, sizeof(buf), "SCORE:%d", game_state->score);
    draw_text(backbuffer, 8, text_y, buf, COLOR_WHITE, 2);

    snprintf(buf, sizeof(buf), "BEST:%d", game_state->best_score);
    draw_text(backbuffer, backbuffer->width - (int)(strlen(buf)) * 12 - 8,
              text_y, buf, COLOR_YELLOW, 2);
  }

  Snake *snake = &game_state->snake;

  for (int i = snake->tail; i != snake->head; i = (i + 1) % MAX_SNAKE) {
    draw_cell(backbuffer, snake->segments[i].x, snake->segments[i].y,
              COLOR_LIME_GREEN);
  }

  draw_cell(backbuffer, game_state->snake.segments[snake->head].x,
            game_state->snake.segments[snake->head].y, COLOR_DARK_GREEN);

  if (game_state->is_game_over) {
    int field_y = HEADER_ROWS * CELL_SIZE;
    int field_w = GRID_WIDTH * CELL_SIZE;
    int field_h = GRID_HEIGHT * CELL_SIZE;
    int cx = field_w / 2;
    int cy = field_y + field_h / 2;

    /* Semi-transparent black overlay over the play field */
    draw_rect_blend(backbuffer, 0, field_y, field_w, field_h,
                    GAME_RGBA(0, 0, 0, 180));

    /* Red border: 4 thin rectangles */
    draw_rect(backbuffer, 0, field_y, field_w, 4, COLOR_RED);
    draw_rect(backbuffer, 0, field_y + field_h - 4, field_w, 4, COLOR_RED);
    draw_rect(backbuffer, 0, field_y, 4, field_h, COLOR_RED);
    draw_rect(backbuffer, field_w - 4, field_y, 4, field_h, COLOR_RED);

    /* "GAME OVER" centered at scale 3 */
    {
      int scale = 3;
      int str_w = 9 * 6 * scale; /* strlen("GAME OVER") * char_width */
      draw_text(backbuffer, cx - str_w / 2, cy - 36, "GAME OVER", COLOR_RED,
                scale);
    }

    /* Score and restart hint */
    snprintf(buf, sizeof(buf), "SCORE %d", game_state->score);
    {
      int sw = (int)(strlen(buf)) * 12;
      draw_text(backbuffer, cx - sw / 2, cy + 6, buf, COLOR_WHITE, 2);
    }
    {
      int hw = 9 * 12; /* "R RESTART" */
      draw_text(backbuffer, cx - hw / 2, cy + 28, "R RESTART", COLOR_WHITE, 2);
    }
    {
      int qw = 6 * 12; /* "Q QUIT" */
      draw_text(backbuffer, cx - qw / 2, cy + 50, "Q QUIT", COLOR_GRAY, 2);
    }
  }
}

void game_update(GameState *game_state, GameInput *input, float delta_time) {
  if (game_state->is_game_over) {
    if (input->restart)
      game_init(game_state);
    return;
  }

  Snake *snake = &game_state->snake;

  SNAKE_DIR turned;
  /* Direction: turn fires on just-pressed (not held).
   * half_transition_count > 0 means the key was pressed this frame. */
  if (input->turn_left.ended_down &&
      input->turn_left.half_transition_count > 0) {
    // Going left (CCW) means subtracting 1 from direction, but we need to wrap
    // to 3 if we go below 0, so we can add DIRECTION_SNAKE_SIZE - 1 (which is
    // 3) instead of -1, and then mod by DIRECTION_SNAKE_SIZE (which is 4).
    turned =
        (snake->direction + (DIRECTION_SNAKE_SIZE - 1)) % DIRECTION_SNAKE_SIZE;
    snake->next_direction = turned;
  }
  if (input->turn_right.ended_down &&
      input->turn_right.half_transition_count > 0) {
    // Going right (CW) means adding 1 to direction, but we need to wrap to 0 if
    // we go above 3, so we can add 1 instead of 5, and then mod by
    // DIRECTION_SNAKE_SIZE (which is 4).
    turned = (snake->direction + 1) % DIRECTION_SNAKE_SIZE;
    snake->next_direction = turned;
  }

  /* Accumulate time — only move when interval is reached.
   * Subtracting (not zeroing) preserves sub-frame overshoot
   * so movement stays accurate at any frame rate. */
  game_state->snake.move_repeat.timer += delta_time;
  if (game_state->snake.move_repeat.timer >=
      game_state->snake.move_repeat.interval) {
    game_state->snake.move_repeat.timer -=
        game_state->snake.move_repeat.interval;

    snake->direction = snake->next_direction;

    // get dir x and y
    int dx = DIRECTION_SNAKE_X[snake->direction];
    int dy = DIRECTION_SNAKE_Y[snake->direction];
    // get new x and y in respect to head x + dir x and head y + dir y
    int new_x = snake->segments[snake->head].x + dx;
    int new_y = snake->segments[snake->head].y + dy;

    if (new_x < 1 || new_x > GRID_WIDTH - 1 || new_y < 0 ||
        new_y > GRID_HEIGHT - 1) {
      game_state->is_game_over = true;
    }

    int tail_idx = snake->tail;
    int remaining = snake->length;
    while (remaining > 0) {
      if (snake->segments[tail_idx].x == new_x &&
          snake->segments[tail_idx].y == new_y) {
        game_state->is_game_over = true;
      }
      tail_idx = (tail_idx + 1) % MAX_SNAKE;
      --remaining;
    }

    // update head _(note to wrap)_ index by 1
    snake->head = (snake->head + 1) % MAX_SNAKE;
    // update head value by dirs
    snake->segments[snake->head].x = new_x;
    snake->segments[snake->head].y = new_y;
    // tail _(note to wrap)_ index by 1
    snake->tail = (snake->tail + 1) % MAX_SNAKE;
  }
}

/*
Add a 2D occupancy array for O(1) self-collision:

1. Declare at the top of `main()`:
   ```c
   int occupied[GRID_HEIGHT][GRID_WIDTH];
   memset(occupied, 0, sizeof(occupied));
   ```

2. When advancing the head (adding a new segment at `new_x`, `new_y`):
   ```c
   occupied[new_y][new_x] = 1;
   ```

3. When advancing the tail (removing the rearmost segment):
   ```c
   occupied[segments[tail].y][segments[tail].x] = 0;
   tail = (tail + 1) % MAX_SNAKE;
   ```

4. Replace the self-collision loop with:
   ```c
   if (occupied[new_y][new_x]) {
       game_over = 1;
   }
   ```

5. Also initialize `occupied` to match the starting snake position (mark each
initial segment as `1`).

Notice the index order: `occupied[y][x]`, not `occupied[x][y]`. Rows first,
columns second — the same way 2D arrays work in memory in C (row-major order).
*/