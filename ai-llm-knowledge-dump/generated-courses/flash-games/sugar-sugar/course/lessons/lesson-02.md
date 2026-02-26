# Lesson 02 — Mouse Input + Line Drawing

**By the end of this lesson you will have:**
Hold the left mouse button and drag to draw dark lines anywhere on the canvas. The lines persist between frames. Press R to erase all lines and start fresh. The window still closes with Escape.

---

## Step 1 — How Button State Works

In JavaScript a keydown event fires once per physical press (plus repeat events). You track state yourself if you need "is it held right now?"

In our engine every button carries two pieces of information:

```
GameButtonState {
    half_transition_count   ← how many state changes happened this frame
    ended_down              ← is the key currently held? (1 or 0)
}
```

`half_transition_count` is called "half" because a full press-and-release cycle is **two** transitions: down → up.

Examples for one frame:

```
Key tapped quickly:
  half_transition_count = 2   (down, then up)
  ended_down            = 0   (released by frame end)

Key just pressed, still held:
  half_transition_count = 1
  ended_down            = 1

Key held from last frame, still held:
  half_transition_count = 0   (no change this frame)
  ended_down            = 1

Key not touched at all:
  half_transition_count = 0
  ended_down            = 0
```

The two macros you'll use constantly:

```c
#define BUTTON_PRESSED(b)   ((b).half_transition_count > 0 && (b).ended_down)
#define BUTTON_RELEASED(b)  ((b).half_transition_count > 0 && !(b).ended_down)
```

`BUTTON_PRESSED` is true only on the **first frame** the button goes down — like `mousedown`, not `mousemove`. This is what you want for "toggle gravity once per press."

---

## Step 2 — UPDATE_BUTTON: Recording a Transition

`platform_get_input` calls this macro when X11 reports a key or mouse event:

```c
#define UPDATE_BUTTON(button, is_down)                     \
    do {                                                   \
        if ((button).ended_down != (is_down)) {            \
            (button).half_transition_count++;              \
            (button).ended_down = (is_down);               \
        }                                                  \
    } while(0)
```

Read it step by step for a KeyPress event (is_down = 1):

```
Before:  ended_down=0, half_transition_count=0
Check:   0 != 1  → true, so we enter the if
After:   ended_down=1, half_transition_count=1
```

For a KeyRelease event (is_down = 0) on the same frame (rare but possible):

```
Before:  ended_down=1, half_transition_count=1
Check:   1 != 0  → true
After:   ended_down=0, half_transition_count=2
```

The `do { ... } while(0)` wrapper makes the macro safe inside an `if` without braces. This is a standard C macro idiom — it forces the macro to behave like a single statement.

---

## Step 3 — prepare_input_frame: Reset Per-Frame Data

At the start of each frame we zero the transition counts but **keep** `ended_down`:

```c
/* Already in game.c from lesson 01: */
void prepare_input_frame(GameInput *input) {
    input->mouse.left.half_transition_count  = 0;
    input->mouse.right.half_transition_count = 0;
    input->escape.half_transition_count      = 0;
    input->reset.half_transition_count       = 0;
    input->gravity.half_transition_count     = 0;
    input->mouse.prev_x = input->mouse.x;
    input->mouse.prev_y = input->mouse.y;
}
```

Why not reset `ended_down`? Because if you're holding a key, the OS only sends one KeyPress event. It does NOT re-send it every frame. So if we zeroed `ended_down`, the key would appear "released" on every frame after the first.

The loop is:

```
Frame N:   KeyPress arrives  → UPDATE_BUTTON sets ended_down=1, count=1
Frame N+1: prepare clears count to 0, ended_down stays 1
           No new KeyPress → count stays 0, ended_down stays 1
Frame N+2: same
...
Frame M:   KeyRelease arrives → UPDATE_BUTTON sets ended_down=0, count=1
```

So `ended_down = 1` means "held right now." `count > 0` means "changed this frame."

---

## Step 4 — LineBitmap: A 1-Bit Canvas

We need to store which pixels have been drawn on. We do NOT store the color — line pixels are always `COLOR_LINE`. We only need a flag.

```c
/* Already defined in game.h: */
typedef struct {
    uint8_t pixels[CANVAS_W * CANVAS_H];
} LineBitmap;
```

`uint8_t` is an unsigned 8-bit integer (0–255). We use it as a boolean: 0 = empty, 1 = has line.

Memory: 640 × 480 × 1 byte = **307,200 bytes = 300 KB**. Small enough to live inside `GameState` without a heap allocation.

Address formula — same pattern as the backbuffer:

```
index = y * CANVAS_W + x

Pixel at (5, 2):
  index = 2 * 640 + 5 = 1285
```

To set a pixel: `lb->pixels[y * CANVAS_W + x] = 1;`
To clear all:   `memset(lb->pixels, 0, sizeof(lb->pixels));`

---

## Step 5 — Bresenham Line: stamp_circle + draw_brush_line

A mouse drag goes from `(prev_x, prev_y)` to `(x, y)`. If you just stamp a circle at the endpoint you get gaps when the mouse moves fast. We need to fill every pixel along the path.

Bresenham's line algorithm walks from one point to another in integer steps, always choosing the pixel closest to the ideal line. No division, no floating point.

Here is the algorithm with a concrete example first:

```
Draw a line from (2,1) to (6,4):
  dx = 6-2 = 4
  dy = 4-1 = 3
  Step in x (longer axis).
  For each x from 2 to 6, compute the nearest y.

  x=2, y=1   ← start
  x=3, y=2
  x=4, y=2
  x=5, y=3
  x=6, y=4   ← end

The algorithm uses integer error accumulation instead of floats.
```

Now the brush version. Instead of setting one pixel per step, we stamp a filled circle (radius = brush size):

Add these functions to `game.c`:

```c
static void stamp_circle(LineBitmap *lb, int cx, int cy, int r, uint8_t val) {
    /*
     * r*r test avoids sqrt — just compare squared distances.
     * For r=3: pixels within 9 units² of center are filled.
     */
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy <= r*r) {
                int px = cx + dx;
                int py = cy + dy;
                if (px >= 0 && px < CANVAS_W && py >= 0 && py < CANVAS_H) {
                    lb->pixels[py * CANVAS_W + px] = val;
                }
            }
        }
    }
}

static void draw_brush_line(LineBitmap *lb,
                             int x0, int y0,
                             int x1, int y1,
                             int radius) {
    /*
     * Bresenham's line algorithm.
     * Walks from (x0,y0) to (x1,y1) one pixel at a time.
     * At each step, stamps a circle of the given radius.
     */
    int dx =  abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;   /* step direction: +1 or -1 */
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;            /* error accumulator */

    while (1) {
        stamp_circle(lb, x0, y0, radius, 1);

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
        /*
         * e2 = 2*err is compared against dy and dx to decide
         * whether to step in x, y, or both.
         * This keeps the line as straight as possible.
         */
    }
}
```

---

## Step 6 — Rendering Lines

When we render, we scan every pixel in the LineBitmap. If it is non-zero, we write `COLOR_LINE` to the backbuffer at the same position.

Add this to `game.c`:

```c
static void render_lines(const LineBitmap *lb, GameBackbuffer *bb) {
    /*
     * Scan all 307,200 pixels.
     * Branchy, but modern CPUs handle this well.
     * Optimization (if ever needed): SIMD or dirty-rect tracking.
     */
    for (int y = 0; y < CANVAS_H; y++) {
        for (int x = 0; x < CANVAS_W; x++) {
            if (lb->pixels[y * CANVAS_W + x]) {
                bb->pixels[y * bb->width + x] = COLOR_LINE;
            }
        }
    }
}
```

---

## Step 7 — Wire It All Together

Update `game_update` and `game_render` in `game.c`:

```c
#define BRUSH_RADIUS 4

void game_update(GameState *state, GameInput *input, float delta_time) {
    (void)delta_time;

    /* Escape → quit */
    if (BUTTON_PRESSED(input->escape)) {
        state->should_quit = 1;
        return;
    }

    /* R key → clear all lines */
    if (BUTTON_PRESSED(input->reset)) {
        memset(state->lines.pixels, 0, sizeof(state->lines.pixels));
    }

    /* Left mouse held → draw a line from prev to current position */
    if (input->mouse.left.ended_down) {
        draw_brush_line(
            &state->lines,
            input->mouse.prev_x, input->mouse.prev_y,
            input->mouse.x,      input->mouse.y,
            BRUSH_RADIUS
        );
    }
}

void game_render(const GameState *state, GameBackbuffer *backbuffer) {
    /* 1. Fill background */
    draw_rect(backbuffer, 0, 0, backbuffer->width, backbuffer->height, COLOR_BG);

    /* 2. Draw lines on top */
    render_lines(&state->lines, backbuffer);
}
```

Note the render order: background first, then lines. Each frame we re-fill the background and then re-draw the lines from the LineBitmap. This is the **double-buffer, clear-and-redraw** pattern — every frame is a fresh paint.

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
Hold the left mouse button and drag — dark lines appear. Draw ramps, bowls, anything. Press R to erase everything. Lines persist across frames until erased.

---

## Key Concepts

- `GameButtonState`: two fields — `ended_down` (is it held?) and `half_transition_count` (did it change this frame?). The pair together tell you everything about a button.
- `BUTTON_PRESSED(b)`: true only on the frame the button first goes down. Use for one-shot actions (toggle, reset).
- `ended_down` is **not** reset by `prepare_input_frame` — the OS only sends one KeyPress event per press, so we must remember the held state ourselves.
- `LineBitmap pixels[y*W+x]`: 1 byte per canvas pixel used as a boolean. Zero = empty, non-zero = line.
- Bresenham's line: integer-only line rasterisation. Avoids floating-point by tracking an integer error term.
- `stamp_circle` with `dx*dx + dy*dy <= r*r`: fills a circle without `sqrt`. The squared-distance comparison is faster and exact.
- Clear-and-redraw: every frame starts by filling the backbuffer with the background color. Persistent state lives in `LineBitmap` (and later `GrainPool`), not in the pixels themselves.

---

## Exercise

Change `BRUSH_RADIUS` to `8` and rebuild. Lines will be much thicker. Then try `1` for a single-pixel pen. Notice that at radius 1 fast mouse movement leaves gaps — that's exactly what `draw_brush_line` (vs just stamping at the endpoint) is solving.
