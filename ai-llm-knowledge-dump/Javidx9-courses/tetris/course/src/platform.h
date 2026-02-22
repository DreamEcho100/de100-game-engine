#ifndef PLATFORM_H
#define PLATFORM_H

/* ============================================================
 * platform.h — Platform API Contract (the "interface")
 *
 * This header defines the six functions that every platform backend
 * MUST implement. The game loop in main() calls only these functions
 * and tetris_tick(). It knows nothing about X11 or Raylib directly.
 *
 * This is the Handmade Hero platform layer pattern:
 *   "The platform layer handles OS details.
 *    The game layer handles game details.
 *    They only talk through this small, stable interface."
 *
 *  Dependency graph:
 *
 *    main_x11.c   ──→  platform.h  ←──  main_raylib.c
 *                           │
 *                      tetris.h / tetris.c
 *                      (shared game logic)
 *
 * ============================================================ */

#include "tetris.h"

/* Initialize the platform: open window, load colors, set up input.
 * Wave 1 resource — lives for the entire program lifetime.
 * Called ONCE at program start. */
void platform_init(void);

/* Poll input state for this tick and write into *input.
 * 'rotate' must fire exactly ONCE per physical key press (no repeat).
 * Called every tick before tetris_tick(). */
void platform_get_input(PlatformInput *input);

/* Render the current game state to the window.
 * 🔴 HOT PATH — called every tick (~20x/second).
 * Must NOT allocate memory. Must NOT do file I/O.
 * Reads state as read-only; does not modify it. */
void platform_render(const GameState *state);

/* Sleep for approximately `ms` milliseconds.
 * Used to pace the game loop to ~50ms per tick (20 ticks/second). */
void platform_sleep_ms(int ms);

/* Returns 1 if the user has requested to quit (ESC, Q, window close).
 * Checked at the top of the main loop each tick. */
int platform_should_quit(void);

/* Release all platform resources (window, GC, display connection).
 * Called ONCE at program exit. */
void platform_shutdown(void);

#endif /* PLATFORM_H */
