# Lesson 10 — Platform Riding: River Logs Carry the Frog

## By the end of this lesson you will have:

River rows (0–3) that carry the frog laterally as the logs move, and an off-screen kill that triggers when the frog is swept out of bounds — matching the classic frogger mechanic.

---

## The Two Zones

```
Row 0: home row (speed=0, fixed)          ← frog wins here
Rows 1–3: river (speed=2,3,4, scrolling)  ← frog rides logs or drowns
Row 4: median strip (speed=0, safe)
Rows 5–9: road (speed=2,-3,4,-2,3)        ← frog dies if hit by car
```

**Road zone** — frog never moves laterally. Cars move through the scene; frog stays where it hops.
**River zone** — logs move; frog must be on one to cross. If on a log, frog is carried with it.

---

## Step 1 — River carrying in `frogger_tick`

```c
/* After processing hop input, before collision check */
int fy_int = (int)(state->frog_y);

/* River rows 1-3: carrying */
if (fy_int >= 1 && fy_int <= 3) {
    state->frog_x -= lane_speeds[fy_int] * delta_time;
}
```

**Why subtract?**
`lane_speeds[1] = 2.0f` (logs drift left → negative `frog_x` change → frog moves left). Wait — is the sign convention forward or backward?

```
tile_start = (int)(time * speed) % PATTERN_LEN;
```
Positive speed → `tile_start` increases over time → tiles shift right-to-left visually (pattern scans left). So a log moving left has `speed > 0` in our convention. The frog sits on it and moves left → `frog_x -= speed * dt`.

Negative speed → tiles shift left-to-right → log moves right → `frog_x += |speed| * dt` → which is `frog_x -= speed * dt` since speed is negative. Same formula works for both.

**The unified rule:**
```c
state->frog_x -= lane_speeds[fy_int] * delta_time;
```
Works for all signs of speed. Positive speed: frog drifts left. Negative speed: frog drifts right.

---

## Step 2 — Off-screen kill

```c
if (fy_int >= 1 && fy_int <= 3) {
    float frog_px_x = state->frog_x * TILE_PX;
    if (frog_px_x < 0.0f || frog_px_x + TILE_PX > SCREEN_PX_W) {
        state->dead = 1;
        state->dead_timer = DEAD_ANIM_TIME;
    }
}
```

`frog_x` is in tile-space (float). `frog_px_x = frog_x * TILE_PX` is the pixel X of the frog's left edge. `frog_px_x + TILE_PX` is the right edge.

- Off-screen left: `frog_px_x < 0` — right edge of log swept past left wall
- Off-screen right: `frog_px_x + TILE_PX > SCREEN_PX_W` — left edge of log swept past right wall

**Row 0 (speed=0):**
```c
lane_speeds[0] = 0.0f;
```
Home row has speed 0 — frog is never carried, and we check `fy_int >= 1` so the off-screen kill doesn't trigger on the home row either.

---

## Step 3 — Interaction with danger buffer

Carrying and collision are separate concerns:

1. `frog_x -= lane_speeds[fy] * dt` — carrying (every tick on river rows)
2. `danger[]` check — determines if the frog's current position is on a log (`safe=0`) or water (`danger=1`)

The danger check uses the frog's already-updated position after carrying. This order is important:
```c
/* tick order */
frogger_process_hop(&state, &input);   /* hop input */
frogger_carry_river(&state, dt);       /* river carrying */
frogger_check_off_screen(&state);      /* off-screen kill */
frogger_build_danger(&state);          /* rebuild danger map */
frogger_check_collision(&state);       /* danger-based kill */
```

If we checked collision before carrying, the frog could survive a frame where it's in mid-air between tiles (log has moved out but danger map hasn't been rebuilt yet).

---

## Step 4 — Visualizing the carry

```
t=0.0:  log at columns 5-8, frog on it at column 6
t=0.5:  log has moved left 1 tile (speed=2, dt=0.5 → dx=1)
         frog has also moved left 1 tile → still on the log
t=1.0:  frog has moved off-screen left → dead
```

Without carrying:
```
t=0.0:  frog on log (column 6), danger[6]=0 (safe)
t=0.5:  log moved to columns 4-7, frog still at column 6
         danger[6]=0 still safe (log extended to column 7)
t=0.75: log at columns 3-6, frog still at column 6
         danger[6]=0 barely safe (log ends at column 6)
t=1.0:  log at columns 2-5, frog at column 6
         danger[6]=1 DEAD — frog "fell through" the log
```

With carrying the frog follows the log automatically, making it through.

---

## Key Concepts

- `frog_x -= lane_speeds[fy] * dt` — works for both directions due to sign convention
- River rows 1–3 carry frog; row 0 (speed=0) and road rows (4–9) do not
- Off-screen kill: `frog_px_x < 0 || frog_px_x + TILE_PX > SCREEN_PX_W`
- Carrying runs before danger-map rebuild → position is always current when collision is checked
- Without carrying, frog "falls off" logs even while standing still

---

## Exercise

What happens if you apply carrying to road rows (5–9)? Change the condition to `fy_int >= 1 && fy_int <= 9` and rebuild. Run the game — can you explain the behavior? (Hint: road tiles are safe for the frog to stand on, but cars that hit it use the danger buffer, not the carrying logic.)
