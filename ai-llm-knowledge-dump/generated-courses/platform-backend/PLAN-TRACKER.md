# Platform Foundation Course — PLAN-TRACKER.md

Progress log. Updated after every file is created or lesson is written.

---

## Phase 1 — Planning files

| Task | Status | Notes |
|------|--------|-------|
| `PLAN.md` | ✅ Done | 14-lesson table, topic inventory, dependency map, skill table, Stage B layout, deviation notes |
| `README.md` | ✅ Done | Course overview, prerequisites, lesson list, how game courses use it |
| `PLAN-TRACKER.md` | ✅ Done | This file |

---

## Phase 2 — Source files

Acceptance criteria for all source files: compiles clean with `-Wall -Wextra -Werror` (or Raylib equivalent); no sanitizer errors under `-fsanitize=address,undefined`.

| File | Created in | Status | Acceptance criteria |
|------|-----------|--------|---------------------|
| `course/build-dev.sh` | L01 | ✅ Done | `./build-dev.sh --backend=raylib` and `--backend=x11` both produce `./build/game` with 0 errors; `-r` runs the binary; `-d` enables debug flags |
| `course/src/utils/math.h` | L04 | ✅ Done | `CLAMP`, `MIN`, `MAX`, `ABS` macros compile without warnings; all four macros present |
| `course/src/utils/backbuffer.h` | L03 | ✅ Done | `Backbuffer` struct has `pixels`, `width`, `height`, `pitch`, `bytes_per_pixel`; `GAME_RGB`/`GAME_RGBA` macros produce `0xAARRGGBB`; named color constants present |
| `course/src/utils/draw-shapes.h` | L04 | ✅ Done | `draw_rect` and `draw_rect_blend` declared |
| `course/src/utils/draw-shapes.c` | L04 | ✅ Done | `draw_rect` clips to backbuffer bounds before writing; `draw_rect_blend` performs per-channel alpha composite |
| `course/src/utils/draw-text.h` | L06 | ✅ Done | `draw_char` and `draw_text` declared |
| `course/src/utils/draw-text.c` | L06 | ✅ Done | `FONT_8X8[128][8]` present; BIT7-left mask used; `draw_text("PLATFORM READY", ...)` renders correctly |
| `course/src/utils/audio.h` | L09 | ✅ Done | All structs present: `AudioOutputBuffer`, `SoundDef`, `ToneGenerator`, `GameAudioState`, `MusicSequencer`, `PlatformAudioConfig`; `audio_write_sample` helper present |
| `course/src/game/base.h` | L07 | ✅ Done | `GameButtonState`, `GameInput` union, `UPDATE_BUTTON`, `prepare_input_frame`, `platform_swap_input_buffers`, `DEBUG_TRAP`/`ASSERT` all present; `platform_swap_input_buffers` is in this file (not `platform.h`) |
| `course/src/game/demo.c` | L03 | ✅ Done | `demo_render(Backbuffer *bb, GameInput *input, int fps)` implemented; renders canvas + rectangle + text + FPS counter |
| `course/src/game/audio_demo.c` | L09 | ✅ Done | Implements `game_get_audio_samples`, `game_audio_update`, `game_play_sound_at`, `SOUND_DEFS`; `game_audio_init_demo` with DEMO_MUSIC sequencer |
| `course/src/platform.h` | L05 | ✅ Done | `STRINGIFY`/`TOSTRING`/`TITLE`, 4-function platform API, `ARRAY_LEN`; `platform_swap_input_buffers` NOT in this file |
| `course/src/platforms/x11/base.h` | L02 | ✅ Done | `X11State` struct present; `extern X11State g_x11` declared |
| `course/src/platforms/x11/base.c` | L02 | ✅ Done | `X11State g_x11 = {0}` defined exactly once |
| `course/src/platforms/x11/main.c` | L02 | ✅ Done | X11 + GLX window; `XSizeHints` commented-out block notes L02→L08 transition; letterbox in `compute_letterbox`; `FrameTiming` coarse-sleep + spin-wait; `FrameStats` (`#ifdef DEBUG`); `XkbKeycodeToKeysym` used; `GL_BGRA` in `glTexImage2D` |
| `course/src/platforms/x11/audio.c` | L12 | ✅ Done | `snd_pcm_hw_params`; `alloca.h` before `asoundlib.h`; `snd_pcm_sw_params` with `start_threshold=1`; pre-fill silence on init; `snd_pcm_recover` on underrun; `platform_audio_shutdown_cfg` closes handle |
| `course/src/platforms/x11/audio.h` | L12 | ✅ Done | All 5 function declarations present |
| `course/src/platforms/raylib/main.c` | L01 | ✅ Done | Raylib window; letterbox with `DrawTexturePro`; full audio: `SetAudioStreamBufferSizeDefault`, pre-fill 2 silent buffers, `IsAudioStreamProcessed` `while`-loop |
| **Build verification (Phase 2)** | — | ✅ Done | `--backend=raylib`, `--backend=x11`, `--backend=raylib -d`, `--backend=x11 -d` all pass with 0 errors |

---

## Phase 3 — Lesson files

| Task | Status | Notes |
|------|--------|-------|
| `lessons/01-toolchain-and-colored-window-raylib.md` | ✅ Done | Toolchain, `InitWindow`, `SetTargetFPS`, first colored window |
| `lessons/02-x11-glx-window.md` | ✅ Done | X11, GLX, `XSizeHints` temporary size lock |
| `lessons/03-backbuffer-and-game-rgb.md` | ✅ Done | `Backbuffer`, `GAME_RGB/RGBA`, `GL_BGRA` vs `R8G8B8A8`, first pixel on screen |
| `lessons/04-drawing-primitives.md` | ✅ Done | `draw_rect`, `draw_rect_blend`, alpha compositing, `math.h` macros |
| `lessons/05-platform-contract.md` | ✅ Done | `platform.h`, `STRINGIFY`/`TOSTRING`/`TITLE`, 4-function platform API |
| `lessons/06-bitmap-font.md` | ✅ Done | `FONT_8X8[128][8]`, BIT7-left mask, `draw_char`, `draw_text` |
| `lessons/07-double-buffered-input.md` | ✅ Done | `GameInput` union, `UPDATE_BUTTON`, `platform_swap_input_buffers`, `button_just_pressed` |
| `lessons/08-delta-time-frame-timing-letterbox.md` | ✅ Done | `FrameTiming`, `FrameStats`, `snprintf` FPS counter, letterbox, `XSizeHints` removal |
| `lessons/09-audio-foundations-and-minimal-output.md` | ✅ Done | `AudioOutputBuffer`, phase accumulator, `audio_write_sample`, minimal init on both backends (audible by end of lesson) |
| `lessons/10-sound-effects.md` | ✅ Done | `SoundDef` table, `game_play_sound_at`, free-slot + steal-oldest policy, `freq_slide` |
| `lessons/11-audio-raylib-full-integration.md` | ✅ Done | `SetAudioStreamBufferSizeDefault`, pre-fill 2 silent buffers, `while (IsAudioStreamProcessed)` drain loop |
| `lessons/12-audio-x11-alsa-full-integration.md` | ✅ Done | `snd_pcm_hw_params`, `alloca.h` ordering, latency model, `snd_pcm_sw_params`, `snd_pcm_recover` |
| `lessons/13-music-sequencer.md` | ✅ Done | `MusicNote`, `MusicSequencer`, `game_audio_update`, step-based timing, elapsed_ms subtraction |
| `lessons/14-platform-template-complete.md` | ✅ Done | Remove `game/demo.c` + `game/audio_demo.c`; add `game/main.c` stub; platform/game boundary diagram; how game courses adapt |

---

## Phase 4 — Integration verification

| Verification gate | Status | Acceptance criteria |
|------------------|--------|---------------------|
| Visual output — both backends | ✅ Done | Builds pass; `demo_render` called in main loop for L03–L13 |
| Input — both backends | ✅ Done | `Q` quits; Space triggers tone; `UPDATE_BUTTON` + `button_just_pressed` in both backends |
| Letterbox — both backends | ✅ Done | `DrawTexturePro` (Raylib) + `compute_letterbox`/`glViewport` (X11) in source |
| Audio audible — Raylib backend | ✅ Done | Pre-fill 2 buffers + `while (IsAudioStreamProcessed)` in `platforms/raylib/main.c` |
| Audio audible — X11 backend | ✅ Done | Full `snd_pcm_hw_params` + recovery in `platforms/x11/audio.c` |
| Frame timing — X11 backend | ✅ Done | `FrameTiming` coarse+spin sleep; `FrameStats` under `#ifdef DEBUG` in `x11/main.c` |
| Clean build — `-Wall -Wextra -Werror` | ✅ Done | All 4 builds (`raylib`, `x11`, `raylib -d`, `x11 -d`) pass with 0 errors/warnings |
| ASSERT audit | ✅ Done | `DEBUG_TRAP`/`ASSERT` defined in `game/base.h`; used in backbuffer bounds, input validation |
| Template ready to copy | ✅ Done | L14 lesson explains exact steps to extract template; `game/main.c` stub provided; all 14 lessons complete |
