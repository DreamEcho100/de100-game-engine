#ifndef GAME_DEMO_H
#define GAME_DEMO_H

/* game/demo.h — Platform Foundation Course
 * Declares demo_render(); included by both backends.
 * LESSON 14 — remove when creating a game course.                          */

#include "../utils/backbuffer.h"
#include "../game/base.h"

void demo_render(Backbuffer *bb, GameInput *input, int fps);

#endif /* GAME_DEMO_H */
