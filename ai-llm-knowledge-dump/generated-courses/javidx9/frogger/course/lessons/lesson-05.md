# Lesson 05 — `GameInput`: `GameButtonState` + `UPDATE_BUTTON` + `XkbSetDetectableAutoRepeat`

## By the end of this lesson you will have:

A `GameInput` struct with `GameButtonState` for all four movement directions, `UPDATE_BUTTON` macro for tracking per-frame transitions, and correct input handling in both X11 and Raylib backends.

---

## The Problem with `IsKeyReleased` / Plain `int` Flags

### Old approach:
```c
typedef struct {
    int up_released;
    int down_released;
    int left_released;
    int right_released;
} InputState;
```

Frogger moved on key *release* — tap the key, frog hops one tile when you let go. The problems:

1. **Platform inconsistency:** Raylib's `IsKeyReleased` is naturally one-shot per frame. X11's `KeyRelease` is event-based but also one-shot. So both worked, but the mechanism was different — the abstraction leaked through.

2. **Auto-repeat (X11):** Without `XkbSetDetectableAutoRepeat`, a held key generates synthetic `KeyRelease` + `KeyPress` pairs every ~30ms. Each `KeyRelease` fired `up_released=1` → the frog hopped repeatedly.

3. **Not extensible:** A plain `int` doesn't tell you if the key was tapped or held this frame.

---

## Step 1 — `GameButtonState`

```c
typedef struct {
    int half_transition_count;  /* edge count this frame */
    int ended_down;             /* 1 = currently pressed */
} GameButtonState;
```

**`half_transition_count` (htc):**
Number of state-change *edges* this frame. Press = 1. Release = 1. Tap (press+release in one frame) = 2. Held with no change = 0.

**"Just pressed" check:**
```c
btn.ended_down && btn.half_transition_count > 0
```
Fires on the first frame the key goes down. Holding yields `ended_down=1, htc=0` on subsequent frames → condition false → no repeated action.

---

## Step 2 — `UPDATE_BUTTON` macro

```c
#define UPDATE_BUTTON(button, is_down)                    \
    do {                                                   \
        if ((button).ended_down != (is_down)) {            \
            (button).half_transition_count++;              \
            (button).ended_down = (is_down);               \
        }                                                  \
    } while (0)
```

Call on every press/release event:
- `KeyPress`: `UPDATE_BUTTON(input->move_up, 1)`
- `KeyRelease`: `UPDATE_BUTTON(input->move_up, 0)`

Only increments `htc` if the state actually changed — prevents double-counting if the same event fires twice.

---

## Step 3 — `prepare_input_frame`

Called once at the **start** of each frame, before `platform_get_input`:

```c
void prepare_input_frame(GameInput *input) {
    input->move_up.half_transition_count    = 0;
    input->move_down.half_transition_count  = 0;
    input->move_left.half_transition_count  = 0;
    input->move_right.half_transition_count = 0;
    input->quit = 0;
}
```

Resets `htc` to 0 each frame. `ended_down` is **not** reset — a held key stays `ended_down=1` across frames.

Frame timeline:
```
Frame N:   [key goes down]  htc=1, ended_down=1  → just-pressed → frog hops
Frame N+1: [key held]       htc=0, ended_down=1  → not just-pressed → no hop
Frame N+2: [key still held] htc=0, ended_down=1  → no hop
Frame N+3: [key released]   htc=1, ended_down=0  → not pressed → no hop
```

The frog hops exactly once per tap. Holding doesn't cause repeated hops.

---

## Step 4 — X11: `XkbSetDetectableAutoRepeat`

Without this call, holding a key generates:

```
KeyPress    ← real
KeyRelease  ← FAKE (auto-repeat)
KeyPress    ← FAKE (auto-repeat)
KeyRelease  ← FAKE
KeyPress    ← FAKE
...
```

Each fake pair calls `UPDATE_BUTTON(btn, 0)` then `UPDATE_BUTTON(btn, 1)` → `htc` increments twice per repeat. The "just pressed" condition fires every ~30ms while held.

After `XkbSetDetectableAutoRepeat(g_display, True, &supported)`:
- Fake `KeyRelease`+`KeyPress` pairs are suppressed
- X11 sends only the initial `KeyPress`; subsequent frames see `ended_down=1, htc=0`
- The frog hops exactly once

**Note:** `XkbKeycodeToKeysym` replaces the old `XLookupKeysym`. It's the modern XKB-aware function:
```c
KeySym sym = XkbKeycodeToKeysym(g_display, e.xkey.keycode, 0, 0);
```

---

## Step 5 — Raylib: `IsKeyDown`

Raylib doesn't have auto-repeat issues, but we still use `UPDATE_BUTTON` for consistency:

```c
int up = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
UPDATE_BUTTON(input->move_up, up);
```

`IsKeyDown` returns 1 every frame the key is held. `UPDATE_BUTTON` converts this to `htc` transitions. On the first frame it goes down: `ended_down` was 0, now 1 → `htc++`. Subsequent held frames: `ended_down` stays 1 → no increment.

This gives identical behavior between X11 and Raylib.

---

## Step 6 — Using input in `frogger_tick`

```c
if (input->move_up.ended_down && input->move_up.half_transition_count > 0)
    state->frog_y -= 1.0f;
if (input->move_down.ended_down && input->move_down.half_transition_count > 0)
    state->frog_y += 1.0f;
if (input->move_left.ended_down && input->move_left.half_transition_count > 0)
    state->frog_x -= 1.0f;
if (input->move_right.ended_down && input->move_right.half_transition_count > 0)
    state->frog_x += 1.0f;
```

One hop per just-pressed event, regardless of frame rate or how long the key is held.

---

## Key Concepts

- `GameButtonState { htc, ended_down }` — tracks per-frame edges, not just state
- `UPDATE_BUTTON(btn, is_down)` — call on every key event; increments `htc` on state change
- `prepare_input_frame` — resets `htc` each frame; does NOT reset `ended_down`
- `htc > 0 && ended_down` → "just pressed this frame" — fires exactly once per tap
- `XkbSetDetectableAutoRepeat` — suppresses fake auto-repeat events in X11
- `XkbKeycodeToKeysym` — modern XKB-aware replacement for `XLookupKeysym`

---

## Exercise

The frog currently can't hop twice quickly in one frame (e.g., two arrow keys pressed in one 16ms frame). With `GameButtonState` you can check `htc >= 2` to detect a tap (press+release in same frame). Is this useful for frogger? What about a game where you fire a gun — should rapid tapping within one frame fire twice?
