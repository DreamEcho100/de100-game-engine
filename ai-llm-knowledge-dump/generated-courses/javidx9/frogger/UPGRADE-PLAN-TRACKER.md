# Frogger Course — Upgrade Plan Tracker

Reference: [UPGRADE-PLAN.md](./UPGRADE-PLAN.md)

## Phase 1 — Backup

| ID | Task | Status |
|----|------|--------|
| backup-lessons | Create `course/lessons/old/` and move all 10 existing lesson files | ✅ done |

## Phase 2 — Source Files

| ID | Task | Status |
|----|------|--------|
| update-frogger-h | Rewrite `src/frogger.h` — FroggerBackbuffer, FROGGER_RGB, GameButtonState, UPDATE_BUTTON, GameInput, frogger_render declaration, remove frogger_run | ✅ done |
| update-frogger-c | Rewrite `src/frogger.c` — move draw_sprite_partial + frogger_render here (writes to backbuffer), update input to GameInput, remove frogger_run | ✅ done |
| update-platform-h | Rewrite `src/platform.h` — 4-function contract (init, get_time, get_input; display_backbuffer static/inline in platform files) | ✅ done |
| update-main-x11 | Rewrite `src/main_x11.c` — GLX/OpenGL backbuffer, VSync, XkbSetDetectableAutoRepeat, GameInput, delta-time loop in main | ✅ done |
| update-main-raylib | Rewrite `src/main_raylib.c` — Texture2D backbuffer pipeline, GameInput, delta-time loop in main | ✅ done |
| update-build-x11 | Update `build_x11.sh` — clang, -lX11 -lxkbcommon -lGL -lGLX | ✅ done |
| update-build-raylib | Update `build_raylib.sh` — clang | ✅ done |

Both builds verified: zero warnings, zero errors.

## Phase 3 — Lessons

| ID | Task | Status |
|----|------|--------|
| write-lesson-01 | `lessons/lesson-01.md` — Window + GLX/OpenGL Context | ✅ done |
| write-lesson-02 | `lessons/lesson-02.md` — FroggerBackbuffer: Platform-Independent Canvas | ✅ done |
| write-lesson-03 | `lessons/lesson-03.md` — Color System: FROGGER_RGB + CONSOLE_PALETTE | ✅ done |
| write-lesson-04 | `lessons/lesson-04.md` — SpriteBank: .spr Binary Format + Pool Loader | ✅ done |
| write-lesson-05 | `lessons/lesson-05.md` — GameInput: GameButtonState + UPDATE_BUTTON + XkbSetDetectableAutoRepeat | ✅ done |
| write-lesson-06 | `lessons/lesson-06.md` — Data-Oriented Design: Lane Data | ✅ done |
| write-lesson-07 | `lessons/lesson-07.md` — lane_scroll: Pixel-Accurate Scrolling | ✅ done |
| write-lesson-08 | `lessons/lesson-08.md` — frogger_render: Sprites into the Backbuffer | ✅ done |
| write-lesson-09 | `lessons/lesson-09.md` — Delta-Time Game Loop + platform_get_time | ✅ done |
| write-lesson-10 | `lessons/lesson-10.md` — Platform Riding: River Logs Carry the Frog | ✅ done |
| write-lesson-11 | `lessons/lesson-11.md` — Danger Buffer + Collision Detection | ✅ done |
| write-lesson-12 | `lessons/lesson-12.md` — Death Flash, Win State + Bitmap Font HUD | ✅ done |

---
_Last updated: all 20 tasks complete — Phase 1 ✅ Phase 2 ✅ Phase 3 ✅_
