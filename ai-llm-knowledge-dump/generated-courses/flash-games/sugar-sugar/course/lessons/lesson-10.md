# Lesson 10 — All 30 Levels + Complete Title Screen

**By the end of this lesson you will have:**
The complete game: a title screen with a 6×5 grid of 30 level buttons, all levels playable in sequence. Completing level 30 leads to a free-play sandbox. Locked levels are grey and unclickable.

---

## Step 1 — Why levels.c uses no file I/O

In a JS game you might load levels from a JSON file at runtime:

```js
const levels = await fetch('/levels.json').then(r => r.json());
```

In Sugar Sugar, every level is hardcoded at compile time:

```c
// levels.c
LevelDef g_levels[TOTAL_LEVELS] = {
    [0]  = { ... },
    [1]  = { ... },
    ...
    [29] = { ... },
};
```

The compiler places `g_levels[]` in the binary's `.data` segment. At runtime it is already in memory — zero startup cost, zero disk reads, zero error handling for missing files.

Trade-off: adding a level requires recompiling. That is fine for a fixed-puzzle game shipped as a single binary.

`g_levels` is declared in `game.h` as `extern`:

```c
// game.h
#define TOTAL_LEVELS 30
extern LevelDef g_levels[TOTAL_LEVELS];
```

`extern` says "this array exists somewhere — the linker will find it." `levels.c` provides the actual storage. `game.c` and the platform backends reference it through the `extern` declaration.

JS analogy: `extern` is like `import { g_levels } from './levels.js'` — you use it without re-declaring it.

---

## Step 2 — Sparse array initialization with `[N] = { ... }`

The 30 level structs in `levels.c` use designated array initializers:

```c
LevelDef g_levels[30] = {
    [0]  = { .index = 0,  .emitter_count = 1, ... },
    [5]  = { .index = 5,  .emitter_count = 1, ... },
    [29] = { .index = 29, .emitter_count = 2, ... },
};
```

You can define any slot in any order. Slots you skip are zero-initialized. This is a **sparse array initializer**.

Why is this useful?
- You can add level 17 without worrying about levels 1–16 being in order in the source file.
- A zero-initialized `LevelDef` is a valid (empty) level — `emitter_count = 0`, `cup_count = 0`. The game handles it gracefully.

Concrete check: `g_levels[3]` not listed in `levels.c`?
- C standard guarantees it is filled with zeroes.
- `emitter_count = 0` → no sugar spawns → nothing to do → level cannot be won.
- The progression system will never reach it unless `unlocked_count` advances past it from a completed level, which requires a previous level to exist.

---

## Step 3 — `unlocked_count` advances on completion

`unlocked_count` starts at 1 (only level 1 is playable). Each time a level completes:

```c
// game.c — update_level_complete()
static void update_level_complete(GameState *state, float dt) {
    state->phase_timer += dt;

    if (state->phase_timer > 2.5f) {
        int next = state->current_level + 1;

        // advance the unlock frontier
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

Walkthrough for completing level 1 (index 0):
```
next = 0 + 1 = 1
next + 1 = 2
unlocked_count was 1, now becomes 2
level_load(state, 1)  → load level 2
change_phase → PHASE_PLAYING
```

Walkthrough for completing level 30 (index 29):
```
next = 29 + 1 = 30
30 >= TOTAL_LEVELS (30)  → true
change_phase → PHASE_FREEPLAY
```

No level 31 is ever loaded.

---

## Step 4 — `PHASE_FREEPLAY`: sandbox after the last level

Freeplay is the same simulation as `PHASE_PLAYING` but with no win condition. The player draws lines, grains fall, nothing checks if cups are full.

```c
// game.c
static void update_freeplay(GameState *state, GameInput *input, float dt) {
    // same drawing + physics calls as update_playing
    // ... handle mouse drawing, spawn_grains, update_grains ...

    // Escape → go back to title
    if (BUTTON_PRESSED(input->escape)) {
        change_phase(state, PHASE_TITLE);
    }

    // R → clear lines and grains, restart freeplay
    if (BUTTON_PRESSED(input->reset)) {
        memset(&state->grains, 0, sizeof(state->grains));
        memset(&state->lines,  0, sizeof(state->lines));
    }
    // no check_win() call here — freeplay never ends automatically
}
```

Rendering freeplay uses `render_playing()` minus the cup fill bars (or with them — it doesn't matter since cups have `required_count = 0` in the freeplay state).

The level loaded for freeplay is the last level by convention:

```c
// in update_level_complete(), when next >= TOTAL_LEVELS:
level_load(state, TOTAL_LEVELS - 1); // keep using level 30's geometry
change_phase(state, PHASE_FREEPLAY);
```

---

## Step 5 — Complete title screen: 30 buttons in a 6×5 grid

`render_title()` draws all 30 buttons using the grid math from Lesson 07:

```c
// game.c — render_title()
static void render_title(const GameState *state, GameBackbuffer *bb) {
    draw_rect(bb, 0, 0, CANVAS_W, CANVAS_H, COLOR_BG);

    // Centered title text
    draw_text(bb, 220, 40, "SUGAR  SUGAR", COLOR_UI_TEXT);
    draw_text(bb, 180, 62, "click a level to play", COLOR_UI_TEXT);

    const int cols   = 6;
    const int btn_w  = 70, btn_h  = 40;
    const int gap    = 8;
    const int grid_x = 60,  grid_y = 110;

    for (int i = 0; i < TOTAL_LEVELS; i++) {
        int col = i % cols;
        int row = i / cols;
        int bx  = grid_x + col * (btn_w + gap);
        int by  = grid_y + row * (btn_h + gap);

        int locked  = (i >= state->unlocked_count);
        int hovered = (state->title_hover == i);

        uint32_t bg = locked  ? GAME_RGB(160, 155, 150)
                    : hovered ? COLOR_BTN_HOVER
                    :           COLOR_BTN_NORMAL;

        draw_rect        (bb, bx, by, btn_w, btn_h, bg);
        draw_rect_outline(bb, bx, by, btn_w, btn_h, COLOR_BTN_BORDER);

        if (locked) {
            draw_text(bb, bx + 28, by + 14, "?", COLOR_UI_TEXT);
        } else {
            draw_int(bb, bx + 22, by + 14, i + 1, COLOR_UI_TEXT);
        }
    }

    // Freeplay button — always visible after beating all levels
    if (state->unlocked_count > TOTAL_LEVELS) {
        draw_text(bb, 220, 370, "FREEPLAY UNLOCKED", COLOR_COMPLETE);
    }
}
```

Grid math check for the 30th button (index 29):
```
col = 29 % 6 = 5
row = 29 / 6 = 4
bx  = 60 + 5 * (70 + 8) = 60 + 390 = 450
by  = 110 + 4 * (40 + 8) = 110 + 192 = 302
right edge = 450 + 70 = 520  < 640  ✓ (doesn't clip)
bottom edge = 302 + 40 = 342  < 480  ✓
```

ASCII layout (first 12 buttons, 6 per row):

```
x=60                                              x=450
 [L01][L02][L03][L04][L05][L06]   y=110
 [L07][L08][L09][L10][L11][L12]   y=158
 ...
 [L25][L26][L27][L28][L29][L30]   y=302
```

---

## Step 6 — `update_title()`: hover detection and click handling

```c
// game.c
static void update_title(GameState *state, GameInput *input) {
    int mx = input->mouse.x;
    int my = input->mouse.y;
    state->title_hover = -1; // reset every frame

    const int cols   = 6;
    const int btn_w  = 70, btn_h  = 40;
    const int gap    = 8;
    const int grid_x = 60,  grid_y = 110;

    for (int i = 0; i < TOTAL_LEVELS; i++) {
        int col = i % cols;
        int row = i / cols;
        int bx  = grid_x + col * (btn_w + gap);
        int by  = grid_y + row * (btn_h + gap);

        if (mx >= bx && mx < bx + btn_w &&
            my >= by && my < by + btn_h) {
            state->title_hover = i; // record which button the mouse is over
        }
    }

    // Click: only if hovering an unlocked button
    if (BUTTON_PRESSED(input->mouse.left) && state->title_hover >= 0) {
        int idx = state->title_hover;
        if (idx < state->unlocked_count) {
            level_load(state, idx);
            change_phase(state, PHASE_PLAYING);
        }
    }

    // Escape on title screen = quit
    if (BUTTON_PRESSED(input->escape)) {
        state->should_quit = 1;
    }
}
```

Note `state->title_hover = -1` at the top. Every frame it resets. If the mouse is over no button, `title_hover` stays -1. The render function checks `state->title_hover == i` — a -1 will never match a valid index.

---

## Step 7 — All 30 level definitions (overview)

The full `levels.c` contains all 30 levels grouped by mechanic:

| Range    | Theme                            |
|----------|----------------------------------|
| 1–4      | Basic routing: one to four cups  |
| 5–8      | Obstacle shelves                 |
| 9–12     | Color filters and colored cups   |
| 13–16    | Gravity switch                   |
| 17–20    | Cyclic (wrap-around)             |
| 21–24    | Teleporters                      |
| 25–28    | Mixed mechanics                  |
| 29–30    | Complex multi-mechanic puzzles   |

Each level's `index` field matches its 0-based position in `g_levels[]`. This is redundant but useful for debugging: if `state->current_level != state->level.index`, something went wrong in `level_load()`.

---

## Build & Run

```bash
clang -Wall -Wextra -O2 -o sugar \
    src/main_x11.c src/game.c src/levels.c -lX11 -lm
./sugar
```

**What you should see:**
- Title screen with 30 buttons in a 6×5 grid.
- Only button 1 is coloured; buttons 2–30 show `?`.
- Clicking button 1 starts the first puzzle.
- Completing each level unlocks the next.
- After level 30, the screen shows "FREEPLAY UNLOCKED" and transitions to sandbox.
- Escape from freeplay returns to the title.

---

## Key Concepts

- `extern LevelDef g_levels[TOTAL_LEVELS]`: declared in `game.h`, defined in `levels.c`. The linker connects them. JS analogy: `export` / `import`.
- **Compile-time data**: level arrays live in the binary's `.data` segment. Zero runtime I/O, zero error handling for missing files.
- **Sparse array initializer** `[N] = { ... }`: skip slots freely; unset slots are zero-initialized.
- `unlocked_count`: the single integer gating progression. Comparing `i >= unlocked_count` determines locked vs. playable.
- `PHASE_FREEPLAY`: same physics as `PHASE_PLAYING`, no win check, Escape returns to title.
- `title_hover = -1`: reset every frame. Render checks equality to index — -1 matches nothing.
- `BUTTON_PRESSED(input->mouse.left)` + `title_hover >= 0` + `idx < unlocked_count`: three guards before loading a level.

---

## Exercise

Add a "RESTART TOUR" button to the title screen that appears only when `unlocked_count >= TOTAL_LEVELS`. Draw it at `(220, 400, 200, 36)`. When clicked, set `state->unlocked_count = 1` and call `change_phase(state, PHASE_TITLE)`. This resets progression without quitting.
