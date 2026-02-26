# Lesson 08 — Ship Physics

## By the end of this lesson you will have:
A white triangle you can steer: left/right arrows rotate it, up arrow fires a thrust engine that pushes it in the direction it's pointing, and the ship wraps around the screen edges rather than flying off into nothingness. The ship keeps its velocity between frames — letting go of thrust doesn't stop it immediately, exactly like a real spacecraft.

---

## Step 1 — Rotation with `ROTATE_SPEED * dt`

**Why:** We don't want rotation to be "1 degree per frame" — that would spin faster on a 120 Hz monitor than a 60 Hz one. Instead we express rotation as *radians per second* and multiply by `dt` (seconds elapsed this frame), making behaviour identical at any frame rate.

**JS analogy:** It's the same idea as `x += speed * deltaTime` in a browser `requestAnimationFrame` loop — you always scale by the elapsed time so the result is frame-rate-independent.

The constant in `asteroids.h`:

```c
#define ROTATE_SPEED  4.0f   /* radians/second */
```

4 rad/s means a full rotation (~6.28 rad) takes about 1.6 seconds. In `asteroids_update`:

```c
if (input->left.ended_down)
    state->player.angle -= ROTATE_SPEED * dt;
if (input->right.ended_down)
    state->player.angle += ROTATE_SPEED * dt;
```

`ended_down` is `1` while the key is held, `0` otherwise — just like checking `event.type === 'keydown'` state in JS, but without re-firing on every browser-generated repeat event.

**Worked example — dt = 0.016 s (one 60 Hz frame):**

```
Δangle = 4.0 × 0.016 = 0.064 rad  (~3.7°)
```

After 25 frames holding right: `25 × 0.064 = 1.6 rad` (~91°) — just over a quarter turn. Feels responsive, not twitchy.

**What you see:** The triangle rotates smoothly when you hold left/right. No physics yet — it just points in a new direction.

---

## Step 2 — Thrust Direction with `sinf` / `cosf`

**Why:** The ship has an `angle` in radians. We need to convert that angle into an (x, y) direction vector so thrust pushes the ship along its nose. `sin` and `cos` do exactly that conversion.

**The y-down screen coordinate system:** In a math textbook, y points up. On screen, pixel row 0 is at the *top* — y points *down*. This flips the sign of the vertical component:

```
In maths (y-up):    dx = sin(angle),  dy =  cos(angle)   → angle 0 points UP (+y)
On screen (y-down): dx = sin(angle),  dy = -cos(angle)   → angle 0 still points UP (−y row)
```

**Derivation with a worked example — angle = 0 (pointing straight up):**

```
sin(0) =  0.0   →  dx = 0.0       (no left/right movement)
cos(0) =  1.0   →  dy = -1.0      (negative y = moving up on screen ✓)
```

**Another example — angle = π/2 (pointing right, 90°):**

```
sin(π/2) = 1.0  →  dx =  1.0      (moving right ✓)
cos(π/2) = 0.0  →  dy = -0.0 = 0  (no vertical movement ✓)
```

The thrust code in `asteroids_update`:

```c
if (input->up.ended_down) {
    state->player.dx += sinf(state->player.angle) * THRUST_ACCEL * dt;
    state->player.dy += -cosf(state->player.angle) * THRUST_ACCEL * dt;
}
```

`THRUST_ACCEL` is 150 px/s². We're adding to `dx`/`dy`, not setting them — velocity *accumulates*.

**JS analogy:** Think of `dx`/`dy` as `velocityX`/`velocityY` in a Canvas physics loop. Each call to `requestAnimationFrame` adds a small push to the velocity; the position update below applies the total velocity.

**What you see:** Pressing up makes the ship accelerate in the direction it's currently facing. If you spin and thrust, the ship curves.

---

## Step 3 — Euler Integration: velocity → position

**Why:** We have acceleration (from thrust) and we need position. Euler integration applies each quantity one level at a time, each scaled by `dt`:

```
acceleration  →  velocity:   v += a * dt
velocity      →  position:   p += v * dt
```

This is called "explicit Euler" integration. It's not physically perfect (energy slowly drifts), but it's simple, fast, and perfectly fine for an arcade game.

**JS analogy:** This is exactly the two-line physics update in every JS game tutorial:
```js
velocityX += accelerationX * deltaTime;
x         += velocityX      * deltaTime;
```

**Worked example — one 60 Hz frame, dt = 0.016 s, ship pointing up (angle = 0):**

```
Before:  dx = 0, dy = 0,  x = 400, y = 300

Thrust pressed → accel = 150 px/s²
  dx += sin(0) * 150 * 0.016  =  0   * 2.4  =  0.0
  dy += -cos(0)* 150 * 0.016  = -1.0 * 2.4  = -2.4

Position update:
  x += 0.0   * 0.016  =  400.00
  y += -2.4  * 0.016  =  299.96      ← moved 0.038 px upward this frame
```

After holding thrust for 60 frames (1 second):
```
  dy ≈ -150 * 1.0 = -150 px/s
  y has moved upward roughly 75 pixels  (area under the velocity ramp)
```

The position update in `asteroids_update`:

```c
state->player.x += state->player.dx * dt;
state->player.y += state->player.dy * dt;
wrap_coordinates(&state->player.x, &state->player.y);
```

Notice `wrap_coordinates` is called immediately after — before we use the position for anything else this frame.

---

## Step 4 — `wrap_coordinates`: toroidal space

**Why:** We want the screen to "wrap around" like a torus — the right edge connects to the left edge, and the bottom connects to the top. This removes the need for any boundary collision logic and keeps all objects in bounds forever.

```c
static void wrap_coordinates(float *x, float *y) {
    if (*x <  0.0f)            *x += (float)SCREEN_W;
    if (*x >= (float)SCREEN_W) *x -= (float)SCREEN_W;
    if (*y <  0.0f)            *y += (float)SCREEN_H;
    if (*y >= (float)SCREEN_H) *y -= (float)SCREEN_H;
}
```

The function takes **pointers** (`float *x`) because it needs to modify the caller's variables. In JS you'd return a new object `{ x, y }`. In C you pass pointers so the function writes directly to the original memory.

**Four cases:**

| Condition | Action |
|-----------|--------|
| `x < 0`        | Add screen width — teleport to right side |
| `x >= SCREEN_W` | Subtract screen width — teleport to left side |
| `y < 0`        | Add screen height — teleport to bottom |
| `y >= SCREEN_H` | Subtract screen height — teleport to top |

**Worked example — ship exits left edge:**
```
x = -3.2  → x += 800 → x = 796.8   (reappears near right edge)
```

**What you see:** The ship flies off one edge and reappears on the opposite side instantly. This applies to asteroids and bullets too (added in later lessons).

---

## Build & Run

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11 -lX11 -lxkbcommon -lGL -lGLX -lm
./build/asteroids_x11
```

**What you should see:** A white triangle in the centre of a black screen. Hold **← →** to rotate. Hold **↑** to thrust — the ship accelerates in its facing direction, drifts when you release, and wraps across screen edges. It never stops (no friction) — you're in space.

---

## Key Concepts

- **Frame-rate independence** — multiply any per-second rate by `dt` so behaviour is identical at 30, 60, or 120 Hz.
- **y-down coordinates** — screen y increases downward, so "up" is `-cos(angle)`, not `+cos(angle)`.
- **Euler integration** — `v += a*dt` then `p += v*dt`; each step uses the value from the *start* of the frame.
- **Pointer parameters** — `float *x` lets a function modify the caller's variable; the call site passes `&player.x`.
- **Toroidal wrapping** — four `if` checks keep every floating-point position inside `[0, SCREEN_W)` × `[0, SCREEN_H)`.
- **No friction by design** — real arcade Asteroids has no drag; the ship glides forever, adding navigational challenge.

---

## Exercise

Add a drag (friction) constant — say `FRICTION = 0.98f` — and multiply `player.dx` and `player.dy` by it each frame:

```c
state->player.dx *= 0.98f;
state->player.dy *= 0.98f;
```

Play with the game. Does it feel better or worse? Why does a value of exactly `1.0` reproduce the original behaviour?
