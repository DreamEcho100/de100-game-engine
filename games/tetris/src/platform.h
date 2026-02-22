#ifndef PLATFORM_H
#define PLATFORM_H
#include "tetris.h"

/* ── Platform functions ──────────────────────────────── */
/* These are declared here and implemented in main_x11.c / main_raylib.c */

int platform_init(const char *title, int width, int height);
void platform_get_input(GameState *state, GameInput *input);
void platform_render(GameState *state);
// void platform_sleep_ms(int ms);
// int platform_should_quit(void);
/* Get current time in seconds since program start.
 * Used for delta time calculations. */
double platform_get_time(void);
void platform_shutdown(void);

#endif /* PLATFORM_H */