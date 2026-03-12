# Adding a New Platform Backend

This document explains how to add a new platform backend (e.g., SFML 3, SDL 3, macOS/Cocoa, Windows/Win32) to the Platform Foundation Course and how game courses inherit the new backend.

---

## Architecture overview

Every backend must implement the **platform contract** declared in `src/platform.h`:

```
platform_init()         → open window, init OpenGL/renderer, set up input
platform_audio_init()   → open audio device, allocate AudioOutputBuffer
platform_audio_get_samples_to_write() → non-blocking drain query
platform_audio_write()  → write AudioOutputBuffer to device
platform_run_loop()     → enter main loop (or is called by main loop)
platform_shutdown()     → release all OS resources
```

And it must satisfy the **game contract**:

```
game_get_audio_samples(state, buf, n) → synthesise PCM into buf
game_audio_update(state, dt_ms)       → advance sequencer / slides
game_update(state, input, dt_ms)      → advance game state
game_render(state, backbuffer)        → write pixels to backbuffer
```

The platform provides everything above the dashed line.  The game provides everything below it.  A new platform backend adds a new entry point in `src/platforms/<name>/main.c` and a new build target in `build-dev.sh`.

---

## Step-by-step: adding a new backend

### 1. Create the directory structure

```
src/platforms/<name>/
    main.c          ← the new backend
    base.h          ← platform-specific types (window handle, context, etc.)
    audio.h         ← audio-specific types for this backend
```

### 2. Implement window creation and rendering

**2a. Window + GL context (if GL-based)**

Follow the X11 backend (`src/platforms/x11/main.c`) as the reference.  Key points:

- Allocate the `Backbuffer` using `malloc(GAME_W * GAME_H * sizeof(uint32_t))`.
- Set `pitch = GAME_W * sizeof(uint32_t)`, `bytes_per_pixel = 4`.
- Create the GPU texture **once** in `init_gl` using `glTexImage2D(..., GL_RGBA, GL_UNSIGNED_BYTE, NULL)`.
- Upload pixels every frame using `glTexSubImage2D(..., GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels)`.

**PIXEL FORMAT** — always use `GL_RGBA` (or the equivalent `R8G8B8A8` format in the platform's API).  Our `GAME_RGB(r,g,b)` stores bytes as `[R][G][B][A]` in memory and `GL_RGBA` reads them in that order.  Never use `GL_BGRA` unless the new platform's GPU/driver requires it AND you swap the GAME_RGB macro to match.

**2b. If the new backend has its own 2D API (e.g., SFML `sf::Texture`, SDL `SDL_Texture`)**

Create a texture from the backbuffer pixels each frame using the platform's equivalent of `UpdateTexture`:
- SFML 3: `texture.update(reinterpret_cast<const sf::Uint8*>(bb.pixels))`
- SDL 3: `SDL_UpdateTexture(tex, NULL, bb.pixels, bb.pitch)`

The pixel format argument must be `SDL_PIXELFORMAT_RGBA32` (or `SFML_PIXEL_RGBA`) — same [R,G,B,A] byte order.

### 3. Implement letterboxing

Copy the `compute_letterbox` function from `src/utils/math.h`:

```c
void compute_letterbox(int win_w, int win_h, int canvas_w, int canvas_h,
                       float *scale, int *off_x, int *off_y);
```

Use it to compute a destination rectangle that preserves aspect ratio:

```
dst_rect = { off_x, off_y, off_x + canvas_w * scale, off_y + canvas_h * scale }
```

Draw the backbuffer texture scaled into `dst_rect`.  The rest of the window is cleared to black.

### 4. Implement frame timing

Use the DE100 timing pattern from `src/platforms/x11/main.c`:

```c
timing_begin();        // capture frame_start = clock_gettime(CLOCK_MONOTONIC)
// ... game logic, audio, rendering ...
timing_mark_work_done();
if (!vsync_enabled) {
    timing_sleep_until_target();   // coarse nanosleep + spin-wait
}
timing_end();          // capture frame_end, compute total_seconds
float dt_ms = g_timing.total_seconds * 1000.0f;
```

**For VSync:** if the new platform supports hardware VSync (SwapInterval, `glfwSwapInterval(1)`, `SDL_GL_SetSwapInterval(1)`), enable it and skip `timing_sleep_until_target()`.

**FPS display:** use a 60-frame rolling average, not raw `(int)(1/dt)`:

```c
static float accum = 0.0f; static int count = 0; static int fps_display = 60;
accum += g_timing.total_seconds; count++;
if (count >= 60) { fps_display = (int)(60.0f / accum + 0.5f); accum = 0; count = 0; }
```

### 5. Implement audio

**5a. Choose an approach:**
- **Callback-based** (SFML, SDL, OpenAL): a separate thread calls `game_get_audio_samples` when the driver needs data.  The game thread calls `game_audio_update` each frame.  No explicit drain loop needed.
- **Pull-based** (ALSA): the game loop calls `platform_audio_get_samples_to_write()` then `game_get_audio_samples` then `snd_pcm_writei`.

**5b. Normalise loudness with `AUDIO_MASTER_VOLUME`:**
The `AUDIO_MASTER_VOLUME = 0.6f` constant in `audio_demo.c` (and game courses' `audio.c`) applies a global gain before writing to the buffer.  This compensates for different internal gain stages in different backends.  If the new platform sounds louder or quieter than Raylib at equal settings, adjust `AUDIO_MASTER_VOLUME` accordingly.

**5c. Anti-click fade-out:**
`AUDIO_FADE_FRAMES = 441` (10 ms at 44100 Hz) ramps square-wave voices to 0 before deactivation.  This is backend-agnostic — it lives in `audio_demo.c` / the game's `audio.c` and requires no per-backend change.

**5d. Pre-fill silence on init:**
Fill the audio buffer with silence before starting playback to avoid a click on the very first callback.

### 6. Implement input

Map the platform's key/button events to `GameInput.buttons`:

```c
// Use UPDATE_BUTTON from platform.h:
UPDATE_BUTTON(input->buttons.play_tone, IsKeyDown(KEY_SPACE));
```

At the end of each frame, call `platform_swap_input_buffers(input)` to rotate the double buffer.

### 7. Add a build target in `build-dev.sh`

```bash
elif [ "$BACKEND" = "sfml3" ]; then
    PLATFORM_SRC="src/platforms/sfml3/main.cpp"
    PLATFORM_LIBS="-lsfml-graphics -lsfml-audio -lsfml-window -lsfml-system"
    PLATFORM_FLAGS="-std=c++17"
fi
```

---

## Checklist for a new backend

- [ ] `GAME_RGB` pixel format: GL_RGBA or equivalent [R,G,B,A] format — NO GL_BGRA
- [ ] Backbuffer allocated with `malloc`, `pitch = GAME_W * 4`
- [ ] `glTexImage2D` (allocate once) + `glTexSubImage2D` (update every frame)
- [ ] Letterbox with `compute_letterbox`
- [ ] Frame timing: `timing_begin / timing_mark_work_done / timing_sleep_until_target / timing_end`
- [ ] FPS display: 60-frame rolling average
- [ ] Audio: `AUDIO_MASTER_VOLUME` applied in `game_get_audio_samples`
- [ ] Audio: `AUDIO_FADE_FRAMES` anti-click fade present in synthesis loop
- [ ] Double-buffered input: `platform_swap_input_buffers` called each frame
- [ ] Build target added to `build-dev.sh`
- [ ] Compiles with `-Wall -Wextra -Werror` (or the C++ equivalent)
- [ ] Both backends produce identical colors and text on screen
- [ ] Both backends produce equal loudness for the same sounds

---

## Platform-specific notes

### Windows (Win32 + OpenGL)

- Use `wglCreateContext` / `wglMakeCurrent` instead of `glXCreateContext`.
- Frame timing: use `QueryPerformanceCounter` instead of `clock_gettime(CLOCK_MONOTONIC)`.
- Sleep: use `timeBeginPeriod(1)` + `Sleep(ms)` for 1ms resolution; spin-wait for sub-ms precision.
- Audio: use WASAPI (exclusive mode for low latency) or XAudio2.  WASAPI is the Windows equivalent of ALSA.
- VSync: `SwapIntervalEXT` via `wglGetProcAddress`.

### macOS (Cocoa + Metal or OpenGL)

- OpenGL is deprecated in macOS 10.14+ but still works.  Metal is preferred for new work.
- For Metal: use `MTKView` with `MTKViewDelegate.drawInMTKView`; upload the backbuffer as an `MTLTexture` each frame.
- Frame timing: use `mach_absolute_time` / `mach_timebase_info` for nanosecond-resolution timing.
- Audio: use Core Audio (`AudioQueueNewOutput`) or AudioUnit.
- VSync: Metal's `MTKView.isPaused = NO` + `preferredFramesPerSecond = 60` handles it automatically.

### SDL 3

- `SDL_CreateWindow` + `SDL_GL_CreateContext` (if OpenGL) or `SDL_CreateRenderer` (software/hardware).
- Software renderer: `SDL_UpdateTexture(tex, NULL, bb.pixels, bb.pitch)` with `SDL_PIXELFORMAT_RGBA32`.
- OpenGL: same as X11 — `glTexSubImage2D` with `GL_RGBA`.
- Audio: `SDL_OpenAudioDevice` with callback; callback thread calls `game_get_audio_samples`.
- Frame timing: `SDL_GetPerformanceCounter` / `SDL_GetPerformanceFrequency`; sleep with `SDL_Delay`.

### SFML 3

- Use `sf::RenderWindow` + `sf::Texture::update(pixels, GAME_W, GAME_H, 0, 0)`.
- Pixel format: SFML expects `[R,G,B,A]` RGBA8 — matches our layout.  No swap needed.
- Audio: `sf::SoundStream` subclass; override `onGetData` to call `game_get_audio_samples`.
- Frame timing: `sf::Clock` + `sf::Time`; VSync via `window.setVerticalSyncEnabled(true)`.
