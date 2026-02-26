# Lesson 09 — Special Mechanics: Gravity, Cyclic, Teleporters

**By the end of this lesson you will have:**
Three distinct mechanics working:
(a) Press **G** → gravity flips. Grains that were falling down now fall up.
(b) On a "cyclic" level, grains that exit the bottom of the screen reappear at the top.
(c) A grain that touches the blue portal disappears and reappears at the orange portal.

---

## Step 1 — `gravity_sign`: flip with a single field

The physics update multiplies `GRAVITY` by `gravity_sign`:

```c
// inside update_grains() — game.c
p->vy[i] += GRAVITY * state->gravity_sign * dt;
```

`gravity_sign` is either `+1` (down) or `-1` (up). That is the entire flip mechanism.

Concrete numbers — first frame, dt = 0.016 s, gravity_sign = +1:
```
vy += 500.0 * (+1) * 0.016
vy += 8.0
```
Grain accelerates downward.

Same frame, gravity_sign = -1:
```
vy += 500.0 * (-1) * 0.016
vy += -8.0
```
Grain accelerates upward.

No separate "flipped physics" code path. One field, one multiply.

Toggling:
```c
// inside update_playing(), when gravity button is pressed
if (BUTTON_PRESSED(input->gravity) && state->level.has_gravity_switch) {
    state->gravity_sign = -state->gravity_sign; // +1 → -1 → +1 → ...
}
```

---

## Step 2 — Keyboard binding: G key in `main_x11.c`

The platform backend translates OS key events into `GameInput` button state. Open `src/main_x11.c` and look at `platform_get_input()`:

```c
// main_x11.c — already written
case KeyPress: {
    KeySym ks = XLookupKeysym(&ev.xkey, 0);
    if (ks == XK_Escape) UPDATE_BUTTON(input->escape,  1);
    if (ks == XK_r || ks == XK_R) UPDATE_BUTTON(input->reset,   1);
    if (ks == XK_g || ks == XK_G) UPDATE_BUTTON(input->gravity, 1);
    break;
}
case KeyRelease: {
    KeySym ks = XLookupKeysym(&ev.xkey, 0);
    if (ks == XK_Escape) UPDATE_BUTTON(input->escape,  0);
    if (ks == XK_r || ks == XK_R) UPDATE_BUTTON(input->reset,   0);
    if (ks == XK_g || ks == XK_G) UPDATE_BUTTON(input->gravity, 0);
    break;
}
```

`UPDATE_BUTTON(btn, is_down)` increments `half_transition_count` only when the state actually changes (prevents repeated KeyPress auto-repeat events from counting as multiple presses).

The game never sees `XK_g` — it only sees `GameInput.gravity`. That is the whole point of the platform abstraction layer.

---

## Step 3 — `has_gravity_switch`: level-controlled feature flag

Not every level has a gravity button. The flag in `LevelDef` controls it:

```c
// in game.h
int has_gravity_switch; // 1 = show and enable gravity flip button
```

Level 13 (`g_levels[12]`) sets it:
```c
[12] = {
    .emitter_count     = 1,
    .emitters          = { EMITTER(320, 440, 80) }, // emitter at BOTTOM
    .cup_count         = 1,
    .cups              = { CUP(278, 30, 85, 80, GRAIN_WHITE, 150) }, // cup at TOP
    .has_gravity_switch = 1,
},
```

The emitter is at y=440 (near bottom). The cup is at y=30 (near top). Grains spawn moving upward only after the player flips gravity.

In `render_playing()` the button is only drawn when the flag is set:
```c
if (state->level.has_gravity_switch) {
    // draw the gravity toggle button
    uint32_t btn_col = state->gravity_hover ? COLOR_BTN_HOVER : COLOR_BTN_NORMAL;
    draw_rect(bb, 560, 10, 70, 28, btn_col);
    draw_rect_outline(bb, 560, 10, 70, 28, COLOR_BTN_BORDER);
    if (state->gravity_sign == 1)
        draw_text(bb, 564, 17, "GRAV v", COLOR_UI_TEXT);
    else
        draw_text(bb, 564, 17, "GRAV ^", COLOR_UI_TEXT);
}
```

The caret `^` and `v` give an immediate visual signal about which direction is "down".

---

## Step 4 — Cyclic screen wrap: `iy >= H → ny = 1`

"Cyclic" means the bottom edge connects to the top edge — like Pac-Man's screen wrapping. A grain that falls off the bottom reappears at the top.

Inside `update_grains()`, after moving the grain by one sub-step:

```c
// game.c — inside the sub-step loop
int iy = (int)p->y[i];

if (iy >= CANVAS_H) {
    if (state->level.is_cyclic) {
        p->y[i] = 1.0f;  // teleport to top, keep vx/vy unchanged
        continue;         // skip deactivation, keep sub-stepping
    } else {
        p->active[i] = 0; // off the bottom — deactivate
        break;
    }
}
if (iy < 0) {
    if (state->level.is_cyclic) {
        p->y[i] = (float)(CANVAS_H - 2); // wrap to bottom
        continue;
    } else {
        p->active[i] = 0;
        break;
    }
}
```

`continue` restarts the sub-step loop with the new position. The grain is not deactivated; it just jumps rows.

Why `ny = 1` not `ny = 0`? Row 0 is the very top edge — if the emitter is there, newly spawned grains would immediately re-enter the wrap zone and double-count. Row 1 is safe.

---

## Step 5 — Teleporter circle test: no `sqrt()` needed

A `Teleporter` has two portals A and B:

```c
typedef struct {
    int ax, ay; // portal A center
    int bx, by; // portal B center
    int radius; // trigger radius in pixels
} Teleporter;
```

To check if a grain at `(gx, gy)` is inside portal A:

```
dist² = (gx - ax)² + (gy - ay)²
if dist² <= radius²  → inside
```

Concrete example — portal A at (320, 150), radius 18, grain at (310, 158):
```
dx = 310 - 320 = -10
dy = 158 - 150 =   8
dist² = (-10)² + 8² = 100 + 64 = 164
radius² = 18² = 324
164 <= 324  → inside portal A ✓
```

We never call `sqrt()`. Comparing squares is identical to comparing distances, and integer multiplication is much cheaper than square root.

In `update_grains()`:

```c
for (int t = 0; t < lv->teleporter_count; t++) {
    Teleporter *tp = &lv->teleporters[t];
    if (p->tpcd[i] > 0) { p->tpcd[i]--; continue; }

    int dax = gx - tp->ax, day = gy - tp->ay;
    int dbx = gx - tp->bx, dby = gy - tp->by;
    int r2  = tp->radius * tp->radius;

    if (dax*dax + day*day <= r2) {
        p->x[i] = (float)tp->bx;
        p->y[i] = (float)tp->by;
        p->tpcd[i] = 8; // 8-frame cooldown
    } else if (dbx*dbx + dby*dby <= r2) {
        p->x[i] = (float)tp->ax;
        p->y[i] = (float)tp->ay;
        p->tpcd[i] = 8;
    }
}
```

Portal A teleports to B. Portal B teleports to A. Bidirectional with a single struct.

---

## Step 6 — `tpcd`: teleport cooldown prevents infinite loops

Without cooldown, what happens on frame N:
1. Grain enters portal A at (320, 150).
2. Game moves grain to portal B at (100, 350).
3. *Same frame*, grain is still inside A's radius (it was just placed at B, but the loop continues). → teleports back to A. → teleports to B again. → infinite loop.

`tpcd` (teleport cooldown) is a per-grain countdown in frames:

```c
uint8_t tpcd[MAX_GRAINS]; // 0 = ready, >0 = cooling down
```

After teleporting, set `tpcd[i] = 8`. Each frame decrement it. Only teleport when `tpcd[i] == 0`. The grain spends 8 frames immune from re-entry — long enough to physically leave the portal radius.

```
Frame 0:  grain enters A → teleport to B, tpcd=8
Frame 1:  tpcd=7, no teleport check
...
Frame 8:  tpcd=0, normal checks resume
```

8 frames at 60 fps = 133 ms. In that time a grain moving at ~200 px/s travels ~27 px, which is larger than the 18 px radius. So it is guaranteed to have left the portal.

---

## Step 7 — Level 17 (cyclic) and Level 21 (teleporter) definitions

```c
// levels.c — already written
[16] = {  // Level 17 — cyclic
    .index         = 16,
    .emitter_count = 1,
    .emitters      = { EMITTER(320, 20, 80) },
    .cup_count     = 1,
    .cups          = { CUP(490, 50, 85, 80, GRAIN_WHITE, 150) },
    .is_cyclic     = 1,
},

[20] = {  // Level 21 — teleporter
    .index              = 20,
    .emitter_count      = 1,
    .emitters           = { EMITTER(320, 20, 80) },
    .cup_count          = 1,
    .cups               = { CUP(60, 370, 85, 100, GRAIN_WHITE, 150) },
    .teleporter_count   = 1,
    .teleporters        = { TELE(320, 150, 100, 350, 18) },
},
```

In Level 21: the emitter is at (320, 20). The cup is at the left. A grain falling straight down hits portal A at (320, 150) and pops out at portal B (100, 350) — just above the cup.

---

## Step 8 — `render_teleporters()`: pulsing circles

Portals pulse in size using `phase_timer` as a sine input:

```c
// game.c — render_playing() calls this
static void render_teleporters(const GameState *state, GameBackbuffer *bb) {
    for (int t = 0; t < state->level.teleporter_count; t++) {
        Teleporter *tp = &state->level.teleporters[t];

        // pulse: radius oscillates ±3 px at 2 Hz
        float pulse = (float)tp->radius + 3.0f * sinf(state->phase_timer * 6.28f * 2.0f);
        int   r     = (int)pulse;

        draw_circle       (bb, tp->ax, tp->ay, r,   COLOR_PORTAL_A);
        draw_circle_outline(bb, tp->ax, tp->ay, r+2, COLOR_PORTAL_A);

        draw_circle       (bb, tp->bx, tp->by, r,   COLOR_PORTAL_B);
        draw_circle_outline(bb, tp->bx, tp->by, r+2, COLOR_PORTAL_B);
    }
}
```

`sinf(phase_timer * 6.28f * 2.0f)`:
- `phase_timer` counts seconds since the level started.
- Multiply by `6.28 * 2` = 12.56 rad/s → 2 complete oscillations per second.
- `sinf` returns -1 to +1 → radius varies from `tp->radius - 3` to `tp->radius + 3`.

The fill circle and the outline circle draw at slightly different radii for a glowing halo effect.

---

## Build & Run

```bash
clang -Wall -Wextra -O2 -o sugar \
    src/main_x11.c src/game.c src/levels.c -lX11 -lm
./sugar
```

**What you should see:**
- **Level 13**: emitter at the bottom, cup at top. Press G — grains fly upward and fill the cup.
- **Level 17**: grains fall off the bottom and reappear at the top in a continuous loop. Draw a ramp to steer them right.
- **Level 21**: grains fall into the blue circle and exit the orange circle near the left cup. Two glowing rings pulse on screen.

---

## Key Concepts

- `gravity_sign` (+1 / -1): multiply into `GRAVITY` — one field flips all physics with no extra branches.
- `UPDATE_BUTTON()` in `main_x11.c`: translates OS key events into platform-independent `GameInput`. The game only sees button state, never key codes.
- `has_gravity_switch`: a feature flag in `LevelDef`. The game checks it before rendering the G button and before accepting G input.
- **Cyclic wrap**: `if (iy >= CANVAS_H && is_cyclic) p->y[i] = 1.0f; continue;` — one `if` in the sub-step loop handles infinite recycling.
- **Circle test without sqrt**: compare `dx² + dy²` to `radius²`. Integer multiply, no floating-point root.
- `tpcd` (teleport cooldown): a per-grain frame counter. Prevents A→B→A→B re-entry by making the grain immune to teleportation for 8 frames.
- `render_teleporters()` pulse: `sinf(timer * 2π * frequency)` drives size oscillation — same formula as any animation easing.

---

## Exercise

Create a level with **two teleporters** where portal A₁ leads to B₁ and portal A₂ leads to B₂, forming an S-shaped route. Place the emitter at the top-center, portal A₁ at (320, 150), B₁ at (500, 300), A₂ at (500, 300), B₂ at (320, 400), and the cup at (278, 370). Add it as `g_levels[24]` (level 25). Does the grain travel the full chain? Why or why not? (Hint: what happens when two portals share the same point?)
