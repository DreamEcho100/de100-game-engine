# Lesson 18: Send Wave Early and Interest Mechanic

## What we're building

Two economy mechanics straight from the original Desktop Tower Defense flash
game.  **Send Next Wave** lets the player sacrifice their timer bonus and
immediately trigger the next wave — in return they receive a small gold bonus
for the time they skipped.  The **interest** system pays 5 % of the player's
current gold at the start of every wave, rewarding conservative spending.
Together they add a meaningful economic layer: rush waves to compound interest
faster, or slow down and save gold.

## What you'll learn

- How to implement `send_wave_early()` without duplicating wave-start logic
- How interest is applied at wave start and displayed as a floating text
  particle
- Why selling a tower mid-wave and re-running BFS requires no special creep
  handling
- How to place a conditional button in the side panel that changes label based
  on game phase

## Prerequisites

- Lessons 01–17 complete: `start_wave()`, `change_phase()`, and the particle
  system all exist; the side panel renders with `draw_ui_panel()`
- `spawn_particle(state, x, y, vx, vy, lifetime, color, type, text)` exists
- `WAVE_CLEAR_DURATION` and `state->remaining_wave_time` are tracked in
  `GameState`
- `GOLD_PER_SECOND_EARLY` and `INTEREST_RATE` constants to be added to
  `game.h`

---

## Step 1: New constants in `game.h`

```c
/* src/game.h — economy tuning constants */
#define GOLD_PER_SECOND_EARLY  3     /* gold per second of wave timer skipped  */
#define INTEREST_RATE          0.05f /* 5 % of current gold awarded each wave  */
#define INTEREST_CAP           50    /* max interest payout per wave            */
```

**What's happening:**

- `GOLD_PER_SECOND_EARLY` means a player with 10 seconds left on the timer
  earns 30 bonus gold by sending early.
- `INTEREST_CAP` prevents exponential runaway at very high gold counts — a
  player sitting on 10 000 gold gets 50 gold, not 500.

---

## Step 2: `apply_interest()` called at wave start

```c
/* src/game.c — apply_interest(), called from start_wave() */
static void apply_interest(GameState *state) {
    int bonus = (int)((float)state->player_gold * INTEREST_RATE);
    if (bonus > INTEREST_CAP) bonus = INTEREST_CAP;
    if (bonus <= 0) return;

    state->player_gold += bonus;

    /* Spawn a floating "+N interest" text particle above the gold display */
    char buf[24];
    snprintf(buf, sizeof(buf), "+%d interest", bonus);
    spawn_particle(state,
                   PANEL_X + 20, 55,   /* near the gold display row  */
                   0.0f, -18.0f,        /* float upward               */
                   1.8f,                /* lifetime seconds           */
                   COLOR_GOLD_TEXT,
                   PARTICLE_TEXT,
                   buf);
}

/* src/game.c — start_wave(): call apply_interest first */
void start_wave(GameState *state) {
    apply_interest(state);
    /* ... rest of existing wave-start logic ... */
}
```

**What's happening:**

- Interest is calculated on the gold balance *before* the wave starts, so the
  player is rewarded for the gold they accumulated during the previous wave.
- The floating particle uses `PARTICLE_TEXT` type so the render loop draws the
  `buf` string rather than a coloured square.

---

## Step 3: `send_wave_early()`

```c
/* src/game.c — send_wave_early(): skip timer, collect bonus, start next wave */
void send_wave_early(GameState *state) {
    /* Calculate bonus from remaining timer */
    int bonus = (int)(state->remaining_wave_time * (float)GOLD_PER_SECOND_EARLY);

    if (bonus > 0) {
        state->player_gold += bonus;

        /* Floating "+N bonus" text near the Send button */
        char buf[24];
        snprintf(buf, sizeof(buf), "+%d bonus", bonus);
        spawn_particle(state,
                       PANEL_X + 20, 335,
                       0.0f, -20.0f,
                       1.5f,
                       COLOR_GOLD_TEXT,
                       PARTICLE_TEXT,
                       buf);
    }

    /* Advance wave counter and start immediately */
    state->current_wave++;
    if (state->current_wave > WAVE_COUNT) {
        change_phase(state, GAME_PHASE_VICTORY);
        return;
    }

    state->remaining_wave_time = 0.0f;
    start_wave(state);
}
```

**What's happening:**

- `remaining_wave_time` is set to zero so `start_wave()` does not double-count
  the timer.
- If `current_wave > WAVE_COUNT` we skip directly to victory — the player can
  send early on wave 40 to finish the game before the last creeps are cleared.
- `start_wave()` calls `apply_interest()` internally, so the player also
  receives interest on the early send.

---

## Step 4: Wire the button in `draw_ui_panel()`

The button label and click handler change depending on current phase:

```c
/* src/render.c — inside draw_ui_panel(), button section around y=330 */

/* Draw a 120×24 button */
int btn_x = PANEL_X + 8;
int btn_y = 328;
int btn_w = 144;
int btn_h = 22;

const char *btn_label;
if (state->phase == GAME_PHASE_WAVE_CLEAR) {
    btn_label = "Send Next Wave";
} else if (state->phase == GAME_PHASE_WAVE) {
    btn_label = "Send Early";
} else {
    btn_label = "";   /* hidden during other phases */
}

if (btn_label[0]) {
    draw_rect(bb, btn_x, btn_y, btn_w, btn_h, GAME_RGB(0x33, 0x66, 0x33));
    draw_rect_outline(bb, btn_x, btn_y, btn_w, btn_h, GAME_RGB(0x88, 0xCC, 0x44));
    draw_text(bb, btn_x + 6, btn_y + 6, btn_label, COLOR_WHITE);
}

/* Store rect in state for click detection in game_handle_input() */
state->send_wave_btn = (Rect){ btn_x, btn_y, btn_w, btn_h };
state->send_wave_btn_visible = (btn_label[0] != '\0');
```

**What's happening:**

- The button is visible during `WAVE_CLEAR` (normal "start next wave") and
  during `WAVE` (early send for a bonus).
- We store the button rectangle in `GameState` so the input handler can do a
  simple point-in-rect hit test without re-deriving coordinates.

---

## Step 5: Click handling in `game_handle_input()`

```c
/* src/game.c — inside game_handle_input(), left-click section */
if (input->left_click) {
    int mx = input->mouse_x;
    int my = input->mouse_y;

    /* Send Wave button */
    if (state->send_wave_btn_visible &&
        mx >= state->send_wave_btn.x &&
        mx <  state->send_wave_btn.x + state->send_wave_btn.w &&
        my >= state->send_wave_btn.y &&
        my <  state->send_wave_btn.y + state->send_wave_btn.h)
    {
        if (state->phase == GAME_PHASE_WAVE_CLEAR) {
            state->current_wave++;
            if (state->current_wave > WAVE_COUNT) {
                change_phase(state, GAME_PHASE_VICTORY);
            } else {
                start_wave(state);
            }
        } else if (state->phase == GAME_PHASE_WAVE) {
            send_wave_early(state);
        }
        return;  /* consume click */
    }

    /* ... rest of click handling (tower place, sell, etc.) */
}
```

---

## Step 6: Mid-wave tower sell and BFS

When the player sells a tower mid-wave, a blocked cell opens up.  BFS must
be re-run immediately so the path distance field is up to date.

```c
/* src/game.c — sell_tower() */
void sell_tower(GameState *state, int tower_idx) {
    Tower *t = &state->towers[tower_idx];
    if (!t->active) return;

    /* Refund 60 % of placement cost */
    state->player_gold += (int)((float)TOWER_DEFS[t->type].cost * 0.6f);

    /* Free the grid cell */
    state->grid[t->grid_row][t->grid_col] = CELL_EMPTY;
    t->active = 0;

    /* Re-run BFS immediately so the distance field reflects the new opening */
    bfs_compute(state);
}
```

> **Mid-wave sell and creep routing**
>
> Selling a tower mid-wave re-runs BFS immediately.  Creeps currently moving
> toward a cell that is no longer blocked will naturally re-route on their next
> cell transition — no special handling is needed.  `creep_move_toward_exit()`
> reads the BFS distance field every frame to choose the next step, so as soon
> as the field updates the creep will take the newly-opened shorter path.

---

## Build and run

```bash
mkdir -p build

# Raylib
clang -Wall -Wextra -std=c99 -O0 -g \
      -o build/dtd \
      src/main_raylib.c src/game.c src/render.c \
      src/creeps.c src/towers.c src/levels.c src/bfs.c \
      -lraylib -lm
./build/dtd

# X11
clang -Wall -Wextra -std=c99 -O0 -g \
      -o build/dtd \
      src/main_x11.c src/game.c src/render.c \
      src/creeps.c src/towers.c src/levels.c src/bfs.c \
      -lX11 -lm
./build/dtd
```

**Expected:** During the inter-wave countdown a "Send Next Wave" button is
visible in the side panel.  During a live wave it reads "Send Early".  Clicking
either spawns a gold-coloured floating "+N bonus" particle and immediately
starts the next wave.  The first wave start after having gold shows a floating
"+N interest" text.  Selling a tower mid-wave causes nearby creeps to reroute
within one frame.

---

## Exercises

1. **Beginner** — Add a `MIN_INTEREST` constant of `1` gold so players with
   very little gold still get something.  Update `apply_interest()` to enforce
   it and add a test: start a wave with `player_gold = 5` and verify the
   balance becomes `6` afterward.

2. **Intermediate** — Display the expected interest bonus in the side panel
   during the inter-wave phase as a small grey hint text like
   `"Interest: +N"`.  Compute it with the same formula as `apply_interest()`
   but do not award it yet — only show the preview.

3. **Challenge** — The Send Early bonus is calculated from `remaining_wave_time`.
   Add a *streak bonus*: if the player has sent early on the previous two
   consecutive waves, award an extra 10 gold on top of the normal bonus.
   Track consecutive early-sends with a `send_early_streak` counter in
   `GameState`.

---

## What's next

In Lesson 19 we finish the complete **side panel UI**: every tower shop button,
the wave/lives/gold HUD, the selected-tower info panel with mode display and
sell button, and the `ASSERT` macros that guard all array accesses.
