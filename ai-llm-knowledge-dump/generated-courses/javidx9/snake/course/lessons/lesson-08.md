# Lesson 08 — Delta-Time Game Loop + Move Timer

## By the end of this lesson you will have:

A smooth game loop driven by `platform_get_time()`, where the snake moves at exactly 150ms per step regardless of frame rate, using a delta-time accumulator that replaces the old `tick_count` / `sleep_ms` approach.

---

## The Problem with `sleep_ms`

The original loop:

```c
while (!platform_should_quit()) {
    platform_sleep_ms(150);  /* hard-coded 150ms sleep */
    platform_get_input(&input);
    snake_tick(&state, input);
    platform_render(&state);
}
```

Problems:
1. `sleep_ms(150)` means the loop literally idles for 150ms. Input events queued during sleep are handled late.
2. Changing snake speed means changing the sleep duration — only one global speed for the whole game.
3. The game can't run at a consistent high frame rate. Each frame is either sleeping or drawing — never both.
4. `platform_should_quit` is a polling function in the game loop — the platform bleeds into the game architecture.

---

## Step 1 — `platform_get_time()`

```c
/* Returns seconds since an arbitrary fixed point, as a double.
 * Uses CLOCK_MONOTONIC — always increases, never jumps backward. */
double platform_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
```

**New C concept — `struct timespec`:**
`clock_gettime` fills this struct with `tv_sec` (whole seconds) and `tv_nsec` (nanoseconds, 0–999999999). Converting to a double gives fractional seconds with nanosecond precision.

**Why `CLOCK_MONOTONIC`?**
`CLOCK_REALTIME` is the wall clock — it can jump if NTP adjusts the system time. `CLOCK_MONOTONIC` only moves forward. `delta_time` must never be negative.

**Raylib equivalent:**
```c
double platform_get_time(void) { return GetTime(); }
```

---

## Step 2 — The delta-time game loop

```c
double prev_time = platform_get_time();

while (g_is_running) {
    double curr_time  = platform_get_time();
    float  delta_time = (float)(curr_time - prev_time);
    prev_time = curr_time;

    /* Cap: prevent giant jumps when resuming from a debugger pause */
    if (delta_time > 0.1f) delta_time = 0.1f;

    prepare_input_frame(&input);
    platform_get_input(&input);

    if (input.quit) break;

    snake_update(&state, &input, delta_time);
    snake_render(&state, &backbuffer);
    platform_display_backbuffer(&backbuffer);
}
```

`delta_time` is "seconds elapsed since last frame." At 60fps: ~0.0167s. At 30fps: ~0.033s.

The 0.1f cap prevents the snake teleporting many cells if the process was paused in a debugger for several seconds.

No `platform_sleep_ms`. VSync inside `platform_display_backbuffer` paces the loop to the monitor refresh rate.

---

## Step 3 — `move_timer` in `snake_update`

```c
void snake_update(GameState *s, const GameInput *input, float delta_time) {
    if (s->game_over) {
        if (input->restart) snake_init(s);
        return;
    }

    /* ... handle input turns ... */

    /* Accumulate time — only move when interval is reached */
    s->move_timer += delta_time;
    if (s->move_timer < s->move_interval) return;
    s->move_timer -= s->move_interval;  /* keep overshoot, don't zero */

    /* ... movement, collision, food logic ... */
}
```

**The subtraction trick:**
`s->move_timer -= s->move_interval` (not `s->move_timer = 0.0f`).

Why? At 60fps, each frame is 16.67ms. `move_interval=150ms` means the snake should move every ~9 frames. Frame times aren't exactly equal — sometimes 16ms, sometimes 17ms. If we zero the timer, small errors accumulate into a slightly inconsistent movement rhythm.

By keeping the remainder (`timer -= interval` instead of `= 0`), the overshoot carries forward. If a frame was 18ms and the interval was 150ms with a 5ms remainder, the timer becomes -132ms... wait, that's wrong. Let me restate:

```
move_interval = 0.150
Frame 1: timer = 0.017 → not >= 0.150 → no move
Frame 2: timer = 0.034 → no move
...
Frame 9: timer = 0.150 → 0.150 >= 0.150 → MOVE! timer = 0.150 - 0.150 = 0.000
Frame 10: timer = 0.017 → no move (fresh start from 0, not from negative)
```

But with a slightly long frame 9 (e.g., 19ms):
```
Frame 9: timer = 0.152 → MOVE! timer = 0.152 - 0.150 = 0.002  ← overshoot carried
Frame 10: timer = 0.002 + 0.017 = 0.019 → still no move (counting from 0.002, not 0)
```

The next move fires at `0.002 + ~9 frames` instead of exactly `9 frames` — the overshoot makes timing more accurate over time.

---

## Step 4 — Speed scaling

In `snake_update`, when the snake eats food:

```c
if (s->score % 3 == 0 && s->move_interval > 0.05f)
    s->move_interval -= 0.01f;
```

`move_interval` starts at 0.15f (150ms). Every 3 points, it decreases by 10ms. Floor at 0.05f (50ms = 20 steps/sec).

Compare to old code:
```c
if (s->score % 3 == 0 && s->speed > 2)
    s->speed--;
```

Old `speed` was a tick count (integer, platform-frame-dependent). New `move_interval` is real seconds (float, frame-rate independent). At 60fps, `speed=8` meant "move every 8/60 ≈ 133ms." At 30fps, the same `speed=8` would mean "move every 8/30 ≈ 267ms" — twice as slow! With `move_interval`, behavior is identical at any frame rate.

---

## Key Concepts

- `platform_get_time()` — `clock_gettime(CLOCK_MONOTONIC)` → fractional seconds
- `delta_time = curr - prev; prev = curr` — time since last frame
- `delta_time > 0.1f` cap — prevents debugger-pause teleportation
- `move_timer += delta_time; if (timer >= interval) { move; timer -= interval; }` — accumulator pattern
- `timer -= interval` (not `= 0`) — preserves overshoot for accurate timing
- `move_interval` in seconds replaces `speed` in ticks — frame-rate independent
- VSync in `platform_display_backbuffer` paces the loop — no `sleep_ms` needed

---

## Exercise

The delta-time cap is `0.1f` (100ms). What happens if you set it to `0.016f` (one frame at 60fps)? Try it: pause in the debugger and resume. The snake can't move more than one step per update now — is this better or worse for gameplay? What about setting it to `1.0f`?
