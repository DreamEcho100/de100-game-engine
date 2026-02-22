#ifndef PLATFORM_H
#define PLATFORM_H

/* ============================================================
 * platform.h — Platform API Contract
 *
 * Six functions that every backend (X11, Raylib, etc.) MUST implement.
 * The game loop in main() calls ONLY these functions + snake_tick().
 * It never calls X11 or Raylib APIs directly.
 *
 * This is the "interface" that decouples game logic from display.
 * Want a new backend (SDL2, terminal, web)? Implement these 6 functions.
 * ============================================================ */

#include "snake.h"

/* Open the window, allocate colors, initialize input handling.
 * Called ONCE at program start. */
void platform_init(void);

/* Fill *input with the current key state for this tick.
 * turn_left/turn_right/restart fire exactly ONCE per physical key press.
 * The platform layer is responsible for the latch (fire-once) logic. */
void platform_get_input(PlatformInput *input);

/* Draw the current game state to the screen.
 * 🔴 HOT PATH — called every tick. No malloc. No file I/O.
 * const: the renderer may NOT modify state. */
void platform_render(const GameState *state);

/* Sleep for approximately ms milliseconds.
 * Paces the game loop to speed_ms per tick. */
void platform_sleep_ms(int ms);

/* Returns 1 if the user closed the window or pressed Q/Escape. */
int platform_should_quit(void);

/* Release all platform resources (window, GC, etc.).
 * Called ONCE at program exit. */
void platform_shutdown(void);

#endif /* PLATFORM_H */
