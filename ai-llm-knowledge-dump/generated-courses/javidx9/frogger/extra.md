# Frogger — Extra Notes

## What This Implementation Handles

- **10-lane map**: home row (wall + 3 homes), 3 river rows (logs + water), safe median strip, 5 road rows (cars + bus), safe start row.
- **Smooth sub-pixel lane scroll**: `lane_scroll()` computes scroll in pixels, not tile units, giving fluid motion at any frame rate.
- **River-carry physics**: on river rows (y ≤ 3) `frog_x -= lane_speeds[fy] * dt` is applied every tick — the frog rides the log.
- **Danger buffer**: `uint8_t danger[SCREEN_CELLS_W * SCREEN_CELLS_H]` rebuilt from lane patterns each tick; collision is a single array lookup.
- **Center-cell collision**: uses the frog's center pixel `(frog_px + TILE_PX/2, frog_py + TILE_PX/2)` divided by `CELL_PX` — immune to log-edge flickering.
- **Three vehicle types**: `car1` (2-cell), `car2` (2-cell), `bus` (4-cell); each tile character maps directly to a sprite section via `tile_to_sprite`.
- **Death flash**: frog blinks at 10 Hz (`dead_timer / 0.05f`) then resets to start row after 0.4 s; a semi-transparent black overlay dims the scene.
- **Win detection**: pattern position at `frog_y == 0` checked against `lane_patterns[0]`; `'h'` tiles count as homes reached.
- **HUD**: `HOMES: N` counter in top-left; `DEAD!` overlay; `YOU WIN!` overlay at 3 homes.
- **Bitmap font**: inline 5 × 7 column-major font; `draw_text` renders any ASCII string at arbitrary scale.
- **Alpha-blended overlays**: `draw_rect_blend` does software alpha-compositing for the death/win dimmer.
- **No heap allocations in game loop**: all sprites live in a flat `SpriteBank` pool allocated once at init.

---

## Input Feel Analysis

### 1. Responsiveness ✅ Good
Input is read by the platform layer each frame and passed to `frogger_tick` as `GameInput`. Movement fires in the same call:
```c
if (input->move_up.ended_down && input->move_up.half_transition_count > 0)
    state->frog_y -= 1.0f;
```
There is zero queuing or buffering. A hop lands within one frame of the key event.

### 2. Consistency ✅ Good
`lane_scroll()` is called identically in both `frogger_tick` (danger buffer) and `frogger_render` (drawing). The collision grid always matches what is on screen — no frame-offset mismatch. Same input always produces the same tile delta.

### 3. Forgiveness ⚠️ Partial
Center-cell collision (`cx = (frog_px + TILE_PX/2) / CELL_PX`) means the frog must have its **center** over a deadly tile to die — touching a log edge with the frog's corner is forgiving. However there is no input buffer (tap while mid-hop does nothing) and no coyote-time on log edges, so pixel-perfect river timing is still punishing.

### 4. Feedback ⚠️ Partial
Death is confirmed by a 10 Hz frog blink + semi-transparent `DEAD!` overlay:
```c
int flash = (int)(state->dead_timer / 0.05f);
show_frog = (flash % 2 == 0);
```
There is no audio, no particle effect, and no animation for reaching a home (frog instantly teleports back). Win state shows `YOU WIN!` but provides no per-home celebration.

### 5. Control ✅ Good
One-tile-per-press movement gives the player precise deterministic control. The `GameButtonState` design (`half_transition_count > 0`) prevents repeated hops from a held key — essential for a grid game where holding would cause immediate death.

### 6. Precision ✅ Good
Grid-aligned hops mean each move lands exactly on a tile boundary. River carry is continuous float math (`frog_x -= lane_speeds[fy] * dt`) which lets the player drift naturally without sudden position snaps.

### 7. Fluidity ⚠️ Partial
Log carry is smooth (delta-time float math) but the frog's own movement is instant — no hop animation arc. The frog teleports one tile rather than sliding, which is faithful to the arcade original but may feel abrupt.

### 8. Intuitiveness ✅ Good
Arrow key / WASD movement matching the on-screen direction is immediately obvious. The "hop once per press" feel matches the original arcade cabinet. New players learn the control scheme in seconds.

### 9. Customization ❌ Missing
Key bindings are hardcoded in the platform layer. There is no remapping, no gamepad support, and no accessibility options (e.g., hold-to-hop for players with motor difficulties).

### 10. Low Latency ✅ Good
The platform layer polls events, calls `frogger_tick`, then immediately blits the backbuffer — single-buffered presentation. There is no extra frame of latency from a command queue or render thread.

---

## What We're Good At

- **Smooth scrolling**: positive-modulo pixel math in `lane_scroll()` fixes the C-truncation jump bug that plagued the original console version; sprites never jump 8 pixels at a time.
- **Collision accuracy**: the danger buffer + center-cell check is more reliable than per-sprite AABB in the original, which could have edge-cell false positives.
- **Platform independence**: `frogger.c` has zero X11 / Raylib symbols; swapping the platform layer is a single compile-time choice.
- **No allocation in hot path**: `SpriteBank` is a flat pool in `GameState`; no `malloc` / `free` occurs during gameplay.
- **Readable DOD layout**: `lane_speeds[]` (40 bytes) fits in one cache line and is the only array touched when rebuilding the danger buffer.

---

## What We're Bad At / Missing vs Original

| Missing feature | How to implement |
|-----------------|-----------------|
| **Countdown timer per frog** | Add `float frog_timer` to `GameState`; decrement in `frogger_tick`; death when `frog_timer <= 0`; reset on hop to start and on reaching home. Draw a shrinking bar in `frogger_render`. |
| **Lives system (3 lives)** | Add `int lives` to `GameState`; decrement on death instead of immediate respawn; show life icons in HUD; game-over state when `lives == 0`. |
| **Difficulty waves (increasing speed)** | Multiply `lane_speeds[y]` by a `float speed_mult` that increments each time `homes_reached` hits a multiple of 3. Store `speed_mult` in `GameState`. |
| **Crocodile / snake hazards** | Add a `'c'` (croc) and `'s'` (snake) tile character to `lane_patterns`; make them deadly; add `SPR_CROC` / `SPR_SNAKE` sprite IDs and `.spr` files; handle in `tile_to_sprite` and `tile_is_safe`. |
| **Home occupation animation** | Add `int home_occupied[3]` to `GameState`; when a home is reached set the flag and draw the frog sprite over the home tile in `frogger_render`; block re-entry to occupied homes. |
| **Score for time remaining** | At win-per-home: `state->score += (int)(frog_timer * 10)` where `frog_timer` is the per-frog countdown; display score in HUD. |
| **Proper Frogger scoring tiers** | Large asteroid → 200 pts; log hop without moving → 10 pts; reaching home → 50 pts + time bonus. Add `int score` to `GameState` and increment at each event. |
| **Lily-pad / home entry animation** | Play a short 4-frame splash sequence when the frog lands on `'h'`; store `home_anim_timer` + `home_anim_frame` in `GameState`; draw in `frogger_render`. |

---

## Features We Added (Not in Original)

| Feature | Why it's better |
|---------|----------------|
| **`GameButtonState` with `half_transition_count`** | The original used key-released detection for hops, which added ~1 frame of latency and broke fast tapping. `half_transition_count > 0` fires the hop on *key-down*, making it feel snappier. |
| **Backbuffer pipeline** | The original wrote colored characters to a Windows console buffer cell-by-cell. Our approach writes a CPU pixel array once per frame and blits it as a texture — simpler, faster, and supports sub-cell detail. |
| **Delta-time everywhere** | The original ran on a fixed Windows console tick. Our `dt`-driven logic (lane carry, death timer) runs at any frame rate without changing game speed. |
| **`danger[]` buffer rebuilt from scroll math** | The original did per-sprite AABB at hit-test time. Rebuilding a flat byte array once per tick makes collision detection a single `danger[cy * W + cx]` lookup — O(1), cache-friendly. |
| **Center-cell collision** | Avoids log-edge false positives where the frog center is safely on a log but one corner cell overlaps water. Original arcade used full-tile AABB which was visually harsh at log edges. |
| **Positive-modulo pixel scroll** | Fixes the C `(int)` truncation-toward-zero bug for negative lane speeds; sprites scroll smoothly rather than jumping one cell per sign change. |

---

## Fun Extension Ideas

### 1. Countdown Timer Bar
**What**: A horizontal HUD bar that shrinks each second; frog dies when it hits zero.  
**How**: Add `float frog_timer` (init to 30.0f) to `GameState`. In `frogger_tick` decrement by `dt`; trigger death at `<= 0`. In `frogger_render` draw a colored `draw_rect` bar proportional to `frog_timer / 30.0f`.  
**Optional**: `#define FEATURE_TIMER` — `#ifdef` the decrement and the bar render.

### 2. Three-Lives System
**What**: Player gets 3 lives; lose one per death; game-over overlay after the last.  
**How**: Add `int lives` (init 3) to `GameState`. On death decrement `lives`; if `lives == 0` enter a new `PHASE_GAMEOVER`. Draw 3 frog-icon sprites in the HUD.  
**Optional**: `#define FEATURE_LIVES`.

### 3. Speed Waves
**What**: After every 3 homes, all lane speeds increase by 15%.  
**How**: Add `float speed_mult` (init 1.0f) to `GameState`. In `frogger_tick` multiply looked-up speeds by `speed_mult` when calling `lane_scroll`. Increment `speed_mult += 0.15f` when `homes_reached % 3 == 0`.  
**Optional**: `GameState` flag `int waves_enabled`.

### 4. Crocodile Hazard
**What**: A croc tile that looks like a log but is deadly on the head end.  
**How**: Add `'C'` (body, safe) and `'c'` (head, deadly) to `lane_patterns`, `tile_is_safe`, and `tile_to_sprite`. Add `SPR_CROC` with two 8-cell sections.  
**Optional**: `#define FEATURE_CROC`.

### 5. Score System
**What**: Points for each hop forward (+10), reaching a home (+500 + time bonus), extra life at 10 000 pts.  
**How**: Add `int score` to `GameState`. Increment in `frogger_tick` at each hop (`frog_y` decreasing) and at home arrival. Display `SCORE: %d` in HUD.  
**Optional**: Always on; controlled by `#define FEATURE_SCORE`.

### 6. Home Occupation Tracking
**What**: Each home slot shows a small frog after being reached; can't re-enter an occupied home.  
**How**: Replace `homes_reached` int with `int home_occupied[3]`. Map `frog_x` to home index on win detection; set `home_occupied[idx] = 1`. In render, draw `SPR_FROG` over home tiles whose flag is set.  
**Optional**: `GameState` flag `int track_homes`.

### 7. Parallax Background Stars
**What**: A static dot pattern behind the river to give a sense of depth.  
**How**: Generate a `uint16_t star_x[64], star_y[64]` array once at `frogger_init` using random values. In `frogger_render`, draw each star as a 2×2 `draw_rect` in dark blue before the lane tiles.  
**Optional**: `#define FEATURE_STARS`.

### 8. Input Replay / Ghost Mode
**What**: Record all inputs + timestamps; play back as a ghost frog (translucent blue overlay).  
**How**: Add `GameInput replay_log[4096]; int replay_len; int replay_pos` to `GameState`. On each tick, append current input. In ghost mode, drive a second `float ghost_x, ghost_y` through `frogger_tick` using `replay_log[replay_pos++]` and draw it with `draw_rect_blend` at 50% alpha.  
**Optional**: `#define FEATURE_GHOST`; triggered by pressing `R`.
