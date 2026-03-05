/* src/utils/draw-shapes.c  —  Desktop Tower Defense | Drawing Primitives */
#include <stdlib.h>
#include "draw-shapes.h"

void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color)
{
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > bb->width  ? bb->width  : (x + w);
    int y1 = (y + h) > bb->height ? bb->height : (y + h);
    if (x0 >= x1 || y0 >= y1) return;

    int stride = bb->pitch / 4;
    for (int row = y0; row < y1; row++) {
        uint32_t *line = bb->pixels + row * stride;
        for (int col = x0; col < x1; col++) {
            line[col] = color;
        }
    }
}

void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h, uint32_t color)
{
    uint32_t alpha = (color >> 24) & 0xFF;
    if (alpha == 0) return;
    if (alpha == 255) { draw_rect(bb, x, y, w, h, color); return; }

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > bb->width  ? bb->width  : (x + w);
    int y1 = (y + h) > bb->height ? bb->height : (y + h);
    if (x0 >= x1 || y0 >= y1) return;

    int      stride    = bb->pitch / 4;
    uint32_t inv_alpha = 255 - alpha;
    uint32_t src_r     = (color >> 16) & 0xFF;
    uint32_t src_g     = (color >>  8) & 0xFF;
    uint32_t src_b     =  color        & 0xFF;

    for (int row = y0; row < y1; row++) {
        uint32_t *line = bb->pixels + row * stride;
        for (int col = x0; col < x1; col++) {
            uint32_t dst   = line[col];
            uint32_t dst_r = (dst >> 16) & 0xFF;
            uint32_t dst_g = (dst >>  8) & 0xFF;
            uint32_t dst_b =  dst        & 0xFF;
            uint32_t out_r = (src_r * alpha + dst_r * inv_alpha) >> 8;
            uint32_t out_g = (src_g * alpha + dst_g * inv_alpha) >> 8;
            uint32_t out_b = (src_b * alpha + dst_b * inv_alpha) >> 8;
            line[col] = ((uint32_t)0xFF << 24) | (out_r << 16) | (out_g << 8) | out_b;
        }
    }
}

void draw_line(Backbuffer *bb, int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx  = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy  = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2;
    int stride = bb->pitch / 4;

    while (1) {
        if (x0 >= 0 && x0 < bb->width && y0 >= 0 && y0 < bb->height)
            bb->pixels[y0 * stride + x0] = color;
        if (x0 == x1 && y0 == y1) break;
        int e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
}

void draw_circle(Backbuffer *bb, int cx, int cy, int r, uint32_t color)
{
    int bx0 = cx - r < 0           ? 0              : cx - r;
    int by0 = cy - r < 0           ? 0              : cy - r;
    int bx1 = cx + r >= bb->width  ? bb->width  - 1 : cx + r;
    int by1 = cy + r >= bb->height ? bb->height - 1 : cy + r;
    int stride = bb->pitch / 4;

    for (int y = by0; y <= by1; y++) {
        int dy = y - cy;
        for (int x = bx0; x <= bx1; x++) {
            int dx = x - cx;
            if (dx * dx + dy * dy <= r * r)
                bb->pixels[y * stride + x] = color;
        }
    }
}

void draw_circle_outline(Backbuffer *bb, int cx, int cy, int r, uint32_t color)
{
    int x = 0, y = r, d = 1 - r;
    int stride = bb->pitch / 4;

#define PLOT(px, py) \
    do { \
        if ((px) >= 0 && (px) < bb->width && (py) >= 0 && (py) < bb->height) \
            bb->pixels[(py) * stride + (px)] = color; \
    } while (0)

    while (x <= y) {
        PLOT(cx + x, cy + y); PLOT(cx - x, cy + y);
        PLOT(cx + x, cy - y); PLOT(cx - x, cy - y);
        PLOT(cx + y, cy + x); PLOT(cx - y, cy + x);
        PLOT(cx + y, cy - x); PLOT(cx - y, cy - x);
        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }

#undef PLOT
}
