# Lesson 19: Side Panel UI and HUD

## What we're building

The complete Desktop Tower Defense side panel: a 160-pixel-wide strip on the
right of the canvas that shows the wave counter, lives, gold, seven tower-shop
buttons, a Send Wave button, and — when a tower is selected — its name, fire
mode, and a sell button.  We also add `ASSERT` and `DEBUG_TRAP` macros that
fire an immediate breakpoint in debug builds on any out-of-bounds array access,
so future bugs surface with a precise file and line number instead of a
mysterious crash or corrupted state.

## What you'll learn

- How to lay out a pixel-precise UI column with hardcoded Y offsets (DTD's
  original approach)
- How `snprintf` feeds dynamic text into `draw_text` without heap allocation
- The sell-button interaction: left-click on a rect while a tower is selected
- `ASSERT(cond)` and `DEBUG_TRAP`: how they work in debug vs release builds,
  and where to place them

## Prerequisites

- Lessons 01–18 complete: `draw_rect`, `draw_text`, `draw_rect_outline`,
  and `draw_hp_bar` all exist in `render.c`
- `TowerDef TOWER_DEFS[]` table exists with `.name`, `.cost`, `.color`
- `state->selected_tower_idx` is `-1` when nothing is selected
- `sell_tower(state, idx)` implemented in Lesson 18

---

## Step 1: `ASSERT` and `DEBUG_TRAP` macros

Add these near the top of `game.h` so every translation unit gets them:

```c
/* src/game.h — debug assertion macros */
#ifndef ASSERT
  #if defined(DEBUG)
    /* __builtin_trap() fires an immediate processor trap / breakpoint.
     * The debugger stops exactly here with a full call stack.
     * fprintf before the trap so the message appears in the terminal
     * even without a debugger attached. */
    #include <stdio.h>
    #define DEBUG_TRAP() __builtin_trap()
    #define ASSERT(cond) \
        do { \
            if (!(cond)) { \
                fprintf(stderr, "ASSERT FAILED: %s\n  file: %s  line: %d\n", \
                        #cond, __FILE__, __LINE__); \
                DEBUG_TRAP(); \
            } \
        } while (0)
  #else
    /* Release build: zero overhead — the compiler optimises assertions away. */
    #define DEBUG_TRAP() ((void)0)
    #define ASSERT(cond) ((void)(cond))
  #endif
#endif
```

Place `ASSERT` guards in these two hot spots:

```c
/* src/render.c — pixel-to-cell conversion */
static int pixel_to_col(int px) {
    int col = px / CELL_SIZE;
    ASSERT(col >= 0 && col < GRID_COLS);
    return col;
}

/* src/towers.c — any direct tower array access */
Tower *get_tower(GameState *state, int idx) {
    ASSERT(idx >= 0 && idx < MAX_TOWERS);
    return &state->towers[idx];
}
```

**What's happening:**

- In debug builds (`-DDEBUG`) `ASSERT(false)` calls `__builtin_trap()`.
  On x86 this emits an `int3` instruction; GDB/LLDB will pause here.
- In release builds the entire macro expands to `((void)(cond))` — the
  condition is evaluated for side-effects but no branch is emitted.
- `#cond` is a preprocessor string-ification: `ASSERT(idx < MAX_TOWERS)`
  prints the literal text `idx < MAX_TOWERS`, not a number.

---

## Step 2: Panel layout constants

```c
/* src/game.h — side panel geometry */
#define PANEL_X      600   /* left edge of the panel column         */
#define PANEL_W      160   /* panel width (canvas is 760 px wide)   */
#define CANVAS_W     760   /* extend from 640 to accommodate panel  */
#define CANVAS_H     600

/* Tower shop button geometry */
#define SHOP_BTN_X   (PANEL_X + 4)
#define SHOP_BTN_W   (PANEL_W - 8)
#define SHOP_BTN_H   33
#define SHOP_BTN_Y0  70    /* y of first shop button                */
```

---

## Step 3: Complete `draw_ui_panel()`

```c
/* src/render.c — draw_ui_panel(): full side panel */
void draw_ui_panel(GameBackbuffer *bb, GameState *state) {
    /* Panel background */
    draw_rect(bb, PANEL_X, 0, PANEL_W, CANVAS_H, GAME_RGB(0x1A, 0x1A, 0x2A));

    /* ---- Title ---- */
    draw_text(bb, PANEL_X + 8, 5, "DTD", COLOR_WHITE);

    /* ---- Wave / Lives / Gold HUD ---- */
    char buf[64];

    snprintf(buf, sizeof(buf), "Wave: %d/%d", state->current_wave, WAVE_COUNT);
    draw_text(bb, PANEL_X + 8, 20, buf, COLOR_WHITE);

    snprintf(buf, sizeof(buf), "Lives: %d", state->player_lives);
    draw_text(bb, PANEL_X + 8, 35, buf, GAME_RGB(0xFF, 0x44, 0x44));

    snprintf(buf, sizeof(buf), "Gold: %d", state->player_gold);
    draw_text(bb, PANEL_X + 8, 50, buf, COLOR_GOLD_TEXT);

    /* Separator */
    draw_rect(bb, PANEL_X + 4, 63, PANEL_W - 8, 1, GAME_RGB(0x44, 0x44, 0x55));

    /* ---- Tower Shop Buttons (7 towers, 33 px each) ---- */
    static const struct { const char *label; int cost; } SHOP_ROWS[TOWER_COUNT] = {
        { "Pellet   5g",   5  },
        { "Squirt  15g",  15  },
        { "Dart    40g",  40  },
        { "Snap    75g",  75  },
        { "Swarm  125g", 125  },
        { "Frost   60g",  60  },
        { "Bash    90g",  90  },
    };

    for (int i = 0; i < TOWER_COUNT; i++) {
        int by = SHOP_BTN_Y0 + i * SHOP_BTN_H;
        int bx = SHOP_BTN_X;

        /* Highlight the selected tower type */
        uint32_t bg = (state->selected_tower_type == i)
                    ? GAME_RGB(0x22, 0x44, 0x22)
                    : GAME_RGB(0x22, 0x22, 0x33);
        draw_rect(bb, bx, by, SHOP_BTN_W, SHOP_BTN_H - 2, bg);
        draw_rect_outline(bb, bx, by, SHOP_BTN_W, SHOP_BTN_H - 2,
                          GAME_RGB(0x44, 0x44, 0x66));

        /* Coloured tower preview square (10×10) */
        draw_rect(bb, bx + 3, by + 4, 10, 10, TOWER_DEFS[i].color);

        /* Label text */
        uint32_t text_col = (state->player_gold >= SHOP_ROWS[i].cost)
                          ? COLOR_WHITE
                          : GAME_RGB(0x77, 0x77, 0x77); /* greyed out */
        draw_text(bb, bx + 16, by + 6, SHOP_ROWS[i].label, text_col);

        /* Store rects for hit testing (caller fills state->shop_btns[]) */
        state->shop_btns[i] = (Rect){ bx, by, SHOP_BTN_W, SHOP_BTN_H - 2 };
    }

    /* Separator */
    int sep2_y = SHOP_BTN_Y0 + TOWER_COUNT * SHOP_BTN_H + 2;
    draw_rect(bb, PANEL_X + 4, sep2_y, PANEL_W - 8, 1, GAME_RGB(0x44, 0x44, 0x55));

    /* ---- Send Wave Button ---- */
    int btn_y = sep2_y + 6;
    const char *btn_label = "";
    if (state->phase == GAME_PHASE_WAVE_CLEAR) btn_label = "Send Next Wave";
    else if (state->phase == GAME_PHASE_WAVE)  btn_label = "Send Early";

    if (btn_label[0]) {
        draw_rect(bb, SHOP_BTN_X, btn_y, SHOP_BTN_W, 22,
                  GAME_RGB(0x22, 0x55, 0x22));
        draw_rect_outline(bb, SHOP_BTN_X, btn_y, SHOP_BTN_W, 22,
                          GAME_RGB(0x66, 0xCC, 0x44));
        draw_text(bb, SHOP_BTN_X + 4, btn_y + 6, btn_label, COLOR_WHITE);
        state->send_wave_btn = (Rect){ SHOP_BTN_X, btn_y, SHOP_BTN_W, 22 };
        state->send_wave_btn_visible = 1;
    } else {
        state->send_wave_btn_visible = 0;
    }

    /* ---- Selected Tower Info ---- */
    if (state->selected_tower_idx >= 0) {
        ASSERT(state->selected_tower_idx < MAX_TOWERS);
        Tower *t = &state->towers[state->selected_tower_idx];
        int info_y = btn_y + 30;

        draw_rect(bb, PANEL_X + 4, info_y, PANEL_W - 8, 1,
                  GAME_RGB(0x44, 0x44, 0x55));
        info_y += 4;

        /* Tower type name */
        draw_text(bb, PANEL_X + 8, info_y,
                  TOWER_DEFS[t->type].name, COLOR_WHITE);
        info_y += 14;

        /* Fire mode */
        const char *mode_str = "First";
        if (t->target_mode == TARGET_LAST)    mode_str = "Last";
        else if (t->target_mode == TARGET_STRONG) mode_str = "Strong";
        snprintf(buf, sizeof(buf), "Mode: %s", mode_str);
        draw_text(bb, PANEL_X + 8, info_y, buf, GAME_RGB(0xCC, 0xCC, 0xCC));
        info_y += 14;

        /* DPS display */
        float dps = TOWER_DEFS[t->type].damage / TOWER_DEFS[t->type].fire_rate;
        snprintf(buf, sizeof(buf), "DPS:  %.1f", dps);
        draw_text(bb, PANEL_X + 8, info_y, buf, GAME_RGB(0xAA, 0xDD, 0xFF));
        info_y += 18;

        /* Sell button */
        int sell_val = (int)((float)TOWER_DEFS[t->type].cost * 0.6f);
        snprintf(buf, sizeof(buf), "Sell: %dg", sell_val);
        draw_rect(bb, SHOP_BTN_X, info_y, SHOP_BTN_W, 20,
                  GAME_RGB(0x55, 0x22, 0x22));
        draw_rect_outline(bb, SHOP_BTN_X, info_y, SHOP_BTN_W, 20,
                          GAME_RGB(0xCC, 0x44, 0x44));
        draw_text(bb, SHOP_BTN_X + 4, info_y + 5, buf, COLOR_WHITE);
        state->sell_btn = (Rect){ SHOP_BTN_X, info_y, SHOP_BTN_W, 20 };
        state->sell_btn_visible = 1;
    } else {
        state->sell_btn_visible = 0;
    }
}
```

**What's happening:**

- All Y positions are hardcoded constants derived from the 33-pixel button
  height — the same technique the original DTD used.  No layout engine needed.
- `snprintf` writes into a local `buf[64]`.  No heap allocation, no
  format-string vulnerability.
- The sell value is 60 % of placement cost, consistent with `sell_tower()`.
- `draw_text` renders from a fixed bitmap font defined in Lesson 02; the call
  signature is `draw_text(bb, x, y, str, color)`.

---

## Step 4: Sell button click handling

```c
/* src/game.c — inside game_handle_input(), left-click section */
if (input->left_click) {
    int mx = input->mouse_x;
    int my = input->mouse_y;

    /* Sell button — only active when a tower is selected */
    if (state->sell_btn_visible) {
        Rect *r = &state->sell_btn;
        if (mx >= r->x && mx < r->x + r->w &&
            my >= r->y && my < r->y + r->h)
        {
            sell_tower(state, state->selected_tower_idx);
            state->selected_tower_idx = -1;
            return;
        }
    }

    /* Tower shop buttons */
    for (int i = 0; i < TOWER_COUNT; i++) {
        Rect *r = &state->shop_btns[i];
        if (mx >= r->x && mx < r->x + r->w &&
            my >= r->y && my < r->y + r->h)
        {
            if (state->player_gold >= TOWER_DEFS[i].cost) {
                state->selected_tower_type = i;
            }
            return;
        }
    }

    /* Click on the grid — place or select tower */
    if (mx < PANEL_X) {
        int col = pixel_to_col(mx);
        int row = pixel_to_row(my);
        ASSERT(col >= 0 && col < GRID_COLS);
        ASSERT(row >= 0 && row < GRID_ROWS);
        handle_grid_click(state, row, col);
    }
}
```

---

## Step 5: Target mode cycling on right-click

```c
/* src/game.c — right-click on a placed tower cycles its targeting mode */
if (input->right_click && mx < PANEL_X) {
    int col = pixel_to_col(input->mouse_x);
    int row = pixel_to_row(input->mouse_y);
    int idx = state->grid_tower[row][col];
    if (idx >= 0) {
        Tower *t = &state->towers[idx];
        t->target_mode = (t->target_mode + 1) % TARGET_MODE_COUNT;
        state->selected_tower_idx = idx;
    }
}
```

---

## Build and run

```bash
mkdir -p build

# Raylib — note CANVAS_W is now 760
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG \
      -o build/dtd \
      src/main_raylib.c src/game.c src/render.c \
      src/creeps.c src/towers.c src/levels.c src/bfs.c \
      -lraylib -lm
./build/dtd

# X11
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG \
      -o build/dtd \
      src/main_x11.c src/game.c src/render.c \
      src/creeps.c src/towers.c src/levels.c src/bfs.c \
      -lX11 -lm
./build/dtd
```

**Expected:** Full side panel visible.  Tower buttons show coloured preview
squares and grey out when the player cannot afford them.  Clicking a placed
tower selects it and shows name, mode, DPS, and a Sell button in the lower
panel.  Clicking Sell refunds gold and removes the tower.  Right-clicking a
tower cycles `First → Last → Strong → First`.

---

## Exercises

1. **Beginner** — Add a "Range" line to the selected-tower info block:
   `snprintf(buf, sizeof(buf), "Range: %d", TOWER_DEFS[t->type].range);`
   Display it in light blue between the Mode and DPS lines.

2. **Intermediate** — The sell button currently refunds a flat 60 %.  Change it
   so a tower refunds 80 % if it was placed in the current wave and 60 %
   otherwise.  Add a `placed_wave` field to `Tower` and check it in
   `sell_tower()`.

3. **Challenge** — Add a *Upgrade* button below the Sell button.  For the
   selected tower type, double `damage` and increase `cost` by 50 %.  Implement
   `upgrade_tower(state, idx)` in `towers.c` and wire it to a click rect in
   `draw_ui_panel()`.

---

## What's next

In Lesson 20 we focus on **visual polish and particles**: wiring the particle
pool to creep deaths, adding spinning stun stars and frost ring pulses, and
implementing the semi-transparent overlay screens for Game Over, Victory, and
the title.
