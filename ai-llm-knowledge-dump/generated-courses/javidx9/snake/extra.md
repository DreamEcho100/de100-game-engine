# Snake — Extra Notes

## What This Implementation Handles

- 60×20 grid with 3-row header area for HUD
- Ring buffer snake body: `segments[MAX_SNAKE]` (1200 slots) with `head`, `tail`, `length`
- 4-direction movement: UP/RIGHT/DOWN/LEFT, stored as `SNAKE_DIR` enum
- Input queued into `next_direction`; applied on each move step (not each frame)
- Direction turns fire on "just pressed" only — no held-key auto-repeat for turns
- Wall collision: bounds check `new_x/new_y` against `GRID_WIDTH`/`GRID_HEIGHT`
- Self collision: full ring-buffer walk from `tail` to `head` comparing positions
- Food placement: `snake_spawn_food()` retries random positions until one not on snake
- Eating: `grow_pending += 5`; tail doesn't advance for 5 ticks after eating
- Delta-time movement: `move_timer += delta_time`; step fires when `move_timer >= move_interval`
- Sub-frame overshoot preserved: `move_timer -= move_interval` (not `= 0`)
- Speed scaling: `move_interval -= 0.01f` every 3 score points, floored at `0.05s`
- Score counter; `best_score` preserved across resets (`snake_init()` saves and restores it)
- Game over: body turns dark red; semi-transparent overlay with "GAME OVER", score, restart/quit hints
- Restart: `R` key resets game while preserving `best_score`
- Platform-independent rendering via `SnakeBackbuffer` (CPU pixel array, `0xAARRGGBB`)
- HUD header: title "SNAKE" centered, score left, best score right, green separator line
- Green border walls drawn as cells (not background), red food cell, yellow body, white head
- Alpha-blended `draw_rect_blend()` for game-over dim overlay
- Built-in 5×7 bitmap font for all HUD text

---

## Input Feel Analysis

### 1. Responsiveness — ✅ Good
Turns fire on `ended_down && half_transition_count > 0` — the "just pressed this frame" check from `GameButtonState`. Even at 30 fps, a 20ms tap inside a 33ms frame is caught because `half_transition_count` is incremented on every state change, not just at the frame sample point. Turn input is processed at the top of `snake_update()` unconditionally.

### 2. Consistency — ✅ Good
`next_direction` is only updated on just-pressed, never on held. The turn math `(dir + 1) % 4` (CW) and `(dir + 3) % 4` (CCW) is deterministic. The same key press in the same position always produces the same result. `UPDATE_BUTTON` only increments `half_transition_count` when state actually changes, preventing phantom inputs.

### 3. Forgiveness — ⚠️ Partial
`next_direction` is a queue depth of 1 — the last direction input before a move step is applied. If the player presses two keys in one step (e.g., up then right between two ticks), only the last is kept. A queue depth of 2–3 would allow players to "pre-queue" corner turns. There is also no 180° reversal prevention beyond the natural impossibility (turning CW twice from UP reaches DOWN, but that takes 2 steps, so it's fine).

### 4. Feedback — ⚠️ Partial
Visual feedback is clear: distinct colors for head (white), body (yellow), food (red), death (dark red for entire body), and a bold red-bordered game-over overlay. However, there is **no audio** (no eat sound, no death sound) and no **score delta flash** ("+1" pop-up when eating). The speed increase is invisible — the player has no on-screen indicator of current speed or level.

### 5. Control — ✅ Good
Left/right turn (CCW/CW) relative to viewing direction is intuitive and correctly computed with `(dir ± 1) % 4`. Turns don't auto-repeat (correct for Snake — holding a direction should not spin the snake). `next_direction` is decoupled from `direction` so the snake never reverses on itself even if the player presses the opposite direction key.

### 6. Precision — ✅ Good
The turn is queued into `next_direction` and applied exactly at the next move step boundary — not mid-cell, not delayed by a frame. This means pressing a turn key at any point between steps guarantees it applies on the very next step, with zero drift. Sub-frame overshoot preservation (`move_timer -= move_interval`) ensures consistent step timing.

### 7. Fluidity — ✅ Good
Delta-time accumulation with remainder preservation makes movement smooth and framerate-independent. At 60 fps with `move_interval = 0.15s`, the snake moves every 9 frames exactly. At 30 fps it moves every 4–5 frames. Neither case has jitter from integer-tick rounding.

### 8. Intuitiveness — ✅ Good
Left arrow = turn left (CCW), right arrow = turn right (CW) maps to the most natural expectation. The `SNAKE_DIR` enum comment documents the arithmetic: `(dir+1)%4 = CW`, `(dir+3)%4 = CCW`. R/Space for restart is shown on the game-over screen.

### 9. Customization — ❌ Missing
No runtime keybinding changes. Starting speed (`move_interval = 0.15f`) and growth amount (`grow_pending += 5`) are hardcoded constants with no settings struct or `#define` toggle. Adding a `SnakeConfig` struct passed to `snake_init()` would fix this.

### 10. Low Latency — ✅ Good
Input is processed at the top of `snake_update()` before any timer/movement logic. The backbuffer is written and uploaded every frame with no extra buffering. Turn latency is at most 1 frame for detection + up to `move_interval` seconds until the move fires — which is by design (the snake moves on a grid).

---

## What We're Good At

- **Ring buffer is zero-allocation:** `segments[MAX_SNAKE]` is embedded in `GameState` — no `malloc`, no `free`, no `std::list` iterator overhead. The original used `std::list<sSnakeSegment>` with heap allocation per segment.
- **Sub-frame overshoot preserved:** `move_timer -= move_interval` instead of `move_timer = 0` means at 60 fps + 150ms interval, each move fires at exactly the right time without drift over hundreds of steps.
- **`best_score` survives restart:** `snake_init()` saves `s->best_score` before `memset` and restores after. The original reset everything on restart. One line of code, meaningful quality-of-life.
- **Direction input decoupled from movement step:** `next_direction` is buffered and applied only when the snake actually moves — not every frame. This eliminates input-order sensitivity.
- **Death is visually unambiguous:** entire snake body turns `COLOR_DARK_RED` on game over — you always know what happened and where.
- **Platform separation is total:** `snake.c` has zero OS includes. `snake_init`, `snake_update`, `snake_render` are pure functions of `GameState`/`GameInput`/`SnakeBackbuffer` — fully unit-testable without a display.

---

## What We're Bad At / Missing vs Original

### Walls mode / no-walls toggle
Some Snake variants let the snake wrap around the edges instead of dying.

**How to add:**
1. Add `bool wrap_walls` to `GameState` (or a `SnakeConfig` parameter).
2. In `snake_update()`, replace the wall-collision check with: `new_x = (new_x + GRID_WIDTH) % GRID_WIDTH; new_y = (new_y + GRID_HEIGHT) % GRID_HEIGHT;` when `wrap_walls` is true.
3. Show wrap mode indicator in HUD header.

### Speed level visible to player
The player has no idea how fast they're going or what "level" they're on.

**How to add:**
1. Derive level from score: `int level = s->score / 3 + 1;` (since speed increases every 3 points).
2. Add `draw_text(bb, ..., "LVL:X", ...)` to the header alongside score/best.
3. No `GameState` changes needed — it's a pure render-time calculation.

### Input queue depth > 1
If the player presses two direction keys between two move steps, only the last is kept.

**How to add:**
1. Replace `next_direction` with `SNAKE_DIR dir_queue[3]; int dir_queue_len;` in `GameState`.
2. On just-pressed turn: if `dir_queue_len < 3`, append `dir_queue[dir_queue_len++]`.
3. On each move step: pop from front (`dir_queue[0]`), shift remaining entries down.
4. Validate each queued direction (can't reverse) at pop time, not enqueue time.

### Pause screen
No way to pause mid-game.

**How to add:**
1. Add `GameButtonState pause_btn` to `GameInput` and `bool paused` to `GameState`.
2. In the platform loop: if not paused, call `snake_update()`; always call `snake_render()`.
3. In `snake_render()`: if `paused`, draw a semi-transparent overlay + "PAUSED" text.
4. Press `P` to toggle `state->paused`.

### Highscore persistence to disk
`best_score` is lost when the process exits.

**How to add:**
In the platform layer (`main_x11.c` / `main_raylib.c`), on startup: read `~/.snake_highscore` and set `state.best_score`; on shutdown (or any time `best_score` updates): write the value back with `fwrite()`. Keep this in the platform layer — `snake.c` stays disk-free.

---

## Features We Added (Not in Original)

### `best_score` preserved across resets
**Original:** `game_over = false` and full state wipe on restart — best score gone.
**This port:** `snake_init()` explicitly saves `int saved_best = s->best_score;` before `memset(s, 0, sizeof(*s))` and restores it after. This is a one-line change with meaningful impact on player motivation.

### Delta-time movement (vs original tick-based)
**Original:** sleep/tick loop — framerate-coupled. Running at a different tick rate changes game speed.
**This port:** `move_timer += delta_time; if (move_timer >= move_interval) { move_timer -= move_interval; ... }`. Works identically at 30, 60, or 144 fps. Sub-frame overshoot is preserved, so the snake moves at a mathematically consistent rate.

### `GameButtonState` input system
**Original:** `GetAsyncKeyState()` sampled once per frame — misses taps shorter than one frame, no transition count.
**This port:** `ended_down` + `half_transition_count` catches every state change between frames. Direction turns check `half_transition_count > 0` — a key held from last frame (`htc == 0, ended_down == 1`) does not fire a new turn, preventing unintended auto-turns.

### Decreasing `move_interval` as difficulty ramp
**Original:** constant speed (or a simple multiplier).
**This port:** `move_interval -= 0.01f` every 3 score points, floored at `0.05f` (20 steps/sec). This gives a smooth, continuous difficulty curve rather than discrete level jumps. The formula is visible in `snake_update()` and easy to tune.

---

## Fun Extension Ideas

### 1. No-walls / wrap mode
**What:** Snake wraps around edges instead of dying.
**How:** In `snake_update()`, replace wall-kill check with `new_x = (new_x + GRID_WIDTH) % GRID_WIDTH` etc. when `state->wrap_walls` is set.
**Optional:** `bool wrap_walls` in `GameState`; toggle with `W` key; show "WRAP" in HUD.

### 2. Input queue depth 2
**What:** Buffer up to 2 pending turns so corner sequences feel responsive.
**How:** Replace `next_direction` with `SNAKE_DIR dir_queue[2]; int dir_queue_len`. Append on just-pressed; pop on each move step. Validate (no reversal) at pop time.
**Optional:** `#define INPUT_QUEUE_DEPTH 2` — easy to tune or disable.

### 3. Grow amount difficulty scaling
**What:** Higher levels give more segments per food (harder to avoid your own tail).
**How:** In `snake_update()` food-eat path: `s->grow_pending += 5 + s->score / 10;` (caps out at +15 at score 100).
**Optional:** `int grow_per_food` field in `GameState`; update on level-up.

### 4. Multiple food items
**What:** 2–3 food items on the field at once; eating one spawns a replacement.
**How:** Replace `food_x/food_y` with `Segment food[3]; int food_count`. `snake_spawn_food()` picks an open slot. In the eat check, loop over all food items. Each food can have a different color (yellow = 2pts, red = 1pt).
**Optional:** `int max_food_count` in `GameState`; starts at 1, increases with level.

### 5. Score delta pop-ups
**What:** A "+1" or "+5" text briefly appears at the food location when eaten.
**How:** Add `struct { int x; int y; float ttl; int value; } popups[8]; int popup_count;` to `GameState`. On eat: push a popup. In `snake_update()`: decay `ttl -= delta_time`, remove expired. In `snake_render()`: draw active popups with alpha proportional to `ttl`.
**Optional:** `#define FEATURE_SCORE_POPUPS 1`.

### 6. Shrink power-up
**What:** A special (blue) food item that removes 3 tail segments instead of growing.
**How:** Add `bool shrink_food_active; Segment shrink_food;` to `GameState`. Spawn with 10% probability alongside normal food. On eat: `s->grow_pending -= 3` (clamp to 0 and also shrink `length` by up to 3).
**Optional:** `bool enable_shrink_powerup` in `GameState`.

### 7. Pause with blur overlay
**What:** `P` pauses the game; play field dims while paused.
**How:** Add `bool paused` to `GameState`. In platform loop: `if (!state.paused) snake_update(...)`. In `snake_render()`: if paused, call `draw_rect_blend(bb, 0, HEADER_ROWS * CELL_SIZE, bb->width, GRID_HEIGHT * CELL_SIZE, SNAKE_RGBA(0,0,0,160))` then draw "PAUSED" text centered.
**Optional:** Always available; `P` key binding added to `GameInput`.

### 8. Speed level indicator in HUD
**What:** Show current speed level (e.g., "SPD: 5") in the header bar.
**How:** In `snake_render()`, derive speed level from `move_interval`: `int spd = (int)((0.15f - s->move_interval) / 0.01f) + 1;` then `snprintf(buf, ..., "SPD:%d", spd)` and `draw_text(...)` in the header.
**Optional:** Always-on — purely additive to the render function, no `GameState` changes.
