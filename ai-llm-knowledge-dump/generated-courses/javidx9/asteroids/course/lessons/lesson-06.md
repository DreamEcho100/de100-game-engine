# Lesson 06 — GameInput + GameButtonState

## By the end of this lesson you will have:
Input that distinguishes between "just pressed this frame", "held down", and "just released". You will see the ship fire exactly one bullet per spacebar press — holding space does not rapid-fire. You will also understand why X11 sends fake key events during a hold and how `XkbSetDetectableAutoRepeat` fixes it.

---

## Step 1 — Why a simple `bool` is not enough

Imagine tracking a key with a single boolean:

```c
int space_held; // 1 = down, 0 = up
```

You can detect "is it held right now" but you cannot distinguish:
- **Just pressed** (first frame of a hold)
- **Held** (continuing hold, second frame onward)
- **Just released** (first frame after release)

Games need all three. The ship should thrust every frame the up arrow is held (use "held"). But firing should only trigger once per press (use "just pressed"). A death animation should trigger once per player death (use "just pressed", not "held").

A simple bool also misses events that happen and unhappen within a single frame — for example, a very fast tap that lasts less than one frame period (~16ms). The player pressed and released within the frame, but ended_down is 0 (released), so the press is completely invisible. This matters in fighting games, rhythm games, and anywhere precise input is required.

---

## Step 2 — GameButtonState: two fields that together capture everything

```c
typedef struct {
    int half_transition_count; /* state changes this frame */
    int ended_down;            /* 1 = currently held, 0 = released */
} GameButtonState;
```

`ended_down` — the final state at the end of the frame. Same as the simple bool.

`half_transition_count` (htc) — how many times the button changed state this frame:
- `0` = no change (was held all frame, or was up all frame)
- `1` = changed once (pressed or released exactly once)
- `2` = pressed and released within one frame (very fast tap)
- `3` = press, release, press all within one frame (extremely fast)

With these two fields together:

```
"Just pressed"  = ended_down == 1  AND  htc > 0
"Just released" = ended_down == 0  AND  htc > 0
"Held"          = ended_down == 1  (htc doesn't matter)
"Was tapped"    = ended_down == 0  AND  htc >= 2
```

**JS analogy:** Think of `ended_down` as `event.type === 'keydown'` at the end of the frame, and `half_transition_count` as counting how many `keydown` + `keyup` events arrived during the frame. If two events arrived and it ended up as keydown, it went: up → down → up → down (htc=3, ended_down=1).

---

## Step 3 — The UPDATE_BUTTON macro

```c
#define UPDATE_BUTTON(btn, is_down)                     \
    do {                                                 \
        if ((btn).ended_down != (is_down)) {             \
            (btn).half_transition_count++;               \
            (btn).ended_down = (is_down);                \
        }                                                \
    } while (0)
```

The key detail: it only increments `half_transition_count` **when the state actually changes**. If the key is already down and we call `UPDATE_BUTTON(btn, 1)` again, nothing happens.

**Why `do { ... } while (0)`?** It makes the macro safe to use without braces:

```c
if (something)
    UPDATE_BUTTON(input->left, 1);
else
    do_other_thing();
```

Without the `do/while`, the `if` body would only cover the first statement in the macro expansion, causing a bug. The `do/while(0)` forces the macro body to behave as a single statement.

**JS analogy:** It's exactly like the guard at the start of a state machine transition: "only transition if we're not already in this state."

```js
function updateButton(btn, isDown) {
    if (btn.endedDown !== isDown) {
        btn.halfTransitionCount++;
        btn.endedDown = isDown;
    }
}
```

---

## Step 4 — How UPDATE_BUTTON is called in platform_get_input

```c
void platform_get_input(GameInput *input) {
    while (XPending(g_display)) {
        XEvent e;
        XNextEvent(g_display, &e);

        if (e.type == KeyPress) {
            KeySym sym = XkbKeycodeToKeysym(g_display, e.xkey.keycode, 0, 0);
            if (sym == XK_Left)   UPDATE_BUTTON(input->left,  1);
            if (sym == XK_Right)  UPDATE_BUTTON(input->right, 1);
            if (sym == XK_Up)     UPDATE_BUTTON(input->up,    1);
            if (sym == XK_space)  UPDATE_BUTTON(input->fire,  1);
            if (sym == XK_Escape) { g_is_running = 0; input->quit = 1; }
        }
        if (e.type == KeyRelease) {
            KeySym sym = XkbKeycodeToKeysym(g_display, e.xkey.keycode, 0, 0);
            if (sym == XK_Left)  UPDATE_BUTTON(input->left,  0);
            if (sym == XK_Right) UPDATE_BUTTON(input->right, 0);
            if (sym == XK_Up)    UPDATE_BUTTON(input->up,    0);
            if (sym == XK_space) UPDATE_BUTTON(input->fire,  0);
        }
    }
}
```

`XPending` returns the number of events waiting. We drain the entire queue with `XNextEvent`, calling `UPDATE_BUTTON` for each key event. Because `UPDATE_BUTTON` only records a real state change, calling it 10 times with `is_down=1` while the key is held only counts as 1 transition, not 10.

---

## Step 5 — prepare_input_frame: resetting between frames

```c
void prepare_input_frame(GameInput *input) {
    input->left.half_transition_count  = 0;
    input->right.half_transition_count = 0;
    input->up.half_transition_count    = 0;
    input->fire.half_transition_count  = 0;
    input->quit                        = 0;
}
```

This resets `half_transition_count` to 0 at the start of every frame, **but does not touch `ended_down`**. The "held" state persists across frames. The "just pressed" detection works because htc starts at 0 each frame and increments only on a real change.

If we forgot this reset, `htc` would keep accumulating across all frames. After holding left for 1 second at 60 fps with no transitions, htc would be 0 — but with auto-repeat (the X11 problem below), it could be 60 or more.

---

## Step 6 — "Just pressed" detection in asteroids_update

```c
if (input->fire.ended_down && input->fire.half_transition_count > 0) {
    float nose_x = state->player.x + sinf(state->player.angle) * SHIP_SCALE * 5.0f;
    float nose_y = state->player.y - cosf(state->player.angle) * SHIP_SCALE * 5.0f;
    add_bullet(state, nose_x, nose_y, state->player.angle);
}
```

`ended_down == 1` — the key is currently held.  
`half_transition_count > 0` — it changed state this frame, meaning this is the first frame of the press.

Together: "the key was just pressed this frame." This fires exactly once per press. If you hold space, `ended_down` stays 1 but `htc` resets to 0 next frame — so no second bullet.

Contrast with thrust, which fires every frame the key is held:

```c
if (input->up.ended_down) {
    state->player.dx += sinf(state->player.angle) * THRUST_ACCEL * dt;
    state->player.dy += -cosf(state->player.angle) * THRUST_ACCEL * dt;
}
```

No `htc` check — just `ended_down`. The ship accelerates every frame.

---

## Step 7 — The X11 auto-repeat problem and the fix

When you hold a key on X11, the OS generates fake `KeyRelease + KeyPress` pairs at the keyboard repeat rate (~30/sec). Without a fix, `platform_get_input` sees:

```
Frame 1:  KeyPress(space)   → htc=1, ended_down=1  → fires bullet ✓
Frame 2:  KeyRelease(space) → htc=1, ended_down=0
          KeyPress(space)   → htc=2, ended_down=1  → fires bullet AGAIN (wrong!)
Frame 3:  KeyRelease(space), KeyPress(space) → same problem
```

The fix: call `XkbSetDetectableAutoRepeat` once at startup:

```c
Bool supported;
XkbSetDetectableAutoRepeat(g_display, True, &supported);
```

With this enabled, X11 suppresses the fake `KeyRelease + KeyPress` pairs. When a key is held, you see only the initial `KeyPress` and the final `KeyRelease`. Now:

```
Frame 1:  KeyPress(space)  → htc=1, ended_down=1  → fires bullet ✓
Frame 2:  (no events)      → htc=0, ended_down=1  → no bullet ✓
Frame 3:  (no events)      → htc=0, ended_down=1  → no bullet ✓
Frame N:  KeyRelease(space) → htc=1, ended_down=0 → no bullet ✓
```

**Why does this matter?** Without it, holding space for half a second would fire 15 bullets instead of 1. The game would be unplayable.

---

## Build & Run

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11 -lX11 -lxkbcommon -lGL -lGLX -lm
./build/asteroids_x11
```

**What you should see:** Press space once — exactly one bullet fires from the ship's nose. Hold space — no rapid fire, exactly one bullet. Hold the up arrow — continuous thrust accumulates velocity. The terminal output should include `✓ Auto-repeat detection: enabled`.

---

## Key Concepts

- `GameButtonState` stores both `ended_down` (current state) and `half_transition_count` (transitions this frame).
- "Just pressed" = `ended_down == 1 && htc > 0`. "Held" = `ended_down == 1`. "Just released" = `ended_down == 0 && htc > 0`.
- `UPDATE_BUTTON` only increments `htc` when the state actually changes — calling it with the same state is a no-op.
- `prepare_input_frame` resets `htc` to 0 each frame without touching `ended_down`.
- `do { } while (0)` in macros makes them safe to use in single-statement `if` branches.
- `XkbSetDetectableAutoRepeat` suppresses fake KeyRelease+KeyPress pairs during a key hold, preventing unintended "just pressed" detections.

## Exercise

Add a "just released" log: in `platform_get_input`, after the key event processing loop, print to stderr when the fire button was just released:

```c
if (!input->fire.ended_down && input->fire.half_transition_count > 0) {
    fprintf(stderr, "fire released\n");
}
```

Run the game, press and release space a few times. Verify the message appears exactly once per release. Then remove the debug line.
