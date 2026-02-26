# Lesson 8 — Platform Riding (River Logs Carry the Frog)

**By the end of this lesson you will have:**
The frog carried sideways by the current when it's on a river lane (rows 1-3).
Stand on a log — the frog drifts. Step onto water — the frog eventually drifts
into it and resets. The river is alive.

---

## The mechanic

In the original Frogger, when the frog is on a log, the log carries it. If the
frog drifts off the edge of the log (into water), it dies.

Our implementation uses a simpler, elegant trick: **if the frog is in rows 0-3,
it moves horizontally at the lane's scroll speed**. No need to check "is the frog
on a log" — the danger buffer (Lesson 9) will kill the frog if it's on water.
The frog just always moves with the river, and dies if it ends up in water.

---

## Step 1 — The platform-riding code in `frogger_tick`

```c
/* River: logs carry frog
   Rows 0-3 are the river zone. Row 0 has speed=0 (home row).
   The frog is pushed at the lane's scroll speed.                          */
int fy_int = (int)state->frog_y;
if (fy_int >= 0 && fy_int <= 3) {
    /* lane_speeds[] is negative for leftward lanes.
       We subtract it (same direction as lane movement).                   */
    state->frog_x -= lane_speeds[fy_int] * dt;
}
```

Wait — why `frog_x -= lane_speed * dt` instead of `+=`?

Think about it: lane 1 has speed `-3.0` (tiles scroll left = rightward movement
for items ON the lane). If the lane moves right on screen, the frog should also
move right: `frog_x += 3.0 * dt`. But `lane_speeds[1] = -3.0`, so:
`frog_x -= (-3.0) * dt = frog_x + 3.0 * dt`. ✓

**Worked example (lane 1, dt=0.016s):**
```
lane_speeds[1] = -3.0
frog_x -= -3.0 × 0.016 = frog_x + 0.048
```
Frog moves right 0.048 tiles per frame → 3 tiles/second. Same speed as the
lane scrolling — so the frog appears stationary relative to the log. ✓

---

## Step 2 — Why frog_x is a float

If frog_x were an integer, `frog_x += 0.048` would truncate to zero. The frog
would never move with the current.

Because it's a float:
- Each frame adds a tiny fraction: 0.048 tiles
- After ~21 frames: frog_x has moved ~1 tile
- The render function does `(int)(state->frog_x * (float)TILE_PX)` which gives
  pixel-precision — the frog glides smoothly alongside the log.

The frog accumulates sub-tile drift in frog_x, and that fractional position
flows through to the screen with 1-pixel granularity.

**New C concept — `(int)` cast (explicit truncation):**
`(int)(8.7f)` = 8. Truncates toward zero, dropping the decimal.
Not rounding — just throwing away the fraction.
JS analogy: `Math.trunc(8.7)` = 8.

---

## Step 3 — What happens at the edges

If the frog drifts off screen, the pixel-based bounds guard in `frogger_tick` kills it:

```c
int frog_px_x = (int)(state->frog_x * (float)TILE_PX);
int frog_px_y = (int)(state->frog_y * (float)TILE_PX);
if (frog_px_x < 0 || frog_px_x + TILE_PX > SCREEN_PX_W || ...) {
    state->dead = 1;
    ...
}
```

This models the frog falling off the edge of the screen — classic Frogger.

---

## Step 4 — Observe the interaction

Run the game:
```sh
./build_x11.sh && ./frogger_x11
```

1. Press UP four times — the frog is in row 5 (road). Nothing special yet.
2. Press UP four more times — frog reaches row 1 (river).
3. Don't press anything. Watch the frog drift sideways.
4. If frog drifts into water AND collision is on (Lesson 9 will add that) — it dies.

Right now (without the danger buffer from Lesson 9), the frog just floats freely.
You'll add the death condition next lesson.

---

## Step 5 — DOD note: why frog position is in GameState

In OOP you might have a `Frog` object that knows its own position and velocity.
Here, `frog_x` and `frog_y` are plain floats inside `GameState`.

When `frogger_tick` runs, it reads `lane_speeds[fy_int]` and `state->frog_x` — both
are simple memory reads. No virtual calls, no pointer chasing, no object overhead.

```
Memory access pattern (DOD):
  lane_speeds[1] = read 4 bytes from speeds array
  state->frog_x  = read 4 bytes from start of GameState
  → 2 cache misses max, both fast

Memory access pattern (OOP equivalent):
  frog->update()
  → read frog pointer
  → call virtual function (cache miss on vtable)
  → inside: this->lane->getSpeed()
  → read lane pointer, read speed
  → potentially 4+ cache misses
```

For a simple game this doesn't matter. But the habit pays off.

---

## Build & Run

```sh
./build_x11.sh && ./frogger_x11
```

**What you should see:**
Navigate the frog to row 1, 2, or 3. Release the keys and watch it drift.
Row 1: drifts right (speed -3 → frog_x increases)
Row 2: drifts left (speed +3 → frog_x decreases)

---

## Mental Model

Platform riding = "the frog has the same velocity as the lane it's on."

```
frog_x velocity = -lane_speed  (same direction as tiles scrolling)

Lane 1 scrolls at -3 tiles/sec (leftward tile movement):
  Tiles appear to move right on screen.
  Frog must also move right: frog_x += 3 × dt
  Which equals: frog_x -= lane_speeds[1] × dt = frog_x -= (-3) × dt ✓
```

The negative sign is because our scroll convention and the frog convention
are mirrored — scrolling "moves content left" while the frog "moves right."

---

## Exercise

Change the frog's start position to row 2 (`frog_y = 2.0f` in `frogger_init`).
Lane 2 has speed `+3.0` (rightward tile movement, frog drifts left).

Predict: how many seconds before the frog drifts from tile 8 off the left edge?
```
tiles to drift = 8 tiles (from x=8 to x=0)
speed = 3 tiles/sec → time = 8/3 ≈ 2.7 seconds
```

Time it with a watch. Does it match?
Restore `frog_y = 9.0f` when done.
