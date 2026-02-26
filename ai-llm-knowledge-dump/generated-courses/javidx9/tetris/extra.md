# Tetris — Extra Notes

## What This Implementation Handles

- 7 standard tetrominoes (I, J, L, O, S, T, Z), each with distinct color
- 4-rotation system via `tetromino_pos_value()` index math (0°/90°/180°/270°)
- 12×18 play field with left/right/bottom walls (`TETRIS_FIELD_WALL`)
- Collision detection: `tetromino_does_piece_fit()` checks all 4×4 piece cells against the field
- Gravity timer: `tetromino_drop` (`GameActionRepeat`) accumulates delta-time; piece falls when `timer >= interval`
- Soft drop: down arrow with `GameActionWithRepeat` (30ms auto-repeat interval)
- DAS (Delayed Auto-Shift) for left/right movement: `GameActionWithRepeat` with 50ms repeat interval
- Rotation in both directions: clockwise (`X`) and counter-clockwise (`Z`), via `TETROMINO_ROTATE_X_VALUE` enum
- Piece locking: when gravity can't move piece down, it's stamped into `state->field[]`
- Line detection: up to 4 completed rows detected per piece lock (`TETROMINO_LAYER_COUNT`)
- Line clear flash: completed rows tagged `TETRIS_FIELD_TMP_FLASH` (white), game logic frozen for 0.4s
- Line collapse: rows removed bottom-to-top to avoid stale index bugs
- Score: 25 pts per piece + `(1 << lines_cleared) * 100` bonus for multi-line clears
- Level: increments every 25 pieces locked (`pieces_count % 25 == 0`)
- Drop speed: `interval` scales with level (faster per level)
- Next-piece preview in sidebar (half-size preview at 15px cells)
- Game over detection: new piece doesn't fit at spawn position
- HUD sidebar: score, level, pieces count, next piece preview, controls hint
- Game over overlay with semi-transparent alpha-blended panel
- Restart (`R`) and quit (`Q`/`Esc`) from game over screen
- Platform-independent rendering via `TetrisBackbuffer` (CPU pixel array, `0xAARRGGBB`)
- Built-in 5×7 bitmap font for all HUD text

---

## Input Feel Analysis

### 1. Responsiveness — ✅ Good
`GameButtonState.half_transition_count > 0 && ended_down` detects a press within the same frame it occurred, even at 30 fps where a 20ms tap fits inside a 33ms frame. Rotation fires in `tetris_apply_input()` which runs every `tetris_update()` call, before any gravity logic — so rotate always responds first.

### 2. Consistency — ✅ Good
`tetromino_does_piece_fit()` is a pure function of `(state, piece, rotation, x, y)` with no random or time-dependent elements. The same key sequence produces the same result every time. `UPDATE_BUTTON` only increments `half_transition_count` when the state actually changes, preventing phantom double-fires.

### 3. Forgiveness — ⚠️ Partial
DAS (`GameActionWithRepeat`, 50ms repeat for left/right, 30ms for down) gives players time to tap without over-shooting. However, there is no **lock delay** (a standard Tetris feature where the piece waits ~500ms after landing before locking, letting players slide or rotate). Once gravity fires and the piece can't drop, it locks immediately on the same tick. Players who react slowly will lose the piece.

### 4. Feedback — ⚠️ Partial
Completed lines flash white (`TETRIS_FIELD_TMP_FLASH`) for 0.4s — clear visual feedback. Each piece color is distinct (cyan/blue/orange/yellow/green/magenta/red). However, there is **no audio** (no lock sound, no line-clear sound, no level-up ding) and no **ghost piece** showing where the current piece will land.

### 5. Control — ✅ Good
Left/right/down all use `GameActionWithRepeat` with independent timers — holding left doesn't interfere with tapping down. Rotation is separate (`rotate_x.button`) with no repeat, so a held rotate key only fires once per press. Players are not fighting the engine.

### 6. Precision — ⚠️ Partial
DAS 50ms repeat interval is responsive enough for precise column targeting. However, without a **hard drop** (instant fall to bottom) and without a **ghost piece**, precision placement requires careful soft-drop counting. Expert Tetris players rely heavily on hard drop + ghost for precision; this port lacks both.

### 7. Fluidity — ✅ Good
Delta-time movement means pieces fall and respond smoothly at any framerate. `repeat.timer -= repeat.interval` (not zeroing) keeps sub-frame remainder, so auto-repeat fires at mathematically consistent intervals. No stuttering from integer tick accumulation errors.

### 8. Intuitiveness — ✅ Good
Arrow keys for movement, X/Z for CW/CCW rotation is the standard Tetris guideline mapping. Controls hint is rendered in the sidebar at all times. Rotation direction is typed (`TETROMINO_ROTATE_X_VALUE` enum) rather than a raw `±1` int, preventing confusion in the code.

### 9. Customization — ❌ Missing
Auto-repeat intervals are set at `game_init()` time (50ms left/right, 30ms down) with no way for the player to change them at runtime. There are no keybinding options. Adding customization would require exposing `input->move_left.repeat.interval` etc. through a settings struct.

### 10. Low Latency — ✅ Good
The platform layer calls `tetris_update()` → `tetris_render()` → blit every frame with no queuing or interpolation delay. `tetris_apply_input()` runs at the top of `tetris_update()`, before gravity, so input is processed with minimum pipeline delay — effectively 0–1 frame latency.

---

## What We're Good At

- **Framerate independence:** delta-time throughout, including DAS timers and gravity — works correctly at 30, 60, 144 fps or unlocked
- **DAS is correct:** first press fires immediately, then auto-repeat at fixed interval (not a naive "fire every N frames")
- **Bi-directional rotation:** typed enum `TETROMINO_ROTATE_X_VALUE` catches programming errors at compile time; original used a raw int
- **Multi-line scoring:** `(1 << count) * 100` rewards Tetris (4-line) clears exponentially — better than flat per-line
- **Flash freeze is exact:** during line-clear animation, `tetris_apply_input()` is **not** called — player can't accidentally move during the flash (original had a frame-count sleep that blocked input entirely)
- **Platform separation is total:** `tetris.c` has zero `#include <X11/...>` or `#include <raylib.h>` — you can unit-test game logic without a display
- **Alpha-blend overlay:** game-over panel uses `draw_rect_blend()` with proper `src * alpha + dst * (1-alpha)` — not a black box, not a library call

---

## What We're Bad At / Missing vs Original

### Ghost piece (shadow)
The original didn't have one either, but modern Tetris considers it essential. Without it, players can't see where a piece will land.

**How to add:**
1. In `tetris_render()`, after drawing the field and before drawing the current piece, compute ghost Y: start from `state->current_piece.y`, keep incrementing while `tetromino_does_piece_fit()` returns true.
2. Draw the piece at ghost Y with a dimmed color (e.g., `COLOR_DARK_GRAY` or 40% alpha blend of the piece color).
3. No `GameState` changes needed — it's a pure render-time calculation.

### Hold piece
Standard since Tetris guideline (2001). Press `C` to swap current piece with held piece (once per drop).

**How to add:**
1. Add `int hold_index` and `bool hold_used` to `GameState`.
2. Add `GameButtonState hold` to `GameInput`.
3. In `tetris_apply_input()`: if `hold` just pressed and `!hold_used`, swap `current_piece.index` ↔ `hold_index`, reset position/rotation, set `hold_used = true`.
4. Reset `hold_used = false` when a piece locks.
5. Draw hold piece in the sidebar next to the next-piece preview.

### Hard drop
Press `Space` to instantly teleport piece to its ghost position and lock.

**How to add:**
1. Add `GameButtonState hard_drop` to `GameInput`.
2. In `tetris_apply_input()`: compute ghost Y (same loop as ghost piece above), set `current_piece.y = ghost_y`, then immediately trigger the lock logic (extract lock path from `tetris_update()` into a helper).
3. Award 2× score per cell dropped (standard scoring).

### Proper Tetris scoring (BPS or Guideline)
Current: flat 25 pts/piece + `(1 << lines) * 100`. Standard: based on lines cleared × level multiplier (Single=100, Double=300, Triple=500, Tetris=800, all ×level).

**How to change:**
In the lock section of `tetris_update()`, replace:
```c
state->score += 25;
if (state->completed_lines.count > 0)
    state->score += (1 << state->completed_lines.count) * 100;
```
With a lookup: `static const int LINE_SCORES[] = {0, 100, 300, 500, 800};` then `state->score += LINE_SCORES[count] * (state->level + 1);`

### Wall kicks (SRS rotation system)
When a rotation fails at the current position, Tetris SRS tries 4 offset positions before giving up. Currently: if `tetromino_does_piece_fit()` fails, rotation is silently ignored.

**How to add:**
Replace the single fit check in `tetris_apply_input()` with a loop over an SRS offset table (5 test positions per piece per rotation state). Only reject rotation if all 5 tests fail.

### Soft drop scoring
Standard: +1 point per cell soft-dropped. Currently soft drop gives no bonus.

**How to add:** In `handle_action_with_repeat` for `move_down`, when `should_move_down` fires and the piece moves, increment `state->score += 1`.

### Level-up based on lines cleared (not pieces)
Standard Tetris levels up every 10 lines. Currently levels up every 25 pieces, which doesn't match the standard experience.

**How to change:**
Add `int lines_cleared_total` to `GameState`. Add `lines_cleared_total += completed_lines.count` on each lock. Replace `pieces_count % 25` check with `lines_cleared_total / 10 > state->level`.

---

## Features We Added (Not in Original)

### `GameButtonState` input system
**Original:** simple `bool` per key, checked with a single `GetAsyncKeyState()` call — misses fast taps, no transition counting.
**This port:** `ended_down` + `half_transition_count` correctly detects press/release/tap even when a key is held for less than one frame. Eliminates missed inputs at low framerates.

### Delta-time physics
**Original:** tick-count loop (`speed_count++; if speed_count == speed { drop; speed_count = 0; }`). This is framerate-coupled — run at 30 fps and the game is half-speed.
**This port:** `tetromino_drop.timer += delta_time; if timer >= interval { drop; timer -= interval; }`. The `timer -= interval` (not `= 0`) preserves sub-frame overshoot, so 60 fps and 144 fps produce the same drop rate.

### Proper DAS (Delayed Auto-Shift)
**Original:** no auto-repeat at all — one press = one move.
**This port:** `GameActionWithRepeat` with `handle_action_with_repeat()` — first press fires immediately, then fires every `repeat.interval` seconds while held. 50ms left/right and 30ms down is the standard feel.

### Typed rotation direction enum
**Original:** rotation was `(nCurrentRotation + 1) % 4` — a plain integer, easy to pass wrong values.
**This port:** `TETROMINO_ROTATE_X_VALUE` enum (`NONE`, `GO_LEFT`, `GO_RIGHT`) — the compiler catches incorrect values. Rotation direction is explicit and self-documenting.

### Alpha-blended overlays
**Original:** console — no alpha blending; overlays were just `cout` strings.
**This port:** `draw_rect_blend()` performs proper per-pixel alpha compositing for the game-over panel, giving a polished semi-transparent dim effect.

---

## Fun Extension Ideas

### 1. Ghost piece (landing preview)
**What:** A dimmed silhouette showing exactly where the current piece will land.
**How:** In `tetris_render()`, loop `ghost_y = current_piece.y; while (tetromino_does_piece_fit(state, ..., ghost_y+1)) ghost_y++;` then call `draw_piece()` with `COLOR_DARK_GRAY`.
**Optional:** `#define FEATURE_GHOST_PIECE 1` — wrap the render call in `#if FEATURE_GHOST_PIECE`.

### 2. Hold piece
**What:** Press `C` to park the current piece and swap with the held one.
**How:** Add `int hold_index; bool hold_used;` to `GameState`. Add `GameButtonState hold` to `GameInput`. In `tetris_apply_input()`, on just-press of hold: swap and reset current piece. Draw held piece in sidebar.
**Optional:** `bool enable_hold` in `GameState` — set from a config/menu.

### 3. Hard drop
**What:** Press `Space` to instantly drop and lock the piece at its ghost position.
**How:** Compute ghost Y in `tetris_apply_input()`, set `current_piece.y = ghost_y`, then call a `tetris_lock_piece()` helper extracted from `tetris_update()`. Award 2 pts per cell dropped.
**Optional:** `#define FEATURE_HARD_DROP 1`.

### 4. T-spin detection
**What:** Detect when a T-piece locks after a rotation (a skill move worth bonus points).
**How:** After locking, if piece was T and `last_action == ROTATE`: check 3 of 4 corner cells of the T's 3×3 bounding box are filled → T-spin. Add `int last_action` to `CurrentPiece`.
**Optional:** `bool enable_tspins` in `GameState`.

### 5. Combo counter
**What:** Award bonus points for clearing lines on consecutive piece locks.
**How:** Add `int combo` to `GameState`. On lock: if `completed_lines.count > 0`, `combo++`, `score += 50 * combo * level`. If no lines cleared, `combo = 0`.
**Optional:** `bool enable_combos` flag; display combo count in HUD if `combo > 1`.

### 6. Pause screen
**What:** Press `P` to freeze the game and show a "PAUSED" overlay.
**How:** Add `bool paused` to `GameState`. In the game loop: `if (!state->paused) tetris_update(...)`. In `tetris_render()`: if `paused`, draw a semi-transparent overlay + "PAUSED" text with `draw_rect_blend` + `draw_text`.
**Optional:** Always available — just add the key binding.

### 7. Color themes
**What:** Switch between color palettes (classic, monochrome, pastel, terminal-green).
**How:** Define `typedef uint32_t TetrominoColorTable[7]` with named tables. Pass a pointer to `tetris_render()`. Add `int active_theme` to `GameState` or pass from the platform layer.
**Optional:** `#define DEFAULT_THEME THEME_CLASSIC` — compile-time default, runtime switch via `T` key.

### 8. Bag randomizer (7-bag)
**What:** Standard Tetris piece randomization: each set of 7 pieces is a shuffled permutation of all 7 types, not fully random. Prevents long droughts of any piece.
**How:** Add `int bag[7]; int bag_pos;` to `GameState`. On `game_init()`, fill and shuffle bag (Fisher-Yates). On each spawn, take `bag[bag_pos++]`; when `bag_pos == 7`, reshuffle.
**Optional:** `bool use_bag_randomizer` in `GameState` (default on); set to false for pure random.
