# Lesson 05 — The Platform Contract

## Overview

| Item | Value |
|------|-------|
| **Backend** | Both |
| **New concepts** | `platform.h` as interface, `STRINGIFY`/`TOSTRING`/`TITLE` macros, `ARRAY_LEN` |
| **Observable outcome** | Window title shows "Platform Foundation v1.0"; code compiles with shared constants |
| **Files created** | `src/platform.h` |

---

## What you'll build

A single header that serves as the "interface file" between the platform layer (OS window, audio device) and the game layer (rendering, logic, audio generation).

---

## Background

### JS analogy

`platform.h` is like a TypeScript `interface` file — it defines the contract that every backend (Raylib, X11) must satisfy, and the types both sides share.  Game code `#include`s it to access `GameInput`, `Backbuffer`, etc.  Backend code `#include`s it to see the same definitions.

### What goes in `platform.h`?

| ✅ Belongs here | ❌ Does NOT belong here |
|----------------|------------------------|
| Shared constants (`GAME_W`, `TARGET_FPS`) | Game logic (update, render) |
| Platform-provided function prototypes | Backend-specific types (`snd_pcm_t*`) |
| Shared type definitions (`Backbuffer`, `AudioOutputBuffer`) | `platform_swap_input_buffers` (game logic, lives in `game/base.h`) |
| Utility macros (`ARRAY_LEN`, `STRINGIFY`) | |

### `STRINGIFY` / `TOSTRING` / `TITLE`

```c
#define STRINGIFY(x)  #x
#define TOSTRING(x)   STRINGIFY(x)
#define TITLE  GAME_TITLE " v" GAME_VERSION
```

`STRINGIFY(x)` turns `x` into the string literal `"x"` **before** macro expansion.  `TOSTRING(x)` forces expansion first, then stringifies.

```c
#define VERSION 42
STRINGIFY(VERSION)   →  "VERSION"   // didn't expand!
TOSTRING(VERSION)    →  "42"        // expanded first ✓
```

`TITLE` concatenates string literals at compile time — the linker never sees multiple strings.

### Platform-provided audio functions

`platform.h` declares four functions that every backend must implement:

```c
int  platform_audio_init(PlatformAudioConfig *cfg);
void platform_audio_shutdown(void);
int  platform_audio_get_samples_to_write(PlatformAudioConfig *cfg);
void platform_audio_write(AudioOutputBuffer *buf, int num_frames, PlatformAudioConfig *cfg);
```

The implementations live in `platforms/raylib/main.c` (L09/L11) and `platforms/x11/audio.c` (L09/L12).

---

## What to type

### `src/platform.h`

```c
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdlib.h>

#include "utils/backbuffer.h"
#include "utils/audio.h"       /* AudioOutputBuffer, PlatformAudioConfig */

/* Canvas dimensions (override with -DGAME_W=N -DGAME_H=N). */
#ifndef GAME_W
#  define GAME_W 800
#endif
#ifndef GAME_H
#  define GAME_H 600
#endif

/* Loop timing */
#ifndef TARGET_FPS
#  define TARGET_FPS 60
#endif

/* LESSON 05 — STRINGIFY / TOSTRING: two-level macro trick. */
#define STRINGIFY(x)  #x
#define TOSTRING(x)   STRINGIFY(x)

#ifndef GAME_TITLE
#  define GAME_TITLE  "Platform Foundation"
#endif
#ifndef GAME_VERSION
#  define GAME_VERSION  "1.0"
#endif
#define TITLE  GAME_TITLE " v" GAME_VERSION

/* Compile-time array length (safe — not sizeof a pointer). */
#define ARRAY_LEN(arr)  (sizeof(arr) / sizeof((arr)[0]))

/* Platform audio API — implemented per-backend. */
int  platform_audio_init(PlatformAudioConfig *cfg);
void platform_audio_shutdown(void);
int  platform_audio_get_samples_to_write(PlatformAudioConfig *cfg);
void platform_audio_write(AudioOutputBuffer *buf, int num_frames,
                          PlatformAudioConfig *cfg);

#endif
```

---

## Explanation

| Concept | Detail |
|---------|--------|
| `#ifndef GAME_W` guard | Lets each game course override canvas size at compile time: `-DGAME_W=320 -DGAME_H=240` |
| `TOSTRING(VERSION)` | Expands `VERSION` to `42` then wraps in `""` → `"42"`.  Used in `TITLE`. |
| `ARRAY_LEN` | `sizeof(arr) / sizeof(arr[0])` — the classic C idiom for stack-allocated array length |
| `platform_swap_input_buffers` **not** here | That function only swaps two `GameInput` pointers — no OS calls.  It lives in `game/base.h` beside `prepare_input_frame`. |

---

## Build and run

```sh
./build-dev.sh --backend=raylib -r
```

The window title should now show "Platform Foundation v1.0".

---

## Quiz

1. Why does `STRINGIFY(VERSION)` produce `"VERSION"` instead of `"42"`?  What does `TOSTRING` do differently?
2. Why is `platform_swap_input_buffers` in `game/base.h` and not in `platform.h`, even though backends call it?
3. `ARRAY_LEN(arr)` doesn't work if `arr` is a pointer.  Why?  How would you detect misuse?
