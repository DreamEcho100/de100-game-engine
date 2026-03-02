# Snake Course — Upgrade Plan Tracker

Reference: [UPGRADE-PLAN.md](./UPGRADE-PLAN.md)

## Phase 1 — Backup

| ID             | Task                                                               | Status  |
| -------------- | ------------------------------------------------------------------ | ------- |
| backup-lessons | Create `course/lessons/old/` and move all 10 existing lesson files | ✅ done |

## Phase 2 — Source Files

| ID                  | Task                                                                                                                         | Status  |
| ------------------- | ---------------------------------------------------------------------------------------------------------------------------- | ------- |
| update-snake-h      | Rewrite `src/snake.h` — SnakeBackbuffer, GAME_RGB/A, GameButtonState, SNAKE_DIR enum, GameInput, updated GameState           | ✅ done |
| update-snake-c      | Rewrite `src/snake.c` — draw_rect, draw_rect_blend, bitmap font, draw_text, draw_cell, snake_render, delta-time snake_update | ✅ done |
| update-platform-h   | Rewrite `src/platform.h` — 4-function contract (init, get_time, get_input, display_backbuffer)                               | ✅ done |
| update-main-x11     | Rewrite `src/main_x11.c` — GLX/OpenGL backbuffer, VSync, XkbSetDetectableAutoRepeat, delta-time loop                         | ✅ done |
| update-main-raylib  | Rewrite `src/main_raylib.c` — backbuffer texture pipeline, delta-time loop                                                   | ✅ done |
| update-build-x11    | Update `build_x11.sh` — clang, -lGL -lGLX -lxkbcommon                                                                        | ✅ done |
| update-build-raylib | Update `build_raylib.sh` — clang                                                                                             | ✅ done |

## Phase 3 — Lessons

| ID              | Task                                                                               | Status  |
| --------------- | ---------------------------------------------------------------------------------- | ------- |
| write-lesson-01 | `lessons/lesson-01.md` — Window + GLX/OpenGL Context                               | ✅ done |
| write-lesson-02 | `lessons/lesson-02.md` — SnakeBackbuffer: Platform-Independent Canvas              | ✅ done |
| write-lesson-03 | `lessons/lesson-03.md` — Color System: uint32_t, GAME_RGB, Named Constants         | ✅ done |
| write-lesson-04 | `lessons/lesson-04.md` — Typed Enums: SNAKE_DIR                                    | ✅ done |
| write-lesson-05 | `lessons/lesson-05.md` — GameState + Ring Buffer + snake_init                      | ✅ done |
| write-lesson-06 | `lessons/lesson-06.md` — Drawing Primitives: draw_rect, draw_cell                  | ✅ done |
| write-lesson-07 | `lessons/lesson-07.md` — snake_render: The Drawing Function                        | ✅ done |
| write-lesson-08 | `lessons/lesson-08.md` — Delta-Time Game Loop + Move Timer                         | ✅ done |
| write-lesson-09 | `lessons/lesson-09.md` — Input System: GameButtonState + UPDATE_BUTTON             | ✅ done |
| write-lesson-10 | `lessons/lesson-10.md` — Collision Detection: Walls + Self                         | ✅ done |
| write-lesson-11 | `lessons/lesson-11.md` — Food, Growth, Score, Speed Scaling                        | ✅ done |
| write-lesson-12 | `lessons/lesson-12.md` — Bitmap Font + HUD + Game Over Overlay + Final Integration | ✅ done |

---

_Last updated: all phases complete_
