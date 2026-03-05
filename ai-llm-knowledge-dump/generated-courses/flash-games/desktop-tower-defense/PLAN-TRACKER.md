# Desktop Tower Defense — Plan Tracker

> **Research:** `ai-llm-knowledge-dump/prompt/research-desktop-tower-defense.md`
> **Course-builder:** `ai-llm-knowledge-dump/prompt/course-builder.md`
> **Course dir:** `ai-llm-knowledge-dump/generated-courses/flash-games/desktop-tower-defense/`

## Status legend

- `[ ]` pending
- `[~]` in progress
- `[x]` complete
- `[!]` blocked (reason noted inline)

---

## Phase 1 — Planning

- [x] `PLAN.md` — course outline, 21-lesson sequence, topic inventory, skill inventory table, concept dependency map, JS→C mapping, rule exceptions
- [x] `README.md` — overview, prerequisites, build instructions, lesson list, architecture diagram
- [x] `PLAN-TRACKER.md` — this file

---

## Phase 2 — Source files

### Build infrastructure
- [x] `course/build-dev.sh` — `--backend=x11|raylib`, `-r` (run after build), `-d` (debug with sanitizers); default backend = raylib; compiles cleanly

### Utils
- [x] `course/src/utils/backbuffer.h` — `Backbuffer` struct, `GAME_RGB`/`GAME_RGBA` macros (0xAARRGGBB), `pitch` field
- [x] `course/src/utils/draw-shapes.c/.h` — `draw_rect`, `draw_rect_blend`, `draw_line` (Bresenham), `draw_circle`, `draw_circle_outline`
- [x] `course/src/utils/draw-text.c/.h` — 8×8 bitmap font `FONT_8X8[128][8]`, `draw_char`, `draw_text` (with `scale`), `text_width`, BIT7-left convention
- [x] `course/src/utils/math.h` — `MIN`, `MAX`, `CLAMP`, `ABS` macros
- [x] `course/src/utils/audio.h` — `AudioOutputBuffer`, `SoundDef`, `SoundInstance` (all fields), `GameAudioState`, `ToneGenerator`, `MAX_SIMULTANEOUS_SOUNDS`

### Game layer
- [x] `course/src/platform.h` — 4 mandatory function signatures + audio extensions, `UPDATE_BUTTON` macro
- [x] `course/src/game.h` — all enums (`CellState`, `TowerType`, `TargetMode`, `CreepType`, `GamePhase`, `SfxId`), all structs (`Tower`, `Creep`, `Projectile`, `WaveDef`, `TowerDef`, `CreepDef`, `Particle`, `MouseState`, `GameState`), all constants, `ASSERT`/`DEBUG_TRAP` macros
- [x] `course/src/game.c` — all game logic: `game_init`, `game_update`, `game_render`, `change_phase`, BFS, tower/creep/projectile/particle systems, wave system, economy, all draw functions, UI panel
- [x] `course/src/audio.c` — `SOUND_DEFS[15]`, `game_play_sound_at`, `game_play_sound`, `game_audio_update`, `game_get_audio_samples` (PCM loop with phase accumulator, fade envelope, stereo panning)
- [x] `course/src/levels.c` — `WaveDef g_waves[40]` with designated initialisers; correct wave type ordering

### Platform backends
- [x] `course/src/main_raylib.c` — `platform_init`, delta-time loop, letterbox centering + R↔B channel swap, `platform_get_input`, `platform_audio_update` (`IsAudioStreamProcessed` gate, `AUDIO_CHUNK_SIZE = 2048`)
- [x] `course/src/main_x11.c` — `platform_init`, VSync + manual frame cap, mouse coord transform, ALSA setup with `snd_pcm_sw_params_set_start_threshold`

> ✅ **Build verified:** `./build-dev.sh --backend=raylib` compiles clean with `-Wall -Wextra -std=c99 -O0 -g -DDEBUG`

---

## Phase 3 — Lessons

### Foundation (no game logic yet)
- [x] `lessons/lesson-01-window-canvas-and-build-script.md`
- [x] `lessons/lesson-02-grid-system-and-cell-states.md`
- [x] `lessons/lesson-03-mouse-input-and-cell-hit-testing.md`

### Core algorithm
- [x] `lessons/lesson-04-bfs-distance-field.md`

### Tower + creep simulation
- [x] `lessons/lesson-05-tower-placement-and-legality.md`
- [x] `lessons/lesson-06-single-creep-and-flow-navigation.md`
- [x] `lessons/lesson-07-creep-pool-and-wave-spawning.md`

### Game structure
- [x] `lessons/lesson-08-state-machine-and-economy.md`

### Combat basics
- [x] `lessons/lesson-09-tower-barrel-and-targeting.md`
- [x] `lessons/lesson-10-tower-firing-and-damage.md`
- [x] `lessons/lesson-11-hp-bars-and-death-effects.md`

### Wave system
- [x] `lessons/lesson-12-wave-system-and-wave-table.md`

### Tower variety
- [x] `lessons/lesson-13-five-single-target-tower-types.md`
- [x] `lessons/lesson-14-frost-slow-and-swarm-aoe.md`
- [x] `lessons/lesson-15-all-five-targeting-modes.md`

### Creep variety
- [x] `lessons/lesson-16-fast-flying-and-armoured-creeps.md`
- [x] `lessons/lesson-17-spawn-creep-and-boss.md`

### Economy + UI polish
- [x] `lessons/lesson-18-send-wave-early-and-interest.md`
- [x] `lessons/lesson-19-side-panel-ui-and-hud.md`
- [x] `lessons/lesson-20-visual-polish-and-particles.md`

### Audio
- [x] `lessons/lesson-21-audio-system.md`

---

## Phase 4 — Meta

- [ ] `COURSE-BUILDER-IMPROVEMENTS.md` — document any patterns or issues discovered while building this course that should feed back into `course-builder.md`

---

## Notes and decisions

### Coordinate system
- **Y-down pixel coordinates** throughout — intentional exception to course-builder.md Y-up default
- Grid unit is `CELL_SIZE = 20 px` (no `PIXELS_PER_METER`); speeds in px/s; documented in Lesson 02

### Input model
- **Mouse only** — no keyboard gameplay input; `MouseState` replaces `GameButtonState` for core gameplay
- `left_pressed` / `left_released` are edge-detection fields, reset each frame in `platform_get_input`

### Audio tier
- **Custom PCM mixer** (`AudioStream` loop + `game_get_audio_samples`) — all DTD sounds are one-shot; no `PlaySound()` calls
- Background music: `ToneGenerator` ambient loop via `game_audio_update()` sequencer

### Build script
- `build-dev.sh` introduced in **Lesson 01** (not deferred) — DTD always requires two backends; manual `clang` command for both simultaneously would confuse students
- Default backend: **raylib** (matches course-builder convention)

### Key open questions (from research §22 — resolved conservatively)
- Frost duration: **2.0 s** (60 frames at 30 fps) — treat as tunable in source comments
- Bash stun radius: **2 cells** — treat as tunable
- Boss HP formula: `3.0 × base_hp × 1.10^(wave-1)` — approximate; tune for feel
- Kill gold: wiki approximations from research §8 — document as approximate

### Lesson dependencies
- Lessons are strictly sequential — each lesson assumes all prior lessons are complete
- Lesson 04 (BFS) must precede Lesson 05 (legality) must precede Lesson 06 (creep navigation)
- Lesson 08 (state machine) must precede Lesson 12 (wave transitions) and Lesson 21 (audio phase hooks)
- Lesson 10 (damage) must precede Lesson 14 (special damage types)

### Per-lesson checklist (from course-builder.md)
Each lesson file must pass before being written into the course output:
- [ ] "Where we are" callout with current file tree and one-sentence program description
- [ ] ≤ 3 new concepts listed in "What you'll learn"
- [ ] All new concepts have JS-equivalent analogy
- [ ] No truncated code blocks (`...`, `// existing code`)
- [ ] Both X11 and Raylib versions for every platform-specific section
- [ ] Every step has an "Expected output" statement
- [ ] Lesson compiles and runs to visible result
- [ ] No forward reference without anchor
- [ ] All numeric literals annotated
- [ ] Three exercises (beginner / intermediate / challenge)
- [ ] "What's next" closes the lesson
