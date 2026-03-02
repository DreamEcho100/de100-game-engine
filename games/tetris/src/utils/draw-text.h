#ifndef UTILS_DRAW_TEXT_H
#define UTILS_DRAW_TEXT_H

#include "backbuffer.h"
#include <stdint.h>

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

extern const unsigned char FONT_DIGITS[10][7];
extern const unsigned char FONT_LETTERS[26][7];

/* ADD THIS: Special characters lookup table */
typedef struct {
  char character;
  unsigned char bitmap[7];
} FontSpecialChar;

extern const FontSpecialChar FONT_SPECIAL[];

/* Helper function to find special character bitmap */
const unsigned char *find_special_char(char c);

void draw_char(Backbuffer *bb, int x, int y, char c, uint32_t color, int scale);
void draw_text(Backbuffer *bb, int x, int y, const char *text, uint32_t color,
               int scale);

#endif // UTILS_DRAW_TEXT_H