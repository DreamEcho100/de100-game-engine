## Recurring Build Issues & Mitigation Rules

These rules are distilled from real bugs encountered across course builds. Every rule exists because the mistake was made at least once. When generating course code, check each rule before finalizing.

---

### Rendering & Visual Constants

#### Document bit-endianness in every bitmap font header

Before writing a single line of font rendering code, add a comment to the font data file:

```c
/* Bit 0 = left pixel, Bit 7 = right pixel (BIT0-left convention) */
```

The renderer's bitmask must match. If the font is BIT0-left, use `(1 << col)`. If BIT7-left, use `(0x80 >> col)`. **Test with the letter 'N' first** — its diagonal runs top-left → bottom-right. If it runs the other way, the mask direction is wrong.

#### Never guess visual constants — sample from reference

Before writing any `#define COLOR_*`, `#define CANVAS_W`, or sizing constant, open screenshots of the original game and use a color picker. A 30-second verification prevents multi-hour "it still looks wrong" cycles. This applies to:

- Background color
- UI element colors
- Text colors and sizes
- Canvas dimensions
- Particle sizes

#### Design for multiple text sizes from day one

Always implement `draw_text_scaled(bb, x, y, str, color, scale)` alongside `draw_text`. A bitmap font only needs a `scale` integer to render at any size — each pixel becomes an `N×N` block. Retrofitting scaling after the UI is "done" means touching every call site.

#### Separate visual size from collision geometry

A particle can be rendered as a 2×2 block while its collision footprint remains 1×1 in the physics bitmap. Never change collision geometry to adjust visual appearance — change only the rendering pass. This applies to any entity where "how it looks" and "how it collides" can differ.

---

### Layout & UI

#### Never duplicate layout numbers between renderer and input handler

The moment you type the same pixel coordinate in two places, you create a latent mismatch bug. Extract every position, size, and gap into a single named constant (`#define`, `static const int`, or `enum`) used by both `render_*()` and `update_*()`:

```c
#define BTN_X  10
#define BTN_Y  (CANVAS_H - 38)
#define BTN_W  70
#define BTN_H  28
/* Both render_hud() and update_playing() use these — cannot drift apart */
```

#### Zone your canvas at design time

Split the canvas into logical regions (playfield, HUD strip, menu area) and define their boundaries as named constants. Enforce zones in input handlers — if a region has interactive UI elements, it must be off-limits for all other mouse actions:

```c
int over_ui = (input->mouse.y >= UI_ZONE_TOP);
if (input->mouse.left.ended_down && !over_ui) {
    /* only process gameplay clicks outside the UI zone */
}
```

#### Every rendered interactive element needs a click handler on day one

A button drawn without a corresponding click handler is a visual lie. When you render an interactive element, immediately ask: "where does the click handler live?" If the answer is "nowhere yet," add a `// TODO: wire click handler` comment and implement it before moving to the next feature.

#### Center variable-width text by computing actual width

Never use a fixed pixel offset to center text. Always compute the rendered width from the character count:

```c
int text_w = char_count * (FONT_W + 1) * scale;
int tx = container_x + (container_w - text_w) / 2;
```

A fixed offset will be wrong for any string length except the one you tested.

---

### Platform Layer

#### Explicitly set all configurable platform parameters

Never rely on implicit defaults that "happen to work" on your development machine. Set byte order, pixel format, and every other configurable value explicitly:

```c
g_ximage->byte_order = LSBFirst;  /* one line — prevents a bug on different hardware */
```

Platform code must be portable-by-construction, not portable-by-luck.

#### Always cap the frame rate — even with delta-time physics

A delta-time game loop does NOT mean "any frame rate is fine." At uncapped speeds (5,000+ fps), `dt` becomes so small that per-frame thresholds (displacement checks, timer increments, accumulator steps) break down due to floating-point precision. Add both a floor and a ceiling:

```c
/* Cap dt to prevent spiral-of-death (low fps) and micro-dt issues (high fps) */
if (delta_time > 0.1f) delta_time = 0.1f;   /* floor: max 100ms per frame */

/* Sleep to cap at ~60 fps — prevents micro-dt and saves CPU */
double elapsed = platform_get_time() - curr_time;
if (elapsed < 1.0/60.0) {
    struct timespec ts = {0, (long)((1.0/60.0 - elapsed) * 1e9)};
    nanosleep(&ts, NULL);
}
```

#### Snapshot input prev-state once per frame, not once per event

A frame-based game updates the world once per frame. Any "diff" between current and previous state (`prev_x/prev_y`, button transitions) must capture the state at the START of the frame. Updating `prev` inside the event loop makes it a per-event diff — only the last event's delta survives:

```c
void platform_get_input(GameInput *input) {
    /* BEFORE event loop: snapshot frame-start state */
    input->mouse.prev_x = input->mouse.x;
    input->mouse.prev_y = input->mouse.y;

    while (events_pending()) {
        /* INSIDE event loop: only update current position */
        input->mouse.x = event.x;
        input->mouse.y = event.y;
    }
    /* AFTER: prev = frame start, current = frame end — full path covered */
}
```

This is critical for line-drawing (Bresenham from prev to current) and drag gestures. Without it, fast mouse movement produces gaps.

#### If a UI element shows a keyboard hint, wire the key up

"Press ENTER to continue" promises Enter works. Verify that:

1. The key field exists in `GameInput`
2. The platform layer maps the physical key to that field
3. The update function actually checks the field

Audit all hint text against actual input bindings before shipping a lesson.

---

### Particle Systems

These rules apply to any course that implements a particle system (sand, sugar, snow, sparks, fluid, etc.).

#### Use bake-and-free for settled particles — never throttle spawning

When a particle comes to rest permanently, write it into the static collision bitmap and free its pool slot. This is the **bake-and-free pattern**:

> When a particle's purpose as a dynamic object is over, write it into the static world and free its dynamic slot.

A "throttle" that stops the emitter when the pool is full treats the symptom (pool full) not the cause (slots never freed). The emitter should be infinite; the pool should recycle.

```c
/* Bake: write settled grain into static bitmap */
lb->pixels[by * W + bx] = encode_color(grain_color);
/* Free: return slot to pool */
p->active[i] = 0;
```

#### Encode semantic information in bitmap values

A `uint8_t` bitmap cell should encode what KIND of solid it is, not just whether it's solid:

```c
/*  0       = empty
 *  1       = obstacle / wall (stamped at level load)
 *  2..N    = baked particle, color index = value - 2
 *  255     = player-drawn line
 */
```

Physics (`IS_SOLID`) only needs "zero or not." Rendering needs "what kind of solid?" A single byte with range checks serves both consumers without a second parallel array.

#### Use displacement-based settle detection, not velocity-based

Velocity-based settle checks (`|vx| < threshold && |vy| < threshold`) fail when gravity is applied every frame — it re-adds non-zero velocity before the check runs, preventing the counter from ever accumulating.

Use position displacement instead:

```c
float dx = p->x[i] - pre_x;
float dy = p->y[i] - pre_y;
float dist_sq = dx*dx + dy*dy;
/* Scale threshold by dt to be frame-rate independent */
float settle_sq = GRAIN_SETTLE_SPEED * GRAIN_SETTLE_SPEED * dt * dt;
if (dist_sq < settle_sq) p->still[i]++;
else                      p->still[i] = 0;
```

This directly measures "did the particle actually go anywhere?" regardless of what forces ran.

#### Make all per-frame thresholds frame-rate independent

Any fixed per-frame threshold (distance, timer increment, accumulator) is implicitly frame-rate dependent. The correct pattern:

```c
/* WRONG: breaks at high/low fps */
if (dist < 0.2f)

/* RIGHT: works at any fps */
if (dist < SPEED_THRESHOLD * dt)
```

Test at 30 fps, 60 fps, and uncapped to verify behavior is consistent.

#### Never apply an artificial velocity minimum at collision points

A minimum like `if (spd < 60) spd = 60` applied at every collision creates a perpetual-bounce attractor: the particle can never escape it because every attempt to come to rest re-energizes it. Minimums should only be applied at spawn or at a single acceleration step — never repeatedly at every collision.

If grains stall on slopes, use a minimum only in the "free diagonal found" branch — not unconditionally at every surface contact.

#### Distinguish "fresh fall" from "surface riding" in collision response

When a particle hits a surface, check whether it arrived from a genuine fall or is just re-contacting the surface after one frame of gravity:

```c
float vy_abs = fabsf(p->vy[i]);
if (vy_abs >= SURFACE_VY_THRESHOLD) {
    /* Fresh impact: transfer fraction of |vy| to slide speed */
    spd = vy_abs * SLIDE_TRANSFER;
} else {
    /* Surface riding: preserve existing horizontal momentum */
    spd = fabsf(p->vx[i]);
}
```

Without this distinction, the slide formula replaces accumulated horizontal momentum with a tiny value derived from one frame of gravity (~8 px/s), instantly killing all sliding motion.

#### Use partial velocity transfer with three tunable knobs

100% velocity transfer on collision is physically "correct" but perceptually wrong for most games. Use three constants tuned together:

| Knob             | Controls                                | Typical range |
| ---------------- | --------------------------------------- | ------------- |
| `SLIDE_TRANSFER` | Fraction of \|vy\| → vx on fresh impact | 0.5–0.8       |
| `MAX_VX`         | Hard ceiling on horizontal speed        | 100–250 px/s  |
| `HORIZ_DRAG`     | Per-frame decay multiplier              | 0.93–0.98     |

**Tune by drawing a 45° ramp** and watching where grains stop. Changing one knob in isolation produces a different imbalance — always adjust all three together.

#### Always apply horizontal drag

Any particle system with horizontal velocity needs per-frame drag. Without it, particles slide indefinitely on flat surfaces. A simple multiplicative decay is sufficient:

```c
p->vx[i] *= HORIZ_DRAG;  /* e.g., 0.97 per frame */
```

#### Add a coefficient of restitution with a minimum threshold

Perfectly inelastic collision (`vy = 0` on impact) looks dead and flat. A small bounce adds life:

```c
if (fabsf(p->vy[i]) > BOUNCE_MIN) {
    p->vy[i] = -fabsf(p->vy[i]) * BOUNCE_COEFF;  /* e.g., 0.2–0.3 */
} else {
    p->vy[i] = 0.0f;  /* below threshold: stop completely */
}
```

The `BOUNCE_MIN` threshold is critical — without it, gravity re-accumulates a tiny vy each frame, which bounces perpetually at sub-pixel scale.

#### Use both position jitter AND velocity jitter for natural spawning

- **Velocity jitter** (`EMITTER_JITTER`): spreads particles AFTER they start moving — creates a fan shape during flight
- **Position jitter** (`EMITTER_SPREAD`): spreads particles AT birth — prevents all particles from spawning at the same pixel and colliding immediately

Both are needed. Without position jitter, all particles hit the same surface point and route to the same 2–3 slide destinations, creating discrete streams instead of a smooth pour.

#### Control angle of repose via diagonal check radius

In a cellular-automaton particle system, the angle of repose equals `atan(1/R)` where R is the diagonal slide check radius:

| R (pixels checked) | Angle of repose                     |
| ------------------ | ----------------------------------- |
| 1                  | 45° (too steep — looks like a wall) |
| 2                  | 27° (natural for sand/sugar)        |
| 3                  | 18° (very flat spread)              |

When checking beyond ±1, add a **path-clear guard** — verify all intermediate pixels are free before allowing the slide.

#### Separate physical containers from absorption zones

A cup/container needs two independent definitions:

- **Physical walls**: solid pixels stamped into the collision bitmap (left, right, bottom walls; top left open)
- **Absorption zone**: a slightly smaller AABB inside the walls where particles are collected

The ±1 pixel offset between them decouples physics from game logic. When the container is full, incoming particles hit the solid walls, pile up, and overflow — enabling emergent gameplay.

#### Clear derived per-frame data at the point of removal

When removing a particle (absorbed, destroyed, baked), immediately clear it from all derived data structures (occupancy bitmaps, spatial hashes, etc.). Do not rely on "the next full rebuild will fix it" — other particles processed later in the same frame see stale data:

```c
s_occ[gy * W + gx] = 0;   /* clear immediately — not at next frame's rebuild */
p->active[i] = 0;
```

#### Scale gravity to feel, not to real-world physics

There is no correct mapping from real-world m/s² to px/s². The "right" gravity depends on canvas size, particle type, and desired pacing:

| Game feel             | Gravity range | Example                 |
| --------------------- | ------------- | ----------------------- |
| Heavy/fast (action)   | 400–600 px/s² | Projectiles, explosions |
| Medium (platformer)   | 250–400 px/s² | Player character, coins |
| Light/floaty (puzzle) | 150–280 px/s² | Sugar, sand, snow       |

Always calibrate by running the game and watching — not by calculation. Also tune spawn velocity alongside gravity: a high spawn vy bypasses the "slow start" phase and makes the pour look unnatural regardless of gravity.

#### Tune particle constants as a system, not individually

The following constants form an interconnected system. Changing one in isolation produces a different imbalance:

```
GRAVITY ←→ MAX_VY ←→ SPAWN_VY
    ↕           ↕
SLIDE_TRANSFER ←→ MAX_VX ←→ HORIZ_DRAG
    ↕                           ↕
EMITTER_JITTER ←→ EMITTER_SPREAD
    ↕
GRAIN_SLIDE_REACH ←→ GRAIN_SLIDE_MIN
    ↕
BOUNCE_COEFF ←→ BOUNCE_MIN
    ↕
SETTLE_SPEED ←→ SETTLE_FRAMES
```

The tuning method:

1. Set gravity first — watch raw freefall speed
2. Set spawn vy — watch the pour start
3. Draw a 45° ramp — tune slide transfer, max vx, and drag by watching where grains stop
4. Watch the pile form — tune jitter, spread, and slide reach by watching pile shape
5. Watch grains land on flat ground — tune bounce and settle by watching how quickly they come to rest
6. Run for 30+ seconds — verify the pool doesn't exhaust and the stream stays smooth

Never commit constants without running this full sequence.

---

### General Debugging Patterns

#### Test asymmetric glyphs to catch rendering bugs

When implementing any bitmap font renderer, render the letter **'N'** first. Its diagonal runs top-left → bottom-right. If it runs the other way, your bitmask direction is wrong. 'R', 'F', 'P', and '7' also expose left/right asymmetry bugs.

#### Draw debug bounding boxes for all interactive elements

When a click doesn't register or registers in the wrong place, draw the hit-test rectangle as a colored outline on top of the render. The mismatch between visual bounds and input bounds becomes immediately visible. Remove the debug overlay before shipping the lesson, but mention it as a debugging technique.

#### Audit every UI hint against actual input bindings

Before finalizing any lesson, grep for all user-visible strings that mention keys or mouse actions ("Press ENTER", "Click to continue", "R to reset"). For each one, verify:

1. The `GameInput` struct has a field for that key
2. The platform layer maps the physical key to that field
3. The update function checks the field and performs the promised action

A hint that doesn't work is worse than no hint — it teaches the student that the game is broken.

---

### Summary Checklist (Pre-Submission)

Before finalizing any course's source code, run through this checklist:

- [ ] All color constants verified against reference screenshots _(ask for it)_
- [ ] Font header documents bit-endianness; renderer bitmask matches
- [ ] `draw_text_scaled` exists and is used for headings and HUD
- [ ] All layout coordinates are named constants shared by render and update
- [ ] Canvas is zoned; input handlers enforce zone boundaries
- [ ] Every rendered button has a corresponding click handler
- [ ] Platform layer explicitly sets byte order, pixel format, etc.
- [ ] Frame rate is capped (sleep-based) AND dt is clamped (spiral-of-death guard)
- [ ] `prev_x`/`prev_y` are set once before the event loop, not inside it
- [ ] All UI hint text has corresponding wired-up input bindings
- [ ] Particle pool uses bake-and-free (if particles can come to rest)
- [ ] Bitmap values encode semantic type, not just boolean solid/empty
- [ ] Settle detection is displacement-based AND frame-rate independent
- [ ] No artificial velocity minimums at collision points
- [ ] Collision response distinguishes fresh impact from surface riding
- [ ] Velocity transfer is partial (3-knob system: transfer, cap, drag)
- [ ] Horizontal drag is applied every frame
- [ ] Bounce has a minimum-speed threshold to prevent perpetual micro-bounce
- [ ] Emitter uses both position spread AND velocity jitter
- [ ] Diagonal slide reach > 1 for natural angle of repose (if applicable)
- [ ] Container walls are stamped as physics solids; absorption is a smaller inner AABB
- [ ] Derived per-frame structures are cleared at point of particle removal
- [ ] Gravity and spawn velocity tuned by observation, not calculation
- [ ] Full constant system tested together (45° ramp test, 30-second run test)
- [ ] Game tested at both capped (60 fps) and uncapped frame rates
