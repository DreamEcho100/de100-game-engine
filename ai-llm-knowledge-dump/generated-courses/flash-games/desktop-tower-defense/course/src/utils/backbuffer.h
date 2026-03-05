/* src/utils/backbuffer.h  —  Desktop Tower Defense | Backbuffer */
#ifndef DTD_BACKBUFFER_H
#define DTD_BACKBUFFER_H

#include <stdint.h>

/* Pixel format: 0xAARRGGBB  (little-endian: bytes [B, G, R, A] in memory).
 * X11: reads bytes natively as BGR — no swap needed.
 * Raylib: reads as RGBA — R↔B swap required before UpdateTexture. */

#define GAME_RGB(r, g, b) \
    (((uint32_t)0xFF << 24) | ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) <<  8) |  (uint32_t)(b))

#define GAME_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) <<  8) |  (uint32_t)(b))

/* The backbuffer: a flat array of ARGB pixels the game writes each frame.
 * pitch = bytes per row (= width * 4 for a packed, no-padding buffer).
 * Use pitch (not width*4) everywhere — stays correct if padding is ever added. */
typedef struct {
    uint32_t *pixels; /* pixels[y * (pitch/4) + x]  or  pixels[y * width + x] */
    int       width;
    int       height;
    int       pitch;  /* bytes per row */
} Backbuffer;

#endif /* DTD_BACKBUFFER_H */
