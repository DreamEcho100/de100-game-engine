# Asteroids — Extra Notes

## What This Implementation Handles

- **Wireframe ship**: isosceles triangle model (`ship_model[3]`) scaled by `SHIP_SCALE = 10`; rotated and translated each frame by `draw_wireframe`.
- **Jagged asteroid model**: 20-vertex polygon built once at init; each vertex displaced ±20% from a unit circle for organic shapes; reused for all asteroid sizes.
- **Three asteroid sizes**: `LARGE_SIZE = 64`, `MEDIUM_SIZE = 32`, `SMALL_SIZE = 16` (radii in pixels). Scoring: `(LARGE_SIZE / size) × 100` per hit (100 / 200 / 400).
- **Asteroid splitting**: on hit, if `size > SMALL_SIZE`, two children spawned at random angles with `new_speed = 60 px/s` and `new_size = size / 2`.
- **Euler physics**: `dx += sin(angle) × THRUST_ACCEL × dt`; `x += dx × dt`. No drag — velocity persists indefinitely.
- **Toroidal space**: `wrap_coordinates` on player, bullets, and asteroids; `draw_pixel_w` also wraps so wireframe lines crossing the edge are seamless.
- **Bullet lifetime**: `BULLET_LIFE = 2.5 s`; `b->timer -= dt`; marked inactive when expired.
- **Circle collision**: `dx²+dy² < r²` — squared distance avoids `sqrt`. Used for both bullet↔asteroid and player↔asteroid.
- **Entity pools with `compact_pool`**: swap-with-last removes inactive objects; O(1) amortized, no shifting.
- **Level clear**: `asteroid_count == 0` → `score += 1000`; two new large asteroids spawned 150 px left/right of player heading.
- **Death state machine**: `PHASE_DEAD` freezes `asteroids_update`, blinks ship red at 10 Hz, resets after `DEAD_ANIM_TIME = 1.5 s`.
- **HUD**: `SCORE: N` in top-left; `GAME OVER` overlay with red tint during death.
- **Bitmap font**: inline 5 × 7 column-major font shared with frogger; `draw_text` at arbitrary scale.
- **Fixed pools, no heap in loop**: `SpaceObject asteroids[64]`, `SpaceObject bullets[32]` — all in `GameState` on the stack/static area.

---

## Input Feel Analysis

### 1. Responsiveness ✅ Good
Rotation and thrust are checked against `ended_down` every frame with no frame delay:
```c
if (input->left.ended_down)
    state->player.angle -= ROTATE_SPEED * dt;
if (input->up.ended_down)
    state->player.dx += sinf(state->player.angle) * THRUST_ACCEL * dt;
```
The ship begins turning and accelerating in the same frame the key is pressed.

### 2. Consistency ✅ Good
Euler integration is deterministic: same `dt` sequence → same trajectory. `compact_pool` never reorders active objects during the current frame (it runs after both collision loops complete), so collision order is stable.

### 3. Forgiveness ⚠️ Partial
Circle collision with the ship uses the **player's center point** tested inside each asteroid circle:
```c
is_point_inside_circle(a->x, a->y, (float)a->size, state->player.x, state->player.y)
```
This is a single-point test — the ship's triangle geometry is ignored. Passing close to an asteroid's drawn edge is possible without dying, which is forgiving. However there is no invincibility period after respawn, so the player can immediately die on re-entry if an asteroid happens to overlap the spawn point.

### 4. Feedback ⚠️ Partial
Death triggers a 1.5 s red-blink + `GAME OVER` overlay — visually clear. Bullet hits are silent (no particle burst, no flash). Level clear has no fanfare; new asteroids appear immediately. No audio anywhere.

### 5. Control ✅ Good
`ROTATE_SPEED = 4.0 rad/s` is fast enough to feel responsive yet slow enough for precision. Thrust is additive — tap for micro-corrections, hold for acceleration. Fire is strictly once-per-press:
```c
if (input->fire.ended_down && input->fire.half_transition_count > 0)
    add_bullet(...)
```
Holding Space does not spray bullets.

### 6. Precision ✅ Good
Continuous analog rotation (`ROTATE_SPEED × dt`) allows the player to dial in exactly the angle they want. The triangle nose spawns bullets from the correct world position (`nose_x = player.x + sin(angle) × SHIP_SCALE × 5`), so aim matches the visual direction.

### 7. Fluidity ✅ Good
Float-position physics + delta-time give smooth inertial movement at any frame rate. Asteroids visually rotate (`a->angle += 0.5f × dt`) which adds life to the scene. Bullet dots move at `300 px/s` which feels fast and responsive.

### 8. Intuitiveness ✅ Good
Left/right to rotate, up to thrust, space to fire — identical to every Asteroids port since 1979. Players who have never seen this code already know the controls. `GameButtonState` ensures held thrust ≠ repeated fire, which matches intuition.

### 9. Customization ❌ Missing
No key remapping, no sensitivity slider for `ROTATE_SPEED` or `THRUST_ACCEL`, no gamepad support. Constants are `#define`d in `asteroids.h` (easily changed at compile time), but there is no runtime settings screen.

### 10. Low Latency ✅ Good
Single-buffered: platform polls events → `asteroids_update` → `asteroids_render` → blit. No command queue, no async render thread, no extra buffered frames.

---

## What We're Good At

- **Seamless edge-crossing wireframes**: `draw_pixel_w` wraps coordinates per-pixel inside the Bresenham inner loop. A line from `x=795` to `x=805` (800-wide screen) correctly draws `795..799` on the right and `0..5` on the left without any special case.
- **O(1) entity removal**: `compact_pool` overwrites dead slots with the last element and decrements count. The original C++ used `std::vector::erase` which is O(n) due to shifting.
- **Reusable asteroid model**: one 20-vertex polygon built at init is shared across all asteroid instances and sizes. Randomness is baked into the model once — no per-draw noise.
- **`trig` cost minimized**: `draw_wireframe` calls `cosf`/`sinf` exactly once per object per frame, before the vertex transform loop.
- **Level progression preserved**: clearing the wave awards a 1000-point bonus and immediately spawns the next wave, keeping the game alive indefinitely.

---

## What We're Bad At / Missing vs Original

| Missing feature | How to implement |
|-----------------|-----------------|
| **Hyperspace (panic teleport)** | Add `GameButtonState hyperspace` to `GameInput`. On just-pressed: set `player.x = rand() % SCREEN_W`, `player.y = rand() % SCREEN_H`. Optional death chance: if new position overlaps an asteroid (reuse `is_point_inside_circle` loop), trigger `PHASE_DEAD`. |
| **Extra lives at score milestones** | Add `int lives` and `int next_life_score` (init 10 000) to `GameState`. In `asteroids_update` after score increment: `if (score >= next_life_score) { lives++; next_life_score += 10000; }`. Draw life icons in `asteroids_render`. |
| **Multiple lives + game-over** | Add `int lives` (init 3). On `PHASE_DEAD`: decrement `lives`; if `lives == 0` enter `PHASE_GAMEOVER` (new enum value); render a high-score screen instead of resetting. |
| **Sound effects (pew / boom)** | Link a tiny audio library (e.g., miniaudio). Add `int sound_fire`, `int sound_explode` flags to `GameState`; set in `asteroids_update`; consume in platform layer: `if (state->sound_fire) play_sfx(SFX_FIRE)`. |
| **High score table** | Add `int hi_scores[10]` + `char hi_names[10][4]` to a persistent file. On `PHASE_GAMEOVER`, if `score > hi_scores[9]`, enter name-entry state; write to `~/.asteroids_scores`. |
| **Shields (brief invincibility)** | Add `float shield_timer` to `GameState`. On Shield key just-pressed: `shield_timer = 0.5f`. In update, skip player↔asteroid collision if `shield_timer > 0`. Draw a circle around the ship in `asteroids_render` while active. |
| **Difficulty scaling** | Multiply asteroid velocity by `1.0f + level * 0.1f` where `int level` increments on each wave clear. Store `level` in `GameState`. |
| **Proper arcade scoring** | Large asteroid: 20 pts; Medium: 50 pts; Small: 100 pts (original Atari values). Replace the `(LARGE_SIZE/size)*100` formula. |

---

## Features We Added (Not in Original)

| Feature | Why it's better |
|---------|----------------|
| **`GameButtonState` with `half_transition_count`** | Original used `bHeld` / `bReleased` booleans. `half_transition_count > 0` catches sub-frame taps that a polled bool would miss, and gives unambiguous just-pressed semantics without a "previous frame" shadow copy. |
| **Backbuffer + Bresenham lines** | Original drew ASCII characters to a Windows console buffer — one `█` per cell. Our approach writes actual pixel lines at sub-cell precision; wireframes look like wireframes, not character art. |
| **Delta-time physics** | Original used `fElapsedTime` but was written as a class method on a fixed-rate loop. Explicit `dt` parameter to `asteroids_update` makes the contract clear and allows variable-rate platforms (VSync off, high-refresh monitors). |
| **`compact_pool` entity management** | Original used `std::vector` with `remove_if` / `erase` — O(n) shifting per removal. Swap-with-last is O(1) amortized and avoids any heap churn in the hot loop. |
| **`PHASE_DEAD` state machine** | Original set `bDead = true` and called `ResetGame` on the next frame — a one-frame gap where update still ran. Explicit `PHASE_DEAD` freezes all update logic immediately and drives the death animation with a proper timer. |
| **Named color constants** | `COLOR_WHITE`, `COLOR_YELLOW`, `COLOR_RED`, etc. defined in `asteroids.h` avoid magic `ASTEROIDS_RGB(...)` calls throughout render code. |
| **Level clear spawn placement** | New asteroids are placed 150 px to the player's left and right (90° off heading) rather than at fixed corners. This gives the player a brief safe corridor straight ahead at wave start. |

---

## Fun Extension Ideas

### 1. Hyperspace Jump
**What**: Press `H` to instantly teleport to a random position — with a small chance of dying.  
**How**: Add `GameButtonState hyperspace` to `GameInput`. On just-pressed: pick `new_x = rand() % SCREEN_W`, `new_y = rand() % SCREEN_H`. Run `is_point_inside_circle` for each asteroid; if inside one, trigger `PHASE_DEAD`. Otherwise set `player.x/y`.  
**Optional**: `#define FEATURE_HYPERSPACE`.

### 2. Particle Explosion
**What**: A burst of 12–16 short-lived dots when an asteroid is destroyed.  
**How**: Add `SpaceObject particles[256]; int particle_count` to `GameState`. On asteroid death: loop 16 times, call `add_particle(state, a->x, a->y, random_angle)` with `timer = 0.4f` and random speed 80–150 px/s. In update, decrement timer and mark inactive. Render as colored `draw_pixel_w` dots.  
**Optional**: `#define FEATURE_PARTICLES`.

### 3. Shield Mechanic
**What**: Press `S` to activate a 0.5 s invincibility bubble.  
**How**: Add `float shield_timer; float shield_cooldown` to `GameState`. On just-pressed and `shield_cooldown <= 0`: `shield_timer = 0.5f; shield_cooldown = 5.0f`. Skip player↔asteroid collision while `shield_timer > 0`. Draw `draw_wireframe` with a circle model around the player.  
**Optional**: `GameState` flag `int shields_enabled`.

### 4. Thrust Exhaust Trail
**What**: While thrusting, emit a short trail of fading white dots behind the ship.  
**How**: Reuse the particle pool (from idea 2) or a dedicated `trail[8]` circular buffer. Each frame thrust is held, push current `player.x/y` (adjusted for the ship tail position) with `timer = 0.15f` and zero velocity. Render with reduced alpha (approximate via color dimming: `COLOR_GRAY`).  
**Optional**: `#define FEATURE_TRAIL`.

### 5. Score Multiplier
**What**: Destroying asteroids in rapid succession (within 2 s of each other) multiplies points.  
**How**: Add `float combo_timer; int combo_mult` to `GameState`. On each asteroid kill: `combo_mult++; combo_timer = 2.0f`. In update: `combo_timer -= dt; if <= 0, combo_mult = 1`. Apply `score += base_pts * combo_mult`. Draw `xN` multiplier next to score.  
**Optional**: `#define FEATURE_COMBO`.

### 6. UFO Enemy
**What**: A periodic small saucer that flies across the screen and fires at the player.  
**How**: Add a single `SpaceObject ufo; int ufo_active; float ufo_spawn_timer` to `GameState`. Timer counts down from 15 s; on zero, spawn UFO at a random screen edge moving at 80 px/s. Every 2 s the UFO fires a bullet toward `player.x/y`. UFO dies to player bullets (same circle collision). Score: 300 pts.  
**Optional**: `#define FEATURE_UFO`.

### 7. Co-op Two-Player (Split Ship Model)
**What**: A second ship controlled by IJKL keys on the same keyboard.  
**How**: Add `SpaceObject player2; GameButtonState p2_left, p2_right, p2_up, p2_fire` to `GameState` / `GameInput`. Duplicate the thrust/rotate/fire/collision blocks for `player2`. Draw the second ship in `COLOR_CYAN`. Share asteroids and score.  
**Optional**: `#define FEATURE_2P`.

### 8. Asteroid Belt Density Wave
**What**: Every 5 waves, spawn an extra-dense wave (8 large asteroids instead of 2).  
**How**: Add `int wave_number` to `GameState`; increment on level clear. In the level-clear block: `int count = (wave_number % 5 == 0) ? 8 : 2; for (i=0; i<count; i++) add_asteroid(...)` arranged evenly around the player at 150 px radius.  
**Optional**: `GameState` flag `int density_waves_enabled`.
