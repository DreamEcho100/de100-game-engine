# Adding New Features to the Platform Course

This document explains how to add new capabilities (input devices, new rendering primitives, audio effects, networking, etc.) to the Platform Foundation Course and how game courses inherit and adapt those additions.

---

## Guiding principles

1. **Platform course teaches the mechanism; game course demonstrates the use.**  A new feature added to the platform course should be a general, game-agnostic building block.  Specific behaviour goes in the game course.

2. **New concepts per lesson ≤ 3.**  If a feature requires more than three new concepts, split it across two lessons.

3. **Observable outcome rule.**  Every lesson must end with something the student can see, hear, or verify by running the binary.  A feature that only adds internal infrastructure (no visible/audible effect) must be paired with a small demo.

4. **Coverage check.**  Every new API surface introduced must appear in exactly one lesson's "New concepts" cell.  Run a text search across all `.md` files to verify there are no orphan concepts.

5. **Both backends must be updated simultaneously.**  If a feature requires OS-level changes, implement it in both `platforms/raylib/main.c` and `platforms/x11/main.c` in the same lesson, or split into explicit L_raylib / L_x11 sub-sections.

---

## Workflow for adding a feature

### Step 1 — Define the platform contract extension

Add the new API surface to `src/platform.h`.  New platform-facing declarations belong in `platform.h`.  Game-facing declarations that don't touch OS handles belong in `src/game/base.h`.

```c
// Example: adding a simple timer query
double platform_get_time_seconds(void);   // → platform.h
```

### Step 2 — Implement in both backends

Add the implementation to both `platforms/raylib/main.c` and `platforms/x11/main.c`.  Mark each block with the new lesson number:

```c
/* LESSON N — platform_get_time_seconds: returns seconds since program start. */
double platform_get_time_seconds(void) {
#ifdef BACKEND_RAYLIB
    return GetTime();
#elif BACKEND_X11
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
#endif
}
```

### Step 3 — Add to `src/utils/` if reusable

If the feature is backend-agnostic (math helpers, new drawing primitives, data structures), add it to the appropriate `src/utils/` file.  Keep OS-specific code in `src/platforms/`.

### Step 4 — Add a demo in `src/game/demo.c`

Update `demo_render` or `game_audio_update` to exercise the new feature visually/audibly.  This is the observable outcome for the new lesson.

### Step 5 — Write the lesson markdown

Create `course/lessons/NN-feature-name.md` following the template:

```markdown
# Lesson NN — Feature Name

## Overview
| Item         | Value |
|--------------|-------|
| Backend      | Both / Raylib / X11 |
| New concepts | concept1, concept2, concept3 (≤ 3 grouped) |
| Observable   | What the student sees/hears |
| Files created | ... |
| Files modified | ... |

## What you'll build
...
## Background
...
## What to type
...
## Explanation
...
## Build and run
...
## Quiz
...
```

### Step 6 — Update PLAN-TRACKER.md

Add a row to the "Files" table (lesson → file mapping) and add an acceptance criterion to the "Notes" column.

### Step 7 — Update the concept dependency map in PLAN.md

Insert the new lesson into the dependency graph so its prerequisites are explicit.

---

## Common feature patterns

### New rendering primitive (e.g., circles, lines)

1. Add `draw_circle` / `draw_line` to `src/utils/draw-shapes.h` and `.c`.
2. Demo in `demo_render` — a circle or line visible in the window.
3. No platform changes needed (rendering is pure CPU into the backbuffer).
4. Lesson: 1 new concept (`draw_circle` algorithm), ≤ 3 total.

### New input type (e.g., mouse, gamepad)

1. Add `MouseState` / `GamepadState` struct to `src/game/base.h`.
2. Fill it in both `platforms/raylib/main.c` (Raylib's `GetMouseX`, `IsGamepadButtonDown`) and `platforms/x11/main.c` (X11 `MotionNotify`, `XQueryPointer`).
3. Demo in `demo_render` — highlight a tile under the mouse cursor.
4. Two sub-lessons (Raylib first, then X11) or one lesson with two "What to type" sections.

### New audio waveform (e.g., sine, triangle, sawtooth)

1. Add a waveform function to `src/utils/audio.h`:
   ```c
   static inline float audio_wave_sine(float phase) { return sinf(phase * 6.283185f); }
   static inline float audio_wave_triangle(float phase) {
       return (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
   }
   ```
2. Update `game_get_audio_samples` in `audio_demo.c` to use the new waveform for one of the `SOUND_DEFS`.
3. Demo: press a new key to hear the sine tone (demo already has Space → square, add Tab → sine).
4. Note: sine waves are inherently click-free at amplitude 0 crossings; the `AUDIO_FADE_FRAMES` trick is still good practice but less critical.

### AUDIO_MASTER_VOLUME changes

If a new audio feature changes the number of simultaneously active voices, re-evaluate `AUDIO_MASTER_VOLUME`.  The rule: N voices at full `volume = 1.0f` + master volume should not clip (`N * 1.0 * AUDIO_MASTER_VOLUME ≤ 1.0`).  With `MAX_SOUNDS = 8` and `volume ≤ 0.5`, `AUDIO_MASTER_VOLUME = 0.6f` keeps the peak mix ≤ `8 * 0.5 * 0.6 = 2.4` — the `audio_write_sample` clamp handles occasional transient peaks.  If voices are louder, reduce master volume accordingly.

### New platform service (e.g., file I/O, network sockets)

1. Declare in `platform.h`:
   ```c
   int platform_read_file(const char *path, void **out_data, size_t *out_size);
   void platform_free_file(void *data);
   ```
2. Implement in both backends.  Raylib: `LoadFileData` / `UnloadFileData`.  X11: `fopen` / `malloc` / `fread` / `free`.
3. Demo: load a text file and display its first line on screen.

---

## Handling differences between platforms

### Timing API differences

| Concept | X11 (Linux) | Raylib | Windows | macOS |
|---------|-------------|--------|---------|-------|
| High-res clock | `clock_gettime(CLOCK_MONOTONIC)` | `GetTime()` | `QueryPerformanceCounter` | `mach_absolute_time` |
| ms sleep | `nanosleep` | `WaitTime` | `Sleep(ms)` | `usleep(µs)` |
| VSync | `glXSwapIntervalEXT` | `SetTargetFPS` | `wglSwapIntervalEXT` | Metal's `preferredFramesPerSecond` |

Wrap differences in `src/platforms/<name>/main.c`.  Do not leak platform-specific timing types into `platform.h`.

### Audio API differences

| Concept | X11 (ALSA) | Raylib | Windows (WASAPI) | macOS (Core Audio) |
|---------|-----------|--------|-----------------|-------------------|
| Init | `snd_pcm_open` + `hw_params` | `InitAudioDevice` | `CoCreateInstance(IMMDeviceEnumerator)` | `AudioQueueNewOutput` |
| Write | `snd_pcm_writei` | `UpdateAudioStream` | `IAudioRenderClient::GetBuffer` + `ReleaseBuffer` | `AudioQueueEnqueueBuffer` |
| Query available | `snd_pcm_avail_update` | `IsAudioStreamProcessed` | `IAudioClient::GetCurrentPadding` | callback-driven |
| Recovery | `snd_pcm_recover` | automatic | re-init | automatic |

All backends call the same `game_get_audio_samples` — only the "how many frames to write" and "where to write them" differ.

### Color/pixel format consistency

**The rule: all backends must use the same [R,G,B,A] byte order.**  Our `GAME_RGB` macro is defined once in `backbuffer.h` and produces [R,G,B,A] bytes on little-endian hardware.  Upload with:
- OpenGL (any platform): `GL_RGBA, GL_UNSIGNED_BYTE`
- Raylib: `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8`
- SDL 3: `SDL_PIXELFORMAT_RGBA32`
- SFML 3: `sf::Texture::update` with the raw `uint8_t*` pointer (SFML assumes RGBA)

If you ever see R and B channels visually swapped (red appears blue), the format constant is wrong — do NOT change the GAME_RGB macro; change the format constant passed to the GPU.

### Text rendering consistency

Text is rendered by `draw_char` / `draw_text` into the CPU backbuffer before it is uploaded to the GPU.  There are no platform-specific text paths — both backends get identical pixels.  If text looks wrong on one backend, the bug is in the backbuffer upload format (see "Color/pixel format" above), not in the font renderer.

**Font encoding reminder:** `FONT_8X8` uses LSB-first encoding.  The correct mask is `(1 << col)`, NOT `(0x80 >> col)`.  `(0x80 >> col)` mirrors every glyph horizontally.  This applies to all platforms.

---

## Adapting the platform course for a game course

When a game course builds on top of this platform course, it:

1. **Copies** the entire `course/src/` tree into the game project.
2. **Removes** `src/game/demo.c`, `src/game/demo.h`, `src/game/audio_demo.c`, `src/game/audio_demo.h`.
3. **Adds** `src/game/main.c` implementing `game_init`, `game_update`, `game_render`.
4. **Adds** `src/game/audio.c` implementing `game_get_audio_samples`, `game_audio_update`, and game-specific `SoundID` / `SOUND_DEFS`.
5. **Modifies** the two backend `main.c` files only where the game needs different window size, title, or audio config.

When a new platform feature is added to the platform course **after** a game course was created, the game course author:
- Reviews the lesson for the new feature.
- Cherry-picks the relevant changes into their game project's `platforms/` files.
- Does NOT need to copy `demo.c` changes (the demo is already replaced by the game).

### Adding a feature that only makes sense for a specific game

If a feature is too game-specific for the platform course (e.g., a sprite atlas loader for a specific art style), it belongs exclusively in the game course.  The platform course teaches primitives; game courses compose them.

---

## Version-tracking features across courses

When a platform course feature changes significantly (e.g., the audio fade-out threshold is tuned, the frame timing constant changes), update this document and bump the "Platform Course Version" in the README.  Game courses can then compare their copied `platform.h` version against the course version to know what to upgrade.

Add a `PLATFORM_VERSION` constant to `src/platform.h`:

```c
#define PLATFORM_COURSE_VERSION "1.0.0"   /* bump on any breaking API change */
```
