/* src/utils/draw-text.h  —  Desktop Tower Defense | Bitmap Font Rendering */
#ifndef DTD_DRAW_TEXT_H
#define DTD_DRAW_TEXT_H

#include <stdint.h>
#include "backbuffer.h"

/* Draw a single character at (x, y) with the given color and pixel scale.
 * scale=1 → 8×8 pixels; scale=2 → 16×16 pixels. */
void draw_char(Backbuffer *bb, int x, int y, char c, uint32_t color, int scale);

/* Draw a null-terminated string. Each character is 8*scale pixels wide. */
void draw_text(Backbuffer *bb, int x, int y, const char *text, uint32_t color, int scale);

/* Text width in pixels for layout: strlen(text) * 8 * scale */
int text_width(const char *text, int scale);

#endif /* DTD_DRAW_TEXT_H */
