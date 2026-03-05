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

## Phase 4 — Improvements (post-launch)

### Audio fixes
- [x] **Bug fix**: `game_audio_init()` was never called in `game_init()` → `master_volume` was 0 → complete silence
- [x] **Bug fix**: `game_audio_update()` was never called in `game_update()` → music drone never ramped up
- [x] **Missing SFX**: added `SFX_WAVE_START`, `SFX_INTEREST_EARN`, `SFX_EARLY_SEND` (was 15 SFX, now 18)
- [x] **Lesson 21 updated**: corrected SFX enum names (SFX_FROST_PULSE/SFX_BASH_HIT not SFX_TOWER_FIRE_FROST/BASH), added "Critical Wiring" pitfalls section, fixed SoundDef struct, fixed build commands, added coverage table

### Tower upgrade system
- [x] `TowerDef.upgrade_cost[2]` and `TowerDef.upgrade_damage_mult[2]` fields
- [x] `Tower.upgrade_level` (0–2) — affects damage and sell value
- [x] Upgrade button in side panel when tower selected; max-level shows "MAX"
- [x] `SFX_TOWER_UPGRADE` sound effect
- [x] `lessons/lesson-22-tower-upgrade-system.md`

### Start menu + screens
- [x] `GAME_PHASE_MOD_SELECT` phase added to `GamePhase` enum
- [x] Proper start screen with PLAY button (not just "click anywhere")
- [x] Mod selection screen: 5 mod buttons with descriptions
- [x] Improved game-over screen with current wave + restart click
- [x] Improved victory screen with click-to-restart
- [x] `lessons/lesson-23-menus-and-screens.md`

### Sprite / animation system
- [x] `src/sprites.h` — `SpriteId` enum (+ `SPR_MISSING`), `SpriteDef`, `SpriteAtlas`, `draw_sprite()` API
- [x] `src/sprites.c` — placeholder mode (colored rect + label); real atlas when PNG loaded
- [x] `course/assets/README.md` — free asset sources, atlas format, ImageMagick workflow
- [x] `build-dev.sh` updated: `sprites.c` added to SHARED_SRCS
- [x] `lessons/lesson-24-sprite-system.md`
- [x] **Update**: `SpriteLoadState` enum (IDLE/LOADING/READY/ERROR) and `error_msg` in `SpriteAtlas`
- [x] **Update**: `sprites_load_async()` + `sprites_is_ready()` with `pthread_create` background loading
- [x] **Update**: `SPR_MISSING` → magenta checkerboard with `"???"` text (Source Engine style)
- [x] **Update**: Error logging to `stderr` via `fprintf(stderr, "[SPRITES] ...")`
- [x] **Update**: `STB_IMAGE_STATIC` + pragma to avoid Raylib/stb_image symbol conflict; no-warning build
- [x] **Update**: `vendor/stb_image.h` downloaded (public domain); `STBI_ONLY_PNG` to minimize code
- [x] **Update**: `course/assets/sprites/` folder structure (towers/ creeps/ tiles/ effects/ README.md)
- [x] **Update**: `build-dev.sh` X11 build now links `-lpthread`
- [x] **Lesson 24 updated**: Section 10 "Production upgrades" covering SPR_MISSING, SpriteLoadState, async loading, error logging, STB_IMAGE_STATIC, folder structure

### Game mods (5 total)
- [x] `GameMod` enum in `game.h`: DEFAULT, TERRAIN, WEATHER, NIGHT, BOSS
- [x] `GameState.active_mod` field

#### MOD_TERRAIN
- [x] `CELL_WATER` / `CELL_MOUNTAIN` added to `CellState`
- [x] `terrain[]` array in GameState; BFS treats mountains as walls
- [x] Creeps slow on water; towers get +range on mountains; towers rejected on water
- [x] Terrain rendered: water = blue tint, mountain = dark brown
- [x] `lessons/lesson-26-terrain-mod.md`

#### MOD_WEATHER
- [x] `weather_phase` / `weather_timer` cycling in game_update
- [x] Rain: 0.8× creep speed; Wind: 1.3× creep speed
- [x] Visual: rain dots / wind streak particles; HUD weather status
- [x] `lessons/lesson-27-weather-mod.md`

#### MOD_NIGHT
- [x] Dark grid overlay via `draw_rect_blend`
- [x] Global 0.75× tower range; Dart 1.2× and Pellet 1.5× fire rate bonuses
- [x] Yellow glow ring on towers in night mode
- [x] `lessons/lesson-28-night-mod.md`

#### MOD_BOSS
- [x] Boss waves every 5 waves (not 10)
- [x] Boss: 5 s damage-immune shield on spawn; spawns children at 50% HP; 1.3× speed / 1.5× HP
- [x] Shield rendered as yellow `draw_circle_outline`
- [x] `lessons/lesson-29-boss-mod.md`

#### Architecture lesson
- [x] `lessons/lesson-25-mod-default-and-architecture.md`
- [x] **Update**: "Critical bug fix: init_mod_state()" appendix documents the sequencing bug and fix

### X11 platform bug fixes
- [x] **Bug fix**: `XSelectInput` was missing `KeyPressMask` → Escape key never worked
- [x] **Bug fix**: `wm_delete` atom was re-interned every frame (O(round-trip) per frame) → cached as `static Atom g_wm_delete`
- [x] **Bug fix**: `snd_pcm_writei` could block up to 46 ms when ALSA buffer full → added `snd_pcm_avail_update()` guard
- [x] **Bug fix**: `snd_pcm_drain()` in shutdown blocks until all queued audio plays → replaced with `snd_pcm_drop()`
- [x] `#include <X11/keysym.h>` added; `KeyPress` case with `XLookupKeysym` → `XK_Escape` → `g_should_quit = 1`
- [x] **Lesson 01 updated**: "X11 platform pitfalls" appendix covering all three bugs with code examples

### Terrain/weather mod initialization bug fix
- [x] **Bug fix**: terrain setup in `game_init()` was dead code (checked `active_mod` before it was set)
- [x] **Fix**: extracted `static void init_mod_state(GameState *s)` — called immediately after player picks mod
- [x] `game_init()` now only fills `terrain_slow[]` with baseline 1.0; `init_mod_state()` overwrites for MOD_TERRAIN
- [x] **Lesson 25 updated**: "Critical bug fix: init_mod_state()" appendix

---

## Phase 5 — Meta

- [ ] `COURSE-BUILDER-IMPROVEMENTS.md` — document any patterns or issues discovered while building this course that should feed back into `course-builder.md`

---

## Notes and decisions

### Coordinate system
- **Y-down pixel coordinates** throughout — intentional exception to course-builder.md Y-up default
- Grid unit is `CELL_SIZE = 20 px` (no `PIXELS_PER_METER`); speeds in px/s; documented in Lesson 02

### Input model
- **Mouse only** — no keyboard gameplay input; `MouseState` replaces `GameButtonState` for core gameplay
- `left_pressed` / `left_released` are edge-detection fields, reset each frame in `platform_get_input`

### Audio bugs discovered post-build
Two critical wiring bugs were found when testing audio:
1. **`game_audio_init()` not called in `game_init()`** — `memset` zeroed `master_volume` → no audio ever produced
2. **`game_audio_update()` not called in `game_update()`** — music drone volume never ramped up → no background music
Both fixed in `src/game.c`. Lesson 21 updated with a "⚠️ Critical Wiring" section.
Three missing SFX also added: `SFX_WAVE_START`, `SFX_INTEREST_EARN`, `SFX_EARLY_SEND` (total now 18).


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
