# Lesson 09 — Delta-Time Game Loop + `platform_get_time`

## By the end of this lesson you will have:

A delta-time game loop in `main` that computes `dt` using `platform_get_time()`, passes it to `frogger_tick`, and paces the frame rate with VSync — replacing the old `frogger_run + platform_sleep_ms(16)` approach.

---

## The Old Architecture

```c
/* OLD: frogger_run in frogger.c */
while (!platform_should_quit()) {
    clock_gettime(CLOCK_MONOTONIC, &now);
    float dt = (float)(now.tv_sec - prev.tv_sec) + ...;
    prev = now;

    platform_get_input(&input);
    frogger_tick(&state, &input, dt);
    platform_render(&state);   /* ← platform renders, not game */
    platform_sleep_ms(16);     /* ← hard sleep, not VSync */
}
platform_shutdown();
```

Problems:
1. `frogger_run` imported timing (`clock_gettime`) into `frogger.c` — which should be pure game logic
2. `platform_render` lived in the platform files → rendering duplicated in both X11 and Raylib
3. `platform_sleep_ms(16)` is approximate — actual sleep is 16–20ms depending on OS scheduler
4. `platform_should_quit` and `platform_shutdown` were platform functions called from game code

---

## Step 1 — `platform_get_time`

```c
/* main_x11.c */
double platform_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* main_raylib.c */
double platform_get_time(void) {
    return GetTime();
}
```

Returns seconds since an arbitrary fixed point (process start or boot). `CLOCK_MONOTONIC` never jumps backward — safe for delta-time computation.

**Why `double` not `float`?**
`double` has 53-bit mantissa → ~15 decimal digits of precision. After `3600` seconds (1 hour), `(double)3600.016 - (double)3600.000 = 0.016` is exact. With `float` (23-bit mantissa): `(float)3600.016 - (float)3600.000` loses precision and `dt` becomes incorrect after a few minutes of play.

The computed `dt` is cast to `float` for use in game logic — `float` is fine for `0.016f`, just not for the raw time values.

---

## Step 2 — The new delta-time loop in `main`

```c
double prev_time = platform_get_time();

while (g_is_running) {
    double curr_time  = platform_get_time();
    float  delta_time = (float)(curr_time - prev_time);
    prev_time = curr_time;
    if (delta_time > 0.1f) delta_time = 0.1f;  /* debugger cap */

    prepare_input_frame(&input);
    platform_get_input(&input);
    if (input.quit) break;

    frogger_tick(&state, &input, delta_time);
    frogger_render(&state, &bb);
    platform_display_backbuffer(&bb);  /* VSync here */
}
```

`platform_display_backbuffer` calls `glXSwapBuffers` (X11) or `EndDrawing` (Raylib), both of which wait for the next monitor VSync (~16.67ms at 60Hz). This naturally paces the loop to ~60fps without `sleep_ms`.

**The `0.1f` cap:**
If you pause in a debugger for 5 seconds, `curr_time - prev_time = 5.0`. Without the cap, `frogger_tick` would update with `dt=5.0` — cars teleport across the screen, frog shoots off. The cap limits the maximum single step to 100ms (about 6 frames).

---

## Step 3 — Why the loop moved to `main`

The game loop belongs in `main`, not in a `frogger_run` function:

1. **Separation of concerns:** `frogger.c` contains game logic (`tick`, `render`, `init`). Timing and the main loop are platform infrastructure.

2. **No `platform_*` calls from `frogger.c`:** `frogger_tick` and `frogger_render` never call platform functions — they only receive data via parameters.

3. **Both backends share nothing:** `main_x11.c` and `main_raylib.c` each have their own `main`. They implement the same 4-function contract but otherwise are completely independent.

Compare to the "unified" `frogger_run` approach which required `frogger.c` to `#include "platform.h"` and call `platform_*` functions — making game code depend on platform abstractions.

---

## Step 4 — Frame timing analysis

At 60fps (VSync):
```
prev_time = 0.000
Frame 1: curr_time = 0.016, dt = 0.016
Frame 2: curr_time = 0.033, dt = 0.017  (slightly uneven)
Frame 3: curr_time = 0.050, dt = 0.017
```

Lane at speed=3: moves `3 * 64 * 0.016 = 3.07 pixels/frame` → 192 pixels/second. After 5 seconds: `960 pixels` = 15 tiles of scroll, which is less than one full 64-tile pattern length.

**Without delta-time (old tick-based approach):**
At 60fps tick = "one frame". At 30fps tick = "one frame" but slower. Speed `3` meant "3 tiles/second at 60fps" — at 30fps it was "1.5 tiles/second." With delta-time, `speed * dt` always gives the same real-world velocity.

---

## Key Concepts

- `platform_get_time()` — monotonic clock, returns `double` seconds
- `dt = curr_time - prev_time; prev_time = curr_time` — time since last frame
- `if (dt > 0.1f) dt = 0.1f` — debugger cap prevents time-jump teleportation
- `double` for raw time values, `float` for `dt` in game logic
- `platform_display_backbuffer` blocks on VSync — no `sleep_ms` needed
- Game loop in `main`, not in `frogger.c` — `frogger_tick/render` are pure data transforms

---

## Exercise

Remove the `0.1f` cap from the loop. Run the game normally for 30 seconds. Now attach a debugger, pause for 10 seconds, then resume. What happens? What is `delta_time` on the first frame after resuming? How many tiles does a speed-4 lane theoretically advance in that one frame?
