# Sugar, Sugar — Build Issues Post-Mortem

A reference for course instructors and students. Each section describes a bug
that appeared in the first-draft implementation, its root cause, how it was
diagnosed, and how it was fixed.

---

## Issue 1 — All Text Was Horizontally Mirrored

### Symptom
Every letter with any left/right asymmetry (N, R, F, P, 7, …) appeared as its
mirror image. "RESET" read as if the letters were reflected in a vertical axis.

### Root Cause
The 8×8 bitmap font (`font.h`) stores each row as a `uint8_t` where
**bit 0 is the left-most pixel** (BIT0=left convention). The renderer used:

```c
if (bits & (0x80 >> col))   /* BUG: tests bit 7 first */
```

`0x80 >> 0` = `0x80` (bit 7). When `col == 0`, the code asks "is the
rightmost pixel set?" and draws it on the left. The font data and the renderer
disagree about which end of the byte is left — every asymmetric glyph comes
out backwards.

### Diagnosis
Trace character 'N' (byte 0 = `0x63 = 01100011`):

| col | BIT tested by `0x80>>col` | pixel drawn | expected |
|-----|--------------------------|-------------|---------|
| 0   | bit 7 → 0                | off         | ON (left stem) |
| 5   | bit 2 → 0                | off         | ON (right diagonal) |
| 6   | bit 1 → 1                | **on**      | ON (right stem) |

The diagonal runs top-right → bottom-left instead of top-left → bottom-right.

### Fix
Change the mask to test bit 0 first:

```c
/* CORRECT: bit 0 = left-most pixel, matches font.h BIT0=left convention */
if (bits & (1 << col))
```

Applied in both `draw_char()` and the new `draw_text_scaled()` helper.

### Lesson Learned
**Always document which end of the byte is pixel 0** before writing a
single line of font renderer code. Put a comment in the font header file:
`/* Bit 0 = left pixel, Bit 7 = right pixel (BIT0-left convention) */`.
If you pick up a font from the internet, test it with the letter 'N' first —
its diagonal is the fastest visual sanity check.

---

## Issue 2 — Background Was Warm Cream Instead of Pure White

### Symptom
The game canvas background was a yellowish-cream color. The original Sugar
Sugar game uses a plain white canvas.

### Root Cause
`COLOR_BG` was initialized during early prototyping with a "warm paper" tone:

```c
#define COLOR_BG  GAME_RGB(245, 242, 235)  /* warm cream — wrong */
```

### Fix
```c
#define COLOR_BG  GAME_RGB(255, 255, 255)  /* pure white */
```

### Lesson Learned
Color constants should be set from reference screenshots of the original game,
not from aesthetic guesses made during scaffolding. Capture the exact pixel
value with a color picker tool before writing any `#define COLOR_*` line.

---

## Issue 3 — Button Hit-Box Was Offset From the Drawn Button

### Symptom
Hovering over a level-select button did nothing. Moving the mouse ~60 pixels
**below** the button triggered the hover highlight. Clicking worked only in
that invisible zone below the button.

### Root Cause
The render function and the update (input) function were written independently
with different hard-coded `grid_y` values:

```c
/* in render_title() — drawn here */
int grid_y = 100;

/* in update_title() — hover/click tested here */
int grid_y = 160;
```

The 60-pixel difference meant every button was drawn at its correct position
but clicks were registered 60 pixels lower, in empty space.

### Diagnosis
Draw bounding boxes for each button in the renderer (use a contrasting outline)
and also log `my` vs the tested range in `update_title`. The gap is immediately
obvious.

### Fix
Extracted a single set of layout `#define` macros placed **before** both
functions:

```c
#define TITLE_BTN_W   70
#define TITLE_BTN_H   40
#define TITLE_BTN_GAP  8
#define TITLE_COLS     6
#define TITLE_GRID_X  ((CANVAS_W - (TITLE_COLS*(TITLE_BTN_W+TITLE_BTN_GAP) - TITLE_BTN_GAP)) / 2)
#define TITLE_GRID_Y  160
```

Both `render_title()` and `update_title()` now reference the same constants.
It is **physically impossible** for them to drift apart.

Similarly, the in-game HUD buttons gained their own shared set:

```c
#define UI_BTN_Y    (CANVAS_H - 38)
#define UI_BTN_H    28
#define UI_RESET_X  10
#define UI_RESET_W  70
#define UI_GRAV_X   88
#define UI_GRAV_W   100
```

### Lesson Learned
**Never duplicate layout numbers between a renderer and an input handler.**
The moment you type the same pixel value in two places you create a latent
mismatch bug. The cure is a single named constant — a `#define`, `static const
int`, or `enum` — used in both places. This is the UI equivalent of DRY.

---

## Issue 4 — Reset and Gravity Buttons Had No Mouse Interaction

### Symptom
The Reset and Gravity buttons were drawn on screen, but clicking them did
nothing. Only the keyboard shortcuts (R and G) worked.

### Root Cause
`update_playing()` only handled keyboard input. The `reset_hover` and
`gravity_hover` fields in `GameState` were never updated, so buttons always
rendered in their non-hover style and mouse clicks were silently ignored.

### Fix
At the top of `update_playing()` (and `update_freeplay()`):

```c
int mx = input->mouse.x, my = input->mouse.y;

state->reset_hover =
    (mx >= UI_RESET_X && mx < UI_RESET_X + UI_RESET_W &&
     my >= UI_BTN_Y   && my < UI_BTN_Y   + UI_BTN_H);

state->gravity_hover = state->level.has_gravity_switch &&
    (mx >= UI_GRAV_X && mx < UI_GRAV_X + UI_GRAV_W &&
     my >= UI_BTN_Y  && my < UI_BTN_Y  + UI_BTN_H);

if (BUTTON_PRESSED(input->mouse.left)) {
    if (state->reset_hover) { level_load(state, state->current_level); return; }
    if (state->gravity_hover) { state->gravity_sign = -state->gravity_sign; }
}
```

### Lesson Learned
When you render an interactive element, immediately ask: "where does the
click handler live?" If the answer is "nowhere yet", add a TODO comment
before moving on. Rendering a button without a click handler is a visual lie
that leads to confusing UX.

---

## Issue 5 — Player Could Draw Lines on Top of UI Buttons

### Symptom
Clicking the Reset button would also leave a line drawn on the canvas, because
the brush-line code ran unconditionally whenever the left mouse button was held.

### Root Cause
```c
/* BUG: draws even when mouse is over the UI strip */
if (input->mouse.left.ended_down) {
    draw_brush_line(&state->lines, ...);
}
```

### Fix
Guard with the vertical boundary of the UI strip:

```c
int over_ui = (my >= UI_BTN_Y - 4);
if (input->mouse.left.ended_down && !over_ui) {
    draw_brush_line(&state->lines, ...);
}
```

### Lesson Learned
Split your canvas into logical zones at design time (playfield vs. HUD) and
enforce them explicitly in your input handler. If a region has interactive UI
elements, it should be off-limits for all other mouse actions.

---

## Issue 6 — X11 Byte Order Not Explicitly Set

### Symptom
On most x86-64 Linux machines the game displayed with correct colors, but on
big-endian hosts or X servers with an unusual TrueColor visual the red and blue
channels could be swapped.

### Root Cause
`XCreateImage` does not assume a byte order. On little-endian systems it
defaults to `LSBFirst`, but this is not guaranteed by the X11 specification.
Our pixel format is `0xAARRGGBB` stored in a `uint32_t`. On a little-endian
machine this is bytes `[B, G, R, A]` which happens to be exactly what a
standard TrueColor visual expects — but only by lucky coincidence.

### Fix
```c
g_ximage = XCreateImage( /* ... */ );
/* Make byte order explicit — never rely on an implicit default. */
if (g_ximage) g_ximage->byte_order = LSBFirst;
```

### Lesson Learned
Platform layer code should be explicit about every configurable parameter,
even when the default "happens to work" on your development machine.
A one-line explicit assignment costs nothing and prevents a confusing bug
report from someone on different hardware.

---

## Issue 7 — Text Was Too Small to Read Comfortably

### Symptom
The 8×8 pixel font was legible at close range but extremely small on a 640×480
canvas scaled to a modern high-DPI display. The level-select title ("SUGAR,
SUGAR") was barely noticeable.

### Root Cause
Only a single-size `draw_text()` function existed. Font scaling was not
planned during the initial scaffold.

### Fix
Added `draw_text_scaled(bb, x, y, str, color, scale)` which renders each
glyph pixel as an `N×N` filled block:

```c
static void draw_text_scaled(GameBackbuffer *bb, int x, int y,
                              const char *str, uint32_t color, int scale) {
    int cx = x;
    for (const char *p = str; *p; p++) {
        if (*p == '\n') { cx = x; y += (FONT_H + 1) * scale; continue; }
        for (int row = 0; row < FONT_H; row++) {
            uint8_t bits = FONT_DATA[(*p - 0x20) * FONT_H + row];
            for (int col = 0; col < FONT_W; col++) {
                if (bits & (1 << col))
                    draw_rect(bb, cx + col*scale, y + row*scale, scale, scale, color);
            }
        }
        cx += (FONT_W + 1) * scale;
    }
}
```

Usage:
- Title screen heading: 3× scale
- Level-complete overlay: 2× scale
- Level indicator in HUD: 2× scale

### Lesson Learned
Design for multiple text sizes from day one. A single `scale` integer parameter
is all you need for a bitmap font. Waiting until the UI looks "done" to add
scaling means retrofitting every call site.

---

## Issue 8 — Background Was White Instead of Sky Blue (Round 2)

### Symptom
After the first fix set `COLOR_BG` to pure white, the user confirmed that the
original Sugar Sugar game actually uses a soft **sky blue** background, not
white. The canvas felt clinical and wrong.

### Root Cause
The first fix was made from assumption ("pure white matches the original") without
verifying against actual screenshots. Multiple reference sources
(MobyGames, Coolmath Games, Kongregate Wiki, Newgrounds) all confirm a light
sky-blue canvas — a soft, desaturated sky color often described as "powder blue".

### Fix
```c
/* game.h */
#define COLOR_BG  GAME_RGB(135, 195, 225)  /* soft sky blue — verified from screenshots */
```

### Lesson Learned
**Never guess visual constants.** Before writing any color `#define`, open the
reference game (or its screenshots) and sample the exact pixel with a color
picker. A 30-second verification prevents a multi-hour "the background still
looks wrong" debugging cycle.

---

## Issue 9 — Grain-Line Collision: Grains Did Not Follow Slopes Correctly

### Symptom
Sugar grains fell through or jittered on player-drawn lines. Grains on a slope
did not slide down it smoothly — they either stopped dead or drifted with
obviously wrong horizontal velocity. Grains on curves and loops behaved
erratically.

### Root Cause (three sub-bugs in `update_grains`)

#### Sub-bug A — Artificial fixed slide velocity (`vx = ±80`)
After a grain slid to an adjacent pixel, the code set:
```c
p->vx[i] = (float)dirs[d] * 80.0f;   /* always 80 px/s regardless of fall speed */
```
This constant is independent of how fast the grain was falling. A grain that
has just fallen at 500 px/s (terminal velocity) would suddenly slide at 80 px/s
— six times too slow. The result: grains appeared to "stick" to shallow slopes
instead of flowing smoothly.

#### Sub-bug B — Sub-stepping continued after a collision
After successfully resolving a collision the code did **not** `break` out of the
sub-step loop. The remaining sub-steps ran with the *old* `sdx`/`sdy` deltas
(computed before the collision changed the velocity). The grain would push
further into the solid, triggering more collisions in the same frame and
producing jitter.

#### Sub-bug C — Collision check only used the line bitmap, ignoring other grains
```c
if (lb->pixels[iy * W + ix])   /* only checks drawn lines */
```
Two grains could occupy the same pixel. When many grains pile up in a cup they
all "stack" in the same pixel, and the "full cup" condition is satisfied by a
single grain at the top of the pixel stack. Visually the cup appeared to fill
instantly.

### Fix

**A — Proportional slide velocity**
Transfer the grain's current |vy| into vx when it slides, so faster-falling
grains slide faster:
```c
float spd = fabsf(p->vy[i]);
if (spd < 60.0f) spd = 60.0f;   /* minimum so grain doesn't stall on gentle slopes */
if (spd > MAX_VX) spd = MAX_VX;
p->vx[i] = (float)d * spd;
p->vy[i] = 0.0f;                 /* grain is now "on" the surface */
```

**B — Always break after any collision**
```c
break;  /* single break at bottom of the sub-step loop, after all collision logic */
```
The new velocity takes effect cleanly from the next frame.

**C — Grain-occupancy bitmap for grain-grain collision**
Added a function-local `static uint8_t s_occ[CANVAS_W * CANVAS_H]` (307 KB in
BSS, not on the stack). Rebuilt every frame from active grain positions.
Each grain unmasks its *own* pixel before its update, then re-marks it
at the end:
```c
static uint8_t s_occ[CANVAS_W * CANVAS_H];
memset(s_occ, 0, sizeof(s_occ));
/* ... mark all active grains ... */

for each grain i:
    s_occ[old_y * W + old_x] = 0;   /* unmark self */
    /* ... move, resolve collision using IS_SOLID(x,y)
           which checks both lb->pixels AND s_occ ... */
    s_occ[new_y * W + new_x] = 1;   /* re-mark at new position */
```
This means:
- Grains processed earlier in the loop "take" pixels, blocking later ones.
- The processing order (array index) acts as a simple priority system.
- Grains properly pile up on flat surfaces and on each other.

### IS_SOLID macro
```c
#define IS_SOLID(xx, yy) \
    ((xx) < 0 || (xx) >= W || \
     lb->pixels[(yy)*W+(xx)] || s_occ[(yy)*W+(xx)])
```
Out-of-bounds X is treated as a wall (solid). Out-of-bounds Y is handled by
the separate ceiling/floor logic above the collision block.

### How the "follow the line" rule works correctly now

| Scenario | Behavior |
|----------|---------|
| Grain falls onto **horizontal line** | `iy > oy`, tries `(ox±1, iy)` — both blocked (line extends both sides) → grain settles |
| Grain falls onto **45° slope** | `iy > oy`, tries `(ox+1, iy)` → free → slides with `vx = |vy|` |
| Grain hits **vertical wall** | `iy == oy` (pure horizontal collision) → `vx = 0`, continues falling |
| Grain falls onto **curve** | Curve pixels in bitmap → same diagonal-slide rule applied per step → grain traces the curve |
| **Grain-grain pile-up** | `IS_SOLID` includes `s_occ` → second grain blocked by first; they stack naturally |

### Lesson Learned
1. **After a collision, always break out of sub-steps.** Letting sub-steps continue after changing velocity reuses stale deltas and causes jitter.
2. **Slide velocity must be proportional to impact velocity**, not a constant.
3. **Particle-particle collision requires an occupancy bitmap**, not just a line bitmap. Keep the collision query decoupled from the storage so both can be checked with a single `IS_SOLID` macro.
4. **Do not mix continuous physics with hard-stop semantics** without a break.

---

## Issue 10 — Grains Were 1 px (Original Is ~2 px)

### Symptom
Individual sugar grains were nearly invisible, especially on the sky-blue
background. The stream appeared as a thin trickle rather than the chunky sugar
pour of the original game.

### Root Cause
`render_grains` called `draw_pixel(bb, x, y, color)` — one pixel per grain.
The original Sugar Sugar renders each particle as a ~2-pixel circle.

### Fix
Draw each grain as a 2×2 filled block:
```c
draw_pixel(bb, x,   y,   c);
draw_pixel(bb, x+1, y,   c);
draw_pixel(bb, x,   y+1, c);
draw_pixel(bb, x+1, y+1, c);
```
The physics bitmap still uses 1-pixel precision for collision.  The 2×2 render
is purely cosmetic.

### Lesson Learned
Separate *collision geometry* from *visual size*. The bitmap stores 1-px
grain positions for O(1) lookup; the renderer can draw any size on top without
affecting physics. Never change collision geometry just to make grains look
bigger — change only the rendering pass.

---

## Issue 11 — Grains Slid Forever on Flat Surfaces (No Horizontal Damping)

### Symptom
A grain that slid onto a flat horizontal line would continue drifting sideways
indefinitely, giving an unrealistic ice-like feel. The original game grains slow
to a halt quickly after leaving a ramp.

### Root Cause
After the slide collision transferred `|vy|` to `vx`, nothing ever reduced that
`vx`. Gravity only affects `vy`, so horizontal velocity accumulated without
bound, capped only by `MAX_VX`.

### Fix
Apply a per-frame horizontal drag multiplier immediately after gravity
integration:
```c
p->vx[i] *= GRAIN_HORIZ_DRAG;   /* GRAIN_HORIZ_DRAG = 0.97f */
```
At 60 fps this gives ≈84% speed retained per second — grains decelerate
naturally in about 3–4 seconds from full speed, matching the original's feel.

### Lesson Learned
Physical drag is almost always needed for particles with horizontal velocity.
In a real fluid simulator you'd use Stokes drag (`F = -bv`); here a simple
multiplicative factor per frame is sufficient and costs one multiply per grain.

---

## Issue 12 — Emitter Filled Pool in ~10 s With No Lines Drawn

### Symptom
Within ~10 seconds on level 1 (if the player hadn't drawn any redirecting line)
the grain pool was exhausted and spawning stopped. New grains appeared to
stop falling. The cup never filled.

### Root Cause
At 80 grains/second and a 4096-grain pool, the pool fills in exactly 51 seconds.
BUT: the grains that settled at the bottom of the screen were recycled only when
they fell off-screen or were absorbed by a cup. On a level where no cup is
reachable and no line redirects, ALL grains pile up on the floor until the pool
is full. After that, `grain_alloc` returns -1 and no new grains spawn.

The original Sugar Sugar pauses all emitters once ~500 active grains exist,
preventing the "pool exhaust" state entirely.

### Fix
In `spawn_grains`, count active grains at the top and return early if above
the threshold:
```c
#define EMITTER_MAX_ACTIVE  500

int active = 0;
for (int i = 0; i < p->count; i++)
    if (p->active[i]) active++;
if (active >= EMITTER_MAX_ACTIVE) return;
```

### Lesson Learned
Particle emitters MUST have an upper-bound guard, not just a pool-capacity
guard. A pool-capacity guard triggers only when the pool is 100% full; an
active-count guard throttles well before that point, giving the player time to
act without a hard freeze.

---

## Issue 13 — Cups Had No Physical Walls (Grains Entered From the Side)

### Symptom
Grains approaching from the left or right could enter a cup sideways and be
counted, even though visually the cup appeared to be a closed container with
only an open top. This allowed trivially easy solutions and didn't match the
original game's behavior.

### Root Cause
Cups were defined only as an AABB absorption zone. No solid pixels existed at
the cup boundary, so grains moved freely through the cup's left, right, and
bottom walls.

### Fix
Added `stamp_cups()` called from `level_load()` after `stamp_obstacles()`:
- Left wall: stamp column at `x = cup->x`
- Right wall: stamp column at `x = cup->x + cup->w - 1`
- Bottom wall: stamp row at `y = cup->y + cup->h - 1`
- Top: **left open** (grains must enter from above)

The absorption AABB check was updated to use the *interior* bounds
(`x+1 .. x+w-2`, `y .. y+h-2`), so wall pixels are treated as physics objects,
not absorption triggers.

**Overflow behavior**: when a cup is full, incoming grains are NOT absorbed.
They fall through the open top, hit the solid bottom wall, and pile up. The
pile eventually overflows over the open rim — enabling multi-cup chain strategies
from the original game.

### Lesson Learned
Physical container walls and game-logic absorption zones are two separate
concerns. Define them independently: stamp solid pixels for physics, use a
slightly-smaller AABB for the "absorb grain" logic. The ±1 pixel offset between
them costs nothing and decouples the two systems cleanly.

---

## Issue 14 — Cup Counter Was Tiny and Misaligned

### Symptom
The "remaining grains needed" counter above each cup was rendered at 8×8px
(1× scale) using `draw_int`. It was barely readable and its X position was not
properly centred on the cup.

### Root Cause
`draw_int` calls `draw_text` at 1× scale. The centring formula used a fixed
`-4` pixel offset (`tx = cup->x + cup->w / 2 - 4`) which assumed a single-digit
number and was wrong for two- or three-digit counts.

### Fix
Replaced `draw_int` with `draw_text_scaled(..., 2)` and a proper width
calculation:
```c
int tw  = digit_count * 18;  /* each char = 9 px × 2 scale = 18 px wide */
int tx  = cup->x + (cup->w - tw) / 2;
draw_text_scaled(bb, tx, cup->y - 20, buf, COLOR_UI_TEXT, 2);
```
When the cup is full, "OK" is shown in green (`COLOR_CUP_FILL_FULL`) at the
same 2× scale.

### Lesson Learned
When centring a variable-width text string, always compute the width from the
actual character count (`n * glyph_width * scale`) before placing it. A fixed
pixel offset will be wrong for any string length except the one you tested.


---

## Issue 15 — Sugar Ran Out After ~6 Seconds (Finite Sugar Bug)

### Symptom
The sugar stream appeared to stop permanently after about 6 seconds of play,
even on levels where no cups were filled. The game became unplayable.

### Root Cause
Two problems worked together:

1. **`EMITTER_MAX_ACTIVE 500` hard-pause** — `spawn_grains()` counted all
   active grains and returned early once the count hit 500.  This was added
   as a "throttle" to avoid pool exhaustion.

2. **Settled grains never freed** — a grain that came to rest on a surface
   (after collision) remained `active = 1` in the pool forever.  It occupied
   a slot in the `GrainPool` SoA **and** a cell in the occupancy bitmap
   `s_occ`.  It never moved again, but it was still counted.

With 80 grains/s and no cups, 500 slots filled in ≈6 s.  After that,
`spawn_grains` returned immediately on every frame — the emitter was
permanently blocked even though 499 of those grains were stationary.

### Why the "Throttle" Existed
The throttle was added in Issue 12 (previous round) to stop the pool filling
up.  It treated the symptom (pool full) but not the cause (slots never freed).
A better fix was deferred at the time; this issue is that deferred fix.

### Fix
**Bake settled grains into the static line bitmap, then free the slot.**

When a grain's speed has been below `GRAIN_SETTLE_SPEED` (4 px/s) for
`GRAIN_SETTLE_FRAMES` (3) consecutive frames:

1. Write `lb->pixels[by * W + bx] = (uint8_t)(p->color[i] + 2)` — the grain's
   pixel becomes a permanent solid obstacle, encoded with the grain's color index
   so `render_lines()` can reproduce the original color.
   (Initially written as `= 1`; the color encoding was added in Issue 17.)
2. Set `p->active[i] = 0` — the pool slot is freed immediately.

This is how the original Flash game worked: settled grains merged into the
static collision bitmap and the pool slot was recycled.  The sugar stream is
now truly infinite — the emitter fires continuously, settled grains free their
slots, and the pool stays well under its 4096-slot limit during normal play.

The `EMITTER_MAX_ACTIVE` constant and its throttle check were removed entirely.
Two new named constants replaced it:

```c
#define GRAIN_SETTLE_SPEED  4.0f   /* px/s threshold for "still"   */
#define GRAIN_SETTLE_FRAMES 3      /* frames still before baking   */
```

A new `uint8_t still[MAX_GRAINS]` field was added to `GrainPool` to track
consecutive still-frames per grain.  The field is zeroed on spawn and reset
to 0 whenever speed exceeds the threshold.

### Code Diff (key section in `update_grains`)

```c
/* After teleporter check, before next_grain: */
{
    float ax = p->vx[i] < 0 ? -p->vx[i] : p->vx[i];
    float ay = p->vy[i] < 0 ? -p->vy[i] : p->vy[i];
    if (ax < GRAIN_SETTLE_SPEED && ay < GRAIN_SETTLE_SPEED) {
        if (p->still[i] < 255) p->still[i]++;
    } else {
        p->still[i] = 0;
    }
    if (p->still[i] >= GRAIN_SETTLE_FRAMES) {
        int bx = (int)p->x[i], by = (int)p->y[i];
        if (bx >= 0 && bx < W && by >= 0 && by < H)
            lb->pixels[by * W + bx] = (uint8_t)(p->color[i] + 2);
            /* (color+2) encodes grain color; see Issue 17 for the renderer fix */
        p->active[i] = 0;
        goto next_grain;
    }
}
```

### Why This Works Correctly for Collisions
`IS_SOLID` already checks `lb->pixels`, so future grains pile on top of
baked pixels exactly as they would on a drawn line.  The occupancy bitmap
`s_occ` is rebuilt from scratch every frame, so the freed slot's old `s_occ`
cell is naturally cleared on the next frame — no cleanup needed.

### Effect on Cup Overflow
Grains that settle *inside* a full cup are absorbed before they can settle
(the cup AABB check runs first in the loop).  Only grains in an overflow pile
above the rim settle-and-bake, which is correct — they form a growing mound
that eventually redirects later grains away.

### Lesson Learned
A "throttle" that prevents spawning is the wrong fix for a full pool.  The
correct fix is to recycle slots — in a pixel-collision game the natural
recycling strategy is to merge settled particles into the static bitmap.
This is called the **"bake-and-free"** pattern:

> When a particle's purpose as a dynamic object is over, write it into the
> static world and free its dynamic slot.

The same pattern applies to any particle system with a fixed-size pool where
particles can permanently come to rest: bullets that embed in walls, snow that
accumulates on ledges, sand in a sand-fall simulator, etc.

---

## Issue 16 — Minimum Slide Speed Prevented Grains From Ever Settling

### Symptom
After the Issue 15 bake-and-free fix, only a handful of grains were ever
visible at once.  The sugar stream appeared to stop after 2–3 grains.

### Root Cause
The collision code had:

```c
float spd = fabsf(p->vy[i]);
if (spd < 60.0f) spd = 60.0f;   /* ← minimum slide speed */
p->vx[i] = (float)d * spd;
```

This minimum meant every grain that hit any surface — including a flat pile
at the bottom of the screen — was re-kicked to 60 px/s horizontally.

With `GRAIN_HORIZ_DRAG = 0.97` (per-frame), 60 px/s takes ~90 frames
(≈ 1.5 s) to decay below the `GRAIN_SETTLE_SPEED` threshold of 4 px/s.
But the very next frame the grain tried to move down, hit the same pile,
and was re-kicked back to 60.  **The grain was trapped in a perpetual
60 px/s bounce loop and never settled, never baked, and never freed its
pool slot.**

### Why the Minimum Was There
It was added to prevent a "static-friction stall" where slow grains on gentle
slopes would barely move.  The intention was correct but the implementation
was wrong: a *constant* minimum re-applied every collision prevents settling.

### Fix
Remove the minimum slide speed entirely:

```c
float spd = fabsf(p->vy[i]);
/* no minimum — slow grains on gentle slopes settle naturally */
if (spd > MAX_VX) spd = MAX_VX;
p->vx[i] = (float)d * spd;
```

Now:
- A grain falling fast (vy = 200) slides fast (vx = 200), decays in ~90 frames.
- A grain nearly at rest (vy = 3) slides at 3 px/s, decays below 4 in 1 frame,
  then bakes in `GRAIN_SETTLE_FRAMES` more frames.

`GRAIN_SETTLE_FRAMES` was bumped from 3 → 6 to give grains a couple of
extra frames to start sliding on gentle slopes before baking (prevents
over-eager baking on a steep line where vy is small on first contact).

### Lesson Learned
Any artificial lower-bound on velocity that is reapplied at each collision
will create a perpetual-bounce attractor: the grain can never escape it
because every attempt to come to rest re-energises it.  Physics minimums
should only be applied at spawn or at a single acceleration step, not
repeatedly at every collision.

---

## Issue 17 — Settled Grains Appeared as Black Blobs

### Symptom
Grains that came to rest and were "baked" into the line bitmap appeared as
solid black or dark-grey blobs, losing their original cream / red / green /
orange color.  After a few seconds of play the floor and slopes were
covered in black patches.

### Root Cause
The bake-and-free code wrote a plain `1` into `lb->pixels`:

```c
lb->pixels[by * W + bx] = 1;   /* BUG: no color information */
```

`render_lines()` treated every non-zero pixel identically:

```c
if (lb->pixels[i])
    bb->pixels[i] = COLOR_LINE;  /* always dark grey */
```

The original grain color was thrown away at bake time and could not be
recovered at render time.

### Root Cause (design-level)
The `LineBitmap` pixel was designed as a boolean: 0 = empty, non-zero =
solid.  It stored no semantic information beyond "solid or not."  When
baked grains were added, no mechanism existed to distinguish them from
player-drawn lines.

### Fix
Encode the grain's color index into the pixel value at bake time:

```c
/* LineBitmap pixel encoding:
 *   0        = empty
 *   1        = obstacle / cup wall (stamped at load)
 *   255      = player-drawn brush
 *   2 .. 5   = baked grain, color index = value − 2
 */
lb->pixels[by * W + bx] = (uint8_t)(p->color[i] + 2);
```

`render_lines()` uses the encoded value to recover the color:

```c
uint8_t v = lb->pixels[i];
if (!v) continue;
bb->pixels[i] = (v >= 2 && v <= 5)
                    ? g_grain_colors[v - 2]  /* grain color */
                    : COLOR_LINE;            /* drawn line  */
```

`IS_SOLID` was already `if (lb->pixels[...]) → solid`; values 2–5 are
non-zero, so physics is completely unaffected.

### Cost
One extra compare per non-zero pixel in `render_lines` — the branch
`v >= 2 && v <= 5` is predicted nearly perfectly (it follows a predictable
pattern on any given frame).  No measurable performance impact.

### Lesson Learned
**When writing data into a single-purpose boolean bitmap, plan ahead for
any semantic information you might need later.**  Changing a `uint8_t` from
a boolean to a small tagged value costs nothing upfront and avoids a
later "add a whole second parallel array" refactor.

The key insight is: **physics (IS_SOLID) only needs to know "zero or not",
but rendering needs to know "what kind of solid?"**  Encoding both answers
in a single byte with a simple range check keeps the data structure minimal
while serving both consumers.

---

## Issue 18 — Sugar Stream Too Thin and Grains Too Bouncy

### Symptom
Two related problems appeared simultaneously:

1. **Too few visible grains in flight.** The stream looked like a thin
   trickle — only ~10–20 grains were ever visible at once even though the
   emitter rate was 80/s.

2. **Grains were "bouncy" / over-responsive on slopes.**  After hitting a
   line, a grain would race across the canvas and pile up far from the
   impact point, giving an unrealistically elastic feel.

### Root Cause

#### Sub-problem A — Narrow emitter jitter (stream too thin)
`EMITTER_JITTER` was `30.0f` px/s.  With `MAX_VX = 200` and gravity 500
px/s², a grain spawned with vx = ±15 falls nearly straight down.  The
stream was a narrow 1–2 pixel column.  In the original Sugar Sugar the
stream has a noticeable horizontal spread that makes the flow look fluid.

#### Sub-problem B — Full |vy| transferred to vx (too much bounce)
The slide collision transferred 100% of `|vy|` into `vx`:

```c
float spd = fabsf(p->vy[i]);   /* full transfer — BUG */
p->vx[i] = (float)d * spd;
```

At terminal velocity (600 px/s), a grain hitting a slope would immediately
get `vx = 600 px/s`, racing clear across the screen before the horizontal
drag (`GRAIN_HORIZ_DRAG = 0.97`) had any chance to slow it.

Additionally, `MAX_VX = 200` was too high: even capped, grains slid ~60 px
per second longer than the original game's feel.

Finally `GRAIN_HORIZ_DRAG = 0.97` was too gentle: at 60 fps, `0.97^60 ≈ 0.16`
— a grain retains 16% of its speed after one second, meaning it slides for
~2–3 seconds.  The original game settles in under 1 second.

### Fix

**A — Wider spawn jitter:**

```c
#define EMITTER_JITTER 80.0f   /* was 30.0f */
```

Grains spawn with vx in `[-80, +80]` px/s, visually spreading the stream
into a ~10 px wide column that matches the original game.

**B — Partial slide-velocity transfer + stronger drag + lower cap:**

```c
#define GRAIN_SLIDE_TRANSFER 0.65f  /* new constant */
#define MAX_VX               150.0f /* was 200.0f   */
#define GRAIN_HORIZ_DRAG     0.93f  /* was 0.97f    */
```

Collision slide:

```c
float spd = fabsf(p->vy[i]) * GRAIN_SLIDE_TRANSFER;  /* 65% transfer */
if (spd > MAX_VX) spd = MAX_VX;
p->vx[i] = (float)d * spd;
```

Effect at terminal velocity (vy = 600):
- Old: `vx = min(600, 200) = 200 px/s`, decays in ~2 s.
- New: `vx = min(390, 150) = 150 px/s`, decays in **≈ 0.55 s**.

The three knobs work together:
- `GRAIN_SLIDE_TRANSFER`: controls the "elasticity" of the impact.
- `MAX_VX`: hard ceiling so extreme-angle impacts don't send grains racing.
- `GRAIN_HORIZ_DRAG`: controls deceleration rate.

### How the Values Were Chosen
Test by drawing a 45° ramp and watching where grains stop:
- If they race off the far edge → reduce `GRAIN_SLIDE_TRANSFER` or `MAX_VX`.
- If they barely move and pile up at the impact point → increase `GRAIN_SLIDE_TRANSFER`.
- If they decelerate too slowly → reduce `GRAIN_HORIZ_DRAG`.
- If they stop instantly → increase `GRAIN_HORIZ_DRAG` toward 1.0.

The combination `(0.65, 150, 0.93)` matched the original game's visual feel
in side-by-side comparison.

### Lesson Learned
1. **Particle spawn spread and collision elasticity are separate knobs.**
   Treat them independently: `EMITTER_JITTER` controls the visual width of
   the stream; `GRAIN_SLIDE_TRANSFER` + `MAX_VX` + `GRAIN_HORIZ_DRAG`
   control how far and how fast grains travel after a surface impact.

2. **A 100% velocity transfer is physically "correct" but often wrong for
   games.** Games need to *feel* right, not simulate perfectly. A 65%
   transfer with a lower cap gives the player better control of the flow
   because grains land predictably near their impact point.

3. **Tune drag, cap, and transfer together, not individually.**  Changing
   only drag while keeping a high cap still lets grains travel too far;
   changing only the cap while keeping weak drag still causes long slides.

---

## Issue 19 — Absorbed Grains Left a Ghost in the Occupancy Bitmap

### Symptom
In rare cases (usually when many grains were funnelling into a cup at
once), a grain inside a cup appeared to "block" the grain behind it for
one frame, causing a momentary stutter in the flow just above the cup rim.

### Root Cause
When a grain was absorbed by a cup, the code jumped directly to `next_grain`
without clearing the grain's pixel from the occupancy bitmap `s_occ`:

```c
/* Mark final position so subsequent grains treat this as solid. */
s_occ[fy * W + fx] = 1;   ← marked

/* ... cup check ... */
cup->collected++;
p->active[i] = 0;
goto next_grain;           ← s_occ[fy*W+fx] still = 1 for rest of frame
```

For the remainder of the frame, `IS_SOLID(fx, fy)` returned `true` for the
absorbed grain's position, even though the grain was gone.  Any subsequent
grain trying to enter that pixel was bounced away.

### Fix
Clear the occupancy pixel immediately when absorbing a grain:

```c
s_occ[gy * W + gx] = 0;   /* clear: grain is gone, pixel is free */
p->active[i] = 0;
goto next_grain;
```

Applied in both the "right color, cup not full" and "wrong color, discard"
branches.

### Lesson Learned
**When removing an object from a per-frame derived structure, clean up
the derived structure immediately — do not rely on "the next full rebuild
will fix it."**  The occupancy bitmap is rebuilt from scratch each frame,
but the rebuild happens at the *start* of the loop, before any grain in
the current frame has moved.  An absorbed grain's pixel is still marked
"solid" for every grain that follows it in the loop order.  One `s_occ[…]
= 0` line at the point of absorption is the minimal, correct fix.

---

## Issue 20 — Perpetual-Slide Attractor: Grains Lose Momentum Immediately and Never Settle

### Symptom
After the Issue 16 fix (remove `spd < 60` minimum) physics still felt wrong:
- Grains coming off a ramp stopped almost immediately instead of sliding to the end.
- Some grains on slopes never baked — they oscillated at a constant low speed forever.
- The game felt sluggish and unresponsive regardless of how drag or transfer was tuned.

### Root Cause
The slide-velocity block in `update_grains` unconditionally applied
`spd = fabsf(vy) * GRAIN_SLIDE_TRANSFER` on every collision — including collisions where the grain was **already riding the surface**.

After each collision zeroes `vy`, gravity re-accumulates ~8.33 px/s per frame (500 / 60).
On the very next physics sub-step the grain hits the surface again with `vy ≈ 8 px/s`.
The slide formula then computes `8 × 0.65 = 5.2 px/s` and assigns that to `vx` —
**replacing any existing momentum** (e.g. 120 px/s from a genuine fall) with 5.2 px/s.
The grain effectively stopped on the first sub-step after landing.

Additionally, `GRAIN_HORIZ_DRAG = 0.93` decayed 5.2 to 4.84 px/s the next frame.
Then vy was 8 again → `8 × 0.65 = 5.2 > 4.84` → vx jumped back to 5.2.
Since `GRAIN_SETTLE_SPEED = 4.0`, the grain always had `|vx| > 4.0`, so `still` never
accumulated to `GRAIN_SETTLE_FRAMES` → the grain never baked.
**An attractor at ~5.2 px/s: the grain slides forever.**

### Fix: Surface-Detection Threshold (`GRAIN_SURFACE_VY`)
Distinguish "grain just fell from height" from "grain is already on the surface":

```c
float vy_abs = fabsf(p->vy[i]);
float spd;
if (vy_abs >= GRAIN_SURFACE_VY) {
    /* Fresh fall → transfer fraction of |vy| to slide speed */
    spd = vy_abs * GRAIN_SLIDE_TRANSFER;
} else {
    /* Already on surface (vy = 1–2 frames of gravity) → preserve momentum */
    spd = fabsf(p->vx[i]);
}
if (spd > MAX_VX) spd = MAX_VX;
p->vx[i] = (float)d * spd;
p->vy[i] = 0.0f;
```

`GRAIN_SURFACE_VY = 20.0f`: one frame of gravity = 8.3 px/s; two frames = 16.7 px/s.
Threshold of 20 safely filters surface-re-contact (≤16.7) from genuine falls (≥25 after 3+ frames).

### Parameter Retune
The surface-detection fix required re-tuning all related constants to restore natural feel:

| Constant | Before | After | Reason |
|---|---|---|---|
| `GRAIN_HORIZ_DRAG` | 0.93 | 0.95 | 0.93 decelerated too fast once momentum was preserved |
| `GRAIN_SLIDE_TRANSFER` | 0.65 | 0.75 | More responsive on fresh impacts now that it's not applied every frame |
| `GRAIN_SETTLE_FRAMES` | 6 | 8 | Extra 2 frames ensures grains truly stop before baking |
| `MAX_VX` | 150 | 200 | Surface logic now caps speed correctly; restore original limit |
| `EMITTER_JITTER` | 80 | 50 | 80 was too scattered; 50 gives a natural fan-width |

### Lesson Learned
**Never blindly apply a velocity transfer formula every frame.  Always ask: "Is this grain in the state I think it is?"**  The slide formula was designed for fresh-fall impacts (vy = 150+ px/s), but was being called just as often when vy = 8 px/s from one frame of gravity.  Categorizing the grain's state (fresh fall vs. on-surface) before choosing how to set velocity is the correct pattern.  A threshold constant (`GRAIN_SURFACE_VY`) makes the intent explicit and the value easy to tune independently of the transfer factor.

---



| # | Bug | Root Cause | Fix Pattern |
|---|-----|-----------|-------------|
| 1 | Text mirrored | Font BIT0-left vs renderer `0x80>>col` | Match bitmask direction to font convention |
| 2 | Wrong background | Hard-coded cream color from prototype | Set from reference screenshot |
| 3 | Button hit-box offset | Different `grid_y` in render vs. update | Single shared `#define` constants |
| 4 | Button clicks ignored | `reset_hover`/`gravity_hover` never updated | Compute hover state every frame in update |
| 5 | Drawing bleeds into UI | No zone guard on mouse drawing | Check `my >= UI_BTN_Y` before drawing |
| 6 | Potential color swap on X11 | `XCreateImage` byte order implicit | Set `g_ximage->byte_order = LSBFirst` |
| 7 | Text too small | No scaled text variant existed | Add `draw_text_scaled(bb, x, y, str, color, N)` |
| 8 | Wrong background (round 2) | Assumed white; original is sky-blue | Sample pixel from reference screenshot |
| 9 | Grains jitter on slopes | Fixed slide vx, stale sub-steps, no grain-grain | Proportional slide, break-after-collision, occupancy bitmap |
| 10 | Grains too small | 1-px rendering | Render each grain as 2×2 block (cosmetic only) |
| 11 | Grains slide forever | No horizontal drag | `vx *= GRAIN_HORIZ_DRAG` each frame |
| 12 | Sugar stops after ~10 s | Emitter throttle blocked pool | Active-count guard, not pool-capacity guard |
| 13 | Grains enter cups sideways | No cup wall pixels | `stamp_cups()` stamps solid left/right/bottom |
| 14 | Cup counter tiny/misaligned | Fixed offset, 1× scale | `draw_text_scaled` + proper width calc |
| 15 | Sugar runs out (~6 s) | Settled grains never freed | Bake-and-free: write pixel to line bitmap, free slot |
| 16 | Few grains visible after fix 15 | `spd < 60` minimum caused perpetual bounce | Remove minimum slide speed |
| 17 | Settled grains appear black | `render_lines` used `COLOR_LINE` for all pixels | Encode `grain_color + 2` in bitmap; decode in renderer |
| 18 | Stream thin + grains too bouncy | Low jitter (30), no surface-detection, weak drag | Partial fix: `EMITTER_JITTER=80`, `GRAIN_SLIDE_TRANSFER=0.65`, `MAX_VX=150`; fully fixed in Issue 20 |
| 19 | Ghost pixel blocks grain flow for 1 frame | Absorbed grain not cleared from `s_occ` | `s_occ[…] = 0` at point of absorption |
| 20 | Grains lose momentum immediately; some never settle | `spd=|vy|×transfer` applied every frame including surface-riding frames | Surface-detection: preserve `|vx|` when `|vy| < GRAIN_SURFACE_VY (20)` |

---

## Recurring Themes for course-builder.md

1. **Never duplicate layout numbers** between renderer and input handler.
   Define every pixel coordinate as a named constant shared by both.

2. **Document bit-endianness in font headers** before writing any renderer.
   Test with 'N' — its diagonal should run top-left → bottom-right.

3. **Explicitly set all XCreateImage fields** even when the default works.
   Platform code must be portable-by-construction, not portable-by-luck.

4. **Verify reference visuals before writing color constants.**
   Capture exact pixel values from the original game with a color picker.

5. **Zone your canvas at design time.** Define HUD regions as named constants
   and enforce them in every piece of input code.

6. **Every rendered interactive element needs a click handler on day one.**
   A button without a handler is a visual bug waiting to be filed.

7. **Never use an artificial velocity minimum at a collision point.**
   It creates a perpetual-bounce attractor that prevents grains from settling.

8. **Bake settled particles into the static world and free their slot.**
   This is the "bake-and-free" pattern — the correct fix for a fixed-size
   particle pool where particles can permanently come to rest.

9. **Encode semantic information in bitmap values, not just boolean.**
   A `uint8_t` can distinguish "empty / obstacle / white grain / red grain"
   with a simple range check, avoiding a second parallel array.

10. **Tune particle physics with three knobs together: transfer factor, cap,
    and drag.** Changing one in isolation produces a different imbalance.
    Test by drawing a 45° ramp and measuring where grains stop.

11. **Clear derived per-frame data structures at the point of removal.**
    Do not rely on "the next full rebuild will fix it" — other objects in
    the same frame are already affected by the stale data.

---

## Issue 17 — Three Interacting Physics Bugs Causing Thin/Sparse Sugar Stream

### Symptoms
1. Only a small number of sugar grains visible at once
2. Grains felt "bouncy" and unnatural when hitting surfaces
3. Stream sometimes stopped entirely at level start

### Root Causes (three bugs compounding each other)

#### Bug A: `EMITTER_JITTER = 50 px/s` — stream scattered at birth

The jitter formula `(rand_0_to_1 - 0.5) * EMITTER_JITTER * 2` produces ±50 px/s horizontal velocity. This is huge: on a 640 px canvas with the spout at center (x=320), grains immediately fly sideways and the pour looks thin and scattered rather than a focused stream.

Fix: reduced to ±3 px/s — just enough to create a natural slight fan.

#### Bug B: `GRAIN_SURFACE_VY` path assigned `spd = |vx|` (could be zero)

The "surface-riding" branch of the collision response ran when `|vy| < 20 px/s` (gravity has only re-accumulated 1-2 frames). It set `spd = fabsf(vx)`. If the grain had near-zero horizontal velocity (e.g. grain with low jitter, freshly landed on a gentle slope), `spd = 0` → `vx = 0, vy = 0`. The grain was instantly frozen on the slope and baked in 8 frames.

Fix: removed the GRAIN_SURFACE_VY / GRAIN_SLIDE_TRANSFER two-path system entirely. Replaced with `spd = max(|vy|, GRAIN_SLIDE_MIN=50)`. The minimum is safe because it only fires when a free diagonal is found — flat piles still settle via the `!slid` zero-velocity path.

#### Bug C: Velocity-based settle detection — `still` counter always reset by gravity

The settle check compared `|vx| < 4 && |vy| < 4`. The fatal flaw: gravity is applied at the START of each frame (step 1), before sub-steps (step 3) and before the settle check (step 8). After a collision zeros `vy`, the very next frame runs:

```
1. gravity → vy = 8.3
3. sub-steps → collision → vy = 0
8. settle check: vy = 0 → still++?  NO! — still checked vy AFTER step 3 = 0, OK
```

Wait — actually vx/vy were 0 after step 3. But! The sub-pixel drift issue:

A completely blocked grain at y=4.97 tries each frame to move to y=5.11. If the target pixel (iy=5) happens to be FREE (not the solid below), the grain actually moves there — no collision, no vy zeroing. Then `vy = 8.3` at settle check time → `still = 0`. The grain perpetually drifts sub-pixel and the still counter is reset before it can accumulate.

Fix: **displacement-based detection**. Capture position before sub-steps (`pre_x, pre_y`). After sub-steps, compute `dist² = (x-pre_x)² + (y-pre_y)²`. If `dist² < 0.04` (moved < 0.2 px) → `still++`. If `dist² >= 0.04` → `still = 0`. After `GRAIN_SETTLE_FRAMES (6)` frames → bake.

This correctly handles all cases:
| Scenario | dist²/frame | still |
|----------|-------------|-------|
| Free-falling (vy=50) | 0.69 | 0 |
| Sliding on slope (vx=50) | 0.69 | 0 |
| Sub-pixel drift (blocked, vy=8.3) | 0.02 | ++ |
| Truly stuck (vx=0, vy=0) | 0 | ++ |

### Changed Constants Summary

| Constant | Before | After | Reason |
|----------|--------|-------|--------|
| `EMITTER_JITTER` | 50 px/s | 3 px/s | Focused stream |
| `GRAIN_SLIDE_TRANSFER` | 0.75 | removed | Replaced by GRAIN_SLIDE_MIN |
| `GRAIN_SURFACE_VY` | 20 px/s | removed | Over-complex, caused spd=0 |
| `GRAIN_SLIDE_MIN` | — | 50 px/s | Floor for surface slide |
| `GRAIN_HORIZ_DRAG` | 0.95 | 0.98 | Grains slide further on slopes |
| `GRAIN_SETTLE_SPEED` | 4 px/s | removed | Replaced by GRAIN_SETTLE_DIST2 |
| `GRAIN_SETTLE_DIST2` | — | 0.04 | Displacement threshold squared |
| `GRAIN_SETTLE_FRAMES` | 8 | 6 | Faster bake now detection is reliable |

### Lesson Learned

**Never check velocity for settle detection when gravity is applied every frame.** Gravity always re-adds a non-zero velocity, making any velocity threshold fail unless checked at exactly the right moment in the pipeline. Position displacement is more robust: it directly measures "did the grain go anywhere?" regardless of what forces ran.

**"Surface-riding" logic should preserve momentum, but not via `|vx|` copy.** Setting slide speed to `|vx|` assumes the grain has useful horizontal momentum — but newly-spawned or freshly-bounced grains may have near-zero vx. A `GRAIN_SLIDE_MIN` floor in the *free-diagonal* branch achieves the same goal (keep grains moving on slopes) without the zero-velocity failure case.

---

## Issue 18 — Frame-Rate-Dependent Settle Threshold: "Nothing Drops at All"

### Symptom
With the displacement-based settle fix from Issue 17 in place, opening the game showed no sugar grains at all — nothing dropped, the emitter appeared dead.

### Root Cause
`main_x11.c` had no frame rate cap.  At uncapped speed the game loop ran at
thousands of frames per second (measured: 5,000–20,000 fps on modern hardware).

With `dt ≈ 0.0001 s` and a grain falling at `vy = 50 px/s`:

```
dist = vy × dt = 50 × 0.0001 = 0.005 px per frame
dist² = 0.000025
GRAIN_SETTLE_DIST2 = 0.04   →   0.000025 < 0.04  →  still++
```

Every grain was marked "still" on its very first frame of existence.  After
`GRAIN_SETTLE_FRAMES (6)` frames (which elapsed in less than 1 ms), every
grain was baked into the line bitmap and freed — all before the first XFlush
call had a chance to render them.

The displacement-based fix from Issue 17 solved one problem (gravity resets
velocity-based counters) but introduced a new dt-dependency: the FIXED
threshold `0.04 px²` only works at ~60 fps where 0.2 px/frame is meaningful.
At 10,000 fps, 0.005 px/frame is 10× smaller, making every grain look "still".

### Two-Part Fix

**Part 1 — 60 fps frame rate cap in `main_x11.c`:**

```c
double frame_end  = platform_get_time();
double elapsed    = frame_end - curr_time;
double target     = 1.0 / 60.0;
if (elapsed < target) {
    struct timespec ts;
    ts.tv_sec  = (time_t)0;
    ts.tv_nsec = (long)((target - elapsed) * 1e9);
    nanosleep(&ts, NULL);
}
```

This ensures `dt ≈ 0.0167 s` every frame.  Grain movement is predictable and
all the physics constants that were tuned at 60 fps behave as designed.

**Part 2 — Frame-rate-independent settle threshold in `game.c`:**

Replace the fixed `GRAIN_SETTLE_DIST2` constant with a `dt`-scaled threshold:

```c
/* Old: fixed threshold — breaks at high FPS */
if (dist_sq < GRAIN_SETTLE_DIST2)   /* 0.04 */

/* New: velocity-based threshold — works at ANY fps */
float settle_sq = GRAIN_SETTLE_SPEED * GRAIN_SETTLE_SPEED * dt * dt;
if (dist_sq < settle_sq)            /* (10 px/s)² × dt² */
```

A grain is "still" if its average speed this frame is below
`GRAIN_SETTLE_SPEED (10 px/s)`.  Because we compute it from *displacement per
frame*, the threshold automatically scales with dt:

| FPS | dt | threshold dist | falling grain (50 px/s) dist | settles? |
|-----|----|---------------|------------------------------|----------|
| 60 | 0.0167 | 0.167 px | 0.833 px | ✗ (correct) |
| 120 | 0.0083 | 0.083 px | 0.417 px | ✗ (correct) |
| 10,000 | 0.0001 | 0.001 px | 0.005 px | ✗ (correct) |
| any | dt | 10×dt px | 50×dt px | ✗ (correct) |

A blocked grain (dist ≈ 0) always triggers; a moving grain (dist = v×dt)
only triggers when v < 10 px/s.

`GRAIN_SETTLE_DIST2` was removed from `game.h`; replaced by `GRAIN_SETTLE_SPEED = 10.0f`.

### Secondary Fix — Level Complete Click/Enter to Advance

The level-complete screen rendered `"CLICK or press ENTER for next level"` but
the `update_level_complete` function only received `(state, dt)` — no `input`
pointer.  Clicking or pressing Enter did nothing; the player had to wait the
full 2-second auto-advance.

Fix:
1. Added `GameButtonState enter` field to `GameInput` in `game.h`
2. Mapped `XK_Return` and `XK_space` to `input->enter` in `main_x11.c`
3. Changed `update_level_complete` signature to `(state, input, dt)` and
   updated the dispatch call
4. Advance condition: `(timer > 2.0f) || click || enter` — any of the three
   triggers immediate level advance

### Lessons Learned

**12. Any fixed per-frame threshold (distance, time, count) is implicitly
frame-rate-dependent.**
The correct pattern for a "speed below threshold" check is:
```c
dist < SPEED_THRESHOLD * dt        /* linear */
dist² < SPEED_THRESHOLD² * dt²    /* squared, avoids sqrt */
```
This gives the same behaviour at 30 fps, 60 fps, 144 fps, or 10,000 fps.

**13. Always add a frame rate cap to the game loop — even for physics that
claim to be dt-scaled.**
A dt cap (`if (dt > MAX_DT) dt = MAX_DT`) guards against spiral-of-death at
low FPS.  A sleep-based cap prevents micro-dt issues and CPU waste at high FPS.
The two together bound dt to a safe window (`[1/200, 1/30]` is typical).

**14. If a UI element shows a keyboard hint, wire the key up.**
"CLICK or press ENTER" promises Enter works.  Checking `input->enter` requires
the field to exist in `GameInput` AND the platform layer to map the physical
key.  A systematic audit of all hint text vs. actual input bindings catches
these mismatches before players do.

---

## Issue 19 — Broken Line Drawing: Gaps When Mouse Moves Fast

### Symptom
Drawing lines with the mouse left visible gaps — the faster the mouse moved,
the wider the gaps.  Slow movement produced continuous lines; fast movement
produced a dotted trail of disconnected circles.

### Root Cause
`platform_get_input` in `main_x11.c` processed all queued X11 events in a
`while (XPending(...))` loop.  Inside that loop, every `MotionNotify` event
did:

```c
input->mouse.prev_x = input->mouse.x;   /* set prev = current */
input->mouse.y      = event.xmotion.y;  /* then update current */
```

X11 can queue many motion events between frames — the OS batches them when the
game is busy.  At 60 fps a fast mouse gesture can produce 10–20 queued events.
After processing all N events:

```
Event 1:  prev = pos0,  x = pos1
Event 2:  prev = pos1,  x = pos2
...
Event N:  prev = posN-1, x = posN
```

By the time `draw_brush_line(prev_x, prev_y, x, y)` runs in `update_playing`,
`prev` is only `posN-1` and `x` is `posN` — one tiny step.  All of
`pos0..posN-2` are lost.  Only the last segment of the path is drawn.

### Fix
Set `prev_x/y` ONCE at the very top of `platform_get_input`, BEFORE the event
loop.  Inside the loop, update ONLY `mouse.x/y`:

```c
void platform_get_input(GameInput *input) {
    /* Snapshot position before events — this becomes "start of frame" */
    input->mouse.prev_x = input->mouse.x;
    input->mouse.prev_y = input->mouse.y;

    while (XPending(g_display)) {
        ...
        case MotionNotify:
            input->mouse.x = event.xmotion.x;  /* no prev update here! */
            input->mouse.y = event.xmotion.y;
            break;
    }
}
```

After all events, `prev_x/y` = start-of-frame mouse position, `x/y` =
end-of-frame position.  Bresenham's algorithm in `draw_brush_line` fills every
pixel between them in a single call — gapless regardless of how many events
were batched.

`ButtonPress` is a special case: it still sets `prev_x = x = click_x` so the
brush starts exactly at the click point, not at the last hover position.

### Lesson Learned

**15. Input prev-position must be snapshotted once per frame, not once per
event.**

A frame-based game updates the world once per frame.  Any "diff" between
current and previous state (prev_x/prev_y, button transitions) must capture
the state at the START of the frame.  Updating `prev` inside the event loop
makes it a per-event diff, not a per-frame diff.

The correct pattern for any platform backend:
```c
/* BEFORE event loop: */
input->mouse.prev_x = input->mouse.x;   /* frame start */
/* INSIDE event loop: */
input->mouse.x = event.x;              /* accumulate to final position */
/* AFTER event loop: draw from prev to x — covers full frame path */
```

---

## Issue 20 — Grains Pile Too Steeply; Stream Too Narrow (Angle of Repose + Jitter)

### Symptoms
1. Sugar accumulated into a tall narrow tower directly under the emitter — piled like a wall, not like sand.
2. Stream appeared to only drop from "the middle" — all grains fell in a single column.

### Root Causes

#### Bug A: Diagonal check only ±1 pixel → 45° angle of repose

The collision slide looked for a free pixel at `(cur_x ± 1, iy)`.  This means
a grain can only slide off the pile if the IMMEDIATELY adjacent pixel is free.
Any 2-wide obstacle or 2-grain-wide pile blocks both diagonals → the grain
stacks vertically.  The natural angle of repose is exactly 45°: for every
grain that settles, the pile grows one pixel up AND one pixel out — a perfect
45° wall, not a natural sugar heap.

Real sugar has an angle of repose of ~25–35°.  To reproduce this we need to
check ±2 pixels: a grain can skip over a 1-wide neighbour and land 2 pixels
out.

Fix: changed the inner diagonal loop from:
```c
for (int attempt = 0; attempt < 2 && !slid; attempt++) {
    int sx = cur_ix + d;   /* only ±1 */
```
to:
```c
for (int dist = 1; dist <= GRAIN_SLIDE_REACH && !slid; dist++) {
    for (int attempt = 0; attempt < 2 && !slid; attempt++) {
        int sx = cur_ix + d * dist;   /* ±1 first, then ±2 */
```
with a **path-clear guard** that verifies all intermediate pixels are free:
```c
for (int px = cur_ix + d; px != sx + d; px += d)
    if (IS_SOLID(px, iy)) { ok = 0; break; }
```
`GRAIN_SLIDE_REACH = 2` added to `game.h`.

Result: angle of repose ~27°.  Grains flow outward more naturally and piles
look like a sugar heap, not a brick wall.

#### Bug B: EMITTER_JITTER = ±3 px/s — stream too narrow

With ±3 px/s horizontal spread and GRAIN_HORIZ_DRAG = 0.98/frame, the
horizontal velocity decays from 3 to ~1 px/s within 10 frames.  All grains
travel nearly straight down.  The resulting stream is 2–3 pixels wide — barely
visible as a spread.  The pile forms as a single narrow column directly below
the spout, which visually reads as "all from the middle, nothing from the
sides."

Fix: bumped EMITTER_JITTER from 3.0f → 8.0f px/s.  This creates a visible
small fan (~10-15 px wide at the floor) matching the original game's look.

### Changed Constants

| Constant | Before | After | Reason |
|----------|--------|-------|--------|
| `GRAIN_SLIDE_REACH` | — (not present) | 2 | Shallower angle of repose |
| `EMITTER_JITTER` | ±3 px/s | ±8 px/s | Visible stream spread |

### Lessons Learned

**16. The angle of repose of a cellular automaton is controlled by the
diagonal check radius.**

With radius R, a grain can land R pixels laterally for every 1 pixel vertically
→ angle = `atan(1/R)`.  R=1 → 45°, R=2 → 27°, R=3 → 18°.  The "right" value
depends on the material: sand/sugar ~27°–35° (R=2) feels correct.

**17. Never tune a jitter value from its effect on a single grain — test with a
full stream.**

±3 px/s seems like spread, but with drag the grain is nearly stationary
horizontally within 5 frames.  The only way to see that the stream is too
narrow is to run it and watch the pile form.  Always sanity-check emitter
jitter by observing the pile width, not the initial scatter angle.

---

## Issue 21 — Three Discrete Streams Instead of One Smooth Pour; Line Too Thick

### Symptoms
1. Sugar appeared as three separate trickles: a heavy center stream and two lighter
   streams one on each side — not a smooth, continuous pour.
2. Player-drawn lines were too thick, obscuring detail and feel awkward to draw with.

### Root Cause A: All grains spawn at the exact same pixel

`spawn_grains` placed every grain at `(em->x, em->y)` — always the same integer
pixel.  With 80 grains/sec at 60 fps, ~1-2 grains spawn per frame, all at (320, 20).

The first grain that stalls on its way down marks pixel (320, Y) in the occupancy
bitmap.  Every subsequent grain spawned that frame or the next collides immediately
at that pixel.  The slide resolver tries ±1 first, then ±2.  This creates grains
landing at EXACTLY x=319, x=320, and x=321 (the only 3 slide destinations from
x=320 with GRAIN_SLIDE_REACH=2 when intermediates are blocked).

Each of those three positions grows its own pile, creating three visually discrete
streams.  The center (x=320) pile is heaviest because grains with near-zero velocity
jitter fall straight through; the side piles (x=319, x=321) receive only grains that
slid off the center.

### Root Cause B: GRAIN_SLIDE_MIN = 50 px/s flung satellite grains too far

Even after position spread was added, a slide-off grain launched at 50 px/s traveled
≈25 pixels horizontally before stopping.  This created secondary piles far from the
main stream, making the pour look multi-source.

### Fixes

**Fix A: Add EMITTER_SPREAD (position jitter at spawn)**

Each grain now spawns at `(em->x + rand_int(-EMITTER_SPREAD, +EMITTER_SPREAD), em->y)`.
With `EMITTER_SPREAD = 3`, grains start at 7 possible x positions (317–323 for a
center emitter at 320).  No single pixel is overwhelmed; the pile grows uniformly
across the full nozzle width.

```c
int x_spread = (rand() % (2 * EMITTER_SPREAD + 1)) - EMITTER_SPREAD;
p->x[slot] = (float)(em->x + x_spread);
```

**Fix B: GRAIN_SLIDE_MIN 50 → 25 px/s**

Sliding grains now travel ≈12 pixels before stopping, staying close to the main
pile.  The settle check still works: a grain at 25 px/s moves 25×0.0167 = 0.42 px
per frame >> settle threshold of 0.17 px.

**Fix C: BRUSH_RADIUS 4 → 3 px**

Reduced from 4 to 3 pixels for a slimmer, less obtrusive player line.

### Changed Constants

| Constant | Before | After | Reason |
|----------|--------|-------|--------|
| `EMITTER_SPREAD` | — (not present) | 3 px | Eliminates single-pixel spawn congestion |
| `GRAIN_SLIDE_MIN` | 50 px/s | 25 px/s | Keeps slide-off grains near main pile |
| `BRUSH_RADIUS` | 4 px | 3 px | Thinner player lines |

### Lessons Learned

**18. Velocity jitter ≠ position jitter.**
Random velocity at birth (jitter) only spreads grains AFTER they start moving.
If all grains spawn at the same pixel, the velocity jitter is irrelevant for the
first collision — they all hit the same surface point and get routed to the same
3 slide positions.  Position jitter spreads the initial impact across several pixels
and is mandatory for a natural-looking pour.

**19. The "minimum slide speed" affects visual cohesion of the heap.**
A high minimum (50 px/s) forces sliding grains far from the pile, fragmenting the
stream into multiple satellite heaps.  A low minimum (25 px/s) keeps grains near
the pile, producing a cohesive heap.  The lower bound is set by the displacement
settle check: `minimum > GRAIN_SETTLE_SPEED` guarantees sliding grains are always
detected as "moving".

**20. Tune constants by watching the pile shape, not by inspection of the code.**
The pile's angle, cohesion, and stream shape are emergent properties of the
interaction between EMITTER_JITTER, EMITTER_SPREAD, GRAIN_SLIDE_MIN, and
GRAIN_SLIDE_REACH.  Small changes to any one constant visually reshape the
entire pour.  Always run the game and observe the pile before committing values.

---

## Issue 22 — Gravity Too Strong, Drops Too Fast, No Bounce Feel

### Symptoms
1. Grains fell very quickly — too fast to comfortably guide with drawn lines.
2. No "light sugar" feel — looked more like heavy ball bearings dropping.
3. Grains piled and immediately froze with no energy after landing.
4. Residual three-stream artifact still partly visible after Issue 21 fixes.

### Root Cause A: GRAVITY = 500 px/s² — too aggressive for the canvas size

At 500 px/s², a grain starting from rest at y=20 reaches the floor (y=480) in:

```
t = sqrt(2 × 460 / 500) ≈ 1.36 s
```

The original Sugar, Sugar game has a noticeably slower, lighter fall. The issue
is that 500 px/s² maps to a real-world gravitational constant scaled for a large
canvas (like 1280×960) — on a 640×480 canvas it looks too fast.

Fix: `GRAVITY` 500 → 280 px/s². Same canvas crossing time ≈ 1.8 s — noticeably
more relaxed.  `MAX_VY` also reduced from 600 → 400 px/s.

### Root Cause B: Spawn vy = 50 px/s — grains already moving fast at birth

Grains spawned with initial vy of 50 px/s, meaning they were already moving
nearly a third of terminal velocity at the spout. Combined with high gravity,
they reached the floor very quickly.

Fix: spawn vy 50 → 20 px/s. Grains start nearly from rest and build speed through
gravity alone, matching the original game's gentle trickle.

### Root Cause C: EMITTER_JITTER = ±8 px/s — not enough horizontal spread

With lighter gravity the fall takes longer, but the small ±8 px/s jitter still
produced a narrow pour. With 280 px/s² and 1.8 s fall time, ±8 px/s gives only
~14 px of horizontal spread by the floor. ±22 px/s gives ~40 px — a visible fan.

Fix: `EMITTER_JITTER` 8 → 22 px/s. `EMITTER_SPREAD` 3 → 5 px.

### Root Cause D: No coefficient of restitution — grains froze dead on impact

On vertical collision the code set `vy = 0.0f` immediately. This is physically
correct for inelastic collision but looks wrong for light dry sugar particles,
which bounce slightly before settling.

Fix: Added `GRAIN_BOUNCE = 0.25` and `GRAIN_BOUNCE_MIN = 20 px/s`.  On impact
with `|vy| > 20`:  `vy_after = -|vy| × 0.25`. After 1–2 bounces vy drops below
the threshold and is zeroed — grain settles normally.  The `GRAIN_BOUNCE_MIN`
threshold is critical: gravity re-accumulates ~4.7 px/s per frame (at 280 px/s²).
Without the minimum, every perfectly-blocked grain would bounce by 4.7 × 0.25
= 1.2 px/s perpetually and never bake.

### Changed Constants

| Constant | Before | After | Reason |
|----------|--------|-------|--------|
| `GRAVITY` | 500 px/s² | 280 px/s² | Lighter, more floaty feel |
| `MAX_VY` | 600 px/s | 400 px/s | Matches reduced gravity |
| Spawn `vy` | 50 px/s | 20 px/s | Grains start slow, accelerate naturally |
| `EMITTER_JITTER` | ±8 px/s | ±22 px/s | Wider fan to match lighter gravity fall |
| `EMITTER_SPREAD` | ±3 px | ±5 px | Wider nozzle for more distributed impact |
| `GRAIN_BOUNCE` | — | 0.25 | Lively light-bounce on surface impact |
| `GRAIN_BOUNCE_MIN` | — | 20 px/s | Threshold prevents perpetual micro-bounce |
| `GRAIN_HORIZ_DRAG` | 0.98 | 0.97 | More drag keeps slide-off grains near pile |
| `BRUSH_RADIUS` | 3 px | 2 px | Even slimmer player lines |

### Lessons Learned

**21. Scale gravity to the feel, not to real-world physics.**
Simulations frequently use non-physically-accurate gravity because the mapping
from real-world units to screen pixels depends on the target scale.  500 px/s²
feels "right" for an action game with fast projectiles but too heavy for a slow
puzzle about guiding flowing sugar.  Always calibrate by running and observing,
not by calculation alone.

**22. Spawn velocity matters as much as gravity for perceived speed.**
Even with reduced gravity, starting grains at 50 px/s means they bypass the
"slow start" phase entirely.  Setting spawn vy = 0 (or a small value like 20)
makes the trickle look natural — gravity builds speed gradually from the spout.

**23. Coefficient of restitution transforms "dead" particles into "live" ones.**
A COR of 0 (perfectly inelastic) is physically correct but perceptually flat.
Adding a small COR (0.2–0.3) with a minimum threshold produces organic behaviour:
fast impacts bounce noticeably, slow taps don't.  The minimum threshold is
essential — it prevents the perpetual micro-bounce that would otherwise make
every resting particle vibrate forever.
