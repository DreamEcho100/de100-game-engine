#ifndef PLATFORM_H
#define PLATFORM_H

/* ============================================================
 * platform.h — Platform API Contract (4 functions)
 *
 * Every backend (X11, Raylib, SDL3, ...) MUST implement exactly these.
 * Game logic in snake.c never calls any OS or library API directly.
 *
 * Want a new backend? Implement these 4 functions. That's it.
 * ============================================================ */

#include "snake.h"

/* Open the window and initialize all platform resources.
 * Called ONCE at program start. */
void platform_init(const char *title, int width, int height);

/* Return the current time in seconds (monotonic clock).
 * Used to compute delta_time in the game loop. */
double platform_get_time(void);

/* Fill *input with key events accumulated since the last call.
 * Uses GameButtonState so sub-frame taps are never lost. */
void platform_get_input(GameInput *input);

/* Note: display_backbuffer is handled internally by each backend:
 * - main_x11.c:     static platform_display_backbuffer (GLX texture upload)
 * - main_raylib.c:  UpdateTexture + DrawTexture inline in main() */

#endif /* PLATFORM_H */
