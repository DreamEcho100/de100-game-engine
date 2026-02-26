# Lesson 11 — Professional Input System: GameButtonState, GameActionWithRepeat

## By the end of this lesson you will have:

A professional-grade input system where:
- Button state tracks transitions, not just "is it down?"
- Movement keys auto-repeat independently (holding Left + Down works correctly)
- Rotation fires once per press, never auto-repeats
- The first press fires immediately, then repeats every N milliseconds

---

## The Problem with Simple Booleans

The original `PlatformInput` used simple flags:

```c
typedef struct {
    int move_left;   /* 1 if held this frame */
    int move_right;
    int move_down;
    int rotate;
} PlatformInput;
```

Three problems:

**1. "Just pressed" vs "held" are indistinguishable.**
```
Frame 1: key down → move_left=1 → piece moves
Frame 2: key held → move_left=1 → piece moves again
...60 moves per second at 60fps. Unplayable.
```

**2. No independent timing.**
If you add one shared timer to throttle movement, Left and Down share it. Hold Left + Down — one action waits for the other's timer.

**3. Fast taps miss.**
At 30fps, a 20ms tap starts and ends within one frame. A simple boolean captures only the final state — you'd see it as "not pressed" even though it happened.

---

## Step 1 — `GameButtonState`

This concept originates from Casey Muratori's Handmade Hero series:

```c
typedef struct {
    int half_transition_count;
    /* Number of state changes this frame.
     * 0 = held or idle (no change)
     * 1 = pressed this frame (down→up not yet)
     * 2 = full tap (down + up within one frame)
     * Higher values are theoretically possible but rare */

    int ended_down;
    /* Final state at end of frame.
     * 1 = pressed
     * 0 = released */
} GameButtonState;
```

**Why "half transition"?**  
A "half transition" is one state change: pressed→released OR released→pressed. A full tap (press + release) is two half transitions.

```
Timeline:                   Frame 1    Frame 2    Frame 3
──────────────────────────────────────────────────────────
Normal press + hold:        htc=1      htc=0      htc=0
                            down=1     down=1     down=1

Normal release:                                   htc=1
                                                  down=0

Fast tap (20ms in 33ms):    htc=2
                            down=0     (already released!)
```

Without `half_transition_count`, the fast tap in Frame 1 looks like "not pressed" because `ended_down=0`.

**"Button just pressed this frame" check:**
```c
bool just_pressed = button.ended_down && button.half_transition_count > 0;
```

---

## Step 2 — `UPDATE_BUTTON` macro

```c
#define UPDATE_BUTTON(button, is_down)                    \
    do {                                                  \
        if ((button).ended_down != (is_down)) {           \
            (button).half_transition_count++;             \
            (button).ended_down = (is_down);              \
        }                                                 \
    } while (0)
```

Call this in `platform_get_input` whenever a key state changes:

```c
case KeyPress:
    UPDATE_BUTTON(input->move_left.button, 1);  /* key went down */
    break;
case KeyRelease:
    UPDATE_BUTTON(input->move_left.button, 0);  /* key went up */
    break;
```

The macro only increments `half_transition_count` when the state actually changes (`ended_down != is_down`). Calling it twice with the same value is idempotent.

**New C concept — `do { ... } while (0)` in macros:**  
This idiom wraps multi-statement macros safely. Without it, `if (x) UPDATE_BUTTON(...)` could expand incorrectly. The `do { } while (0)` creates a single statement that requires a semicolon — behaves exactly like a function call at the usage site.

---

## Step 3 — `GameActionWithRepeat`

```c
typedef struct {
    GameButtonState  button;
    GameActionRepeat repeat;  /* from Lesson 05: { float timer; float interval; } */
} GameActionWithRepeat;
```

Used for: `move_left`, `move_right`, `move_down`.

---

## Step 4 — Full `GameInput`

```c
typedef struct {
    GameActionWithRepeat move_left;
    GameActionWithRepeat move_right;
    GameActionWithRepeat move_down;

    struct {
        GameButtonState          button;
        TETROMINO_ROTATE_X_VALUE value;  /* NONE / GO_LEFT / GO_RIGHT */
    } rotate_x;

    int quit;
    int restart;
} GameInput;
```

`rotate_x` does not use `GameActionWithRepeat` — rotation fires once per press, not repeatedly. The `value` field (NONE/GO_LEFT/GO_RIGHT) replaces the old `int rotate_direction` (-1/0/1).

---

## Step 5 — `prepare_input_frame`

```c
void prepare_input_frame(GameInput *input) {
    input->move_left.button.half_transition_count  = 0;
    input->move_right.button.half_transition_count = 0;
    input->move_down.button.half_transition_count  = 0;
    input->rotate_x.button.half_transition_count   = 0;
    input->rotate_x.value = TETROMINO_ROTATE_X_NONE;
}
```

**Must be called once per frame, before `platform_get_input`.**

`half_transition_count` counts changes within the current frame. Resetting it each frame means "these counts are for this frame only." The `ended_down` fields persist across frames — they represent the current state of the hardware key.

**Important:** `GameInput` is **not** zeroed with `memset` each frame. The `ended_down` fields and `repeat.timer` fields persist — they represent ongoing state. Only `half_transition_count` (per-frame events) is reset.

---

## Step 6 — `handle_action_with_repeat` in `tetris.c`

```c
static void handle_action_with_repeat(GameActionWithRepeat *action,
                                       float delta_time,
                                       int *should_trigger) {
    *should_trigger = 0;

    if (!action->button.ended_down) {
        /* Button is up — reset repeat timer */
        action->repeat.timer = 0.0f;
        return;
    }

    /* Button is down */
    if (action->button.half_transition_count > 0) {
        /* Just pressed this frame — trigger immediately, reset timer */
        *should_trigger = 1;
        action->repeat.timer = 0.0f;
    } else {
        /* Held from previous frame — accumulate time */
        action->repeat.timer += delta_time;
        if (action->repeat.timer >= action->repeat.interval) {
            action->repeat.timer -= action->repeat.interval;  /* keep remainder */
            *should_trigger = 1;
        }
    }
}
```

Three cases:

| Condition | Action |
|-----------|--------|
| `ended_down=0` | Reset timer, no trigger |
| `ended_down=1, transitions>0` | Just pressed — fire immediately, reset timer |
| `ended_down=1, transitions=0` | Held — accumulate timer, fire when interval reached |

The "keep remainder" subtraction (`timer -= interval` not `timer = 0`) ensures auto-repeats fire at precisely `interval` seconds apart even when frames aren't exactly that duration.

---

## Step 7 — `tetris_apply_input`

```c
static void tetris_apply_input(GameState *state, GameInput *input,
                                float delta_time) {
    /* ── Rotation: fires on just-pressed only (no auto-repeat) ── */
    if (input->rotate_x.button.ended_down &&
        input->rotate_x.button.half_transition_count > 0 &&
        input->rotate_x.value != TETROMINO_ROTATE_X_NONE) {

        TETROMINO_R_DIR new_rotation;

        if (input->rotate_x.value == TETROMINO_ROTATE_X_GO_RIGHT) {
            /* Advance one step clockwise: 0→90→180→270→0 */
            switch (state->current_piece.rotation) {
            case TETROMINO_R_0:   new_rotation = TETROMINO_R_90;  break;
            case TETROMINO_R_90:  new_rotation = TETROMINO_R_180; break;
            case TETROMINO_R_180: new_rotation = TETROMINO_R_270; break;
            case TETROMINO_R_270: new_rotation = TETROMINO_R_0;   break;
            }
        } else {
            /* Counter-clockwise */
            switch (state->current_piece.rotation) {
            case TETROMINO_R_0:   new_rotation = TETROMINO_R_270; break;
            case TETROMINO_R_90:  new_rotation = TETROMINO_R_0;   break;
            case TETROMINO_R_180: new_rotation = TETROMINO_R_90;  break;
            case TETROMINO_R_270: new_rotation = TETROMINO_R_180; break;
            }
        }

        if (tetromino_does_piece_fit(state,
                state->current_piece.index, new_rotation,
                state->current_piece.x, state->current_piece.y)) {
            state->current_piece.rotation = new_rotation;
        }
    }

    /* ── Horizontal movement: independent auto-repeat ── */
    int should_move_left  = 0;
    int should_move_right = 0;
    handle_action_with_repeat(&input->move_left,  delta_time, &should_move_left);
    handle_action_with_repeat(&input->move_right, delta_time, &should_move_right);

    if (should_move_left &&
        tetromino_does_piece_fit(state,
            state->current_piece.index, state->current_piece.rotation,
            state->current_piece.x - 1, state->current_piece.y))
        state->current_piece.x--;

    if (should_move_right &&
        tetromino_does_piece_fit(state,
            state->current_piece.index, state->current_piece.rotation,
            state->current_piece.x + 1, state->current_piece.y))
        state->current_piece.x++;

    /* ── Soft drop: independent auto-repeat (faster interval) ── */
    int should_move_down = 0;
    handle_action_with_repeat(&input->move_down, delta_time, &should_move_down);

    if (should_move_down &&
        tetromino_does_piece_fit(state,
            state->current_piece.index, state->current_piece.rotation,
            state->current_piece.x, state->current_piece.y + 1))
        state->current_piece.y++;
}
```

Each direction has its own `should_move_*` flag and its own `repeat.timer`. They never interfere with each other.

---

## DAS/ARR (Advanced Section)

Tetris Guideline games define two parameters:
- **DAS** (Delayed Auto Shift): how long to hold before auto-repeat starts
- **ARR** (Auto Repeat Rate): interval between repeats once started

Our `GameActionRepeat` implements ARR directly. For a more realistic feel, add DAS:

```c
typedef struct {
    float timer;
    float interval;
    float das_delay;  /* initial delay before first repeat */
    bool  das_done;   /* has DAS expired? */
} GameActionRepeatDAS;
```

Standard Tetris Guideline values: DAS = 133ms, ARR = 10ms.

---

## Key Concepts

- `GameButtonState`: `half_transition_count` + `ended_down` — captures sub-frame events
- `UPDATE_BUTTON(btn, is_down)` — call on KeyPress/KeyRelease to update state
- `prepare_input_frame` — reset `half_transition_count` once per frame (not `ended_down`)
- `handle_action_with_repeat` — three cases: up (reset), just-pressed (fire), held (accumulate)
- Each action has its own independent timer — holding multiple keys works correctly
- Rotation uses `just_pressed` check only — no auto-repeat
- `timer -= interval` (not `= 0`) — precision trick for consistent repeat timing

---

## Exercise

Implement a "hard drop" action: press Space to instantly teleport the piece to the lowest valid position. This action should:
1. Fire on just-pressed (no auto-repeat)
2. Use `tetromino_does_piece_fit` in a loop to find the drop distance
3. Lock the piece immediately after moving it

How does adding this to `GameInput` differ from adding another `GameActionWithRepeat`? What type should `hard_drop` be?
