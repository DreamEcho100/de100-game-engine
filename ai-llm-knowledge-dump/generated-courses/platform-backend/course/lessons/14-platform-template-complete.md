# Lesson 14 — Platform Template Complete

## Overview

| Item | Value |
|------|-------|
| **Backend** | Both |
| **New concepts** | Template extraction workflow, `SOURCES` cleanup, how game courses adapt this codebase |
| **Observable outcome** | A clean build with `game/demo.c` and `game/audio_demo.c` removed; only a stub `game/main.c` compiles |
| **Files modified** | `build-dev.sh` (SOURCES), `src/game/demo.c` → removed, `src/game/audio_demo.c` → removed |
| **Files created** | `src/game/main.c` (stub) |

---

## What you'll build

Remove all course-demo scaffolding and produce the clean, reusable platform template that game courses will copy.  You'll see exactly which files are platform code and which are game code — the boundary the entire course has been building toward.

---

## Background

### What we're removing

Through lessons 01–13, two demo files drove the visible/audible output:

| File | Purpose | Status after L14 |
|------|---------|-----------------|
| `game/demo.c` | `demo_render()` — visual demo for L03–L08 | **Removed** |
| `game/audio_demo.c` | `SOUND_DEFS`, `DEMO_MUSIC`, mixer — audio demo for L09–L13 | **Removed** |

These files were scaffolding.  In a real game course, each of these is replaced by a `game/main.c` that implements the actual game logic.

### The final file structure

```
course/
├── build-dev.sh
└── src/
    ├── platform.h              ← platform contract (unchanged across game courses)
    ├── utils/
    │   ├── backbuffer.h        ← L03
    │   ├── math.h              ← L04
    │   ├── draw-shapes.h/.c    ← L04
    │   ├── draw-text.h/.c      ← L06
    │   └── audio.h             ← L09
    ├── game/
    │   ├── base.h              ← L07 (input types + platform_swap_input_buffers)
    │   └── main.c              ← NEW (stub — game course fills this in)
    └── platforms/
        ├── raylib/
        │   └── main.c          ← L01–L14 (calls game_update + game_render)
        └── x11/
            ├── base.h/.c       ← L02
            ├── audio.h/.c      ← L09/L12
            └── main.c          ← L02–L14 (calls game_update + game_render)
```

### The platform/game boundary

```
┌─────────────────────────────────────────────────────┐
│ PLATFORM LAYER (this course)                        │
│  raylib/main.c OR x11/main.c                        │
│  • Creates window + GL context                      │
│  • Polls OS input → GameInput                       │
│  • Manages audio device                             │
│  • Calls game_update + game_get_audio_samples       │
│  • Swaps backbuffer to screen                       │
└─────────────────────────┬───────────────────────────┘
                          │ calls
                          ▼
┌─────────────────────────────────────────────────────┐
│ GAME LAYER (game courses fill this in)              │
│  game/main.c                                        │
│  • game_update(GameState*, GameInput*, Backbuffer*) │
│  • game_get_audio_samples(GameAudioState*, buf, n)  │
│  • game_audio_update(GameAudioState*, dt_ms)        │
└─────────────────────────────────────────────────────┘
```

### How a game course uses this template

1. Copy the entire `course/` directory into the game course repository
2. Replace `game/main.c` stub with the game's actual `game_update` + `game_get_audio_samples`
3. Extend `SoundID` enum and `SOUND_DEFS` for game-specific sounds
4. Adjust `GAME_W`, `GAME_H`, `TITLE`, `TARGET_FPS` in `platform.h` as needed
5. The platform backends (raylib/x11) need **zero changes** for most games

Game course lessons then focus on game mechanics, mentioning "copy from the platform course and change X" where adaptations are needed.

---

## What to type

### Step 1 — Remove demo files from `SOURCES` in `build-dev.sh`

**Before** (remove these two lines):
```sh
SOURCES="$SRC/game/demo.c \
         $SRC/game/audio_demo.c \
         ..."
```

**After**:
```sh
SOURCES="$SRC/utils/draw-shapes.c \
         $SRC/utils/draw-text.c \
         $SRC/game/main.c \
         $PLATFORM_SOURCES"
```

### Step 2 — Create `src/game/main.c` stub

```c
/* game/main.c — STUB for game courses.
 *
 * Game courses replace the contents of this file with their game logic.
 * The function signatures here are the platform contract from platform.h.
 *
 * Symbols that the platform layer calls:
 *   game_update()             — called once per game frame
 *   game_get_audio_samples()  — called by the audio drain loop
 *   game_audio_update()       — called once per game frame
 */

#include <string.h>
#include "utils/backbuffer.h"
#include "utils/audio.h"
#include "game/base.h"

void game_update(GameAudioState *audio, Backbuffer *bb,
                 GameInput *input, float dt_ms) {
    /* TODO: replace with game logic */
    (void)audio; (void)bb; (void)input; (void)dt_ms;
    /* Clear to black */
    memset(bb->pixels, 0, (size_t)(bb->width * bb->height) * bb->bytes_per_pixel);
}

void game_get_audio_samples(GameAudioState *audio, AudioOutputBuffer *buf,
                             int num_frames) {
    /* TODO: replace with PCM synthesis */
    (void)audio;
    int frames = (num_frames < buf->max_sample_count)
                 ? num_frames : buf->max_sample_count;
    memset(buf->samples, 0, (size_t)frames * AUDIO_BYTES_PER_FRAME);
}

void game_audio_update(GameAudioState *audio, float dt_ms) {
    /* TODO: replace with audio state advancement */
    (void)audio; (void)dt_ms;
}
```

### Step 3 — Update platform backends to call `game_update`

Replace the `demo_render(...)` call in both `platforms/raylib/main.c` and `platforms/x11/main.c`:

```c
// Before (demo scaffolding):
demo_render(&bb, curr_input, fps);

// After (real game boundary):
game_update(&audio_state, &bb, curr_input, delta_ms);
```

Remove `#include "game/demo.h"` and `#include "game/audio_demo.h"`.  Add any declarations needed for `game_update`, `game_get_audio_samples`, `game_audio_update` from `game/main.c`.

### Step 4 — Verify clean build

```sh
./build-dev.sh --backend=raylib -r
./build-dev.sh --backend=x11   -r
./build-dev.sh --backend=raylib -d
./build-dev.sh --backend=x11   -d
```

Expected: window opens, shows black canvas (the stub clears to black), no audio (stub returns silence).  No warnings with `-Wall -Wextra`.

---

## Explanation

| What changed | Why |
|---|---|
| `game/demo.c` removed | Demo rendering is game-layer code; templates don't include game-specific demos |
| `game/audio_demo.c` removed | Same — SOUND_DEFS and DEMO_MUSIC are game-specific |
| `game/main.c` stub added | This is the seam.  Game courses implement these 3 functions |
| `SOURCES` updated | `build-dev.sh` now compiles only platform + shared utils + game/main.c |
| Backend includes updated | Remove demo.h/audio_demo.h; game/main.c functions are declared in-line or via forward declarations |

### What does NOT change

Everything in `utils/`, `game/base.h`, `platform.h`, and both platform backends is unchanged.  This is the entire point of the platform course — those files are stable and reusable.

---

## Build and run

```sh
./build-dev.sh --backend=raylib -r
# → black window (stub game_update clears to black)
./build-dev.sh --backend=x11 -r
# → same
```

The platform is complete.  It builds cleanly, opens a window, handles input (Q to quit), and drives the game loop at 60 FPS with audio infrastructure ready.  A game course adds the game logic in `game/main.c` and can immediately focus on gameplay.

---

## Course complete — what you now have

You have built a complete, portable C game platform from scratch:

| Component | What you built |
|-----------|---------------|
| **Build system** | `build-dev.sh` with `--backend=`, `-r`, `-d` flags |
| **Rendering** | `Backbuffer` (CPU pixel buffer), `draw_rect`, `draw_text`, letterbox scaling |
| **Input** | Double-buffered `GameInput` with `UPDATE_BUTTON`, `button_just_pressed` |
| **Frame timing** | DE100-style `FrameTiming` (X11); `SetTargetFPS` (Raylib); `FrameStats` debug stats |
| **Audio** | `ToneGenerator` voice pool, `SoundDef` table, `MusicSequencer`, ALSA + Raylib backends |
| **Platform contract** | `platform.h` header — game courses include this and call only these APIs |

**This course is the prerequisite for all game courses.**  Each game course will:
1. Copy this platform as its starting point
2. Fill in `game/main.c` with game logic
3. Adapt `GAME_W`, `GAME_H`, `TITLE`, sounds as needed
4. Lesson by lesson, build the game on top of this foundation

---

## Quiz

1. List the 3 functions that `game/main.c` must implement for the platform layer to call.
2. A game course wants 1280×720 resolution at 30 FPS.  Which two files need to change?
3. Why does the platform course not need to change `platforms/raylib/main.c` or `platforms/x11/main.c` when porting to a new game?
