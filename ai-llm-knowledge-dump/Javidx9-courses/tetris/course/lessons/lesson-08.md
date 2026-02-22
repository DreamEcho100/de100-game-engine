# Lesson 08 — Game Loop: Pieces Fall on Their Own

---

## What You'll Have Working by the End

The game loop runs at exactly 20 ticks per second. Pieces fall one row per second
automatically — you don't touch anything and the piece drops. You can still move
left/right and rotate while it falls. Q and Escape exit cleanly.

The piece hits the floor and stops. It doesn't lock and spawn a new piece yet —
that's lesson 09.

---

## Where We're Starting

At the end of lesson 07:
- `platform_get_input()` works — keys move the piece
- `platform_sleep_ms()` is a stub that does nothing (loop runs too fast)
- `platform_should_quit()` returns `0` — you exit via `input.quit` only
- `GameState` has no `speed` or `speed_count` fields
- No `tetris_tick()` function — the main loop calls `tetris_apply_input()` directly

```c
/* Current main loop — lesson 07 version: */
while (!platform_should_quit()) {
    platform_sleep_ms(50);           /* stub: does nothing */
    platform_get_input(&input);
    if (input.quit) break;
    tetris_apply_input(&state, &input);  /* input only, no gravity */
    platform_render(&state);
}
```

---

## Step 1: Add `speed` and `speed_count` to `GameState`

Open `src/tetris.h`. Find the `GameState` struct. It currently ends with something
like `int game_over;`. Add two new fields:

```c
typedef struct {
    unsigned char field[FIELD_WIDTH * FIELD_HEIGHT];

    int current_piece;
    int current_rotation;
    int current_x;
    int current_y;
    int next_piece;

    int speed;        /* ADD THIS: ticks between forced drops (starts at 20) */
    int speed_count;  /* ADD THIS: counts up to speed, then resets and drops  */
    int piece_count;  /* ADD THIS: total pieces locked — used for difficulty  */

    int score;
    int game_over;
} GameState;
```

Add `speed`, `speed_count`, and `piece_count`. We'll use all three in this lesson
and lesson 09.

---

## Step 2: Initialize the New Fields in `tetris_init()`

Open `src/tetris.c`. Find `tetris_init()`. After the existing field/piece setup,
add these three lines:

```c
state->speed       = 20;  /* 20 ticks × 50ms = 1000ms = piece drops 1 row/second */
state->speed_count = 0;
state->piece_count = 0;
```

`speed = 20` means: wait 20 ticks before forcing the piece down one row.
Each tick is 50ms. 20 × 50ms = 1000ms = the piece falls one row per second.
That's a comfortable starting pace — slow enough to see, fast enough to feel
like Tetris.

---

## Step 3: Implement `platform_sleep_ms()` in `main_x11.c`

Find the stub `platform_sleep_ms()` and **replace it**:

```c
void platform_sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;              /* whole seconds: 50/1000 = 0         */
    ts.tv_nsec = (ms % 1000) * 1000000L; /* remainder in nanoseconds: 50×1M=50M */
    nanosleep(&ts, NULL);
}
```

`nanosleep()` is in `<time.h>`, which is already included at the top of
`main_x11.c`. No new `#include` needed.

### Why not just `sleep(1)` or `usleep(50000)`?

`sleep(n)` sleeps for whole seconds — way too coarse.
`usleep(n)` is deprecated on modern Linux.
`nanosleep()` is the current POSIX standard. It takes a `struct timespec`
split into whole seconds and the nanosecond remainder.

For 50ms:

```
50ms = 50 milliseconds
     = 0 full seconds       (50 / 1000 = 0 in integer division)
     = 50,000,000 nanoseconds (50 % 1000 = 50, then × 1,000,000)

  tv_sec  = 0
  tv_nsec = 50,000,000
```

The `L` on `1000000L` forces the compiler to treat it as a `long`. Without it,
`50 * 1000000` could overflow a 32-bit `int` (max ~2 billion; we're fine here,
but it's good practice for larger values like `(ms % 1000) * 1000000`).

For a sanity check: 1500ms (1.5 seconds):

```
1500 / 1000 = 1     (1 full second → tv_sec = 1)
1500 % 1000 = 500   (500ms remaining)
500 * 1000000 = 500,000,000 nanoseconds = 0.5 seconds → tv_nsec = 500000000
```

---

## Step 4: Implement `platform_sleep_ms()` in `main_raylib.c`

Find the stub and **replace it**:

```c
void platform_sleep_ms(int ms) {
    WaitTime((double)ms / 1000.0);
}
```

`WaitTime()` is Raylib's sleep function. It takes seconds as a `double`.
`(double)ms / 1000.0` converts milliseconds to seconds: `50 → 0.05`.

The cast to `(double)` is required. Without it:
```c
50 / 1000    = 0    ← integer division truncates to zero — sleeps nothing!
(double)50 / 1000.0 = 0.05  ← correct
```

---

## Step 5: Implement `platform_should_quit()` in Both Files

### `main_x11.c` — find the stub and replace it:

```c
int platform_should_quit(void) {
    return g_should_quit;
}
```

`g_should_quit` is the global flag already set in `platform_get_input()` when
the user presses Q, Escape, or clicks the ✕ button. This function just exposes it.

### `main_raylib.c` — find the stub and replace it:

```c
int platform_should_quit(void) {
    return WindowShouldClose();
}
```

`WindowShouldClose()` returns `1` when the user clicks ✕ or presses Escape.
Raylib handles the OS window close protocol internally.

---

## Step 6: Write `tetris_tick()` in `tetris.c`

This is the main addition in this lesson. Open `src/tetris.c` and add this new
function **after** `tetris_apply_input()`:

```c
/* Run one tick of game logic (called every 50ms by the platform layer).
 * Processes player input, then advances the speed counter.
 * When the counter hits the speed limit, the piece is forced down one row.
 * Locking and spawning are not handled here yet — added in lesson 09. */
void tetris_tick(GameState *state, const PlatformInput *input) {

    if (state->game_over) return;  /* frozen — nothing happens */

    /* ── 1. Handle player movement and rotation ── */
    tetris_apply_input(state, input);

    /* ── 2. Advance the speed counter ── */
    state->speed_count++;

    /* Not yet time to force a drop — exit early. */
    if (state->speed_count < state->speed)
        return;

    /* Time to drop! Reset the counter. */
    state->speed_count = 0;

    /* ── 3. Try to move the piece down one row ── */
    if (tetris_does_piece_fit(state, state->current_piece, state->current_rotation,
                              state->current_x, state->current_y + 1)) {
        state->current_y++;   /* fits — drop it */
    }
    /* If it doesn't fit, the piece has landed.
     * For now: do nothing. Piece just sits at the floor.
     * Lesson 09 adds locking and spawning here. */
}
```

Now add the declaration to `src/tetris.h` in the Public API section:

```c
/* Run one tick of game logic.
 * Handles input, gravity, and (in lesson 09) locking and spawning.
 * Call every 50ms from the platform layer. */
void tetris_tick(GameState *state, const PlatformInput *input);
```

### The speed counter, visualized

With `speed = 20`:

```
Tick:          1    2    3    4    5   ...  18   19   20   21   22
speed_count:   1    2    3    4    5        18   19   20    1    2
Drop piece?   no   no   no   no   no       no   no  YES   no   no
                                                     ↑
                                              speed_count == speed
                                              → reset to 0 → drop
```

`20 ticks × 50ms = 1000ms = 1 drop per second.`

If you change `speed = 10`, the piece drops every 10 ticks = 500ms = twice per
second. The smaller the number, the faster the drop. Minimum is `10` (fastest).

---

## Step 7: Update `main()` in Both Files

### `src/main_x11.c` — replace `main()`:

```c
int main(void) {
    srand((unsigned int)time(NULL));

    GameState     state;
    PlatformInput input;

    platform_init();
    tetris_init(&state);

    while (!platform_should_quit()) {
        /* ── 1. Sleep: pace the loop to ~20 ticks/second ── */
        platform_sleep_ms(50);

        /* ── 2. Input: read what the player did this tick ── */
        platform_get_input(&input);

        if (input.quit) break;

        /* ── 3. Tick: advance game state ──
         * If game over and restart pressed, reset the game.
         * Otherwise run normal game logic. */
        if (state.game_over && input.restart)
            tetris_init(&state);
        else
            tetris_tick(&state, &input);

        /* ── 4. Render: draw the updated state ── */
        platform_render(&state);
    }

    platform_shutdown();
    printf("Final score: %d\n", state.score);
    return 0;
}
```

### `src/main_raylib.c` — replace `main()` with the identical code.

The loop is now `sleep → input → tick → render`. This order matters:

- **Sleep first**: every tick takes the same wall-clock time, regardless of how
  long the logic takes. The game runs at the same speed on all machines.
- **Input before tick**: `tetris_tick()` reads the input. If we swapped them,
  we'd act on last tick's input.
- **Render after tick**: we show what the state looks like *after* updating, not
  before.

Note: `tetris_apply_input()` is now called *inside* `tetris_tick()`. Remove any
direct call to `tetris_apply_input()` from `main()` — it's no longer needed there.

---

## Step 8: Build and Run

```bash
./build_x11.sh    && ./tetris_x11
./build_raylib.sh && ./tetris_raylib
```

### What to test

| What to do | Expected result |
|------------|----------------|
| Wait without pressing anything | Piece falls one row per second |
| Press ← while falling | Piece slides left while continuing to fall |
| Press Z while falling | Piece rotates while continuing to fall |
| Piece reaches the floor | Piece stops — stays there (no locking yet) |
| Press Q or Escape | Game exits, prints final score |
| Click ✕ | Game exits cleanly |

### What's missing (lesson 09)

When the piece hits the floor, it stops forever. No new piece appears. That's
intentional — the purpose of this lesson is just to get the timing right. Lesson
09 adds locking and spawning.

---

## Key Concept: Tick-Based Loops vs JavaScript's `setInterval`

If you've written browser games, you've probably done:

```js
setInterval(() => {
  readInput();
  updateGame();
  render();
}, 50);
```

Our C game loop does the same thing, but synchronously:

```c
while (running) {
  sleep(50);   // wait
  input();     // read
  tick();      // update
  render();    // draw
}
```

No callbacks. No async. No event loop machinery. The program just pauses for
50ms, does its work, pauses again. Simpler to reason about — you always know
exactly what order things happen.

The mental model: each loop iteration is one "frame" of a flipbook. Sleep is
the pause between flipping pages. 20 flips per second = smooth enough for
Tetris.

---

## Exercise

**Experiment with speed values.**

In `tetris_init()`, temporarily change `state->speed = 20` to these values
and observe the effect. Change it back to 20 when done.

```c
state->speed = 5;   /* drops 4 times/second — very fast */
state->speed = 40;  /* drops every 2 seconds — very slow */
state->speed = 1;   /* drops every tick — basically instant */
```

This gives you visceral intuition for what the speed counter controls.
The number is "how many 50ms ticks before the next drop."

---

## Checkpoint

Files changed in this lesson:

| File | What changed |
|------|-------------|
| `src/tetris.h` | Added `speed`, `speed_count`, `piece_count` to `GameState`; declared `tetris_tick()` |
| `src/tetris.c` | Set new fields in `tetris_init()`; added `tetris_tick()` |
| `src/main_x11.c` | Implemented `platform_sleep_ms()`; implemented `platform_should_quit()`; updated `main()` |
| `src/main_raylib.c` | Implemented `platform_sleep_ms()`; implemented `platform_should_quit()`; updated `main()` |

**Next:** Lesson 09 — Locking, Spawning & Game Over
