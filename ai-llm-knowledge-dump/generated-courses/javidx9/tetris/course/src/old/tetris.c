#include "tetris.h"

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
 * Bitmap Font Data (5x7 pixels per character)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Each row is 5 bits wide (bits 4-0), packed into a byte.
 * Bit 4 = leftmost pixel, Bit 0 = rightmost pixel.
 *
 * Example: Letter 'A' = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}
 *
 *   0x0E = 01110 =  .###.
 *   0x11 = 10001 =  #...#
 *   0x11 = 10001 =  #...#
 *   0x1F = 11111 =  #####
 *   0x11 = 10001 =  #...#
 *   0x11 = 10001 =  #...#
 *   0x11 = 10001 =  #...#
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

/* ADD THIS: Special characters lookup table */
typedef struct {
  char character;
  unsigned char bitmap[7];
} FontSpecialChar;

static const FontSpecialChar FONT_SPECIAL[] = {
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},  /* period */
    {',', {0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08}},  /* comma */
    {':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}},  /* colon */
    {';', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08}},  /* semicolon */
    {'!', {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}},  /* exclamation */
    {'?', {0x0E, 0x11, 0x01, 0x06, 0x04, 0x00, 0x04}},  /* question */
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},  /* dash/minus */
    {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}},  /* plus */
    {'=', {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00}},  /* equals */
    {'/', {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}},  /* slash */
    {'\\', {0x10, 0x08, 0x08, 0x04, 0x02, 0x02, 0x01}}, /* backslash */
    {'(', {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},  /* left paren */
    {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}},  /* right paren */
    {'[', {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E}},  /* left bracket */
    {']', {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E}},  /* right bracket */
    {'<', {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02}},  /* less than */
    {'>', {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08}},  /* greater than */
    {'_', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}},  /* underscore */
    {'*', {0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00}},  /* asterisk */
    {'#', {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A}},  /* hash */
    {'@', {0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E}},  /* at sign */
    {'%', {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13}},  /* percent */
    {'&', {0x08, 0x14, 0x14, 0x08, 0x15, 0x12, 0x0D}},  /* ampersand */
    {'\'', {0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00}}, /* single quote */
    {'"', {0x0A, 0x0A, 0x14, 0x00, 0x00, 0x00, 0x00}},  /* double quote */
    /* Arrow symbols using extended ASCII or custom mapping */
    {'^', {0x00, 0x04, 0x0E, 0x15, 0x04, 0x04, 0x00}}, /* up arrow ↑ */
    {'v',
     {0x00, 0x04, 0x04, 0x15, 0x0E, 0x04,
      0x00}}, /* down arrow ↓ (use lowercase v) */
    {'{',
     {0x00, 0x04, 0x08, 0x1E, 0x08, 0x04, 0x00}}, /* left arrow ← (use {) */
    {'}',
     {0x00, 0x04, 0x02, 0x0F, 0x02, 0x04, 0x00}}, /* right arrow → (use }) */
    {0, {0}}                                      /* Terminator */
};

/* Helper function to find special character bitmap */
static const unsigned char *find_special_char(char c) {
  for (int i = 0; FONT_SPECIAL[i].character != 0; i++) {
    if (FONT_SPECIAL[i].character == c) {
      return FONT_SPECIAL[i].bitmap;
    }
  }
  return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Drawing Primitives
 * ═══════════════════════════════════════════════════════════════════════════
 */

void draw_rect(TetrisBackbuffer *bb, int x, int y, int w, int h,
               uint32_t color) {
  /* Clip to backbuffer bounds */
  int x0 = x < 0 ? 0 : x;
  int y0 = y < 0 ? 0 : y;
  int x1 = (x + w) > bb->width ? bb->width : (x + w);
  int y1 = (y + h) > bb->height ? bb->height : (y + h);

  for (int py = y0; py < y1; py++) {
    uint32_t *row = bb->pixels + py * (bb->pitch / 4);
    for (int px = x0; px < x1; px++) {
      row[px] = color;
    }
  }
}

void draw_rect_blend(TetrisBackbuffer *bb, int x, int y, int w, int h,
                     uint32_t color) {
  /* Extract alpha */
  uint8_t alpha = (color >> 24) & 0xFF;
  if (alpha == 255) {
    draw_rect(bb, x, y, w, h, color);
    return;
  }
  if (alpha == 0)
    return;

  uint8_t src_r = (color >> 16) & 0xFF;
  uint8_t src_g = (color >> 8) & 0xFF;
  uint8_t src_b = color & 0xFF;

  int x0 = x < 0 ? 0 : x;
  int y0 = y < 0 ? 0 : y;
  int x1 = (x + w) > bb->width ? bb->width : (x + w);
  int y1 = (y + h) > bb->height ? bb->height : (y + h);

  for (int py = y0; py < y1; py++) {
    uint32_t *row = bb->pixels + py * (bb->pitch / 4);
    for (int px = x0; px < x1; px++) {
      uint32_t dst = row[px];
      uint8_t dst_r = (dst >> 16) & 0xFF;
      uint8_t dst_g = (dst >> 8) & 0xFF;
      uint8_t dst_b = dst & 0xFF;

      /* Simple alpha blend: out = src * alpha + dst * (1 - alpha) */
      uint8_t out_r = (src_r * alpha + dst_r * (255 - alpha)) / 255;
      uint8_t out_g = (src_g * alpha + dst_g * (255 - alpha)) / 255;
      uint8_t out_b = (src_b * alpha + dst_b * (255 - alpha)) / 255;

      row[px] = TETRIS_RGB(out_r, out_g, out_b);
    }
  }
}

static void draw_char(TetrisBackbuffer *bb, int x, int y, char c,
                      uint32_t color, int scale) {
  const unsigned char *bitmap = NULL;

  if (c >= '0' && c <= '9') {
    bitmap = FONT_DIGITS[c - '0'];
  } else if (c >= 'A' && c <= 'Z') {
    bitmap = FONT_LETTERS[c - 'A'];
  } else if (c >= 'a' && c <= 'z') {
    bitmap = FONT_LETTERS[c - 'a']; /* Lowercase maps to uppercase */
  } else if (c == ' ') {
    return; /* Space - just advance cursor, draw nothing */
  } else {
    /* Try special characters */
    bitmap = find_special_char(c);
    if (!bitmap) {
      return; /* Unknown character - skip */
    }
  }

  /* Draw the 5x7 bitmap */
  for (int row = 0; row < 7; row++) {
    for (int col = 0; col < 5; col++) {
      if (bitmap[row] & (0x10 >> col)) {
        draw_rect(bb, x + col * scale, y + row * scale, scale, scale, color);
      }
    }
  }
}

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

static void draw_cell(TetrisBackbuffer *bb, int col, int row, uint32_t color) {
  int x = col * CELL_SIZE + 1;
  int y = row * CELL_SIZE + 1;
  int w = CELL_SIZE - 2;
  int h = CELL_SIZE - 2;
  draw_rect(bb, x, y, w, h, color);
}

static void draw_piece(TetrisBackbuffer *bb, int piece_index, int field_col,
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

void draw_text(TetrisBackbuffer *bb, int x, int y, const char *text,
               uint32_t color, int scale) {
  int cursor_x = x;
  while (*text) {
    draw_char(bb, cursor_x, y, *text, color, scale);
    cursor_x += 6 * scale;
    text++;
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main Render Function - Called by Platform Layer
 * ═══════════════════════════════════════════════════════════════════════════
 */

void tetris_render(TetrisBackbuffer *backbuffer, GameState *state) {
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
             state->current_piece.rotation);

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
  if (state->game_over) {
    int cx = FIELD_WIDTH * CELL_SIZE / 2;
    int cy = FIELD_HEIGHT * CELL_SIZE / 2;

    /* Semi-transparent overlay */
    draw_rect_blend(backbuffer, cx - 80, cy - 50, 160, 100,
                    TETRIS_RGBA(0, 0, 0, 200));

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

void game_init(GameState *state, GameInput *input) {
  state->score = 0;
  state->game_over = false;
  state->pieces_count = 1;
  state->level = 0;

  state->completed_lines.count = 0;
  state->completed_lines.flash_timer.timer = 0;
  state->completed_lines.flash_timer.interval = 0.4f;
  memset(&state->completed_lines.indexes, 0,
         sizeof(int) * TETROMINO_LAYER_COUNT);

  // Time-based dropping
  state->tetromino_drop.timer = 0.0f;
  state->tetromino_drop.interval = 0.8f; // 1 second between drops initially

  /* Configure auto-repeat intervals */
  input->move_left.repeat.interval = 0.05f;
  input->move_right.repeat.interval = 0.05f;
  input->move_down.repeat.interval = 0.03f;

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
      .rotation = TETROMINO_R_0,
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
}

int tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                             int pos_x, int pos_y) {

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      /* Which character in the piece string? */
      int pi = tetromino_pos_value(px, py, rotation);

      /* Skip empty cells in the piece */
      if (TETROMINOES[piece][pi] == '.')
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
void prepare_input_frame(GameInput *input) {
  input->move_left.button.half_transition_count = 0;
  input->move_right.button.half_transition_count = 0;
  input->move_down.button.half_transition_count = 0;
  input->rotate_x.button.half_transition_count = 0;
  input->rotate_x.value = 0;
};

/**
 * Process an action with auto-repeat behavior.
 *
 * @param action     The action to process (modified in place)
 * @param delta_time Seconds since last frame
 * @param should_trigger Output: set to 1 if action should fire this frame
 *
 * Behavior:
 * - Button just pressed → trigger immediately, reset timer
 * - Button held → accumulate time, trigger when timer >= interval
 * - Button released → reset timer, don't trigger
 */
static void handle_action_with_repeat(GameActionWithRepeat *action,
                                      float delta_time, int *should_trigger) {
  *should_trigger = 0;

  if (!action->button.ended_down) {
    /* Button is up — reset timer, don't trigger */
    action->repeat.timer = 0.0f;
    return;
  }

  /* Button is down */
  if (action->button.half_transition_count > 0) {
    /* Just pressed this frame — trigger immediately */
    *should_trigger = 1;
    action->repeat.timer = 0.0f;
  } else {
    /* Held from previous frame — check auto-repeat timer */
    action->repeat.timer += delta_time;

    if (action->repeat.timer >= action->repeat.interval) {
      /* Timer expired — trigger and reset (keeping remainder for precision) */
      action->repeat.timer -= action->repeat.interval;
      *should_trigger = 1;
    }
  }
}

void tetris_apply_input(GameState *state, GameInput *input, float delta_time) {

  /* Rotate clockwise: try rotation + 1. (% 4 wraps 3 back to 0.) */
  if (input->rotate_x.button.ended_down &&
      input->rotate_x.button.half_transition_count != 0 &&
      input->rotate_x.value != TETROMINO_ROTATE_X_NONE) {

    TETROMINO_R_DIR new_rotation;
    if (input->rotate_x.value == TETROMINO_ROTATE_X_GO_RIGHT) {
      switch (state->current_piece.rotation) {
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
    } else if (input->rotate_x.value == TETROMINO_ROTATE_X_GO_LEFT) {
      switch (state->current_piece.rotation) {
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
      state->current_piece.rotation = new_rotation;
    }

    // input->rotate_x.value = 0;
  }

  /* ── Horizontal movement: independent auto-repeat ── */
  int should_move_left = 0;
  int should_move_right = 0;

  handle_action_with_repeat(&input->move_left, delta_time, &should_move_left);
  handle_action_with_repeat(&input->move_right, delta_time, &should_move_right);

  /* Move left: try current_piece.col - 1 */
  if (should_move_left &&
      tetromino_does_piece_fit(
          state, state->current_piece.index, state->current_piece.rotation,
          state->current_piece.x - 1, state->current_piece.y)) {
    state->current_piece.x--;
  }

  /* Move right: try current_piece.col + 1 */
  if (should_move_right &&
      tetromino_does_piece_fit(
          state, state->current_piece.index, state->current_piece.rotation,
          state->current_piece.x + 1, state->current_piece.y)) {
    state->current_piece.x++;
  }

  /* ── Soft drop: independent auto-repeat (faster interval) ── */
  int should_move_down = 0;

  handle_action_with_repeat(&input->move_down, delta_time, &should_move_down);
  /* Soft drop: try current_piece.row + 1 (down = positive Y) */
  if (should_move_down &&
      tetromino_does_piece_fit(
          state, state->current_piece.index, state->current_piece.rotation,
          state->current_piece.x, state->current_piece.y + 1)) {
    state->current_piece.y++;
  }
}

void tetris_update(GameState *state, GameInput *input, float delta_time) {
  if (state->game_over)
    return;

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

      state->completed_lines.count = 0;
    }
    return; /* freeze all game logic while flashing */
  }

  /* ── Handle player input (always responsive) ── */
  tetris_apply_input(state, input, delta_time);

  /* ── Accumulate time for gravity ── */
  state->tetromino_drop.timer += delta_time;

  int game_speed = state->level - 1 < 0 ? 0 : state->level;
  float tetromino_drop_interval =
      state->tetromino_drop.interval + (0.01f + game_speed * 0.01f);
  if (tetromino_drop_interval < 0.1f) {
    tetromino_drop_interval = 0.1f; /* cap at 100ms per drop */
  }

  /* ── Check if it's time to drop the piece ── */
  if (state->tetromino_drop.timer >= tetromino_drop_interval) {
    // Reset timer, keeping remainder for precision
    // float drop_interval = 0.1f + state->speed * 0.01f;
    // if (drop_interval > 0.8f) {
    //   drop_interval = 0.8f;
    // }
    state->tetromino_drop.timer -= tetromino_drop_interval;

    /* Try to move the piece down one row */
    if (tetromino_does_piece_fit(
            state, state->current_piece.index, state->current_piece.rotation,
            state->current_piece.x, state->current_piece.y + 1)) {
      state->current_piece.y++;
    } else {
      /* If it doesn't fit, piece has landed - locking in lesson 09 */
      for (int px = 0; px < TETROMINO_LAYER_COUNT; ++px) {
        for (int py = 0; py < TETROMINO_LAYER_COUNT; ++py) {
          int pi = tetromino_pos_value(px, py, state->current_piece.rotation);
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
      }

      // reset drop-timer and current piece for next round
      state->tetromino_drop.timer = 0.0f;
      state->current_piece = (CurrentPiece){
          .x = (FIELD_WIDTH * 0.5) - (TETROMINO_LAYER_COUNT * 0.5),
          .y = 0,
          .index = state->current_piece.next_index,
          .next_index = rand() % TETROMINOS_COUNT,
          .rotation = TETROMINO_R_0,
      };

      if (!tetromino_does_piece_fit(
              state, state->current_piece.index, state->current_piece.rotation,
              state->current_piece.x, state->current_piece.y)) {
        state->game_over = true;
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
// tetris_update(&game_state, &input, delta_time * slow_mo_factor);
// ```

// 4. Pause is Trivial
// ```c
// if (!paused) {
//   tetris_update(&game_state, &input, delta_time);
// }
// // When paused, just don't call update - timer doesn't advance
// ```