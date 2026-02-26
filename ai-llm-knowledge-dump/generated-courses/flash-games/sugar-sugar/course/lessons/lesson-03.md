# Lesson 03 — Single Grain + Gravity

**By the end of this lesson you will have:**
One grain spawns at position (320, 20) when the game starts. It falls downward under gravity, accelerating as it goes. When it exits the bottom of the screen it disappears. Press R to reset the grain to the start position.

---

## Step 1 — Why Float, Not Int

Grain position needs to be `float`, not `int`. Here is why.

At 60 fps, `dt ≈ 0.0167` seconds. Starting from rest, after one frame:

```
velocity  = 0 + GRAVITY * dt
          = 0 + 280 * 0.0167
          = 4.68 px/frame   ← first frame

But on frame 1 from rest:
  vy     = 0 + 280 * 0.0167 = 4.68
  y_new  = 0 + 4.68 * 0.0167 = 0.078 pixels

0.08 pixels.
```

If `y` were an `int`, `0.14` would truncate to `0`. The grain would not move for many frames. With `float`, sub-pixel movement accumulates correctly. By frame 7 the grain has moved about 5 pixels — smooth.

In TypeScript you never think about this because JS numbers are always 64-bit floats. In C you choose: `int` = integer, `float` = 32-bit float, `double` = 64-bit float. `float` is fine here — 24 bits of mantissa gives sub-pixel precision well beyond what a 640-pixel screen needs.

---

## Step 2 — Semi-Implicit Euler Integration

This is the physics update. Two lines of math:

```c
vy += GRAVITY * gravity_sign * dt;   /* acceleration → velocity */
y  += vy * dt;                       /* velocity → position     */
```

"Semi-implicit Euler" means we update velocity first, then use the **new** velocity to update position. This is slightly more stable than using the old velocity for both.

Concrete numbers for one frame at 60 fps (dt = 0.01667, starting from rest at y = 20):

```
Frame 1:
  vy = 0    + 280 * 1 * 0.01667 = 4.67
  y  = 20   + 4.67 * 0.01667   = 20.08

Frame 2:
  vy = 4.67 + 280 * 0.01667    = 9.34
  y  = 20.08 + 9.34 * 0.01667 = 20.24

Frame 10:
  vy ≈ 46.7 px/s
  y  ≈ 23.9
```

After ~0.5 seconds the grain is clearly falling. After ~1.3 seconds it has crossed most of the screen. The lighter gravity gives a relaxed, floaty pour matching the original Sugar, Sugar game.

---

## Step 3 — Terminal Velocity Cap

Without a cap, a grain falling for 5 seconds reaches `vy = 280 * 5 = 1400 px/s`. The sub-step collision (Lesson 04) handles this, but capping is still good practice:

```c
#define GRAVITY        280.0f
#define TERM_VEL       400.0f
```

After integrating velocity, clamp it:

```c
if (vy >  TERM_VEL) vy =  TERM_VEL;
if (vy < -TERM_VEL) vy = -TERM_VEL;
```

At `TERM_VEL = 600`, a grain moves at most 10 pixels per frame (600/60). The sub-step in Lesson 04 will handle up to 10 collision checks per frame — exactly matching.

---

## Step 4 — Single Grain in game.c

For this lesson we use a single local grain struct inside `game_update`. We will replace this with `GrainPool` in Lesson 05. The single-grain version lets you understand the physics before adding the complexity of managing many grains.

Add these constants near the top of `game.c`:

```c
#define GRAVITY   280.0f
#define TERM_VEL  400.0f
```

Add a temporary grain to `GameState` by adding these fields. Open `game.h` and add them inside `GameState` (just after `gravity_sign`):

```c
    /* Lesson 03 temporary single-grain (removed in lesson 05) */
    float grain_x, grain_y;
    float grain_vx, grain_vy;
    int   grain_active;
```

Update `game_init` in `game.c` to spawn the grain:

```c
void game_init(GameState *state, GameBackbuffer *backbuffer) {
    (void)backbuffer;
    memset(state, 0, sizeof(*state));
    state->gravity_sign = 1;
    state->unlocked_count = 1;

    /* Spawn single grain */
    state->grain_x      = 320.0f;
    state->grain_y      = 20.0f;
    state->grain_vx     = 0.0f;
    state->grain_vy     = 0.0f;
    state->grain_active = 1;
}
```

---

## Step 5 — Update the Grain

Replace `game_update` in `game.c`:

```c
void game_update(GameState *state, GameInput *input, float delta_time) {
    if (BUTTON_PRESSED(input->escape)) {
        state->should_quit = 1;
        return;
    }

    /* R → reset grain and clear lines */
    if (BUTTON_PRESSED(input->reset)) {
        memset(state->lines.pixels, 0, sizeof(state->lines.pixels));
        state->grain_x      = 320.0f;
        state->grain_y      = 20.0f;
        state->grain_vx     = 0.0f;
        state->grain_vy     = 0.0f;
        state->grain_active = 1;
    }

    /* Draw lines with mouse */
    if (input->mouse.left.ended_down) {
        draw_brush_line(
            &state->lines,
            input->mouse.prev_x, input->mouse.prev_y,
            input->mouse.x,      input->mouse.y,
            BRUSH_RADIUS
        );
    }

    /* Update single grain */
    if (state->grain_active) {
        /* 1. Integrate */
        state->grain_vy += GRAVITY * (float)state->gravity_sign * delta_time;
        state->grain_vx += 0.0f;   /* no horizontal force yet */

        /* 2. Clamp to terminal velocity */
        if (state->grain_vy >  TERM_VEL) state->grain_vy =  TERM_VEL;
        if (state->grain_vy < -TERM_VEL) state->grain_vy = -TERM_VEL;

        /* 3. Move */
        state->grain_y += state->grain_vy * delta_time;
        state->grain_x += state->grain_vx * delta_time;

        /* 4. Deactivate when off-screen */
        if (state->grain_y > (float)CANVAS_H ||
            state->grain_y < 0.0f            ||
            state->grain_x < 0.0f            ||
            state->grain_x > (float)CANVAS_W) {
            state->grain_active = 0;
        }
    }
}
```

---

## Step 6 — Render the Grain

The grain's position is a `float`. We cast to `int` to get a pixel address:

```c
void game_render(const GameState *state, GameBackbuffer *backbuffer) {
    /* 1. Background */
    draw_rect(backbuffer, 0, 0, backbuffer->width, backbuffer->height, COLOR_BG);

    /* 2. Lines */
    render_lines(&state->lines, backbuffer);

    /* 3. Single grain — draw as 1 cream/tan pixel */
    if (state->grain_active) {
        int px = (int)state->grain_x;
        int py = (int)state->grain_y;
        /*
         * The cast truncates toward zero.
         * grain_x = 320.73  →  px = 320
         * grain_x = 319.99  →  px = 319
         *
         * This is correct: sub-pixel position is kept in the float;
         * we only round to a pixel for rendering.
         */
        draw_pixel(backbuffer, px, py, GAME_RGB(210, 180, 140));  /* tan/wheat */
    }
}
```

---

## Build & Run

```bash
cd course
clang -Wall -Wextra -O2 -o sugar \
    src/main_x11.c src/game.c src/levels.c \
    -lX11 -lm
./sugar
```

**What you should see:**
A single tan pixel appears near the top-center of the window and falls downward, accelerating as it goes. When it exits the bottom of the screen it disappears. Draw a horizontal line and... the grain passes right through it (collision comes in Lesson 04). Press R to reset the grain and clear lines.

---

## Key Concepts

- `float` vs `int` for position: use `float` so sub-pixel movement accumulates. An `int` position would truncate `0.14 px` to `0` and the grain would stick for several frames.
- Semi-implicit Euler: update `vy` first, then use the new `vy` to update `y`. Two lines: `vy += a*dt;  y += vy*dt;`. Sufficient for a game — no need for Runge-Kutta.
- `GRAVITY = 280.0f`: pixels per second per second. Lighter than a naive "real-world" scaling — gives grains a floaty, sugar-like feel. After one second of free fall, `vy = 280 px/s` and the grain has travelled ~140 px.
- `(int)grain_x`: truncating cast. The float stores the "true" sub-pixel position; the int is only used for pixel-grid operations (rendering, collision).
- Terminal velocity cap: without it, grains build up unbounded speed during the first frame after a debug pause. The cap at 600 px/s keeps speed reasonable.
- Deactivation on exit: we check all four screen edges. `grain_active = 0` prevents further updates and rendering without "removing" anything from memory.
- **`input->mouse.prev_x` must be set ONCE per frame, not once per event.** If we update `prev_x = x` inside the event loop on each `MotionNotify`, after processing N events `prev_x` holds only the position from event N-1. `draw_brush_line(prev_x, prev_y, x, y)` then draws only the last tiny segment, leaving visible gaps when the mouse moves fast. The fix: set `prev_x = mouse.x` at the TOP of `platform_get_input`, BEFORE the event while-loop. Only `mouse.x/y` are updated inside the loop. This way Bresenham always interpolates the full start-of-frame → end-of-frame path.

---

## Exercise

Add a horizontal force so the grain drifts right as it falls. In `game_update`, set `state->grain_vx = 30.0f` immediately after the grain spawns (in the `game_init` call or the reset block), then observe the curved path. Notice how the float position naturally produces a parabolic arc.
