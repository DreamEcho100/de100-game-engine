# Lesson 08 v2 — Game Loop: Delta Time & Automatic Piece Dropping

---

## What You'll Have Working by the End

The game runs at 60 FPS with smooth, frame-rate independent timing. Pieces fall one row per second automatically using **delta time** (accumulated time between frames). You can move left/right and rotate while the piece falls. Q and Escape exit cleanly.

The piece hits the floor and stops. It doesn't lock and spawn a new piece yet — that's lesson 09.

---

## Why Delta Time Instead of Tick-Based?

The original lesson 08 used a **tick-based** approach (count to 20, then drop). This lesson uses **delta time** instead because it's:

1. **Frame-rate independent**: Works the same on fast and slow computers
2. **More precise**: No rounding errors from fixed sleep intervals
3. **Industry standard**: Used in Unity, Unreal, Godot, and all modern game engines
4. **Flexible**: Easy to pause, slow-mo, or speed up without changing logic
5. **Professional**: This is how real games are made

### Comparison

| Tick-Based (Original)          | Delta Time (This Version)     |
| ------------------------------ | ----------------------------- |
| `speed_count++` every frame    | `drop_timer += delta_time`    |
| Drop when `speed_count == 20`  | Drop when `drop_timer >= 1.0` |
| Fixed 50ms sleep               | Adaptive frame timing         |
| Loses precision on slow frames | Keeps sub-frame precision     |

---

## Where We're Starting

At the end of lesson 07:

- `platform_get_input()` works — keys move the piece
- No automatic dropping — pieces only move when you press keys
- No timing system — the game loop runs as fast as possible
- `GameState` has no timing fields

---

## Step 1: Add Timing Fields to `GameState`

Open `src/tetris.h`. Find the `GameState` struct and add timing fields:

```c
typedef struct {
  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT]; /* the play field */
  CurrentPiece current_piece;

  int score;
  int game_over;

  /* ── ADD THESE: Time-based dropping ── */
  float drop_timer;    /* Accumulates delta time (in seconds) */
  float drop_interval; /* Time between drops (e.g., 1.0 = 1 second) */
  int piece_count;     /* Total pieces locked — used for difficulty scaling */
} GameState;
```

**What these do:**

- `drop_timer`: Counts up from 0. When it reaches `drop_interval`, the piece drops one row.
- `drop_interval`: How many seconds between automatic drops. Start at 1.0 (one drop per second).
- `piece_count`: Tracks how many pieces have been locked. We'll use this in lesson 09 to increase difficulty.

---

## Step 2: Initialize Timing Fields in `game_init()`

Open `src/tetris.c`. Find `game_init()` and add these lines after setting `game_over`:

```c
void game_init(GameState *state) {
  state->score = 0;
  state->game_over = 0;

  /* ── ADD THESE: Time-based dropping ── */
  state->drop_timer = 0.0f;
  state->drop_interval = 1.0f;  /* 1 second between drops initially */
  state->piece_count = 0;

  /* Build the boundary walls. */
  for (int y = 0; y < FIELD_HEIGHT; y++) {
    for (int x = 0; x < FIELD_WIDTH; x++) {
      if (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) {
        state->field[y * FIELD_WIDTH + x] = CELL_WALL_VALUE;
      } else {
        state->field[y * FIELD_WIDTH + x] = 0;
      }
    }
  }

  /* Pick a random starting piece. */
  srand((unsigned int)time(NULL));
  state->current_piece = (CurrentPiece){
      .col = FIELD_WIDTH / 2 - 2,
      .row = 0,
      .index = rand() % TETROMINOS_COUNT,
      .rotation = TETROMINO_R_0,
  };
}
```

---

## Step 3: Add Platform Time Function

We need a way to measure time. Add this declaration to `src/platform.h`:

```c
/* Get current time in seconds since program start.
 * Used for delta time calculations. */
double platform_get_time(void);
```

This function will be implemented differently on X11 and Raylib.

---

## Step 4: Implement `platform_get_time()` in X11

Open `src/main_x11.c`. Add these includes at the top if not already present:

```c
#include <sys/time.h>
#include <time.h>
```

Add these global variables after the `X11_State` declaration:

```c
static double g_start_time = 0.0;
```

Add these two helper functions before `platform_init()`:

```c
/* Get absolute wall-clock time in seconds (with microsecond precision). */
static double get_current_time(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

/* Get time since program start (in seconds). */
double platform_get_time(void) {
  if (g_start_time == 0.0) {
    g_start_time = get_current_time();  /* First call: record start time */
  }
  return get_current_time() - g_start_time;
}
```

**How this works:**

- `gettimeofday()` returns seconds + microseconds since Unix epoch (Jan 1, 1970)
- We convert to a single `double` in seconds (e.g., `1234567890.123456`)
- On first call, we save this as `g_start_time`
- Every subsequent call returns `current_time - start_time`, giving us "seconds since program start"

**Why not just use `time(NULL)`?**

`time(NULL)` only has 1-second precision. We need sub-second precision for smooth 60 FPS timing. `gettimeofday()` gives us microsecond precision (1/1,000,000 of a second).

---

## Step 5: Implement `platform_get_time()` in Raylib

Raylib already provides `GetTime()`, so this is trivial. Open `src/main_raylib.c` and add:

```c
double platform_get_time(void) {
  return GetTime();  /* Raylib's built-in timer */
}
```

That's it! Raylib handles all the platform-specific timing internally.

---

## Step 6: Fix Non-Blocking Input in X11

**Critical bug fix:** The current `platform_get_input()` uses `XNextEvent()`, which **blocks** the entire game loop waiting for keyboard/mouse events. If the user doesn't press anything, the game freezes.

Replace the entire `platform_get_input()` function in `src/main_x11.c`:

```c
void platform_get_input(GameState *state, GameInput *input) {
  memset(input, 0, sizeof(GameInput));

  /* Process ALL pending events without blocking.
   * XPending() returns the number of events in the queue.
   * If 0, we skip the loop and continue — game keeps running. */
  while (XPending(g_x11.display) > 0) {
    XNextEvent(g_x11.display, &g_x11.event);

    switch (g_x11.event.type) {
    case KeyPress: {
      KeySym key = XLookupKeysym(&g_x11.event.xkey, 0);
      switch (key) {
      case XK_q:
      case XK_Q:
      case XK_Escape:
        g_is_game_running = false;
        break;
      case XK_x:
      case XK_X:
        input->rotate_x = 1;
        break;
      case XK_z:
      case XK_Z:
        input->rotate_x = -1;
        break;
      case XK_Left:
      case XK_A:
      case XK_a:
        input->move_left = 1;
        break;
      case XK_Right:
      case XK_D:
      case XK_d:
        input->move_right = 1;
        break;
      case XK_Down:
      case XK_S:
      case XK_s:
        input->move_down = 1;
        break;
      }
      break;  /* End of KeyPress case */
    }

    case KeyRelease: {
      KeySym key = XLookupKeysym(&g_x11.event.xkey, 0);
      switch (key) {
      case XK_Left:
      case XK_A:
      case XK_a:
        input->move_left = 0;
        break;
      case XK_Right:
      case XK_D:
      case XK_d:
        input->move_right = 0;
        break;
      case XK_Down:
      case XK_S:
      case XK_s:
        input->move_down = 0;
        break;
      }
      break;  /* End of KeyRelease case */
    }

    case ClientMessage:
      /* User clicked the ✕ button */
      g_is_game_running = false;
      break;
    }
  }
}
```

**Key changes:**

1. `while (XPending(g_x11.display) > 0)` — only process events if they exist
2. Added missing `break;` statements after `KeyPress` and `KeyRelease` cases (this was causing fall-through bugs)
3. Removed the blocking `XNextEvent()` call outside the loop

---

## Step 7: Implement `platform_sleep_ms()` in X11

We need a way to limit frame rate. Add this function to `src/main_x11.c`:

```c
void platform_sleep_ms(int ms) {
  struct timespec ts;
  ts.tv_sec  = ms / 1000;              /* whole seconds: 50/1000 = 0 */
  ts.tv_nsec = (ms % 1000) * 1000000L; /* remainder in nanoseconds */
  nanosleep(&ts, NULL);
}
```

**Why `nanosleep()` instead of `sleep()` or `usleep()`?**

- `sleep(n)` only takes whole seconds — way too coarse for 60 FPS (16.67ms per frame)
- `usleep(n)` is deprecated on modern POSIX systems
- `nanosleep()` is the current standard and takes nanosecond precision

**How the math works:**

For 16ms (roughly 60 FPS):

```
16ms = 16 milliseconds
     = 0 full seconds       (16 / 1000 = 0 in integer division)
     = 16,000,000 nanoseconds (16 % 1000 = 16, then × 1,000,000)

  tv_sec  = 0
  tv_nsec = 16,000,000
```

The `L` suffix on `1000000L` forces it to be a `long` to avoid potential overflow on 32-bit systems.

---

## Step 8: Write `tetris_update()` Function

This is the heart of the delta time system. Open `src/tetris.c` and add this function **after** `tetris_apply_input()`:

```c
/* Update game state using delta time (time-based approach).
 * delta_time: seconds elapsed since last frame (e.g., 0.016 for 60 FPS).
 * input: player input for this frame.
 *
 * This function:
 * 1. Processes player input (always responsive)
 * 2. Accumulates time in drop_timer
 * 3. When enough time passes, drops the piece one row
 */
void tetris_update(GameState *state, GameInput *input, float delta_time) {
  if (state->game_over) return;  /* Game frozen — do nothing */

  /* ── 1. Handle player input (always responsive) ── */
  tetris_apply_input(state, input);

  /* ── 2. Accumulate time for gravity ── */
  state->drop_timer += delta_time;

  /* ── 3. Check if enough time has passed to drop the piece ── */
  if (state->drop_timer >= state->drop_interval) {
    /* Reset timer, but KEEP the remainder for precision.
     * Example: if drop_interval = 1.0 and drop_timer = 1.02,
     * we reset to 0.02 instead of 0.0. This prevents drift. */
    state->drop_timer -= state->drop_interval;

    /* Try to move the piece down one row */
    if (tetromino_does_piece_fit(
            state, state->current_piece.index,
            state->current_piece.rotation,
            state->current_piece.col,
            state->current_piece.row + 1)) {
      state->current_piece.row++;  /* Fits — drop it */
    }
    /* If it doesn't fit, the piece has landed.
     * Locking and spawning a new piece is added in lesson 09. */
  }
}
```

Now add the declaration to `src/tetris.h` in the public API section:

```c
/* Update game state using delta time.
 * delta_time: seconds elapsed since last frame.
 * input: player input for this frame. */
void tetris_update(GameState *state, GameInput *input, float delta_time);
```

### Why Keep the Remainder?

```c
/* BAD: Loses precision */
state->drop_timer = 0.0f;

/* GOOD: Keeps sub-frame precision */
state->drop_timer -= state->drop_interval;
```

**Example:**

Frame times aren't perfectly consistent. You might get:

```
Frame 1: delta = 0.017s → drop_timer = 0.017
Frame 2: delta = 0.016s → drop_timer = 0.033
...
Frame 60: delta = 0.018s → drop_timer = 1.002  ← 2ms over!
```

If we reset to `0.0`, we lose that 2ms. Over time, this causes the piece to drop slightly slower than intended. By keeping the remainder (`drop_timer = 0.002`), we maintain perfect timing.

---

## Step 9: Update `main()` Loop in X11

Replace the entire `main()` function in `src/main_x11.c`:

```c
int main(void) {
  int screen_width = FIELD_WIDTH * CELL_SIZE;
  int screen_height = FIELD_HEIGHT * CELL_SIZE;

  if (platform_init("Tetris", screen_width, screen_height) != 0) {
    return 1;
  }

  GameInput input = {0};
  GameState game_state = {0};
  game_init(&game_state);

  /* Initialize timing AFTER platform_init() so platform_get_time() works */
  double last_time = platform_get_time();
  const double target_frame_time = 1.0 / 60.0;  /* Target 60 FPS = 0.01667s */

  while (g_is_game_running) {
    /* ── 1. Calculate delta time ── */
    double current_time = platform_get_time();
    double delta_time = current_time - last_time;
    last_time = current_time;

    /* ── 2. Get input (non-blocking) ── */
    platform_get_input(&game_state, &input);

    /* ── 3. Update game logic with delta time ── */
    tetris_update(&game_state, &input, (float)delta_time);

    /* ── 4. Render the current state ── */
    platform_render(&game_state);

    /* ── 5. Frame rate limiting ── */
    double frame_time = platform_get_time() - current_time;
    if (frame_time < target_frame_time) {
      /* Frame finished early — sleep the remainder */
      platform_sleep_ms((int)((target_frame_time - frame_time) * 1000.0));
    }
  }

  platform_shutdown();
  return 0;
}
```

### Loop Breakdown

```
┌─────────────────────────────────────────────────────────┐
│ Frame N starts                                          │
├─────────────────────────────────────────────────────────┤
│ 1. Calculate delta_time (time since last frame)        │
│    Example: 0.016 seconds (60 FPS)                      │
├─────────────────────────────────────────────────────────┤
│ 2. Get input (check keyboard, mouse, window events)    │
│    Non-blocking — returns immediately even if no input  │
├─────────────────────────────────────────────────────────┤
│ 3. Update game logic                                    │
│    - Process input (move/rotate piece)                  │
│    - Add delta_time to drop_timer                       │
│    - Drop piece if timer >= interval                    │
├─────────────────────────────────────────────────────────┤
│ 4. Render (draw field, walls, current piece)           │
├─────────────────────────────────────────────────────────┤
│ 5. Frame limiting                                       │
│    If frame took 10ms but target is 16.67ms,            │
│    sleep for 6.67ms to maintain 60 FPS                  │
└─────────────────────────────────────────────────────────┘
```

**Why this order?**

- **Delta time first**: We need to know how much time passed before updating
- **Input before update**: `tetris_update()` needs the input to process movement
- **Render after update**: Show the state _after_ changes, not before
- **Sleep last**: Only sleep if we have time left in the frame budget

---

## Step 10: Update `main()` Loop in Raylib

Replace the entire `main()` function in `src/main_raylib.c`:

```c
int main(void) {
  int screen_width = FIELD_WIDTH * CELL_SIZE;
  int screen_height = FIELD_HEIGHT * CELL_SIZE;

  if (platform_init("Tetris", screen_width, screen_height) != 0) {
    return 1;
  }

  GameInput input = {0};
  GameState game_state = {0};
  game_init(&game_state);

  while (!WindowShouldClose()) {
    /* ── 1. Get delta time (Raylib provides this automatically) ── */
    float delta_time = GetFrameTime();

    /* ── 2. Get input ── */
    platform_get_input(&game_state, &input);

    /* ── 3. Update game logic ── */
    tetris_update(&game_state, &input, delta_time);

    /* ── 4. Render ── */
    platform_render(&game_state);
  }

  platform_shutdown();
  return 0;
}
```

**Why is Raylib simpler?**

- `GetFrameTime()` automatically calculates delta time for us
- `SetTargetFPS(60)` (called in `platform_init()`) handles frame limiting
- No manual sleep needed — Raylib does it internally

---

## Step 11: Build and Run

```bash
cd games/tetris
./build_x11.sh && ./tetris_x11
```

Or for Raylib:

```bash
./build_raylib.sh && ./tetris_raylib
```

### What to Test

| Action                         | Expected Result                                        |
| ------------------------------ | ------------------------------------------------------ |
| Wait without pressing anything | Piece falls one row per second (smooth, no stuttering) |
| Press ← while falling          | Piece slides left immediately while continuing to fall |
| Press Z while falling          | Piece rotates immediately while continuing to fall     |
| Press ↓ while falling          | Piece drops faster (soft drop)                         |
| Piece reaches the floor        | Piece stops at the floor (no locking yet)              |
| Press Q or Escape              | Game exits cleanly                                     |
| Click ✕ button                 | Game exits cleanly                                     |

### What's Missing (Lesson 09)

When the piece hits the floor, it stops forever. No new piece spawns. This is intentional — the goal of this lesson is to get the timing system working correctly. Lesson 09 adds piece locking, spawning, and game over detection.

---

## Key Concept: Delta Time Explained

### The Problem with Fixed Time Steps

```c
/* BAD: Assumes every frame takes exactly 50ms */
while (running) {
  sleep(50);
  update();  /* What if update() takes 60ms? Now we're behind! */
  render();
}
```

If one frame takes longer than expected (e.g., the OS pauses your process, or rendering is slow), the game slows down.

### The Delta Time Solution

```c
/* GOOD: Measure actual time and adapt */
while (running) {
  delta = current_time - last_time;  /* Actual time elapsed */
  update(delta);  /* Game logic scales with real time */
  render();
  sleep_if_needed();
}
```

Now if a frame takes 20ms instead of 16ms, the next frame compensates. The piece still drops at exactly 1 row per second, regardless of frame rate fluctuations.

### Visualizing Delta Time

```
Target: 60 FPS = 16.67ms per frame

Frame 1: 16ms  → delta = 0.016s → drop_timer = 0.016
Frame 2: 17ms  → delta = 0.017s → drop_timer = 0.033
Frame 3: 15ms  → delta = 0.015s → drop_timer = 0.048
...
Frame 60: 18ms → delta = 0.018s → drop_timer = 1.002 → DROP!
```

Even though individual frames vary (15-18ms), the total time is accurate (1.002 seconds ≈ 1 second).

---

## Advanced: Why Subtract Instead of Reset?

```c
/* Method 1: Reset to zero (LOSES PRECISION) */
if (drop_timer >= drop_interval) {
  drop_timer = 0.0f;  /* Lost the 0.002s overshoot! */
  drop_piece();
}

/* Method 2: Subtract interval (KEEPS PRECISION) */
if (drop_timer >= drop_interval) {
  drop_timer -= drop_interval;  /* Keeps the 0.002s remainder */
  drop_piece();
}
```

**Real-world example:**

After 100 drops with method 1:

```
Expected time: 100 seconds
Actual time:   100.2 seconds  ← 200ms drift!
```

After 100 drops with method 2:

```
Expected time: 100 seconds
Actual time:   100.001 seconds  ← Only 1ms drift!
```

This is why professional games use method 2.

---

## Benefits of Delta Time

### 1. Frame-Rate Independence

```c
/* Works the same at 30 FPS, 60 FPS, or 144 FPS */
state->drop_timer += delta_time;
```

### 2. Easy Difficulty Scaling

```c
/* Make game faster as player progresses */
state->drop_interval = 1.0f / (1.0f + state->piece_count * 0.05f);

/* Results:
 * Piece 0:  1.0 second per drop
 * Piece 10: 0.67 seconds per drop
 * Piece 20: 0.5 seconds per drop
 * Piece 40: 0.33 seconds per drop
 */
```

### 3. Smooth Slow-Motion

```c
/* Just multiply delta_time */
float slow_mo_factor = 0.5f;  /* Half speed */
tetris_update(&game_state, &input, delta_time * slow_mo_factor);
```

### 4. Trivial Pause

```c
if (!paused) {
  tetris_update(&game_state, &input, delta_time);
}
/* When paused, just don't call update — timer doesn't advance */
```

### 5. Replay System

```c
/* Record delta_time and input each frame */
replay[frame] = {delta_time, input};

/* Playback: feed recorded values back in */
tetris_update(&state, &replay[frame].input, replay[frame].delta_time);
```

---

## Exercise: Experiment with Drop Speed

In `game_init()`, try these values and observe the effect:

```c
state->drop_interval = 0.5f;  /* Drops twice per second — fast! */
state->drop_interval = 2.0f;  /* Drops every 2 seconds — slow */
state->drop_interval = 0.1f;  /* Drops 10 times/second — very fast */
```

Change it back to `1.0f` when done.

---

## Common Mistakes and Fixes

### Mistake 1: Blocking Input

```c
/* WRONG: Blocks until user presses a key */
XNextEvent(display, &event);

/* RIGHT: Only process events if they exist */
while (XPending(display) > 0) {
  XNextEvent(display, &event);
}
```

### Mistake 2: Resetting Timer to Zero

```c
/* WRONG: Loses precision */
drop_timer = 0.0f;

/* RIGHT: Keeps remainder */
drop_timer -= drop_interval;
```

### Mistake 3: Calculating Delta Time Before Getting Current Time

```c
/* WRONG: Uses stale last_time */
delta = platform_get_time() - last_time;
last_time = platform_get_time();  /* Called twice! */

/* RIGHT: Get current time once, then calculate */
current_time = platform_get_time();
delta = current_time - last_time;
last_time = current_time;
```

### Mistake 4: Input Only Works During Drop

```c
/* WRONG: Input ignored most of the time */
if (drop_timer >= drop_interval) {
  tetris_apply_input(state, input);  /* Only here! */
}

/* RIGHT: Input always processed */
tetris_apply_input(state, input);  /* Every frame */
if (drop_timer >= drop_interval) {
  drop_piece();
}
```

---

## Checkpoint

Files changed in this lesson:

| File             | What Changed                                                                                      |
| ---------------- | ------------------------------------------------------------------------------------------------- |
| `src/tetris.h`   | Added `drop_timer`, `drop_interval`, `piece_count` to `GameState`; declared `tetris_update()`     |
| `src/tetris.c`   | Initialized timing fields in `game_init()`; added `tetris_update()` function                      |
| `src/platform.h` | Declared `platform_get_time()`                                                                    |
| `src/main_x11.c` | Implemented `platform_get_time()`, `platform_sleep_ms()`; fixed non-blocking input; updated `main |
