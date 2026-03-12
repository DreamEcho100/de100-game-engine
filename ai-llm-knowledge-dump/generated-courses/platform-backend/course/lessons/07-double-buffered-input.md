# Lesson 07 — Double-Buffered Input

## Overview

| Item | Value |
|------|-------|
| **Backend** | Both |
| **New concepts** | `GameButtonState`, `UPDATE_BUTTON`, `prepare_input_frame`, `platform_swap_input_buffers` |
| **Observable outcome** | Press Space → yellow bar appears; release → bar disappears; Q/Esc quits |
| **Files created** | `src/game/base.h` |
| **Files modified** | `src/platforms/raylib/main.c`, `src/platforms/x11/main.c`, `src/game/demo.c` |

---

## What you'll build

A double-buffered input system: each frame produces a clean snapshot of button states, including how many times each button changed state (half-transitions).

---

## Background

### JS analogy

Imagine two snapshots of a gamepad object:

```js
prevInput = { ...currInput };          // save last frame
currInput.space.endedDown = false;     // reset before events
currInput.space.halfTransitions = 0;

for (const event of pendingEvents) {
    if (event.key === 'Space') {
        currInput.space.endedDown = event.type === 'keydown';
        currInput.space.halfTransitions++;
    }
}
```

After processing, `prevInput` has last frame's state and `currInput` has this frame's state.  Game code reads `currInput`; next frame we swap.

### Why double-buffer?

Single-buffer (mutate in place): the game might read a button state that is half-way through being updated by the event loop — a data race.

Double-buffer: the game always reads from the "completed previous frame" snapshot (`prev`).  Events fill the "work in progress" snapshot (`curr`).  At the start of each frame we swap.

### `GameButtonState`

```c
typedef struct {
    int ended_down;        // 1 if button is held at frame end
    int half_transitions;  // number of state changes this frame
} GameButtonState;
```

`half_transitions > 0 && ended_down` → button was just **pressed** this frame.
`half_transitions > 0 && !ended_down` → button was just **released** this frame.
`half_transitions == 0` → button is held or untouched.

### `UPDATE_BUTTON` macro

```c
#define UPDATE_BUTTON(btn_ptr, is_down_val) \
    do { \
        GameButtonState *_b = (btn_ptr); \
        if (_b->ended_down != (is_down_val)) { \
            _b->ended_down = (is_down_val); \
            _b->half_transitions++; \
        } \
    } while (0)
```

Called by the platform event loop for each key event.

### Why `platform_swap_input_buffers` is in `game/base.h`, not `platform.h`

This function only touches two `GameInput` pointers — no OS handles, no platform APIs:

```c
static inline void platform_swap_input_buffers(GameInput **curr, GameInput **prev) {
    GameInput *tmp = *curr;
    *curr = *prev;
    *prev = tmp;
}
```

It belongs beside `prepare_input_frame` (which it's always paired with), not in the platform contract.

---

## What to type

### `src/game/base.h` (key sections)

```c
typedef struct {
    int ended_down;
    int half_transitions;
} GameButtonState;

static inline int button_just_pressed(GameButtonState b) {
    return b.ended_down && b.half_transitions > 0;
}

typedef struct {
    union {
        GameButtonState all[2];
        struct {
            GameButtonState quit;
            GameButtonState play_tone;
        };
    } buttons;
} GameInput;

#define UPDATE_BUTTON(btn_ptr, is_down_val) \
    do { \
        GameButtonState *_b = (btn_ptr); \
        if (_b->ended_down != (is_down_val)) { \
            _b->ended_down = (is_down_val); \
            _b->half_transitions++; \
        } \
    } while (0)

static inline void prepare_input_frame(GameInput *curr, const GameInput *prev) {
    int n = (int)(sizeof(curr->buttons.all) / sizeof(curr->buttons.all[0]));
    for (int i = 0; i < n; i++) {
        curr->buttons.all[i].ended_down       = prev->buttons.all[i].ended_down;
        curr->buttons.all[i].half_transitions = 0;
    }
}

static inline void platform_swap_input_buffers(GameInput **curr, GameInput **prev) {
    GameInput *tmp = *curr; *curr = *prev; *prev = tmp;
}
```

### Main loop pattern (both backends)

```c
GameInput inputs[2];
memset(inputs, 0, sizeof(inputs));
GameInput *curr_input = &inputs[0];
GameInput *prev_input = &inputs[1];

while (running) {
    // 1. Swap: prev gets last frame's final state; curr gets a fresh copy
    platform_swap_input_buffers(&curr_input, &prev_input);
    // 2. Prepare: copy ended_down from prev; reset half_transitions
    prepare_input_frame(curr_input, prev_input);
    // 3. Fill: platform event loop writes into curr_input
    process_events(curr_input, &running);   // X11: XPending loop / Raylib: IsKeyDown
    // 4. React: game reads curr_input
    if (button_just_pressed(curr_input->buttons.play_tone))
        game_play_sound_at(&audio, SOUND_TONE_MID);
}
```

### X11 — `XPending` event drain loop

```c
while (XPending(g_x11.display) > 0) {
    XEvent ev;
    XNextEvent(g_x11.display, &ev);
    switch (ev.type) {
    case KeyPress: {
        KeySym sym = XkbKeycodeToKeysym(g_x11.display, ev.xkey.keycode, 0, 0);
        if (sym == XK_space) UPDATE_BUTTON(&curr->buttons.play_tone, 1);
        if (sym == XK_Escape || sym == XK_q) {
            UPDATE_BUTTON(&curr->buttons.quit, 1); *is_running = 0;
        }
        break;
    }
    case KeyRelease: {
        KeySym sym = XkbKeycodeToKeysym(g_x11.display, ev.xkey.keycode, 0, 0);
        if (sym == XK_space) UPDATE_BUTTON(&curr->buttons.play_tone, 0);
        break;
    }
    case ClientMessage:
        if ((Atom)ev.xclient.data.l[0] == g_x11.wm_delete_window)
            *is_running = 0;
        break;
    }
}
```

**`XkbKeycodeToKeysym`** — preferred over `XLookupKeysym`; handles XKB keyboard layouts correctly.

**`XPending` drain loop** — process ALL pending events per frame (not just one).  If you use `XNextEvent` without `XPending > 0`, the call blocks, stalling the game loop.

### `src/game/demo.c` — L07 addition

```c
// Show yellow bar when Space is held
if (input && input->buttons.play_tone.ended_down) {
    draw_rect(bb, 50, 240, 300, 30, COLOR_YELLOW);
    draw_text(bb, 55, 247, 1, "SPACE: play tone", COLOR_BLACK);
} else {
    draw_text(bb, 50, 247, 1, "SPACE: play tone", COLOR_GRAY);
}
```

---

## Explanation

| Concept | Detail |
|---------|--------|
| `half_transitions` | Counts how many times the button changed state this frame.  Tapped (press+release in one frame) → 2 transitions. |
| `prepare_input_frame` | Carries `ended_down` forward (held keys stay held), resets `half_transitions` to 0 |
| `platform_swap_input_buffers` | Pointer swap — O(1), no data copy |
| `XPending` drain | Without this, `XNextEvent` blocks the game loop when no events are pending |
| `XkbKeycodeToKeysym` | Maps hardware scancode → X11 KeySym accounting for current keyboard layout |

---

## Build and run

```sh
./build-dev.sh --backend=x11 -r
```

Press and hold Space — the yellow bar appears.  Release — it disappears.  Press Q or Esc to quit.

---

## Quiz

1. What does `half_transitions = 2` mean?  Give a real-world example of when this happens.
2. Why do we call `prepare_input_frame(curr, prev)` **after** swapping, not before?
3. Why is `XPending > 0` checked before `XNextEvent`?  What happens without it?
