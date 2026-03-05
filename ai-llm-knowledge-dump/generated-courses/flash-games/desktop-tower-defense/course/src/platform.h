/* src/platform.h  —  Desktop Tower Defense | Platform Contract
 *
 * This header defines the four mandatory functions that every platform backend
 * (main_raylib.c, main_x11.c) must implement. game.c never calls OS or graphics
 * APIs directly — it only sees this contract.
 *
 * JS analogy: platform.h is a TypeScript interface; the backend files are the
 * concrete classes that implement it.
 */
#ifndef DTD_PLATFORM_H
#define DTD_PLATFORM_H

#include "game.h"

/* -------------------------------------------------------------------------
 * UPDATE_BUTTON macro — edge detection for mouse button state.
 * Reads the current OS state and sets the .pressed / .released edge flags.
 * Usage:  UPDATE_BUTTON(mouse.left_pressed, mouse.left_released, mouse.left_down, is_down_now)
 * -------------------------------------------------------------------------*/
#define UPDATE_BUTTON(pressed, released, was_down, is_down_now) \
    do {                                                          \
        (pressed)  = (!( was_down) &&  (is_down_now));           \
        (released) = (( was_down) && !(is_down_now));            \
        (was_down) = (is_down_now);                              \
    } while (0)

/* Open a window of the given pixel dimensions with the given title.
 * Allocates and initialises the pixel backbuffer. */
void platform_init(const char *title, int width, int height);

/* Return a monotonic timestamp in seconds (never jumps backward). */
double platform_get_time(void);

/* Read OS input events; fill *mouse with the current mouse state.
 * Must be called once per frame, before game_update(). */
void platform_get_input(MouseState *mouse);

/* Upload bb->pixels to the GPU and flip the display.
 * For Raylib: performs R↔B swap before UpdateTexture.
 * For X11: calls XPutImage directly. */
void platform_display_backbuffer(const Backbuffer *bb);

/* -------------------------------------------------------------------------
 * Audio extensions — implemented in both backends.
 * game_audio_init() must be called AFTER platform_init().
 * -------------------------------------------------------------------------*/

/* Initialise audio output at samples_per_second sample rate. */
void platform_audio_init(GameState *state, int samples_per_second);

/* Called every frame: check if the audio stream needs more samples;
 * if so, call game_get_audio_samples() and push to hardware. */
void platform_audio_update(GameState *state);

/* Release audio resources. */
void platform_audio_shutdown(void);

#endif /* DTD_PLATFORM_H */
