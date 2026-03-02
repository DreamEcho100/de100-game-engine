#ifndef UTILS_DRAW_SHAPES_H
#define UTILS_DRAW_SHAPES_H

#include "backbuffer.h"
#include <stdint.h>

void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h,
                     uint32_t color);

#endif // UTILS_DRAW_SHAPES_H