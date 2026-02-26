/*
 * platform.h  —  Sugar, Sugar | Platform Contract
 *
 * The ONLY header the platform backends (main_x11.c, main_raylib.c) need to
 * implement.  The game (game.c) calls these 4 functions.  Platforms never
 * call into game.c directly.
 *
 * Rule: if you add a new platform (SDL3, Win32, WebAssembly) you write a new
 * main_*.c that provides exactly these functions — zero changes to game.c.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "game.h"

/* -----------------------------------------------------------------------
 * MANDATORY — all backends must implement these four functions
 * ----------------------------------------------------------------------- */

/* Open a window with the given title and pixel dimensions. */
void   platform_init(const char *title, int width, int height);

/* Return monotonic time in seconds.
 * Called twice per frame; the difference is delta_time. */
double platform_get_time(void);

/* Read OS input events and fill *input with the current state.
 * Must call UPDATE_BUTTON() for every press/release seen this frame. */
void   platform_get_input(GameInput *input);

/* Upload bb->pixels (ARGB uint32_t array) to the display and flip buffers.
 * In X11: XPutImage.   In Raylib: UpdateTexture + DrawTexture. */
void   platform_display_backbuffer(const GameBackbuffer *backbuffer);

/* -----------------------------------------------------------------------
 * OPTIONAL — audio  (backends may implement as empty stubs)
 * ----------------------------------------------------------------------- */

/* Fire-and-forget sound effect.  sound_id is one of the SOUND_* constants
 * defined in sounds.h.  Does nothing if not implemented. */
void   platform_play_sound(int sound_id);

/* Start looping background music.  music_id is one of the MUSIC_* constants
 * defined in sounds.h.  Replaces any currently playing music. */
void   platform_play_music(int music_id);

/* Stop background music immediately. */
void   platform_stop_music(void);

#endif /* PLATFORM_H */
