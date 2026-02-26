# Lesson 07 — Level Definitions + Progression

**By the end of this lesson you will have:**
A title screen showing a 6×5 grid of 30 level buttons. Buttons 2–30 are greyed out (locked). Click button 1 → puzzle plays. Finish it → button 2 unlocks. Escape from a level → returns to the title screen.

---

## Step 1 — Why a struct for level data

In JavaScript you'd write a level as a plain object:

```js
const level1 = {
  emitters: [{ x: 320, y: 20, rate: 80 }],
  cups:     [{ x: 278, y: 370, w: 85, h: 100, color: 'white', required: 150 }],
};
```

In C we use a struct. A struct groups related data under one name, just like a JS object. The difference is every field has a fixed type declared in advance:

```c
// game.h — already written for you
typedef struct {
    int         index;
    Emitter     emitters[MAX_EMITTERS];  int emitter_count;
    Cup         cups[MAX_CUPS];          int cup_count;
    ColorFilter filters[MAX_FILTERS];   int filter_count;
    Teleporter  teleporters[MAX_TELEPORTERS]; int teleporter_count;
    Obstacle    obstacles[MAX_OBSTACLES];     int obstacle_count;
    int         has_gravity_switch;
    int         is_cyclic;
} LevelDef;
```

Because the arrays are fixed-size and embedded directly in the struct, the entire level fits in contiguous memory. No heap allocation, no pointers to chase.

---

## Step 2 — Designated initializers (`.field = value`)

Open `src/levels.c`. Look at level 1:

```c
[0] = {
    .index         = 0,
    .emitter_count = 1,
    .emitters      = { EMITTER(320, 20, 80) },
    .cup_count     = 1,
    .cups          = { CUP(278, 370, 85, 100, GRAIN_WHITE, 150) },
},
```

The `.field = value` syntax is a **designated initializer**. It is exactly like a JS object literal:

```js
// JS
const l = { index: 0, emitterCount: 1, emitters: [...] };

// C
LevelDef l = { .index = 0, .emitter_count = 1, .emitters = { ... } };
```

Any field you don't mention is zero-initialized. So `obstacle_count = 0`, `filter_count = 0`, etc. are implied. You only write the fields that matter.

The `[0] = { ... }` outer syntax is a **designated array initializer**. It means "initialize slot 0 of `g_levels[]` with this struct." Slots you skip stay zero. That is how `g_levels[29]` can be defined without filling in levels 1–28 explicitly.

---

## Step 3 — `level_load()`: copy, reset, stamp

When the player picks a level, `level_load()` runs. Here is what it does step by step:

```c
// game.c — already written
static void level_load(GameState *state, int index) {
    // 1. Copy the read-only definition into the live slot
    state->level = g_levels[index];
    state->current_level = index;

    // 2. Wipe all grains and drawn lines
    memset(&state->grains, 0, sizeof(state->grains));
    memset(&state->lines,  0, sizeof(state->lines));

    // 3. Reset emitter timers (they were 0 in the definition but
    //    may have accumulated time from the previous run)
    for (int i = 0; i < state->level.emitter_count; i++)
        state->level.emitters[i].spawn_timer = 0.0f;

    // 4. Reset cup fill counters
    for (int i = 0; i < state->level.cup_count; i++)
        state->level.cups[i].collected = 0;

    // 5. Gravity back to normal
    state->gravity_sign = 1;

    // 6. Bake obstacles AND cup walls into the collision bitmap
    stamp_obstacles(state);
    stamp_cups(state);      // ← NEW: cup sides + bottom are now solid physics walls
}
```

Why copy instead of working directly on `g_levels[]`? Because the game mutates the level during play (cup `collected` counts go up, emitter timers advance). Keeping `g_levels[]` read-only lets us reload the same level cleanly by copying again.

---

## Step 4 — `stamp_obstacles()` and `stamp_cups()`: everything is just pixels

An obstacle rectangle, a player-drawn line, **and a cup wall** are the same thing to the physics engine: a non-zero byte in `lines.pixels[]`. The only difference is *when* the pixels are written.

```c
// game.c
static void stamp_obstacles(GameState *state) {
    LevelDef *lv = &state->level;
    for (int i = 0; i < lv->obstacle_count; i++) {
        Obstacle *o = &lv->obstacles[i];
        for (int py = o->y; py < o->y + o->h; py++) {
            for (int px = o->x; px < o->x + o->w; px++) {
                if (px >= 0 && px < CANVAS_W && py >= 0 && py < CANVAS_H)
                    state->lines.pixels[py * CANVAS_W + px] = 1;
            }
        }
    }
}

static void stamp_cups(GameState *state) {
    /* Stamp the physical walls of each cup so grains can only enter
     * through the open top and cannot clip through the sides.
     *
     *     x         x+w-1
     *     |  (open)  |     ← y    (grains fall in from above)
     *     |          |
     *     |__________|     ← y+h-1  (solid bottom wall)
     */
    LevelDef *lv = &state->level;
    for (int c = 0; c < lv->cup_count; c++) {
        Cup *cup = &lv->cups[c];
        int cx = cup->x, cy = cup->y, cw = cup->w, ch = cup->h;

        for (int py = cy; py < cy + ch; py++) {
            /* Left wall */
            if (cx >= 0 && cx < CANVAS_W && py >= 0 && py < CANVAS_H)
                state->lines.pixels[py * CANVAS_W + cx] = 1;
            /* Right wall */
            int rx = cx + cw - 1;
            if (rx >= 0 && rx < CANVAS_W && py >= 0 && py < CANVAS_H)
                state->lines.pixels[py * CANVAS_W + rx] = 1;
        }
        /* Bottom wall */
        int by = cy + ch - 1;
        for (int px = cx; px < cx + cw; px++)
            if (px >= 0 && px < CANVAS_W && by >= 0 && by < CANVAS_H)
                state->lines.pixels[by * CANVAS_W + px] = 1;
    }
}
```

**Key design decision**: the cup absorption AABB uses *interior* bounds
(`x+1 .. x+w-2`, `y .. y+h-2`) so that the wall pixels act as pure physics
barriers, not absorption triggers. When a cup fills up, extra grains pile up
inside (resting on the solid bottom) and eventually overflow over the open rim.

> **Note on rendering**: `render_cups` draws the cup rectangle on top of the
> line bitmap, so wall pixels are never visible as `COLOR_LINE` — they are
> hidden behind the cup's own visual rendering.

Concrete numbers: canvas is 640 wide. A pixel at column 10, row 5:
- index = 5 × 640 + 10 = **3210**

Pattern: `index = row * width + col`

That is identical to how a 2D grid works in JS:
```js
const flat = [];
flat[row * width + col] = 1; // same formula
```

---

## Step 5 — `unlocked_count`: progression gating

`GameState` holds one integer:

```c
int unlocked_count; // starts at 1, max = TOTAL_LEVELS
```

Rules:
- Level `i` (0-based) is playable if `i < unlocked_count`.
- When the player completes level `i`, `unlocked_count` becomes `max(unlocked_count, i + 2)`. That means completing level 1 unlocks level 2, but completing level 2 again when level 5 is already unlocked does nothing.

In `update_level_complete()` inside `game.c`:

```c
static void update_level_complete(GameState *state, float dt) {
    state->phase_timer += dt;
    if (state->phase_timer > 2.5f) {
        int next = state->current_level + 1;
        if (next + 1 > state->unlocked_count)
            state->unlocked_count = next + 1;
        if (next >= TOTAL_LEVELS) {
            change_phase(state, PHASE_FREEPLAY);
        } else {
            level_load(state, next);
            change_phase(state, PHASE_PLAYING);
        }
    }
}
```

`phase_timer` accumulates elapsed seconds. After 2.5 s the game automatically advances. No button press needed.

---

## Step 6 — Escape always goes through `change_phase()`

Every phase transition in the game uses exactly one function:

```c
static void change_phase(GameState *state, GAME_PHASE next) {
    state->phase       = next;
    state->phase_timer = 0.0f;
}
```

It does two things: sets the new phase and resets the timer to zero. `phase_timer` is used for animations (the LEVEL_COMPLETE overlay fade, button pulses). If you forget to reset it the animation plays from a wrong start time.

When the player presses Escape during a level:

```c
// inside update_playing()
if (BUTTON_PRESSED(input->escape)) {
    change_phase(state, PHASE_TITLE);
}
```

`BUTTON_PRESSED(b)` is true only when the button was *pressed* this frame (not held). It expands to:
```c
((b).half_transition_count > 0 && (b).ended_down)
```

---

## Step 7 — Level select grid layout math

The title screen shows 30 buttons in a 6-column, 5-row grid. Here is how each button's top-left pixel is calculated:

```
btn_w = 70    btn_h = 40    gap = 8
grid_x = 60   grid_y = 120   (top-left corner of the whole grid)

For button index i (0-based):
  col = i % 6         e.g. i=7  → col = 1
  row = i / 6         e.g. i=7  → row = 1
  bx  = grid_x + col * (btn_w + gap)
  by  = grid_y + row * (btn_h + gap)
```

Concrete example — button 7 (level 8):
```
col = 7 % 6 = 1
row = 7 / 6 = 1
bx  = 60 + 1 * (70 + 8) = 60 + 78  = 138
by  = 120 + 1 * (40 + 8) = 120 + 48 = 168
```

ASCII diagram of the first 12 buttons:

```
grid_x=60
|
[L01][L02][L03][L04][L05][L06]   ← row 0
[L07][L08][L09][L10][L11][L12]   ← row 1
...
```

Each `[Lnn]` is 70 px wide with an 8 px gap to its right neighbor.

---

## Step 8 — `render_title()`: locked vs. unlocked buttons

```c
// game.c — render_title()
static void render_title(const GameState *state, GameBackbuffer *bb) {
    // Clear background
    draw_rect(bb, 0, 0, CANVAS_W, CANVAS_H, COLOR_BG);

    // Title text
    draw_text(bb, 220, 40, "SUGAR  SUGAR", COLOR_UI_TEXT);
    draw_text(bb, 190, 60, "select a level", COLOR_UI_TEXT);

    int cols   = 6, btn_w = 70, btn_h = 40, gap = 8;
    int grid_x = 60, grid_y = 120;

    for (int i = 0; i < TOTAL_LEVELS; i++) {
        int col = i % cols;
        int row = i / cols;
        int bx  = grid_x + col * (btn_w + gap);
        int by  = grid_y + row * (btn_h + gap);

        int locked   = (i >= state->unlocked_count);
        int hovered  = (state->title_hover == i);
        uint32_t bg  = locked  ? GAME_RGB(160,155,150)
                     : hovered ? COLOR_BTN_HOVER
                     :           COLOR_BTN_NORMAL;

        draw_rect        (bb, bx, by, btn_w, btn_h, bg);
        draw_rect_outline(bb, bx, by, btn_w, btn_h, COLOR_BTN_BORDER);

        // Show level number; locked levels show a padlock char
        if (locked) {
            draw_text(bb, bx + 28, by + 14, "?", COLOR_UI_TEXT);
        } else {
            draw_int(bb, bx + 22, by + 14, i + 1, COLOR_UI_TEXT);
        }
    }
}
```

`state->title_hover` is updated every frame in `update_title()` by checking whether the mouse position is inside each button's AABB:

```c
if (mx >= bx && mx < bx + btn_w && my >= by && my < by + btn_h) {
    state->title_hover = i;
}
```

When the left mouse button is pressed on an unlocked button:

```c
if (BUTTON_PRESSED(input->mouse.left) && state->title_hover >= 0) {
    int idx = state->title_hover;
    if (idx < state->unlocked_count) {
        level_load(state, idx);
        change_phase(state, PHASE_PLAYING);
    }
}
```

---

## Build & Run

```bash
clang -Wall -Wextra -O2 -o sugar \
    src/main_x11.c src/game.c src/levels.c -lX11 -lm
./sugar
```

**What you should see:**
- Off-white background with "SUGAR SUGAR" at the top.
- A 6×5 grid of buttons. Button 1 has a number; buttons 2–30 show `?`.
- Hovering over button 1 darkens it. Clicking it starts the first puzzle.
- Pressing Escape during a level returns to the title screen.
- Completing a level unlocks the next button (the `?` becomes a number).

---

## Key Concepts

- `LevelDef`: a struct that holds all level data — same idea as a JS object literal, but with fixed types and embedded arrays.
- **Designated initializer** `.field = value`: set only the fields you care about; everything else is zero. Equivalent to `{ ...defaults, field: value }` in JS.
- `[N] = { ... }`: designated array initializer — set a specific slot in an array literal without filling every preceding slot.
- `level_load()`: copies from the read-only `g_levels[]` array, resets mutable state (grains, lines, timers, gravity).
- `stamp_obstacles()`: writes obstacle rectangles as `1` bytes in the collision bitmap — identical to player-drawn lines from the physics engine's perspective.
- `stamp_cups()`: writes cup left/right/bottom wall pixels into the same bitmap so grains can only enter through the open top and pile up inside full cups (overflow simulation).
- `change_phase()`: the single place that sets `phase` and resets `phase_timer`. Never assign `state->phase` directly.
- `unlocked_count`: gates access to levels. Completing level `i` sets `unlocked_count = max(unlocked_count, i + 2)`.
- Grid math: `col = i % cols`, `row = i / cols`, `bx = grid_x + col * (btn_w + gap)`.
- `BUTTON_PRESSED(b)`: true only on the frame the button goes down — not while it is held.

---

## Exercise

Change the grid to 5 columns instead of 6. Update `cols = 5`, recompute `grid_x` so the grid stays horizontally centered on the 640 px canvas (hint: `grid_x = (640 - 5*(btn_w+gap) + gap) / 2`), and verify all 30 buttons still render without clipping off the right edge.
