# Platform Foundation — A Reusable Game Platform Course

Build the complete platform and backend infrastructure that every game course in this series is built on — once.

---

## Why This Course Exists

Every game course in this series (Snake, Tetris, Asteroids, Frogger, Desktop Tower Defense, …) uses the same platform machinery:

- A CPU pixel buffer (backbuffer) rendered to the screen each frame
- Two platform backends: **X11/GLX** (Linux native) and **Raylib** (portable)
- A unified build system with backend selection
- A double-buffered input system
- A PCM audio mixer with procedural sound effects and chiptune music

Previously, every game course rebuilt this infrastructure from scratch in its first 4–6 lessons, producing nearly-identical platform content before any game-specific code was written.

**This course teaches all of it once.** Game courses then copy the output of this course and immediately start writing game-specific code.

---

## What You'll Build

A complete, working **blank-canvas game template** — a window that opens, renders a pixel buffer, accepts keyboard input, and plays procedural audio. No game logic is included.

At the end of this course, you have a `course/src/` directory that every game course in this series copies verbatim as its starting point.

---

## Prerequisites

**Required:**

- C syntax: variables, loops, functions, structs, pointers, typedefs, enums
- Memory allocation: `malloc`, `free`, understanding of heap vs stack
- Bit manipulation basics: shifting, masking, bitwise OR/AND (used in font rendering and pixel packing)
- Linux with `clang` installed
- One or both display backends (see Build section)

**Helpful but not required:**

- Comfort reading system API documentation (X11, ALSA, Raylib) — every API call introduced in this course is explained, but being able to look things up yourself speeds things up
- Basic mental model of how audio hardware works: a device pulls PCM samples at a fixed rate and the CPU must keep its write buffer ahead of the read cursor

No prior game programming or graphics experience required.

---

## Final File Tree (Stage B Layout)

```
platform-backend/course/
├── build-dev.sh                   ← --backend=x11|raylib, -r, -d/--debug, SOURCES var
└── src/
    ├── utils/
    │   ├── math.h                 ← CLAMP, MIN, MAX, ABS
    │   ├── backbuffer.h           ← Backbuffer struct + GAME_RGB/RGBA + color constants
    │   ├── draw-shapes.h/.c       ← draw_rect, draw_rect_blend
    │   ├── draw-text.h/.c         ← FONT_8X8[128][8], draw_char, draw_text
    │   └── audio.h                ← AudioOutputBuffer, SoundDef, SoundInstance,
    │                              │  GameAudioState, ToneGenerator, MusicSequencer,
    │                              │  PlatformAudioConfig, audio_write_sample
    ├── game/
    │   ├── base.h                 ← GameInput union template, UPDATE_BUTTON,
    │   │                          │  prepare_input_frame, platform_swap_input_buffers,
    │   │                          │  DEBUG_TRAP/ASSERT
    │   ├── demo.c                 ← demo_render() for visual demos L03–L08 (removed L14)
    │   └── audio_demo.c           ← PCM mixer, SoundDef table, MusicSequencer L09–L13 (removed L14)
    ├── platforms/
    │   ├── x11/
    │   │   ├── base.h             ← X11State struct + extern g_x11
    │   │   ├── base.c             ← X11State g_x11 = {0}
    │   │   ├── main.c             ← X11 + GLX + VSync + letterbox + input + FrameTiming
    │   │   └── audio.c            ← ALSA init/write/latency/shutdown
    │   └── raylib/
    │       └── main.c             ← Raylib + audio stream + letterbox + input
    └── platform.h                 ← shared contract: 4 functions + PlatformGameProps +
                                     STRINGIFY/TOSTRING/TITLE
```

---

## Lesson List

| #   | Title                                      | What you build                                        | What you see/hear                                          |
| --- | ------------------------------------------ | ----------------------------------------------------- | ---------------------------------------------------------- |
| 01  | Toolchain + Colored Window (Raylib)        | `build-dev.sh`, Raylib window                         | Solid-color window at 60 fps                               |
| 02  | X11/GLX Window                             | X11 backend matching Lesson 1                         | Same window via Linux X server                             |
| 03  | Backbuffer + GAME_RGB                      | Pixel buffer + GPU upload + `demo_render()`           | Color from `GAME_RGB` fills canvas                         |
| 04  | Drawing Primitives                         | `draw_rect`, `draw_rect_blend`                        | Colored rectangles; semi-transparent overlay               |
| 05  | Platform Contract                          | `platform.h`, `PlatformGameProps`                     | Same visual; code now layered correctly                    |
| 06  | Bitmap Font                                | `FONT_8X8`, `draw_text`                               | "PLATFORM READY" rendered to canvas                        |
| 07  | Double-Buffered Input                      | `GameInput`, `UPDATE_BUTTON`                          | Q quits; held key detected vs just-pressed                 |
| 08  | Delta-time Loop + Frame Timing + Letterbox | Game loop, `FrameTiming`, resizable letterbox window  | Stable 60 fps; FPS counter; canvas letterboxed on resize   |
| 09  | Audio Foundations + Minimal Output         | PCM mixer; minimal audio init on both backends        | **Audible** test tone on spacebar (both backends)          |
| 10  | Sound Effects                              | `SoundDef`, `SoundInstance`, panning, frequency slide | Frequency-sweep SFX; spatial stereo                        |
| 11  | Audio — Raylib Full Integration            | Raylib audio stream (production-quality)              | Robust Raylib audio; no startup click; pre-filled buffers  |
| 12  | Audio — X11/ALSA Full Integration          | ALSA `hw_params`, latency model, underrun recovery    | Robust X11 audio; latency-modelled; recovers from underrun |
| 13  | Music Sequencer                            | `ToneGenerator`, `MusicSequencer`, MIDI               | Background chiptune melody on both backends                |
| 14  | Platform Template Complete                 | Strip demo files; ASSERT audit; final clean build     | Template ready to copy into any game course                |

---

## Build Instructions

### Raylib backend

```bash
sudo apt install libraylib-dev   # Ubuntu 22.04+
cd course/
chmod +x build-dev.sh
./build-dev.sh --backend=raylib -r
```

### X11/GLX backend

```bash
sudo apt install libx11-dev libgl-dev libxkbcommon-dev libasound2-dev
cd course/
./build-dev.sh --backend=x11 -r
```

### Debug build (AddressSanitizer + UBSanitizer)

```bash
./build-dev.sh --backend=raylib -d -r
```

---

## Architecture Overview

```
Platform layer (I/O, timing, OS events, audio device)
        ↓ raw input (GameInput), delta_time
   game_update()   →   GameState (mutated in place)
        ↓
   game_render()   →   Backbuffer (written pixel by pixel)
        ↓
Platform layer (upload backbuffer to GPU, push PCM samples)
```

The game layer (`game/`) never calls X11 or Raylib directly. The platform layer (`platforms/*/`) never contains game logic. `platform.h` defines the exact boundary.

---

## How Game Courses Use This Template

After completing this course, when starting a new game course:

1. **Copy** `course/src/` to the new game's directory
2. **Delete** the two demo files — they are for this course only:
   - `game/demo.c` (visual demo renderer)
   - `game/audio_demo.c` (audio demo — PCM mixer driver)
3. **Adapt** the game-specific parts:
   - `platform.h` → update `TITLE` macro
   - `platforms/x11/main.c` and `platforms/raylib/main.c` → set `GAME_W`, `GAME_H`; update key bindings
   - `game/base.h` → rename `GameInput` button fields; set `BUTTON_COUNT`
4. **Add** game-specific files: `game/main.h`, `game/main.c`, `game/audio.c`, optionally `game/levels.c`

Game course lessons start immediately with game-specific content. When a lesson requires a platform change (e.g., a new key binding), it shows only the changed lines and references this course for the full explanation.

---

## Reference Implementations

- `games/snake/` — Stage B layout matching this course's output
- `games/tetris/` — Stage A layout (single-file backends; useful for simpler games)
- `ai-llm-knowledge-dump/prompt/course-builder.md` — full course-building guidelines
