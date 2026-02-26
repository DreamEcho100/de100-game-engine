# Lesson 14 — Final Integration

## By the end of this lesson you will have:
A fully working, shipping-quality Asteroids clone. The game loop runs at VSync speed (60 Hz on most monitors) with a frame-rate-independent delta-time loop, builds against both an X11/OpenGL backend and a Raylib backend, has zero memory leaks (one `malloc`, one `free`), and is ready for you to extend.

---

## Step 1 — The delta-time game loop

Both `main_x11.c` and `main_raylib.c` use the same loop structure. Here it is from `main_x11.c`:

```c
double prev_time = platform_get_time();

while (g_is_running) {
    double curr_time  = platform_get_time();
    float  delta_time = (float)(curr_time - prev_time);
    prev_time = curr_time;
    if (delta_time > 0.1f) delta_time = 0.1f; /* cap for debugger pauses */

    prepare_input_frame(&input);
    platform_get_input(&input);
    if (input.quit) break;

    asteroids_update(&state, &input, delta_time);
    asteroids_render(&state, &bb);
    platform_display_backbuffer(&bb);
}
```

**Why `double` for time but `float` for `delta_time`?**

`platform_get_time()` returns seconds since program start as a `double`. After the game has been running for an hour, the raw time value is `3600.0`. A `float` only has ~7 significant decimal digits, so `float(3600.0 + 0.016)` would round to `3600.016` with an error of several milliseconds — enough to cause jittery physics. `double` has ~15 digits of precision, so the subtraction `curr_time - prev_time` always gives an accurate small number.

After the subtraction, the small result (≈ 0.016) is safely cast to `float` for use in game code. All the sin/cos/position math only needs `float` precision.

**JS analogy:**
```js
let prevTime = performance.now() / 1000;  // double-precision seconds
function frame(now) {
    const dt = Math.min((now / 1000) - prevTime, 0.1);
    prevTime = now / 1000;
    update(dt);
    render();
    requestAnimationFrame(frame);
}
```

---

## Step 2 — The 0.1 s cap: why it matters

```c
if (delta_time > 0.1f) delta_time = 0.1f;
```

Without this cap, pausing in the debugger for 5 seconds and then resuming would give `dt = 5.0`. The Euler integration would then move every object by 5 seconds of velocity in one step — asteroids and bullets would teleport to random positions, potentially spawning inside each other.

`0.1f` seconds (6 frames' worth at 60 Hz) is a generous but safe cap. At worst the game appears to slow down briefly after a stall; it never explodes.

**Why not `1.0f / 60.0f` (16 ms)?** On a slow machine or when a frame takes longer than 16 ms, capping to exactly 16 ms would cause the game to play in slow motion. `0.1f` allows the game to catch up (run slightly faster-than-real-time) for up to 6 frames worth of stall, which is enough to absorb one momentary OS hiccup.

---

## Step 3 — `platform_get_time` implementations

**X11 (via POSIX `clock_gettime`):**

```c
double platform_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
```

`CLOCK_MONOTONIC` is a clock that never goes backward (unlike wall-clock time, which can jump if the user changes the system clock or NTP adjusts it). `tv_nsec` is nanoseconds (10⁻⁹ seconds), so multiplying by `1e-9` converts to seconds. `_POSIX_C_SOURCE 200809L` must be defined before any `#include` to unlock this API.

**Raylib:**

```c
double platform_get_time(void) {
    return GetTime(); /* seconds since InitWindow, double precision */
}
```

Raylib wraps the same underlying monotonic clock. The API is simpler because Raylib hides the platform details.

---

## Step 4 — The one `malloc` and one `free`

The entire game uses exactly one heap allocation — the pixel backbuffer:

```c
bb.pixels = (uint32_t *)malloc((size_t)(bb.width * bb.height) * sizeof(uint32_t));
if (!bb.pixels) { fprintf(stderr, "FATAL: out of memory\n"); return 1; }
```

`800 × 600 × 4 bytes = 1,920,000 bytes ≈ 1.8 MB`. This must be on the heap because it's too large for the stack (typical stack limit is 1–8 MB; putting 1.8 MB there is unreliable).

At exit:

```c
free(bb.pixels);
```

Every other allocation in the game is on the stack:
- `GameState state` — declared as a local in `main`, includes all `SpaceObject` pools.
- `GameInput input` — declared as a local in `main`.
- `char buf[32]` — in `asteroids_render`, a local stack array for `snprintf`.

**Why not allocate `GameState` on the heap?** `sizeof(GameState)` is modest (a few kilobytes — `SpaceObject` is ~40 bytes, `MAX_ASTEROIDS=64` pool is ~2.5 KB, `MAX_BULLETS=32` pool is ~1.3 KB). Stacks can comfortably hold this. Keeping it on the stack avoids a `free` path and makes ownership obvious.

---

## Step 5 — Verifying zero leaks with Valgrind

```
valgrind --leak-check=full ./build/asteroids_x11
```

Run the game for a few seconds, then press Escape. Valgrind output:

```
==12345== HEAP SUMMARY:
==12345==   in use at exit: 0 bytes in 0 blocks
==12345== LEAK SUMMARY:
==12345==    definitely lost: 0 bytes in 0 blocks
==12345==    indirectly lost: 0 bytes in 0 blocks
==12345==      possibly lost: 0 bytes in 0 blocks
==12345==    still reachable: 0 bytes in 0 blocks
```

The single `malloc` for `bb.pixels` is matched by `free(bb.pixels)` at exit. `glDeleteTextures` frees the GPU-side texture object. `glXDestroyContext` and `XCloseDisplay` release X11 resources.

**Why does zero leaks matter?** In a game, leaks cause memory usage to grow over time. In a long session or on a console with a fixed memory budget, a leak will eventually crash the game. A clean Valgrind run confirms the resource lifecycle is correct.

---

## Step 6 — Build flags reference

| Flag | Purpose |
|------|---------|
| `clang src/asteroids.c src/main_x11.c` | Compile both translation units together |
| `-o build/asteroids_x11` | Output binary |
| `-lX11` | Link X11 display library |
| `-lxkbcommon` | Link XKB keyboard library (used indirectly by XkbSetDetectableAutoRepeat) |
| `-lGL` | Link OpenGL |
| `-lGLX` | Link GLX (OpenGL extension for X11) |
| `-lm` | Link the C math library (sinf, cosf, sqrtf, etc.) |

**Raylib build:**

```
clang src/asteroids.c src/main_raylib.c -o build/asteroids_raylib -lraylib -lm
```

Raylib's single `-lraylib` replaces `-lX11 -lxkbcommon -lGL -lGLX`. Everything else is identical.

**Debug build (add sanitisers):**

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11_dbg \
  -g -fsanitize=address,undefined \
  -lX11 -lxkbcommon -lGL -lGLX -lm
```

`-g` enables debug symbols (readable stack traces). `-fsanitize=address` catches out-of-bounds array accesses and use-after-free. `-fsanitize=undefined` catches integer overflow, null pointer dereference, and other UB. Always build with sanitisers during development.

---

## Step 7 — Complete game controls

| Key | Action |
|-----|--------|
| **← →** | Rotate ship |
| **↑** | Thrust (accelerate in facing direction) |
| **Space** | Fire bullet (once per press) |
| **Escape** | Quit |

There is no pause key, no high-score persistence, and no sound — these are the three most natural extensions.

---

## Step 8 — Suggested extensions

These are ordered from easiest to hardest:

1. **Friction** — multiply `player.dx` and `player.dy` by `0.99f` each frame. Makes the game easier but changes the feel significantly.

2. **Lives system** — add `int lives` to `GameState`. Decrement on death instead of immediately resetting. Show with `draw_text`. Reset only when `lives == 0`.

3. **Thrust exhaust particles** — when thrusting, call `add_bullet` with a short lifetime (`0.2f`) and velocity pointing *backward* from the ship at low speed. Use `COLOR_ORANGE`. It renders free because bullets are already white dots.

4. **High score persistence** — `fopen("score.dat", "wb")` + `fwrite(&state->score, sizeof(int), 1, f)` at exit; `fread` at startup. Teaches basic C file I/O.

5. **Sound** — link against a small audio library (miniaudio, dr_wav). Play a short PCM buffer on asteroid hit and on death. Teaches mixing audio in a game loop.

6. **UFO enemy** — a `SpaceObject` that spawns from a random edge every 20 seconds, moves horizontally, and fires bullets at the player. Demonstrates adding a new active entity type with its own AI update logic.

7. **Fixed-timestep loop** — instead of passing raw `dt` to update, accumulate `dt` into an `accumulator` and call `asteroids_update` with a fixed `1.0f/60.0f` step until the accumulator is drained. Makes physics perfectly deterministic regardless of frame rate.

---

## Build & Run (final game)

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11 -lX11 -lxkbcommon -lGL -lGLX -lm
./build/asteroids_x11
```

**What you should see:** The complete game:
- White triangle ship starts in the centre.
- Two large yellow wireframe asteroids approach from top-left and top-right.
- Rotate with arrows, thrust with up, shoot with Space.
- Asteroids split on hit (64 → 32+32 → 16+16 each).
- Score accumulates: 100 / 200 / 400 per hit size, +1000 on wave clear.
- Touching an asteroid triggers a 1.5-second death flash with red overlay.
- Game resets automatically. Play indefinitely.

---

## Key Concepts

- **Double for time subtraction** — use `double` for wall-clock timestamps to preserve nanosecond precision; cast to `float` only after the subtraction produces a small delta.
- **0.1 f cap** — prevents one-frame Euler integration explosions after debugger pauses or OS hiccups.
- **`CLOCK_MONOTONIC`** — the correct POSIX clock for game timing; never jumps backward.
- **One `malloc`, one `free`** — the pixel buffer is the only heap allocation; all game state lives on the stack.
- **Valgrind** — `--leak-check=full` confirms the resource lifecycle is correct; zero leaks is the target.
- **Translation units** — `asteroids.c` and `main_x11.c` (or `main_raylib.c`) are compiled together in one `clang` invocation; the linker resolves cross-file symbols.
- **`-lm` is always required** — `sinf`, `cosf`, `fabsf` etc. live in `libm`; forgetting `-lm` produces "undefined reference to `sinf`" link errors.
