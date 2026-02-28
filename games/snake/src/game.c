#include "game.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static void draw_rect(Backbuffer *backbuffer, int x, int y, int w, int h,
                      uint32_t color) {
  /* Clip to backbuffer bounds */
  int x0 = MAX(x, 0);
  int y0 = MAX(y, 0);
  int x1 = MIN(x + w, backbuffer->width);
  int y1 = MIN(y + h, backbuffer->height);
  int px, py;
  for (py = y0; py < y1; ++py) {
    for (px = x0; px < x1; ++px) {
      backbuffer->pixels[py * backbuffer->width + px] = color;
    }
  }
}

static void draw_cell(Backbuffer *backbuffer, int x, int y, uint32_t color) {
  int py = (y * CELL_SIZE) + (CELL_SIZE * HEADER_ROWS) + 1;
  int px = (x * CELL_SIZE) + 1;
  int size = CELL_SIZE - 2;
  draw_rect(backbuffer, px, py, size, size, color);
}

static void draw_text(const char *text, int x, int y, int size,
                      uint32_t color) {
  // TODO: Implement me!
  (void)text;
  (void)x;
  (void)y;
  (void)size;
  (void)color;
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
  game_state->snake = (Snake){
      .head = 9,                    /* index of the head segment */
      .tail = 0,                    /* index of the tail segment */
      .length = 10,                 /* number of active segments */
      .direction = SNAKE_DIR_RIGHT, /* start moving RIGHT */
      .speed = 8,                   // TODO: is it needed?!!
      .move_forward = {.timer = 0.0f, .interval = 0.15f, /* 150ms per step */},
  };

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

  /* Score label */
  draw_text("SCORE: 0", 8, CELL_SIZE + 2, 12, COLOR_WHITE);
  draw_text("SPEED: 0", SCREEN_INITIAL_WIDTH - 80, CELL_SIZE + 2, 12,
            COLOR_WHITE);

  Snake *snake = &game_state->snake;

  for (int i = snake->tail; i != snake->head; i = (i + 1) % MAX_SNAKE) {
    draw_cell(backbuffer, snake->segments[i].x, snake->segments[i].y,
              COLOR_LIME_GREEN);
  }

  draw_cell(backbuffer, game_state->snake.segments[snake->head].x,
            game_state->snake.segments[snake->head].y, COLOR_DARK_GREEN);
}

void game_update(GameState *game_state, GameInput *input, float delta_time) {
  (void)input;

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
  game_state->snake.move_forward.timer += delta_time;
  if (game_state->snake.move_forward.timer >=
      game_state->snake.move_forward.interval) {
    game_state->snake.move_forward.timer -=
        game_state->snake.move_forward.interval;

    snake->direction = snake->next_direction;

    // get dir x and y
    int dx = DIRECTION_SNAKE_X[snake->direction];
    int dy = DIRECTION_SNAKE_Y[snake->direction];
    // get new x and y in respect to head x + dir x and head y + dir y
    int new_x = snake->segments[snake->head].x + dx;
    int new_y = snake->segments[snake->head].y + dy;
    // update head _(note to wrap)_ index by 1
    snake->head = (snake->head + 1) % MAX_SNAKE;
    // update head value by dirs
    snake->segments[snake->head].x = new_x;
    snake->segments[snake->head].y = new_y;
    // tail _(note to wrap)_ index by 1
    snake->tail = (snake->tail + 1) % MAX_SNAKE;
  }
}