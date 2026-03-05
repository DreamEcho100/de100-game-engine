/* src/utils/draw-shapes.h  —  Desktop Tower Defense | Drawing Primitives */
#ifndef DTD_DRAW_SHAPES_H
#define DTD_DRAW_SHAPES_H

#include <stdint.h>
#include "backbuffer.h"

/* Solid filled rectangle, clipped to backbuffer bounds. */
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);

/* Alpha-blended rectangle. alpha 0=transparent, 255=opaque.
 * GAME_RGBA color — alpha byte is extracted from color automatically. */
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);

/* 1-pixel Bresenham line, clipped. */
void draw_line(Backbuffer *bb, int x0, int y0, int x1, int y1, uint32_t color);

/* Filled circle (uses dx*dx + dy*dy <= r*r). */
void draw_circle(Backbuffer *bb, int cx, int cy, int r, uint32_t color);

/* Circle outline (1 pixel thick, Bresenham). */
void draw_circle_outline(Backbuffer *bb, int cx, int cy, int r, uint32_t color);

#endif /* DTD_DRAW_SHAPES_H */
