# Lesson 07 — Delta-Time Game Loop and GameActionRepeat

## By the end of this lesson you will have:

A game loop driven by real elapsed time — pieces fall one row per second automatically, regardless of frame rate. `GameActionRepeat` wraps the drop timer, and `tetris_update(state, input, delta_time)` replaces `tetris_tick`.

---

## Why Delta Time?

The original `tetris_tick` used a counter: increment every frame, drop when counter reaches 20. At 50ms per tick that's 1 second per drop. But:

- What if the frame takes 60ms instead of 50ms? The counter still reaches 20, but 1.2 seconds have passed.
- What if you want to run at 144 FPS? Ticks fire 7x more often — the game runs 7x faster.

**Delta time** fixes this: instead of counting frames, accumulate real time.

```c
/* Tick-based (original): */
state->speed_count++;
if (state->speed_count >= state->speed) {
    state->speed_count = 0;
    /* drop piece */
}

/* Delta-time: */
state->tetromino_drop.timer += delta_time;
if (state->tetromino_drop.timer >= state->tetromino_drop.interval) {
    state->tetromino_drop.timer -= state->tetromino_drop.interval;
    /* drop piece */
}
```

The subtraction trick (`timer -= interval` instead of `timer = 0`) preserves the leftover time — if the timer fires 5ms "late," the next interval is 5ms shorter. Precision accumulates rather than being lost each frame.

---

## Step 1 — `GameActionRepeat` recap

From Lesson 05:

```c
typedef struct {
    float timer;    /* accumulated time (seconds) */
    float interval; /* fire threshold */
} GameActionRepeat;
```

Used for `tetromino_drop` in `GameState` and `flash_timer` in `completed_lines`. The same struct serves both because they have identical behavior: accumulate time, fire when threshold reached.

---

## Step 2 — Update the main loop signature

Replace `tetris_tick(state, input)` with:

```c
void tetris_update(GameState *state, GameInput *input, float delta_time);
```

`delta_time` is computed in `main()`:

```c
g_last_frame_time = get_time();

while (g_is_running) {
    double current_time = get_time();
    float  delta_time   = (float)(current_time - g_last_frame_time);
    g_last_frame_time   = current_time;

    prepare_input_frame(&game_input);
    platform_get_input(&game_input);

    if (game_input.quit) break;

    if (game_state.game_over && game_input.restart) {
        game_init(&game_state, &game_input);
    } else {
        tetris_update(&game_state, &game_input, delta_time);
    }

    tetris_render(&backbuffer, &game_state);
    platform_display_backbuffer(&backbuffer);

    if (!g_x11.vsync_enabled) {
        double elapsed = get_time() - current_time;
        if (elapsed < g_target_frame_time)
            sleep_ms((int)((g_target_frame_time - elapsed) * 1000.0));
    }
}
```

`delta_time` is a `float` — 32-bit precision is plenty for sub-second intervals. `double` is used for the raw timestamps to avoid precision loss over long program runs.

---

## Step 3 — `prepare_input_frame`

Add to `tetris.c`:

```c
void prepare_input_frame(GameInput *input) {
    input->move_left.button.half_transition_count  = 0;
    input->move_right.button.half_transition_count = 0;
    input->move_down.button.half_transition_count  = 0;
    input->rotate_x.button.half_transition_count   = 0;
    input->rotate_x.value = TETROMINO_ROTATE_X_NONE;
}
```

**Why this must be called each frame:**  
`half_transition_count` counts state changes *within one frame*. It must be reset to zero at the start of each frame so the new frame's events are counted from scratch. If you forget to call this, the count accumulates across frames and input becomes unreliable.

This is called before `platform_get_input` fills in new events for the frame.

---

## Step 4 — Non-blocking input in X11

The original X11 input loop used `XNextEvent` which **blocks** until an event arrives. With a fixed-tick game that's fine — but a delta-time game loop must never block.

Replace blocking `XNextEvent` with non-blocking `XPending`:

```c
void platform_get_input(GameInput *input) {
    XEvent event;

    /* XPending() returns number of events in queue — non-blocking */
    while (XPending(g_x11.display) > 0) {
        XNextEvent(g_x11.display, &event);

        switch (event.type) {
        case KeyPress: {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            switch (key) {
            case XK_q:
            case XK_Q:
            case XK_Escape:
                g_is_running = false;
                input->quit  = 1;
                break;
            case XK_Left: case XK_A: case XK_a:
                UPDATE_BUTTON(input->move_left.button, 1);
                break;
            case XK_Right: case XK_D: case XK_d:
                UPDATE_BUTTON(input->move_right.button, 1);
                break;
            case XK_Down: case XK_S: case XK_s:
                UPDATE_BUTTON(input->move_down.button, 1);
                break;
            case XK_x: case XK_X:
                UPDATE_BUTTON(input->rotate_x.button, 1);
                input->rotate_x.value = TETROMINO_ROTATE_X_GO_RIGHT;
                break;
            case XK_z: case XK_Z:
                UPDATE_BUTTON(input->rotate_x.button, 1);
                input->rotate_x.value = TETROMINO_ROTATE_X_GO_LEFT;
                break;
            case XK_r: case XK_R:
                input->restart = 1;
                break;
            }
            break;
        }
        case KeyRelease: {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            switch (key) {
            case XK_Left: case XK_A: case XK_a:
                UPDATE_BUTTON(input->move_left.button, 0);
                break;
            case XK_Right: case XK_D: case XK_d:
                UPDATE_BUTTON(input->move_right.button, 0);
                break;
            case XK_Down: case XK_S: case XK_s:
                UPDATE_BUTTON(input->move_down.button, 0);
                break;
            case XK_x: case XK_X: case XK_z: case XK_Z:
                UPDATE_BUTTON(input->rotate_x.button, 0);
                input->rotate_x.value = TETROMINO_ROTATE_X_NONE;
                break;
            case XK_r: case XK_R:
                input->restart = 0;
                break;
            }
            break;
        }
        case ClientMessage:
            g_is_running = false;
            input->quit  = 1;
            break;
        case ConfigureNotify:
            if (event.xconfigure.width  != g_x11.width ||
                event.xconfigure.height != g_x11.height) {
                g_x11.width  = event.xconfigure.width;
                g_x11.height = event.xconfigure.height;
                glViewport(0, 0, g_x11.width, g_x11.height);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glOrtho(0, g_x11.width, g_x11.height, 0, -1, 1);
                glMatrixMode(GL_MODELVIEW);
            }
            break;
        }
    }
}
```

**New: `XkbSetDetectableAutoRepeat`:**  
Without this call, X11 generates synthetic repeated KeyPress events while a key is held. Your `platform_get_input` would see a stream of KeyPress/KeyRelease pairs even when the key is just held. `XkbSetDetectableAutoRepeat(display, True, &supported)` in `platform_init` tells X11: "send only one KeyPress on press, one KeyRelease on release — no fake repeats." Our `GameActionWithRepeat` system (Lesson 11) handles its own repeat logic.

**`ConfigureNotify` handling:**  
When the window is resized, X11 sends `ConfigureNotify` events. We update the OpenGL viewport and projection matrix so the backbuffer image scales correctly.

---

## Step 5 — Implement `tetris_update`

In `tetris.c`:

```c
void tetris_update(GameState *state, GameInput *input, float delta_time) {
    if (state->game_over) return;

    /* ── Flash animation: freeze all game logic while lines flash ── */
    if (state->completed_lines.flash_timer.timer > 0) {
        state->completed_lines.flash_timer.timer -= delta_time;

        if (state->completed_lines.flash_timer.timer <= 0) {
            /* Timer expired: collapse all completed rows, bottom-to-top */
            for (int i = state->completed_lines.count - 1; i >= 0; i--) {
                int line_index = state->completed_lines.indexes[i];

                for (int py = line_index; py > 0; --py) {
                    for (int px = 1; px < FIELD_WIDTH - 1; ++px) {
                        state->field[py * FIELD_WIDTH + px] =
                            state->field[(py - 1) * FIELD_WIDTH + px];
                    }
                }
                for (int px = 1; px < FIELD_WIDTH - 1; px++)
                    state->field[px] = TETRIS_FIELD_EMPTY;

                /* Adjust remaining line indices upward (they shifted down) */
                for (int j = i - 1; j >= 0; --j) {
                    if (state->completed_lines.indexes[j] < line_index)
                        state->completed_lines.indexes[j]++;
                }
            }
            state->completed_lines.count = 0;
        }
        return; /* freeze input and gravity while flashing */
    }

    /* ── Handle input ── (Lesson 11 adds full auto-repeat) */
    tetris_apply_input(state, input, delta_time);

    /* ── Gravity: accumulate time, drop when interval reached ── */
    state->tetromino_drop.timer += delta_time;

    float drop_interval = state->tetromino_drop.interval
                        - (state->level > 0 ? state->level * 0.01f : 0.0f);
    if (drop_interval < 0.1f) drop_interval = 0.1f;

    if (state->tetromino_drop.timer >= drop_interval) {
        state->tetromino_drop.timer -= drop_interval;  /* keep remainder! */

        if (tetromino_does_piece_fit(state,
                state->current_piece.index, state->current_piece.rotation,
                state->current_piece.x, state->current_piece.y + 1)) {
            state->current_piece.y++;
        } else {
            /* Lock the piece */
            /* ... (covered fully in Lesson 05-06 locking logic) ... */
        }
    }
}
```

**Precision trick — `timer -= interval` vs `timer = 0`:**

```
Frame 1: delta=0.016, timer=0.816 (fires!), timer -= 0.8 → timer=0.016
Frame 2: delta=0.016, timer=0.032
...
No precision lost — the 0.016s "overshoot" carries forward.

vs. reset:
Frame 1: timer fires, timer = 0  (0.016s overshoot discarded!)
Frame 2: timer = 0.016 (started from 0, not 0.016)
```

Over time, the "subtract not reset" version drifts less — drops stay closer to exactly 0.8s intervals.

---

## Step 6 — Difficulty scaling

```c
/* Level increases every 25 pieces locked */
if (state->pieces_count % 25 == 0)
    state->level++;

/* Drop interval decreases with level — minimum 100ms */
float drop_interval = state->tetromino_drop.interval
                    - (state->level * 0.01f);
if (drop_interval < 0.1f) drop_interval = 0.1f;
```

Delta time makes difficulty scaling trivial — just change `drop_interval`. With tick-based systems you'd change `state->speed` and hope the conversion to real time stays correct.

---

## Frame Pacing

**With VSync:** `glXSwapBuffers` blocks until the next monitor refresh (typically 16.7ms at 60Hz). No sleep needed — delta_time is naturally ~16.7ms per frame.

**Without VSync:** After rendering, compute elapsed time and sleep the remainder:
```c
double elapsed = get_time() - frame_start;
if (elapsed < g_target_frame_time)
    sleep_ms((int)((g_target_frame_time - elapsed) * 1000.0));
```

---

## Key Concepts

- `delta_time` = seconds since last frame — accumulate for physics, not frames
- `timer -= interval` (not `= 0`) — preserves sub-frame precision
- `XPending` + `while` — non-blocking input loop, never stalls the game
- `XkbSetDetectableAutoRepeat` — suppress X11's synthetic key repeat events
- `prepare_input_frame` — reset transition counts before each frame's events
- `ConfigureNotify` — update OpenGL viewport on window resize

---

## Exercise

Implement "slow motion" mode: when a debug key (e.g., `Tab`) is held, call `tetris_update(&state, &input, delta_time * 0.25f)`. What happens to the drop timer? Does anything else in the game slow down? Why is this trivial with delta time but hard with tick-based timing?
