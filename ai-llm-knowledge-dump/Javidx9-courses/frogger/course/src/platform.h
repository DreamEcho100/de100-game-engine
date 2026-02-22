#ifndef PLATFORM_H
#define PLATFORM_H

/* =============================================================================
   platform.h — The Platform Contract
   =============================================================================

   This file defines exactly 6 functions that every platform backend must
   implement. The game logic never calls X11 or Raylib directly — it only
   calls these 6 functions.

   JS analogy: think of this like an interface/protocol in TypeScript:
     interface Platform { init, getInput, render, sleep, shouldQuit, shutdown }

   The two backends (main_x11.c and main_raylib.c) each implement this
   "interface" in plain C.
   ============================================================================= */

#include "frogger.h"

/* Open a window of the given pixel dimensions with a title string.
   Called once at startup — this is Wave 1 lifetime (lives until process ends). */
void platform_init(int width, int height, const char *title);

/* Fill *input with the current keyboard state.
   Released flags are only set for ONE frame (the frame the key was let go). */
void platform_get_input(InputState *input);

/* Draw the current game state to the screen.
   Takes a read-only pointer — this function MUST NOT modify GameState. */
void platform_render(const GameState *state);

/* Sleep for approximately ms milliseconds.
   Used to cap the frame rate so we don't burn 100% CPU. */
void platform_sleep_ms(int ms);

/* Returns 1 if the user has asked to close the window, 0 otherwise. */
int platform_should_quit(void);

/* Release window/display resources. Called once at shutdown. */
void platform_shutdown(void);

#endif /* PLATFORM_H */
