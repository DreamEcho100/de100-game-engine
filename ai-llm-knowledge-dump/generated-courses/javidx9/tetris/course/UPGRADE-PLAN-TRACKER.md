# Upgrade Plan Tracker

Reference: [UPGRADE-PLAN.md](./UPGRADE-PLAN.md)

## Phase 1 — Backup

| ID | Task | Status |
|----|------|--------|
| backup-lessons | Create `lessons/old/` and move all 14 existing lesson files | ✅ done |

## Phase 2 — Source Files

| ID | Task | Status |
|----|------|--------|
| update-tetris-h | Rewrite `src/tetris.h` | ✅ done |
| update-tetris-c | Rewrite `src/tetris.c` | ✅ done |
| update-platform-h | Update `src/platform.h` | ✅ done |
| update-main-x11 | Rewrite `src/main_x11.c` | ✅ done |
| update-main-raylib | Rewrite `src/main_raylib.c` | ✅ done |
| update-build-x11 | Update `build_x11.sh` | ✅ done |

## Phase 3 — Lessons

| ID | Task | Status |
|----|------|--------|
| write-lesson-01 | `lessons/lesson-01.md` — Window + GLX/OpenGL | ✅ done |
| write-lesson-02 | `lessons/lesson-02.md` — TetrisBackbuffer | ✅ done |
| write-lesson-03 | `lessons/lesson-03.md` — Color System | ✅ done |
| write-lesson-04 | `lessons/lesson-04.md` — Typed Enums | ✅ done |
| write-lesson-05 | `lessons/lesson-05.md` — GameState + CurrentPiece | ✅ done |
| write-lesson-06 | `lessons/lesson-06.md` — Collision Detection | ✅ done |
| write-lesson-07 | `lessons/lesson-07.md` — Delta-Time + GameActionRepeat | ✅ done |
| write-lesson-08 | `lessons/lesson-08.md` — Drawing Primitives | ✅ done |
| write-lesson-09 | `lessons/lesson-09.md` — Bitmap Font | ✅ done |
| write-lesson-10 | `lessons/lesson-10.md` — tetris_render() | ✅ done |
| write-lesson-11 | `lessons/lesson-11.md` — Pro Input System | ✅ done |
| write-lesson-12 | `lessons/lesson-12.md` — Full HUD | ✅ done |
| write-lesson-13 | `lessons/lesson-13.md` — Game Over Overlay | ✅ done |
| write-lesson-14 | `lessons/lesson-14.md` — Final Integration | ✅ done |

---
_Last updated: all 19 tasks complete_
