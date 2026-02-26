# Lesson 09 — Input System: `GameButtonState` + `UPDATE_BUTTON`

## By the end of this lesson you will have:

A `GameButtonState` struct and `UPDATE_BUTTON` macro that correctly track "just pressed this frame" for snake turning, with `prepare_input_frame` resetting the per-frame state, and `XkbSetDetectableAutoRepeat` preventing phantom key-repeat events in X11.

---

## The Problem with Polling Input

Old snake code:

```c
if (platform_key_down(KEY_LEFT))
    state.dir = DIR_LEFT;
```

`platform_key_down` returns `1` whenever the key is held. It fires every frame. Problems:
1. A single tap registers for many frames (fast player taps 16ms, game checks at 60fps = 1 frame → fine; slow player holds for 3 frames → direction changes 3 times in a row but only one should count)
2. Input is checked once per snake step (every 150ms), so finer-grained "just pressed" is irrelevant — but the architecture doesn't tell you whether the player pressed or held
3. Can't distinguish "press" from "hold" — important for restart (should fire once, not every frame)

---

## Step 1 — `GameButtonState`

```c
typedef struct {
    int half_transition_count;  /* number of press/release edges this frame */
    int ended_down;             /* 1 if button is down at end of frame */
} GameButtonState;
```

**`half_transition_count` (htc):**
Counts how many transitions happened this frame. Press = 1 htc. Release = 1 htc. Press+release in one frame = 2 htc. Just held with no change = 0 htc.

**`ended_down`:**
`1` if the key is currently down at the end of the frame. Updated each time `UPDATE_BUTTON` is called.

**"Just pressed" test:**
```c
int just_pressed = button.ended_down && button.half_transition_count > 0;
```

This reads: "the button is down AND at least one edge happened this frame."

---

## Step 2 — `UPDATE_BUTTON` macro

```c
#define UPDATE_BUTTON(btn, is_down)                     \
    do {                                                 \
        if ((btn).ended_down != (is_down)) {             \
            (btn).half_transition_count++;               \
        }                                                \
        (btn).ended_down = (is_down);                    \
    } while (0)
```

Call this every time an event arrives for a button:

```c
/* X11 KeyPress event */
case KeyPress:
    if (sym == XK_Left  || sym == XK_a) UPDATE_BUTTON(input.turn_left,  1);
    if (sym == XK_Right || sym == XK_d) UPDATE_BUTTON(input.turn_right, 1);
    if (sym == XK_r)                    UPDATE_BUTTON(input.restart,     1);
    break;
case KeyRelease:
    if (sym == XK_Left  || sym == XK_a) UPDATE_BUTTON(input.turn_left,  0);
    if (sym == XK_Right || sym == XK_d) UPDATE_BUTTON(input.turn_right, 0);
    if (sym == XK_r)                    UPDATE_BUTTON(input.restart,     0);
    break;
```

**New C concept — `do { ... } while (0)` macro:**
A macro that expands to multiple statements must be wrapped in `do { ... } while (0)`. This ensures it behaves correctly when used inside an `if` without braces:
```c
if (condition)
    UPDATE_BUTTON(btn, 1);   /* expands safely */
else
    /* ... */;
```
Without the wrapper, the `if (btn.ended_down != is_down)` line would become the `if` body and the `btn.ended_down = is_down` assignment would always execute — wrong behavior.

---

## Step 3 — `prepare_input_frame`

Called once at the start of each frame, before `platform_get_input`:

```c
static void prepare_input_frame(GameInput *input) {
    input->turn_left.half_transition_count  = 0;
    input->turn_right.half_transition_count = 0;
    input->restart.half_transition_count    = 0;
    input->quit = 0;
}
```

This resets per-frame state. `ended_down` is intentionally NOT reset — a button held across frames stays `ended_down=1`. Only `half_transition_count` resets to 0 at the frame boundary.

Visually:
```
Frame N:   [press]  htc=1, ended_down=1    → just_pressed=1 → turn fires
Frame N+1: [held]   htc=0, ended_down=1    → just_pressed=0 → no turn
Frame N+2: [held]   htc=0, ended_down=1    → just_pressed=0 → no turn
Frame N+3: [release] htc=1, ended_down=0   → just_pressed=0 → no turn
```

The turn fires exactly once on the first frame the key is pressed. Holding does nothing.

---

## Step 4 — `XkbSetDetectableAutoRepeat`

Without this:

```
User holds LEFT arrow for 1 second.
X11 generates: KeyPress, [silence 500ms], KeyPress, KeyPress, KeyPress, ...
```

Those auto-repeat `KeyPress` events are *not* real presses. The key was never released. But X11 synthesizes a fake `KeyRelease` followed by a fake `KeyPress` for each repeat:

```
KeyPress(LEFT)        ← real
KeyRelease(LEFT)      ← FAKE (auto-repeat)
KeyPress(LEFT)        ← FAKE (auto-repeat)
KeyRelease(LEFT)      ← FAKE
KeyPress(LEFT)        ← FAKE
```

Each fake pair triggers `UPDATE_BUTTON(btn, 0)` then `UPDATE_BUTTON(btn, 1)` → `htc` increments twice per repeat event. The turn-left action fires repeatedly while held.

Fix:
```c
Bool supported;
XkbSetDetectableAutoRepeat(display, True, &supported);
```

After this call, X11 suppresses the fake `KeyRelease`/`KeyPress` pairs and sends only `KeyPress` events while the key is held (with `htc` = 0 after the first frame since `ended_down` doesn't change). The `GameButtonState` correctly reports "held, not newly pressed."

---

## Step 5 — Using the input in `snake_update`

```c
void snake_update(GameState *s, const GameInput *input, float delta_time) {
    if (s->game_over) {
        if (input->restart.ended_down && input->restart.half_transition_count > 0)
            snake_init(s);
        return;
    }

    /* Turn: only on just-pressed */
    if (input->turn_left.ended_down && input->turn_left.half_transition_count > 0)
        s->next_dir = (SNAKE_DIR)((s->current_dir + 3) % 4);
    if (input->turn_right.ended_down && input->turn_right.half_transition_count > 0)
        s->next_dir = (SNAKE_DIR)((s->current_dir + 1) % 4);

    /* ... delta-time movement ... */
}
```

Turn math:
- Turn left: `(dir + 3) % 4` (avoids negative modulo)
- Turn right: `(dir + 1) % 4`

With `SNAKE_DIR_UP=0, RIGHT=1, DOWN=2, LEFT=3`:
- From UP (0): turn right → 1 (RIGHT), turn left → 3 (LEFT) ✓
- From LEFT (3): turn right → 0 (UP), turn left → 2 (DOWN) ✓

---

## Key Concepts

- `GameButtonState { half_transition_count, ended_down }` — tracks press edges, not just state
- `htc > 0 && ended_down` → "just pressed this frame"
- `UPDATE_BUTTON(btn, is_down)` — call on every key event, increments `htc` on state change
- `prepare_input_frame` — resets `htc` to 0 each frame; does NOT reset `ended_down`
- `XkbSetDetectableAutoRepeat` — suppresses X11 synthetic key-repeat events
- Turn math: `(dir+3)%4` = turn left, `(dir+1)%4` = turn right (no negatives)

---

## Exercise

The snake currently ignores 180° reversals — you can turn into your own body by pressing left then right in the same snake step. Add a guard: in `snake_update`, when applying `next_dir`, check if the new direction is the opposite of `current_dir`. If so, ignore the turn (keep `next_dir = current_dir`). Opposite direction: `(dir + 2) % 4`. Where exactly in `snake_update` should this guard go?
