# Lesson 06 — Cups + Win Condition + FSM

**By the end of this lesson you will have:**
A level with one emitter and one cup. Draw lines to guide grains into the cup. When the cup fills to its required count the screen overlays "LEVEL COMPLETE!" in large text. After two seconds the game advances to the next level (or loops back to level 1).

---

## Step 1 — The Cup Struct

A cup is a rectangular region on screen that counts grains landing inside it:

```c
/* Already in game.h: */
typedef struct {
    int        x, y, w, h;          /* screen rect — top-left corner + size */
    GRAIN_COLOR required_color;      /* which color of grain it accepts       */
    int        required_count;       /* how many grains needed to fill it     */
    int        collected;            /* how many have landed so far           */
} Cup;
```

When `collected >= required_count`, the cup is full.

Analogy in JavaScript: a `useState` counter that increments when the drop target fires, and a `useEffect` that checks if the counter reached the goal. Here it is simpler — we check every frame with a plain `if`.

---

## Step 2 — AABB Collision: Is a Grain Inside a Cup?

AABB stands for Axis-Aligned Bounding Box. Four comparisons, no trigonometry:

```
Cup at x=280, y=380, w=80, h=40
Cup occupies: x in [280, 360),  y in [380, 420)

Is grain at (310, 395) inside?
  310 >= 280  ✓
  310 <  360  ✓
  395 >= 380  ✓
  395 <  420  ✓
  → YES, inside the cup
```

In C:

```c
static int point_in_rect(int px, int py, int rx, int ry, int rw, int rh) {
    return px >= rx && px < (rx + rw) &&
           py >= ry && py < (ry + rh);
}
```

This is O(1). You call it once per active grain per cup per frame.

---

## Step 3 — Cup Collection Logic

Add this to `game.c`. It runs inside `game_update` after the grain physics:

```c
static void check_cups(GameState *state) {
    GrainPool *p = &state->grains;

    for (int i = 0; i < p->count; i++) {
        if (!p->active[i]) continue;

        int px = (int)p->x[i];
        int py = (int)p->y[i];

        for (int c = 0; c < state->level.cup_count; c++) {
            Cup *cup = &state->level.cups[c];

            /* Wrong color → skip */
            if (p->color[i] != (uint8_t)cup->required_color) continue;

            /* Not inside → skip */
            if (!point_in_rect(px, py, cup->x, cup->y, cup->w, cup->h))
                continue;

            /* Already full → skip (don't over-count) */
            if (cup->collected >= cup->required_count) continue;

            /* Collect: deactivate grain, increment counter */
            p->active[i] = 0;
            cup->collected++;
            break;   /* a grain can only land in one cup */
        }
    }
}
```

---

## Step 4 — Win Condition

```c
static int check_win(const GameState *state) {
    /*
     * Returns 1 if ALL cups are full, 0 otherwise.
     * Called every frame while PHASE_PLAYING.
     */
    for (int c = 0; c < state->level.cup_count; c++) {
        const Cup *cup = &state->level.cups[c];
        if (cup->collected < cup->required_count) return 0;
    }
    return 1;
}
```

---

## Step 5 — The Game Phase FSM

FSM = Finite State Machine. Our game is always in exactly one of these phases:

```
┌─────────────┐   any key    ┌──────────────┐
│ PHASE_TITLE │ ──────────→  │ PHASE_PLAYING│
└─────────────┘              └──────┬───────┘
                                    │ all cups full
                                    ↓
                           ┌──────────────────────┐
                           │ PHASE_LEVEL_COMPLETE  │
                           └──────────┬────────────┘
                                      │ 2 seconds elapsed
                                      ↓
                               (advance level,
                                go to PHASE_PLAYING)
```

Why an explicit FSM instead of a bunch of boolean flags?

In React you might write:

```js
const [isPlaying, setIsPlaying] = useState(false);
const [isComplete, setIsComplete] = useState(false);
const [showTitle, setShowTitle] = useState(true);
```

Then everywhere in the code you write `if (isPlaying && !isComplete && !showTitle)`. When you add a fourth state ("paused") you have to audit every condition. Flags interact in unexpected ways.

An FSM makes illegal states impossible. There is no `isPlaying = true` AND `isComplete = true` — only one phase is active at a time. Adding "paused" just adds one enum value and one transition.

---

## Step 6 — change_phase: Never Assign Directly

**Rule: never write `state->phase = PHASE_FOO;` anywhere except inside `change_phase`.**

```c
static void change_phase(GameState *state, GAME_PHASE next) {
    /*
     * Always transition through this function.
     * It resets the timer and is the single place to add
     * any on-enter side effects (play a sound, etc.).
     */
    state->phase       = next;
    state->phase_timer = 0.0f;
}
```

Why? If you scatter direct assignments throughout the codebase, you must remember to reset `phase_timer` every time. When you add a sound effect on phase entry, you must find and update every assignment. `change_phase` is the single point of control — like a Redux action creator.

---

## Step 7 — Level Loading

Add a `load_level` function. For now it loads from the `g_levels` array (defined in `levels.c`):

```c
static void load_level(GameState *state, int index) {
    /* Copy level definition from the global array */
    state->level = g_levels[index];
    state->level.index = index;

    /* Reset grain pool and lines */
    memset(&state->grains, 0, sizeof(state->grains));
    memset(&state->lines,  0, sizeof(state->lines));

    /* Reset all cup counters (they may have been zeroed by the copy,
     * but being explicit avoids subtle bugs if you hot-reload levels) */
    for (int c = 0; c < state->level.cup_count; c++) {
        state->level.cups[c].collected = 0;
    }
}
```

---

## Step 8 — levels.c: Level 0

Update `course/src/levels.c` with a real first level:

```c
#include "game.h"

LevelDef g_levels[30] = {
    /* Level 0 */
    [0] = {
        .index          = 0,
        .emitter_count  = 1,
        .emitters = {
            { .x = 320, .y = 30, .grains_per_second = 30, .spawn_timer = 0.0f }
        },
        .cup_count = 1,
        .cups = {
            {
                .x              = 260,
                .y              = 400,
                .w              = 120,
                .h              = 40,
                .required_color = GRAIN_WHITE,
                .required_count = 50,
                .collected      = 0
            }
        },
        .filter_count     = 0,
        .teleporter_count = 0,
        .obstacle_count   = 0,
        .has_gravity_switch = 0,
        .is_cyclic          = 0,
    },
    /* Levels 1–29 left as zeroed stubs for now */
};
```

---

## Step 9 — Cup Rendering

A cup is drawn as an empty rectangle. Inside it, a progress bar shows how full it is. Above it, the remaining count:

```c
static void render_cups(const GameState *state, GameBackbuffer *bb) {
    for (int c = 0; c < state->level.cup_count; c++) {
        const Cup *cup = &state->level.cups[c];

        /* Outline */
        uint32_t cup_color = GRAIN_COLORS[cup->required_color];
        /* Top edge */
        draw_rect(bb, cup->x,             cup->y,              cup->w, 2, cup_color);
        /* Bottom edge */
        draw_rect(bb, cup->x,             cup->y + cup->h - 2, cup->w, 2, cup_color);
        /* Left edge */
        draw_rect(bb, cup->x,             cup->y,              2, cup->h, cup_color);
        /* Right edge */
        draw_rect(bb, cup->x + cup->w - 2, cup->y,             2, cup->h, cup_color);

        /* Fill progress bar: grows upward from the bottom */
        if (cup->required_count > 0 && cup->collected > 0) {
            int fill_h = (cup->collected * (cup->h - 4)) / cup->required_count;
            /*
             * Example: required=50, collected=25, h=40
             *   fill_h = (25 * 36) / 50 = 900 / 50 = 18 pixels
             *
             * Integer division truncates toward zero — fine for a
             * visual indicator where 1-pixel accuracy doesn't matter.
             */
            int fill_y = cup->y + cup->h - 2 - fill_h;
            uint32_t fill_color = GAME_RGB(
                ((cup_color >> 16) & 0xFF) / 2,
                ((cup_color >>  8) & 0xFF) / 2,
                ( cup_color        & 0xFF) / 2
            );   /* half-brightness version of cup color */
            draw_rect(bb, cup->x + 2, fill_y, cup->w - 4, fill_h, fill_color);
        }

        /* Remaining count above the cup */
        int remaining = cup->required_count - cup->collected;
        if (remaining < 0) remaining = 0;
        draw_int(bb, cup->x + cup->w/2 - 8, cup->y - 14, remaining,
                 GAME_RGB(80, 80, 80));
    }
}
```

You will need the `draw_int` helper in `game.c`. Add it after `draw_text`:

```c
static void draw_int(GameBackbuffer *bb, int x, int y, int n, uint32_t color) {
    char buf[12];
    /* Manual integer-to-string: no sprintf needed */
    int i = 0;
    if (n < 0) { buf[i++] = '-'; n = -n; }
    if (n == 0) { buf[i++] = '0'; }
    else {
        char tmp[11]; int t = 0;
        while (n > 0) { tmp[t++] = (char)('0' + n % 10); n /= 10; }
        while (t-- > 0) buf[i++] = tmp[t + 1];  /* reverse */
    }
    buf[i] = '\0';
    draw_text(bb, x, y, buf, color);
}
```

> **Note:** `draw_text` uses the embedded font from `font.h`. We will add the full font file and `draw_text` implementation in the build section below.

---

## Step 10 — Full game_update with FSM

Replace `game_update` in `game.c`:

```c
void game_update(GameState *state, GameInput *input, float delta_time) {
    if (BUTTON_PRESSED(input->escape)) { state->should_quit = 1; return; }

    switch (state->phase) {

    case PHASE_TITLE:
        /* Any key starts the game */
        if (BUTTON_PRESSED(input->reset)   ||
            BUTTON_PRESSED(input->escape)  ||
            BUTTON_PRESSED(input->gravity)) {
            load_level(state, state->current_level);
            change_phase(state, PHASE_PLAYING);
        }
        break;

    case PHASE_PLAYING:
        state->phase_timer += delta_time;

        /* R → restart current level */
        if (BUTTON_PRESSED(input->reset)) {
            load_level(state, state->current_level);
            break;
        }

        /* Draw lines — but NOT when the mouse is over the bottom UI strip.
         * The Reset/Gravity buttons live there; clicking them must not also
         * leave a line on the canvas. */
        int over_ui = (input->mouse.y >= UI_BTN_Y - 4);
        if (input->mouse.left.ended_down && !over_ui) {
            draw_brush_line(
                &state->lines,
                input->mouse.prev_x, input->mouse.prev_y,
                input->mouse.x,      input->mouse.y,
                BRUSH_RADIUS
            );
        }

        /* On-screen Reset button click (mouse) */
        if (BUTTON_PRESSED(input->mouse.left) && state->reset_hover) {
            load_level(state, state->current_level);
            break;
        }

        /* Tick emitters */
        for (int e = 0; e < state->level.emitter_count; e++) {
            update_emitter(state, &state->level.emitters[e], delta_time);
        }

        /* Physics + collision */
        update_grains(state, delta_time);
        check_cups(state);

        /* Win check */
        if (check_win(state)) {
            change_phase(state, PHASE_LEVEL_COMPLETE);
        }
        break;

    case PHASE_LEVEL_COMPLETE:
        state->phase_timer += delta_time;

        if (state->phase_timer >= 2.0f) {
            /*
             * Advance to next level.
             * Wrap back to 0 if we have run out of defined levels
             * (g_levels beyond index 0 are currently zeroed stubs,
             *  so we stay on level 0 for now).
             */
            int next = state->current_level + 1;
            if (next >= 30 || g_levels[next].cup_count == 0) next = 0;
            state->current_level = next;
            if (next > state->unlocked_count - 1)
                state->unlocked_count = next + 1;
            load_level(state, next);
            change_phase(state, PHASE_PLAYING);
        }
        break;

    case PHASE_FREEPLAY:
        /* Lesson 07 */
        break;

    case PHASE_COUNT:
        break;
    }
}
```

---

## Step 11 — Full game_render with FSM

```c
void game_render(const GameState *state, GameBackbuffer *backbuffer) {
    draw_rect(backbuffer, 0, 0, backbuffer->width, backbuffer->height, COLOR_BG);

    switch (state->phase) {

    case PHASE_TITLE:
        draw_text(backbuffer, CANVAS_W/2 - 60, CANVAS_H/2 - 16,
                  "SUGAR, SUGAR", COLOR_LINE);
        draw_text(backbuffer, CANVAS_W/2 - 72, CANVAS_H/2 + 8,
                  "PRESS ANY KEY TO START", GAME_RGB(120,120,120));
        break;

    case PHASE_PLAYING:
    case PHASE_LEVEL_COMPLETE:
        render_lines(&state->lines, backbuffer);
        render_cups(state, backbuffer);
        render_grains(&state->grains, backbuffer);

        if (state->phase == PHASE_LEVEL_COMPLETE) {
            /* Semi-transparent overlay: darken the background slightly
             * by drawing a dark rect — we have no alpha blending,
             * so we use a simple dark banner instead. */
            draw_rect(backbuffer,
                      CANVAS_W/2 - 100, CANVAS_H/2 - 20,
                      200, 40,
                      GAME_RGB(40, 40, 40));
            draw_text(backbuffer,
                      CANVAS_W/2 - 72, CANVAS_H/2 - 6,
                      "LEVEL COMPLETE!", GAME_RGB(220, 220, 80));
        }
        break;

    case PHASE_FREEPLAY:
        render_lines(&state->lines, backbuffer);
        render_grains(&state->grains, backbuffer);
        break;

    case PHASE_COUNT:
        break;
    }
}
```

---

## Step 12 — game_init Starts at Title

```c
void game_init(GameState *state, GameBackbuffer *backbuffer) {
    (void)backbuffer;
    memset(state, 0, sizeof(*state));
    state->gravity_sign   = 1;
    state->unlocked_count = 1;
    change_phase(state, PHASE_TITLE);
    /*
     * We do NOT call load_level here.
     * The level loads when the player presses a key from the title screen.
     * This separates "game initialized" from "level loaded."
     */
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
The title screen displays "SUGAR, SUGAR". Press any key — the level loads, the emitter at (320,30) starts spraying grains, and a cup rectangle appears near the bottom center. The counter above the cup starts at 50. Draw a ramp or funnel to guide grains in. When 50 land inside, a dark banner with "LEVEL COMPLETE!" appears. Two seconds later the level reloads (level 1 is a stub, so it wraps back to level 0). Press R during play to restart the current level.

---

## Key Concepts

- `Cup.collected`: incremented by `check_cups` when an active grain lands inside the AABB. The grain is deactivated on collection — it disappears into the cup.
- AABB test: four integer comparisons. `px >= rx && px < rx+rw && py >= ry && py < ry+rh`. No floats, no trig.
- `check_win`: returns `0` early as soon as any cup is not full. Only returns `1` when every cup is satisfied.
- `GAME_PHASE` enum: the four values are the only valid states. The compiler enforces exhaustive `switch` coverage with `-Wswitch` (included in `-Wextra`).
- `change_phase`: the single function that sets `state->phase`. It also resets `phase_timer` to `0.0f` — without this, the 2-second timer would carry garbage from a previous phase.
- `phase_timer`: a float that accumulates `delta_time` inside a phase. After 2.0 seconds in `PHASE_LEVEL_COMPLETE`, the transition fires. This is simpler and more frame-rate-independent than counting frames.
- Progress bar formula: `fill_h = (collected * inner_height) / required`. Integer division is fine — off-by-one on a 36-pixel bar is invisible.
- `load_level` resets grains, lines, and cup counters. It is the canonical "start fresh" operation. Calling it from both game_init (via title→play) and from the level-complete handler ensures consistent state.

---

## Exercise

Add a second cup to level 0 at `x=460, y=400, w=80, h=40` with `required_count=30`. Rebuild and observe that `check_win` now requires both cups to be filled before the level completes. Notice you changed only `levels.c` — not a single line of `game.c` needed to change. This is the payoff of data-driven design.
