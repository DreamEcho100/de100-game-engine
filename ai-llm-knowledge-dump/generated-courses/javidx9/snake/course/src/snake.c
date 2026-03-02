/* ============================================================
 * snake.c — Platform-independent game logic + rendering
 *
 * No X11. No Raylib. No platform headers.
 * Renders to a SnakeBackbuffer (CPU pixel array).
 * The platform layer uploads that array to the GPU.
 *
 * Port of OneLoneCoder Snake (javidx9) from C++/Windows to C/Linux.
 * ============================================================ */

#include "snake.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ─── Direction deltas ──────────────────────────────────────── */

/* Indexed by SNAKE_DIR: UP=0, RIGHT=1, DOWN=2, LEFT=3 */
static const int DX[4] = {0, 1, 0, -1};
static const int DY[4] = {-1, 0, 1, 0};

/* ═══════════════════════════════════════════════════════════════
 * Bitmap Font Data (5×7 pixels per character)
 * ═══════════════════════════════════════════════════════════════
 *
 * Each row is 5 bits wide (bits 4–0), packed into one byte.
 * Bit 4 = leftmost pixel, Bit 0 = rightmost pixel.
 *
 * Example — Letter 'A':
 *   0x0E = 01110 →  .###.
 *   0x11 = 10001 →  #...#
 *   0x11 = 10001 →  #...#
 *   0x1F = 11111 →  #####
 *   0x11 = 10001 →  #...#
 *   0x11 = 10001 →  #...#
 *   0x11 = 10001 →  #...#
 */

static const unsigned char FONT_DIGITS[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, /* 0 */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 1 */
    {0x0E, 0x11, 0x01, 0x0E, 0x10, 0x10, 0x1F}, /* 2 */
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E}, /* 3 */
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, /* 4 */
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, /* 5 */
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, /* 6 */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, /* 8 */
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, /* 9 */
};

static const unsigned char FONT_LETTERS[26][7] = {
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* A */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, /* B */
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, /* C */
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, /* D */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, /* E */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, /* F */
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, /* G */
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* H */
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* I */
    {0x01, 0x01, 0x01, 0x01, 0x01, 0x11, 0x0E}, /* J */
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, /* K */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, /* L */
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, /* M */
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, /* N */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* O */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, /* P */
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, /* Q */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, /* R */
    {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}, /* S */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* T */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* U */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, /* V */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, /* W */
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, /* X */
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, /* Y */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, /* Z */
};

typedef struct {
  char character;
  unsigned char bitmap[7];
} FontSpecialChar;

static const FontSpecialChar FONT_SPECIAL[] = {
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}}, /* period    */
    {',', {0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08}}, /* comma     */
    {':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}}, /* colon     */
    {'!', {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}}, /* !         */
    {'?', {0x0E, 0x11, 0x01, 0x06, 0x04, 0x00, 0x04}}, /* ?         */
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}}, /* dash      */
    {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}}, /* plus      */
    {'/', {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}}, /* slash     */
    {'(', {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}}, /* (         */
    {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}}, /* )         */
    {'_', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}}, /* _         */
    {'*', {0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00}}, /* *         */
    {'#', {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A}}, /* #         */
    {'^', {0x00, 0x04, 0x0E, 0x15, 0x04, 0x04, 0x00}}, /* ↑ arrow   */
    {'v', {0x00, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00}}, /* ↓ arrow   */
    {'{', {0x00, 0x04, 0x08, 0x1E, 0x08, 0x04, 0x00}}, /* ← arrow   */
    {'}', {0x00, 0x04, 0x02, 0x0F, 0x02, 0x04, 0x00}}, /* → arrow   */
    {0, {0}}                                           /* terminator*/
};

static const unsigned char *find_special_char(char c) {
  int i;
  for (i = 0; FONT_SPECIAL[i].character != 0; i++) {
    if (FONT_SPECIAL[i].character == c)
      return FONT_SPECIAL[i].bitmap;
  }
  return NULL;
}

/* ═══════════════════════════════════════════════════════════════
 * Drawing Primitives
 * ═══════════════════════════════════════════════════════════════ */

static void draw_rect(SnakeBackbuffer *bb, int x, int y, int w, int h,
                      uint32_t color) {
  /* Clip to backbuffer bounds */
  int x0 = x < 0 ? 0 : x;
  int y0 = y < 0 ? 0 : y;
  int x1 = (x + w > bb->width) ? bb->width : x + w;
  int y1 = (y + h > bb->height) ? bb->height : y + h;
  int px, py;
  for (py = y0; py < y1; py++)
    for (px = x0; px < x1; px++)
      bb->pixels[py * bb->width + px] = color;
}

static void draw_rect_blend(SnakeBackbuffer *bb, int x, int y, int w, int h,
                            uint32_t color) {
  int alpha = (color >> 24) & 0xFF;
  if (alpha == 255) {
    draw_rect(bb, x, y, w, h, color);
    return;
  }
  if (alpha == 0)
    return;

  int src_r = (color >> 16) & 0xFF;
  int src_g = (color >> 8) & 0xFF;
  int src_b = color & 0xFF;
  int inv_a = 255 - alpha;

  int x0 = x < 0 ? 0 : x;
  int y0 = y < 0 ? 0 : y;
  int x1 = (x + w > bb->width) ? bb->width : x + w;
  int y1 = (y + h > bb->height) ? bb->height : y + h;
  int px, py;
  for (py = y0; py < y1; py++) {
    for (px = x0; px < x1; px++) {
      uint32_t *dst = &bb->pixels[py * bb->width + px];
      int dst_r = (*dst >> 16) & 0xFF;
      int dst_g = (*dst >> 8) & 0xFF;
      int dst_b = *dst & 0xFF;
      int out_r = (src_r * alpha + dst_r * inv_a) / 255;
      int out_g = (src_g * alpha + dst_g * inv_a) / 255;
      int out_b = (src_b * alpha + dst_b * inv_a) / 255;
      *dst = GAME_RGB(out_r, out_g, out_b);
    }
  }
}

static void draw_char(SnakeBackbuffer *bb, int x, int y, char c, uint32_t color,
                      int scale) {
  const unsigned char *bitmap = NULL;
  int row, col;

  if (c >= '0' && c <= '9')
    bitmap = FONT_DIGITS[c - '0'];
  else if (c >= 'A' && c <= 'Z')
    bitmap = FONT_LETTERS[c - 'A'];
  else if (c >= 'a' && c <= 'z')
    bitmap = FONT_LETTERS[c - 'a'];
  else if (c == ' ')
    return; /* space = skip */
  else {
    bitmap = find_special_char(c);
    if (!bitmap)
      return;
  }

  for (row = 0; row < 7; row++)
    for (col = 0; col < 5; col++)
      if (bitmap[row] & (0x10 >> col))
        draw_rect(bb, x + col * scale, y + row * scale, scale, scale, color);
}

static void draw_text(SnakeBackbuffer *bb, int x, int y, const char *text,
                      uint32_t color, int scale) {
  int cursor_x = x;
  for (; *text; text++) {
    draw_char(bb, cursor_x, y, *text, color, scale);
    cursor_x += 6 * scale; /* 5px glyph + 1px gap */
  }
}

/* Draw one grid cell (inset by 1px) at grid position (col, row).
 * The play field starts at y = HEADER_ROWS * CELL_SIZE pixels. */
static void draw_cell(SnakeBackbuffer *bb, int col, int row, uint32_t color) {
  int field_y_offset = HEADER_ROWS * CELL_SIZE;
  draw_rect(bb, col * CELL_SIZE + 1, field_y_offset + row * CELL_SIZE + 1,
            CELL_SIZE - 2, CELL_SIZE - 2, color);
}

/* ═══════════════════════════════════════════════════════════════
 * Input
 * ═══════════════════════════════════════════════════════════════ */

void prepare_input_frame(GameInput *input) {
  /* Reset per-frame counters — NOT ended_down (that persists).
   * NOT move_timer or any GameState field (those are in snake.c). */
  input->turn_left.half_transition_count = 0;
  input->turn_right.half_transition_count = 0;
  input->restart = 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Game Logic
 * ═══════════════════════════════════════════════════════════════ */

void snake_spawn_food(GameState *s) {
  int ok, idx, rem;
  do {
    s->food_x = 1 + rand() % (GRID_WIDTH - 2);
    s->food_y = 1 + rand() % (GRID_HEIGHT - 2);
    ok = 1;
    idx = s->tail;
    rem = s->length;
    while (rem > 0) {
      if (s->segments[idx].x == s->food_x && s->segments[idx].y == s->food_y) {
        ok = 0;
        break;
      }
      idx = (idx + 1) % MAX_SNAKE;
      rem--;
    }
  } while (!ok);
}

void snake_init(GameState *s) {
  int saved_best = s->best_score;
  int i;
  memset(s, 0, sizeof(*s));
  s->best_score = saved_best;

  s->head = 9;
  s->tail = 0;
  s->length = 10;
  for (i = 0; i < 10; i++) {
    s->segments[i].x = 10 + i;
    s->segments[i].y = GRID_HEIGHT / 2;
  }

  s->direction = SNAKE_DIR_RIGHT;
  s->next_direction = SNAKE_DIR_RIGHT;
  s->move_timer = 0.0f;
  s->move_interval = 0.15f; /* 150ms per step */
  s->grow_pending = 0;
  s->score = 0;
  s->game_over = 0;

  srand((unsigned)time(NULL));
  snake_spawn_food(s);
}

void snake_update(GameState *s, const GameInput *input, float delta_time) {
  int new_x, new_y, idx, rem;
  SNAKE_DIR turned;

  if (s->game_over) {
    if (input->restart) {
      snake_init(s);
    }
    return;
  }

  /* Direction: turn fires on just-pressed (not held).
   * half_transition_count > 0 means the key was pressed this frame. */
  if (input->turn_right.ended_down &&
      input->turn_right.half_transition_count > 0) {
    turned = (SNAKE_DIR)((s->direction + 1) % 4);
    s->next_direction = turned;
  }
  if (input->turn_left.ended_down &&
      input->turn_left.half_transition_count > 0) {
    turned = (SNAKE_DIR)((s->direction + 3) % 4);
    s->next_direction = turned;
  }

  /* Accumulate time — only move when interval is reached.
   * Subtracting (not zeroing) preserves sub-frame overshoot
   * so movement stays accurate at any frame rate. */
  s->move_timer += delta_time;
  if (s->move_timer < s->move_interval)
    return;
  s->move_timer -= s->move_interval;

  s->direction = s->next_direction;

  new_x = s->segments[s->head].x + DX[s->direction];
  new_y = s->segments[s->head].y + DY[s->direction];

  /* Wall collision */
  if (new_x < 1 || new_x >= GRID_WIDTH - 1 || new_y < 0 ||
      new_y >= GRID_HEIGHT - 1) {
    if (s->score > s->best_score)
      s->best_score = s->score;
    s->game_over = 1;
    return;
  }

  /* Self collision — walk the ring buffer from tail to head */
  idx = s->tail;
  rem = s->length;
  while (rem > 0) {
    if (s->segments[idx].x == new_x && s->segments[idx].y == new_y) {
      if (s->score > s->best_score)
        s->best_score = s->score;
      s->game_over = 1;
      return;
    }
    idx = (idx + 1) % MAX_SNAKE;
    rem--;
  }

  /* Advance head */
  s->head = (s->head + 1) % MAX_SNAKE;
  s->segments[s->head].x = new_x;
  s->segments[s->head].y = new_y;
  s->length++;

  /* Food eaten */
  if (new_x == s->food_x && new_y == s->food_y) {
    s->score++;
    s->grow_pending += 5;
    /* Speed up every 3 score points, down to a floor of 0.05s */
    if (s->score % 3 == 0 && s->move_interval > 0.05f)
      s->move_interval -= 0.01f;
    snake_spawn_food(s);
  }

  /* Advance tail (unless growing) */
  if (s->grow_pending > 0) {
    s->grow_pending--;
  } else {
    s->tail = (s->tail + 1) % MAX_SNAKE;
    s->length--;
  }
}

/* ═══════════════════════════════════════════════════════════════
 * Rendering
 * ═══════════════════════════════════════════════════════════════ */

void snake_render(const GameState *s, SnakeBackbuffer *bb) {
  char buf[64];
  int col, row, idx, rem;
  uint32_t body_color, head_color;

  /* 1. Clear to black */
  draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BLACK);

  /* 2. Header background */
  draw_rect(bb, 0, 0, bb->width, HEADER_ROWS * CELL_SIZE, COLOR_DARK_GRAY);

  /* 3. Header separator line */
  draw_rect(bb, 0, (HEADER_ROWS - 1) * CELL_SIZE, bb->width, 2, COLOR_GREEN);

  /* 4. Header text: "SNAKE" centered, score left, best right */
  {
    int title_x = (bb->width - 5 * 6 * 2) / 2; /* center "SNAKE" at scale 2 */
    int text_y = (HEADER_ROWS * CELL_SIZE - 14) / 2; /* vertically centered */
    draw_text(bb, title_x, text_y, "SNAKE", COLOR_GREEN, 2);

    snprintf(buf, sizeof(buf), "SCORE:%d", s->score);
    draw_text(bb, 8, text_y, buf, COLOR_WHITE, 2);

    snprintf(buf, sizeof(buf), "BEST:%d", s->best_score);
    draw_text(bb, bb->width - (int)(strlen(buf)) * 12 - 8, text_y, buf,
              COLOR_YELLOW, 2);
  }

  /* 5. Border walls (left, right, bottom columns/rows of play field) */
  for (row = 0; row < GRID_HEIGHT; row++) {
    draw_cell(bb, 0, row, COLOR_GREEN);              /* left wall  */
    draw_cell(bb, GRID_WIDTH - 1, row, COLOR_GREEN); /* right wall */
  }
  for (col = 0; col < GRID_WIDTH; col++) {
    draw_cell(bb, col, GRID_HEIGHT - 1, COLOR_GREEN); /* bottom wall */
  }

  /* 6. Food */
  draw_cell(bb, s->food_x, s->food_y, COLOR_RED);

  /* 7. Snake body and head
   *    Dead snake: whole body turns dark red */
  body_color = s->game_over ? COLOR_DARK_RED : COLOR_YELLOW;
  head_color = s->game_over ? COLOR_DARK_RED : COLOR_WHITE;

  idx = s->tail;
  rem = s->length - 1; /* all segments except head */
  while (rem > 0) {
    draw_cell(bb, s->segments[idx].x, s->segments[idx].y, body_color);
    idx = (idx + 1) % MAX_SNAKE;
    rem--;
  }
  draw_cell(bb, s->segments[s->head].x, s->segments[s->head].y, head_color);

  /* 8. Game over overlay */
  if (s->game_over) {
    int field_y = HEADER_ROWS * CELL_SIZE;
    int field_w = GRID_WIDTH * CELL_SIZE;
    int field_h = GRID_HEIGHT * CELL_SIZE;
    int cx = field_w / 2;
    int cy = field_y + field_h / 2;

    /* Semi-transparent black overlay over the play field */
    draw_rect_blend(bb, 0, field_y, field_w, field_h, GAME_RGBA(0, 0, 0, 180));

    /* Red border: 4 thin rectangles */
    draw_rect(bb, 0, field_y, field_w, 4, COLOR_RED);
    draw_rect(bb, 0, field_y + field_h - 4, field_w, 4, COLOR_RED);
    draw_rect(bb, 0, field_y, 4, field_h, COLOR_RED);
    draw_rect(bb, field_w - 4, field_y, 4, field_h, COLOR_RED);

    /* "GAME OVER" centered at scale 3 */
    {
      int scale = 3;
      int str_w = 9 * 6 * scale; /* strlen("GAME OVER") * char_width */
      draw_text(bb, cx - str_w / 2, cy - 36, "GAME OVER", COLOR_RED, scale);
    }

    /* Score and restart hint */
    snprintf(buf, sizeof(buf), "SCORE %d", s->score);
    {
      int sw = (int)(strlen(buf)) * 12;
      draw_text(bb, cx - sw / 2, cy + 6, buf, COLOR_WHITE, 2);
    }
    {
      int hw = 9 * 12; /* "R RESTART" */
      draw_text(bb, cx - hw / 2, cy + 28, "R RESTART", COLOR_WHITE, 2);
    }
    {
      int qw = 6 * 12; /* "Q QUIT" */
      draw_text(bb, cx - qw / 2, cy + 50, "Q QUIT", COLOR_GRAY, 2);
    }
  }
}
