/* =============================================================================
   platform.h — 4-Function Platform Contract

   Any platform backend (X11, Raylib, SDL3, WebAssembly...) must implement
   exactly these 4 functions. Game code in asteroids.c never calls them —
   only main() in each platform file calls these.

   platform_display_backbuffer is intentionally NOT declared here because it
   takes AsteroidsBackbuffer* (a game type) and is static within each platform
   file — it is never called from game code.
   ============================================================================= */

#pragma once

#include "asteroids.h"

/* Create the window, initialise the graphics context, set up VSync.
 * Call once before the game loop.                                            */
void platform_init(const char *title, int width, int height);

/* Return seconds since an arbitrary fixed point (monotonic, never goes back).
 * Use double to avoid precision loss after long play sessions.               */
double platform_get_time(void);

/* Poll all pending input events and update GameInput via UPDATE_BUTTON.
 * Sets input->quit = 1 on window close or Escape.                           */
void platform_get_input(GameInput *input);

/* NOTE: platform_display_backbuffer is static in each platform file.
 * It uploads bb->pixels (uint32_t array, bytes [RR,GG,BB,AA]) to the GPU
 * as a GL_RGBA texture (X11) or R8G8B8A8 Texture2D (Raylib), then presents
 * the frame. VSync blocks here — no sleep_ms needed.                        */
