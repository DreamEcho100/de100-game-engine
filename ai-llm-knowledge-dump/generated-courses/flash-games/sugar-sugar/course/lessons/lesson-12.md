# Lesson 12 — Audio: Platform Sound API

**By the end of this lesson you will have:**
(Raylib backend only.) A short chime plays when a cup fills to 100%. A fanfare plays when all cups are filled and the level completes. Ambient music loops in the background during gameplay. The X11 backend compiles and runs silently — audio is purely additive.

---

## Step 1 — The audio extension to `platform.h`

`platform.h` already declares three audio functions:

```c
// platform.h — already written
void platform_play_sound(int sound_id);
void platform_play_music(int music_id);
void platform_stop_music(void);
```

These are optional. A platform backend that doesn't support audio provides empty function bodies:

```c
// main_x11.c — stub implementations
void platform_play_sound(int sound_id) { (void)sound_id; }
void platform_play_music(int music_id) { (void)music_id; }
void platform_stop_music(void)         { }
```

`(void)sound_id` suppresses the "unused parameter" compiler warning. The function body is empty. The binary compiles. No audio plays. The game still runs correctly.

JS analogy: this is like providing a no-op implementation of an interface:
```js
const AudioBackend = {
  playSound: (id) => {},   // stub — no-op
  playMusic: (id) => {},
  stopMusic: ()  => {},
};
```

---

## Step 2 — `SOUND_ID` and `MUSIC_ID` enums in `sounds.h`

```c
// sounds.h — already written
typedef enum {
    SOUND_CUP_FILL,       // short chime when one cup reaches 100%
    SOUND_LEVEL_COMPLETE, // fanfare when all cups are filled
    SOUND_COUNT
} SOUND_ID;

typedef enum {
    MUSIC_GAMEPLAY,       // ambient loop during active levels
    MUSIC_COUNT
} MUSIC_ID;
```

The game calls `platform_play_sound(SOUND_CUP_FILL)`. It passes an `int` (the enum value) to the platform. The platform maps that integer to a specific audio file. The game never knows the filename, the sample rate, or the audio library.

Adding a new sound: add a constant here and add a case in the Raylib backend. Zero changes to `game.c`.

---

## Step 3 — Where to call `platform_play_sound(SOUND_CUP_FILL)`

A cup fills the moment `cup->collected` reaches `cup->required_count`. That happens inside `update_grains()` in `game.c`:

```c
// game.c — inside update_grains(), cup AABB check
if (color_ok && cup->collected < cup->required_count) {
    cup->collected++;
    p->active[i] = 0;

    // play chime exactly when the cup just reached 100%
    if (cup->collected == cup->required_count) {
        platform_play_sound(SOUND_CUP_FILL);
    }
}
```

The `if (cup->collected == cup->required_count)` guard fires exactly once per cup — on the frame the last grain lands. Not on every grain, not on every frame the cup is full.

---

## Step 4 — Where to call level-complete and gameplay music

`platform_play_sound(SOUND_LEVEL_COMPLETE)` fires when `check_win()` first returns true:

```c
// game.c — update_playing()
if (check_win(state)) {
    platform_play_sound(SOUND_LEVEL_COMPLETE);
    platform_stop_music();              // silence the ambient loop
    change_phase(state, PHASE_LEVEL_COMPLETE);
}
```

`platform_play_music(MUSIC_GAMEPLAY)` fires once when a level starts:

```c
// game.c — change_phase()
static void change_phase(GameState *state, GAME_PHASE next) {
    state->phase       = next;
    state->phase_timer = 0.0f;

    if (next == PHASE_PLAYING || next == PHASE_FREEPLAY) {
        platform_play_music(MUSIC_GAMEPLAY);
    }
    if (next == PHASE_TITLE) {
        platform_stop_music();
    }
}
```

Music starts when entering `PHASE_PLAYING`. It stops when winning (above) or when going back to the title. Restarting a level calls `change_phase(state, PHASE_PLAYING)` which calls `platform_play_music()` again — Raylib restarts the stream from the beginning.

---

## Step 5 — Why audio lives in `platform`, not `game`

The game logic knows *when* to play a sound (cup fills, level ends). It does not know *how* — that depends on the OS, the audio library, the hardware.

```
game.c                    platform.h contract         main_raylib.c
──────────────────────    ────────────────────────    ───────────────────────
cup fills         ──────► platform_play_sound()  ──►  PlaySound(g_sounds[id])
level completes   ──────► platform_play_sound()  ──►  PlaySound(g_sounds[id])
level starts      ──────► platform_play_music()  ──►  PlayMusicStream(g_music)
                          platform_stop_music()  ──►  StopMusicStream(g_music)
```

JS analogy: game code calls `audioContext.play(id)`. Whether that means WebAudio API, HTML5 `<audio>`, or a mock in tests is the platform's concern.

If you port Sugar Sugar to SDL3 or WebAssembly, you write a new `main_sdl3.c` or `main_wasm.c` with the four mandatory functions plus the three audio functions. `game.c` is unchanged.

---

## Step 6 — Raylib audio implementation

Open `src/main_raylib.c`. The audio setup:

```c
// main_raylib.c — already written
#include "raylib.h"
#include "sounds.h"

static Sound g_sounds[SOUND_COUNT];
static Music g_music[MUSIC_COUNT];

void platform_init(const char *title, int width, int height) {
    InitWindow(width, height, title);
    SetTargetFPS(60);

    InitAudioDevice();  // must call before loading any audio

    g_sounds[SOUND_CUP_FILL]       = LoadSound("assets/cup_fill.wav");
    g_sounds[SOUND_LEVEL_COMPLETE]  = LoadSound("assets/level_complete.wav");
    g_music[MUSIC_GAMEPLAY]         = LoadMusicStream("assets/gameplay.ogg");
}
```

Sound effects vs. music streams:
- `Sound` = entire audio file loaded into memory. Plays with `PlaySound()`. Good for short clips (< 2 s).
- `Music` = streamed from disk frame-by-frame. Plays with `PlayMusicStream()` + `UpdateMusicStream()` every frame. Good for long loops.

```c
// main_raylib.c — audio function implementations
void platform_play_sound(int sound_id) {
    if (sound_id >= 0 && sound_id < SOUND_COUNT)
        PlaySound(g_sounds[sound_id]);
}

void platform_play_music(int music_id) {
    if (music_id >= 0 && music_id < MUSIC_COUNT) {
        StopMusicStream(g_music[music_id]);  // reset to start
        PlayMusicStream(g_music[music_id]);
    }
}

void platform_stop_music(void) {
    for (int i = 0; i < MUSIC_COUNT; i++)
        StopMusicStream(g_music[i]);
}
```

`UpdateMusicStream()` must be called every frame inside the game loop:

```c
// main_raylib.c — game loop
while (!WindowShouldClose() && !state.should_quit) {
    // ... input, update, render ...
    for (int i = 0; i < MUSIC_COUNT; i++)
        UpdateMusicStream(g_music[i]); // refills the audio buffer
}
```

Without `UpdateMusicStream()` the music buffer runs dry and playback stops after a few seconds.

Cleanup on exit:

```c
for (int i = 0; i < SOUND_COUNT; i++) UnloadSound(g_sounds[i]);
for (int i = 0; i < MUSIC_COUNT; i++) UnloadMusicStream(g_music[i]);
CloseAudioDevice();
```

---

## Step 7 — Free CC0 sounds: freesound.org

1. Go to [freesound.org](https://freesound.org).
2. Search for "chime" or "bell ding". Filter by **CC0** license (Creative Commons Zero — no attribution required).
3. Download as `.wav` (not `.mp3` — some builds of Raylib don't include an MP3 decoder).
4. Place in `course/assets/cup_fill.wav`.

For the ambient loop, search "ambient music loop CC0", download as `.ogg` (Ogg Vorbis — universal Raylib support), place in `course/assets/gameplay.ogg`.

Verify file names match what `platform_init()` loads.

---

## Step 8 — Embedding sound as a C array with `xxd`

If you want a single self-contained binary with no `assets/` folder:

```bash
xxd -i cup_fill.wav > cup_fill_data.h
```

This generates a file like:

```c
// cup_fill_data.h — generated by xxd -i cup_fill.wav
unsigned char cup_fill_wav[] = {
  0x52, 0x49, 0x46, 0x46, 0xac, 0x58, 0x00, 0x00,  // RIFF....
  0x57, 0x41, 0x56, 0x45, 0x66, 0x6d, 0x74, 0x20,  // WAVEfmt
  // ... thousands more bytes ...
};
unsigned int cup_fill_wav_len = 22700;
```

Load it in Raylib using `LoadSoundFromMemory()`:

```c
#include "cup_fill_data.h"

// in platform_init():
Wave w = LoadWaveFromMemory(".wav", cup_fill_wav, (int)cup_fill_wav_len);
g_sounds[SOUND_CUP_FILL] = LoadSoundFromWave(w);
UnloadWave(w); // the Sound has its own copy now; we can free the Wave
```

The `.wav` file bytes are compiled directly into the binary. No separate `assets/` directory is needed at runtime.

---

## Step 9 — X11 stubs compile and run silently

The X11 backend in `main_x11.c` already contains the three stub implementations shown in Step 1. The key point: **all three functions must exist** even if empty, because `game.c` calls them and the linker must resolve those symbols.

```
Linker's view:
  game.o        → calls platform_play_sound (unresolved)
  main_x11.o    → defines platform_play_sound = empty function (resolved ✓)
```

If you remove the stub and build:
```
undefined reference to `platform_play_sound'
```

The stubs make audio an additive feature — add it when you have a backend that supports it, leave the stubs when you don't.

---

## Build & Run (X11 — no audio)

```bash
clang -Wall -Wextra -O2 -o sugar \
    src/main_x11.c src/game.c src/levels.c -lX11 -lm
./sugar
```

**What you should see:** full game, no audio. No errors, no warnings about missing audio.

## Build & Run (Raylib — with audio)

```bash
# Requires raylib installed; see build_raylib.sh
bash build_raylib.sh
./sugar_raylib
```

**What you should see:** same game, plus:
- A chime plays the moment each cup hits 100%.
- A fanfare plays on level complete; ambient music stops.
- Ambient music loops continuously during gameplay.

---

## Key Concepts

- `platform_play_sound(int)` / `platform_play_music(int)` / `platform_stop_music()`: the three audio functions in `platform.h`. The game calls them; the platform implements them.
- `SOUND_ID` / `MUSIC_ID` enums in `sounds.h`: typed constants that map sound names to integer IDs. The platform converts IDs to file loads.
- **Stub = empty function body**: `void platform_play_sound(int id) { (void)id; }` compiles and links. Audio is additive — the game doesn't break without it.
- `Sound` vs. `Music` in Raylib: `Sound` is fully loaded into memory (short clips). `Music` is streamed; call `UpdateMusicStream()` every frame.
- `xxd -i file.wav > data.h`: converts a binary file to a C array. Embed audio directly in the binary with `LoadSoundFromWave(LoadWaveFromMemory(...))`.
- Audio belongs in `platform`, not `game`: the game knows *when* to play; the platform knows *how*. Same separation-of-concerns principle as rendering.
- Play-on-transition: call `platform_play_music()` inside `change_phase()` when entering `PHASE_PLAYING`. Stop on win and on title return.

---

## Exercise

Add a second sound, `SOUND_GRAIN_LAND`, that plays a soft tick when any grain is deactivated inside a cup (not just on the cup-full event). In `sounds.h`, add `SOUND_GRAIN_LAND` before `SOUND_COUNT`. In `main_x11.c`, add a stub. In `main_raylib.c`, load a short tick sound (`assets/grain_land.wav`) at index `SOUND_GRAIN_LAND`. In `game.c`, call `platform_play_sound(SOUND_GRAIN_LAND)` every time `p->active[i] = 0` inside the cup collection block. Build and run — you should hear a rapid ticking as grains fall into cups.
