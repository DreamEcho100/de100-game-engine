# PLAN.md — Sugar, Sugar (Flash Game Reconstruction)

> Reconstruction target: **Sugar, Sugar** by Bart Bonte (2011)
> Architecture: C + custom engine, backbuffer pipeline, X11 + Raylib backends
> Course style: Handmade Hero approach — zero magic, everything explained from scratch

---

## 1. Historical Overview

| Field           | Detail                                                                                       |
|-----------------|----------------------------------------------------------------------------------------------|
| Developer       | Bart Bonte (independent, Belgium)                                                            |
| Release Year    | 2011                                                                                         |
| Original Platform | Adobe Flash (ActionScript 3)                                                               |
| Host Sites      | Newgrounds, Kongregate, Armor Games, Coolmath Games                                         |
| Sequels         | Sugar, Sugar 2 (adds more mechanics); Sugar, Sugar 3                                        |
| Mobile          | Remastered HTML5/mobile versions exist on Poki, ABCya                                       |
| Status          | Flash EOL (December 2020) — original no longer playable without emulation (Ruffle)          |

### What the Game Does

Sugar, Sugar is a physics-based puzzle game. A stream of sugar particles falls from a fixed emitter at the top of each level. The player draws freehand lines with the mouse. Those lines act as ramps and barriers, redirecting sugar into cups. Each cup requires a specific amount of sugar (and optionally a specific color) to be filled. Fill all cups → advance to the next level.

30 levels. Freeplay mode unlocks on completion.

---

## 2. Mechanics Decomposition

### 2.1 Core Gameplay Loop

```
FRAME START
  ↓
Spawn new sugar grain at emitter position (fixed rate, e.g. 2–3 per frame)
  ↓
For each active grain:
  Apply gravity (or anti-gravity if flipped)
  Check collision with: drawn lines | cup rects | color filters | teleporters | screen edges
  Update position or resolve contact
  Check cup membership → increment cup counter
  ↓
Draw backbuffer:
  Clear to background color
  Draw level obstacles (static text/shapes)
  Draw player-drawn lines
  Draw cups (with fill progress)
  Draw color filters
  Draw teleporters
  Draw all active grains
  Draw UI (level number, reset button, gravity button if present)
  ↓
FRAME END → display backbuffer
```

**Win condition:** All cups in the current level have `collected >= required`.
**Fail condition:** None (no time limit, no lives). Player hits Reset to restart.
**Progress:** Completing a level advances `current_level`. After level 30 → freeplay.

---

### 2.2 Input System

| Action              | Device  | Type       | Notes                                              |
|---------------------|---------|------------|----------------------------------------------------|
| Draw line           | Mouse   | Continuous | MouseMove while LButton held; writes to line bitmap |
| Erase all lines     | Button  | Discrete   | "Reset" button → clears line bitmap, resets grains |
| Flip gravity        | Button  | Discrete   | "Gravity" toggle button (levels that have it)       |
| Menu navigation     | Mouse   | Discrete   | Hover + click on level buttons                      |
| Quit/Back           | Keyboard | Discrete  | Escape → title / pause                              |

**Mouse state tracking (per frame):**

```c
typedef struct {
    int x, y;                    /* current pixel position */
    int prev_x, prev_y;          /* last frame position */
    GameButtonState left_button;
    int is_drawing;              /* 1 while LButton held over canvas */
} MouseInput;
```

**Brush:** When `left_button.ended_down && is_drawing`, rasterize a thick line segment from `(prev_x, prev_y)` to `(x, y)` into the `line_bitmap`. Brush radius ~3px.

**No keyboard input during gameplay** beyond Escape. All interaction is mouse-driven.

---

### 2.3 Game State Machine

#### Global Game Phases

```c
typedef enum {
    PHASE_TITLE,          /* Title/menu screen */
    PHASE_PLAYING,        /* Active puzzle — particles falling, drawing enabled */
    PHASE_LEVEL_COMPLETE, /* All cups filled — brief celebration then advance */
    PHASE_FREEPLAY,       /* Post-completion sandbox */

    PHASE_COUNT
} GAME_PHASE;
```

#### Playing Sub-Phases

```c
typedef enum {
    PLAY_RUNNING,         /* Normal simulation tick */
    PLAY_PAUSED,          /* (Optional) Escape pressed */

    PLAY_COUNT
} PLAY_PHASE;
```

#### Transition Table

| From                | Event                          | To                   |
|---------------------|--------------------------------|----------------------|
| `PHASE_TITLE`       | Player clicks level N          | `PHASE_PLAYING`      |
| `PHASE_PLAYING`     | All cups filled                | `PHASE_LEVEL_COMPLETE` |
| `PHASE_PLAYING`     | Reset clicked                  | `PHASE_PLAYING` (reinit level) |
| `PHASE_PLAYING`     | Escape pressed                 | `PHASE_TITLE`        |
| `PHASE_LEVEL_COMPLETE` | Celebration timer expires   | `PHASE_PLAYING` (next level) OR `PHASE_FREEPLAY` |
| `PHASE_FREEPLAY`    | Escape pressed                 | `PHASE_TITLE`        |

#### Guard Conditions

- Drawing is **disabled** in `PHASE_TITLE`, `PHASE_LEVEL_COMPLETE`, `PHASE_FREEPLAY`
- Gravity flip button only active in levels that define `has_gravity_switch = 1`
- Teleporters only active in levels that define teleporter pairs

---

### 2.4 Entity System

#### Sugar Grain (Particle)

Each grain is a value type. Stored as **Struct of Arrays** for cache efficiency.

```c
#define MAX_GRAINS 4096

typedef struct {
    float x[MAX_GRAINS];
    float y[MAX_GRAINS];
    float vx[MAX_GRAINS];
    float vy[MAX_GRAINS];
    uint8_t color[MAX_GRAINS];    /* GRAIN_WHITE, GRAIN_RED, GRAIN_GREEN, GRAIN_ORANGE */
    uint8_t active[MAX_GRAINS];
    int count;
} GrainPool;
```

Why SoA? When iterating all grains for gravity update, only `x`, `y`, `vx`, `vy` are needed — the CPU loads only those arrays, not unrelated fields. Cache miss count drops significantly compared to Array-of-Structs.

#### Color Enum

```c
typedef enum {
    GRAIN_WHITE  = 0,
    GRAIN_RED    = 1,
    GRAIN_GREEN  = 2,
    GRAIN_ORANGE = 3,
    GRAIN_COLOR_COUNT
} GRAIN_COLOR;
```

#### Static Level Objects

```c
typedef struct {
    int x, y, w, h;
    GRAIN_COLOR required_color;
    int required_count;
    int collected;
} Cup;

typedef struct {
    int x, y, w, h;
    GRAIN_COLOR output_color;
} ColorFilter;

typedef struct {
    int ax, ay;   /* entry portal center */
    int bx, by;   /* exit portal center */
    int radius;
} Teleporter;

typedef struct {
    int x, y;     /* emitter pixel position */
    int grains_per_frame;
} Emitter;
```

#### Line Bitmap (Player-Drawn Lines)

A single-channel `uint8_t` bitmap, same dimensions as the game canvas (e.g. 640×480).
Pixel value `0` = empty. Pixel value `255` = solid line. Used for per-pixel collision.

```c
typedef struct {
    uint8_t *pixels;  /* width * height bytes, heap-allocated once */
    int width;
    int height;
} LineBitmap;
```

Drawn using Bresenham circle stamps along mouse path segments.

#### Level Definition

```c
#define MAX_CUPS       8
#define MAX_FILTERS    4
#define MAX_TELEPORTERS 2
#define MAX_EMITTERS   2

typedef struct {
    int          level_index;
    Emitter      emitters[MAX_EMITTERS];
    int          emitter_count;
    Cup          cups[MAX_CUPS];
    int          cup_count;
    ColorFilter  filters[MAX_FILTERS];
    int          filter_count;
    Teleporter   teleporters[MAX_TELEPORTERS];
    int          teleporter_count;
    int          has_gravity_switch;
    int          is_cyclic;          /* sugar wraps bottom→top */
} LevelDef;
```

All 30 levels are defined as a static `LevelDef levels[30]` array in `game.c` — no file I/O at runtime.

---

## 3. Mathematics & Physics

### 3.1 Coordinate System

- Origin `(0, 0)` = **top-left**
- X increases rightward
- Y increases downward (screen-space convention)
- Canvas: **640 × 480** pixels (matches original Flash dimensions)

### 3.2 Unit Scaling

- 1 unit = 1 pixel
- All positions are `float` internally for sub-pixel accumulation
- Rendering snaps to nearest integer pixel

### 3.3 Particle Integration (Semi-Implicit Euler)

```
// Per frame, per grain:
vy += GRAVITY * dt;          // apply gravity (or -GRAVITY if flipped)
x  += vx * dt;
y  += vy * dt;
```

`GRAVITY = 600.0f` pixels/sec² (tuned to feel like original).

**Why Semi-Implicit Euler?** It is energy-stable for simple gravity — grains don't accelerate indefinitely. Full explicit Euler would drift. RK4 is overkill for this use case.

### 3.4 Collision Detection

**Grain vs Line Bitmap:** Per-pixel test at the grain's next position.

```
next_x = (int)(x + vx * dt);
next_y = (int)(y + vy * dt);
if (line_bitmap[next_y * width + next_x] != 0):
    resolve collision
```

Collision resolution for lines:
1. Try sliding left or right along the line surface (check `(next_x±1, next_y)`)
2. If neither is free → grain comes to rest (zero velocity)
3. Resting grains accumulate; when enough pile up, they can overflow the edge

**Grain vs Cup:** AABB test.
```c
if (x >= cup.x && x < cup.x + cup.w &&
    y >= cup.y && y < cup.y + cup.h) {
    if (grain.color == cup.required_color || cup.required_color == GRAIN_WHITE) {
        cup.collected++;
    }
    deactivate grain;
}
```

**Grain vs Color Filter:** AABB test. On collision, change `grain.color` to `filter.output_color`, do NOT deactivate.

**Grain vs Teleporter:** Circle test (`dist(grain, portal_a) < radius`). On entry, teleport to `portal_b` position, preserve velocity direction, maintain color.

**Screen edges:**
- Left/Right: bounce (`vx = -vx * 0.5f`) or clamp
- Bottom: if `is_cyclic` → wrap to top; else → deactivate (grain lost)
- Top: clamp (grains can't go above the canvas)

### 3.5 Spatial Partitioning

Not needed for 4096 grains against a bitmap. Per-pixel collision is O(1) per grain.
If grain count is raised significantly (>16K), consider a spatial grid.

---

## 4. Rendering Architecture

### 4.1 Draw Order (Back-to-Front)

1. Background fill (white `0xFFFFFFFF`)
2. Level static shapes / level title text (embedded as pixel data or simple geometry)
3. Color filter zones (colored rectangles, semi-transparent)
4. Teleporter portals (colored circles)
5. Cups (outlines + fill-progress bar)
6. Player-drawn lines (read from `line_bitmap`, draw colored pixels)
7. Sugar grains (one pixel per grain, color-mapped)
8. UI overlay: level number, reset button, gravity button, "LEVEL COMPLETE" flash

### 4.2 Camera

- No camera. Static 640×480 canvas. No scrolling.

### 4.3 Grain Rendering

Each grain = 1 pixel in the backbuffer at `(round(x), round(y))`.

```c
uint32_t grain_color_table[GRAIN_COLOR_COUNT] = {
    [GRAIN_WHITE]  = 0xFFE8D5B7,  /* off-white / cream */
    [GRAIN_RED]    = 0xFFE53935,
    [GRAIN_GREEN]  = 0xFF43A047,
    [GRAIN_ORANGE] = 0xFFFB8C00,
};
```

### 4.4 Line Rendering

When drawing lines (player-drawn), the brush rasterizes a filled circle at each mouse position into `line_bitmap`, then draws every set pixel in `line_bitmap` as a dark gray pixel `0xFF444444` in the backbuffer.

### 4.5 Text / UI Font

Use a small embedded 8×8 bitmap font (public domain, e.g. the classic PC-8 font) stored as a static `uint8_t font_data[128][8]` in `game.h`. `draw_text()` blits characters into the backbuffer — no external font library.

---

## 5. Audio System

Sugar, Sugar uses simple, minimal audio.

| Sound          | Trigger                              | Loop  |
|----------------|--------------------------------------|-------|
| Background music | Level load                         | Yes   |
| Sugar hiss/trickle | Grains actively falling          | Yes (ambient) |
| Cup fill ding  | A cup reaches 100% fill              | No    |
| Level complete | All cups filled                      | No    |

### Implementation Plan

- Use a minimal platform-layer audio callback: `platform_play_sound(SOUND_ID id)` and `platform_play_music(MUSIC_ID id)`.
- Sounds are raw PCM WAV data embedded as `uint8_t` arrays in a `sounds.h` header.
- Platform layer (X11: ALSA or PulseAudio simple API; Raylib: `PlaySound()`).
- Audio is a **stretch goal** — core mechanics first.

```c
typedef enum {
    SOUND_CUP_FILL,
    SOUND_LEVEL_COMPLETE,
    SOUND_COUNT
} SOUND_ID;

typedef enum {
    MUSIC_GAMEPLAY,
    MUSIC_COUNT
} MUSIC_ID;
```

---

## 6. Asset Inventory

### 6.1 Required Assets

| Category       | Asset                          | Format       | Source Plan                                    |
|----------------|--------------------------------|--------------|------------------------------------------------|
| Font           | 8×8 bitmap font                | C array      | Embed public-domain PC font (no file needed)   |
| Colors         | All defined as `uint32_t` constants | —        | Defined in `game.h`                            |
| Level data     | 30 level definitions           | C struct array | Hand-authored in `levels.c` from gameplay video |
| Background music | Ambient/chill loop            | WAV/OGG      | CC0 source (e.g. freesound.org) or generated   |
| SFX: cup fill  | Short chime                    | WAV          | CC0 source                                     |
| SFX: level complete | Completion fanfare          | WAV          | CC0 source                                     |

### 6.2 Acquiring Assets

**Sounds:** Download CC0 sounds from freesound.org. Convert to raw 16-bit PCM WAV with:
```
ffmpeg -i input.mp3 -ar 44100 -ac 1 -f s16le output.raw
```
Then embed as a C array using `xxd -i output.raw > sound_name.h`.

**Font:** Use the public-domain [Terminus font](https://files.ax86.net/terminus-ttf/) bitmapped at 8×8, or the classic CP437 font from `8x8crafft` (no licensing issues). Store as:
```c
static const uint8_t font8x8[128][8] = { ... };
```

**No external image files needed.** All visual elements (cups, filters, portals) are drawn procedurally using `draw_rect` and `draw_circle` in `game_render`.

---

## 7. Recommended Reconstruction Stack

**Recommendation: C + Custom Engine (X11 + Raylib backends)**

| Option            | Pros                                   | Cons                                   |
|-------------------|----------------------------------------|----------------------------------------|
| **C + Custom**    | Full control, matches course goals     | More work                              |
| C + SDL           | Cross-platform, simpler audio          | Extra dependency, hides platform layer |
| Godot             | Fast to prototype                      | Wrong for low-level learning goals     |
| Unity             | Wrong domain entirely                  | Way too high-level                     |
| WebGL + Canvas    | Easy deployment                        | JS, not C                              |

**Justification:** The entire point of this course series is to build a custom engine from scratch in C, learning memory management, backbuffer rendering, and data-oriented design. Sugar, Sugar's mechanics are perfectly sized: complex enough to learn from, simple enough to complete in ~10 lessons. The C custom engine approach integrates seamlessly with existing `platform.h` / `game.h` contract from earlier courses.

---

## 8. Performance Considerations

### Memory Layout

- `GrainPool` as SoA → `x[]`, `y[]`, `vx[]`, `vy[]` processed in tight loops — cache-friendly
- `LineBitmap` as flat `uint8_t[]` — 640×480 = 307,200 bytes (~300 KB) — fits in L2 cache
- Backbuffer as flat `uint32_t[]` — 640×480 × 4 = 1.2 MB — fits in L3 cache
- Level definition is fully static — no heap after init

### Hot Path (per frame)

```
game_update():
  Spawn grains         — O(grains_per_frame)
  Update all grains    — O(MAX_GRAINS) = O(4096), single pass
  Grain vs bitmap      — O(1) per grain (direct array index, no search)
  Grain vs cups        — O(cup_count) = O(8) per grain

game_render():
  Clear backbuffer     — 1 memset() call
  Draw line_bitmap     — 307,200 byte scan
  Draw grains          — O(MAX_GRAINS), 1 pixel write each
  Draw UI              — O(constant)
```

Total per frame: predictable, bounded, zero dynamic allocation.

### Branch Prediction

The grain update loop is a tight `for` + integer compare (line_bitmap lookup). Branch predictor trains quickly. The `active[]` flag check could be refactored to a compacted active-grain list if profiling shows benefit.

### State Transition Cost

Negligible. `change_phase()` writes one enum value + one float.

---

## 9. Development Roadmap

### Phase 1 — Skeleton (Window + Backbuffer)

**Deliverables:**
- `main_x11.c` / `main_raylib.c` with `platform.h` contract
- `game.h` with `GameBackbuffer`, `GameState`, `GameInput` stubs
- `game.c` with `game_update()` / `game_render()` that fills backbuffer with solid color
- `build_x11.sh` / `build_raylib.sh`

**Test checkpoint:** Window opens, background renders as white rectangle, closes cleanly.

**Risk:** None. This is a pure scaffolding step.

---

### Phase 2 — Input & Mouse Drawing

**Deliverables:**
- `MouseInput` tracking (position, left button state, `is_drawing` flag)
- `LineBitmap` allocated on heap (640×480), zeroed on reset
- Bresenham circle stamp along mouse drag path → writes to `line_bitmap`
- `game_render` draws all non-zero `line_bitmap` pixels as dark gray

**Test checkpoint:** Player can draw dark lines on white canvas with mouse. Lines persist until reset.

**Risk:** Bresenham line interpolation gap when mouse moves fast → mitigate by filling intermediate points at sub-pixel spacing.

---

### Phase 3 — Single Grain Physics

**Deliverables:**
- `GrainPool` (SoA, MAX_GRAINS=4096)
- Emitter spawning 2 grains/frame at fixed point
- Semi-implicit Euler integration with gravity
- Grain rendered as 1 cream-colored pixel
- Screen-edge deactivation (fall off bottom → deactivate)

**Test checkpoint:** Grains fall from emitter, spread across canvas, fall off bottom and disappear.

**Risk:** Floating-point precision — grains should not tunnel through the line bitmap at high speed. Cap `vy` at terminal velocity `~600px/s`.

---

### Phase 4 — Grain vs Line Collision

**Deliverables:**
- Per-pixel collision test: check `line_bitmap[next_y * W + next_x]`
- Slide resolution: try `(±1, 0)` before resting
- Grains pile up on drawn lines

**Test checkpoint:** Draw a slope → grains slide down it. Draw a bowl → grains fill it.

**Risk:** Grains "tunneling" when `vy` is large (>1 px/frame). Mitigate with sub-step integration:
```c
// Split dt into N sub-steps if vy > 1.0f
int steps = (int)(fabsf(vy) * dt) + 1;
float sub_dt = dt / steps;
```

---

### Phase 5 — Cups & Level Definition

**Deliverables:**
- `Cup` structs rendered as labeled rectangles
- Cup fill counting: grain enters cup AABB → `collected++`, deactivate grain
- Level win condition: all cups `collected >= required`
- `PHASE_LEVEL_COMPLETE` transition → brief flash → load next level
- 5 hand-authored `LevelDef` entries (enough to test the loop)

**Test checkpoint:** Fill a cup → counter decrements. Fill all cups → "LEVEL COMPLETE" shown → next level loads.

**Risk:** Cup AABB must be large enough to catch grains that approach at angle. Use generous hitbox (2–4px wider than drawn rect).

---

### Phase 6 — Color Filters

**Deliverables:**
- `ColorFilter` structs drawn as colored translucent rectangles
- Grain enters filter AABB → `grain.color` changed → grain continues falling (not deactivated)
- Cups with `required_color != GRAIN_WHITE` only count matching colors

**Test checkpoint:** Route white grain through red filter → grain turns red → red cup fills. White cup ignores red grain.

**Risk:** Grains passing through filter too fast (thin filter, high velocity). Make filter height at least 10px.

---

### Phase 7 — Special Mechanics

**Deliverables:**
- **Gravity Switch:** Toggle button → flip `gravity_sign` (±1) → `vy += GRAVITY * gravity_sign * dt`
- **Cyclic Screen:** If `is_cyclic` → grain exiting bottom teleports to top with same `vx`/`vy`
- **Teleporters:** Circle overlap test → teleport grain from portal A to portal B

**Test checkpoint (gravity):** Grains fall upward when flipped. Drawing inverted ramps catches them.
**Test checkpoint (cyclic):** Grain falls off bottom → reappears at top.
**Test checkpoint (teleporter):** Grain enters portal → exits at second portal location.

**Risk:** Teleporter can cause infinite loops if exit portal is inside another teleporter. Guard with a `teleport_cooldown` flag per grain (1 frame immunity after teleport).

---

### Phase 8 — All 30 Levels

**Deliverables:**
- Complete `LevelDef levels[30]` array
- Title screen with level select grid (30 buttons)
- Level progression state (`unlocked_up_to` saved in `GameState`)
- Freeplay mode after level 30

**Test checkpoint:** All 30 levels load, cups are solvable. Level select shows correctly.

**Risk:** Level data accuracy. Original is closed-source. Reproduce from gameplay footage + manual testing.

---

### Phase 9 — Rendering Polish

**Deliverables:**
- Smooth cup fill progress bar (visual percentage)
- Level title text rendered with bitmap font
- "LEVEL COMPLETE" celebration overlay (grain confetti burst)
- Gravity flip button visual (arrow icon drawn procedurally)
- Teleporter portal pulsing animation (phase_timer-driven radius oscillation)

**Test checkpoint:** Game is visually recognizable as Sugar, Sugar.

---

### Phase 10 — Audio

**Deliverables:**
- `platform_play_sound()` / `platform_play_music()` in both backends
- Embed CC0 sounds as C arrays
- Cup fill chime, level complete fanfare, ambient background loop

**Test checkpoint:** Sounds play on cup fill and level completion. Music loops during gameplay.

---

## 10. Risk Assessment

| Risk                          | Likelihood | Impact | Mitigation                                                    |
|-------------------------------|------------|--------|---------------------------------------------------------------|
| Grain tunneling through lines | Medium     | High   | Sub-step integration, cap terminal velocity                   |
| Level data inaccuracy         | High       | Medium | Reproduce from YouTube gameplay footage; approximate is fine  |
| Grain count performance       | Low        | Medium | SoA layout + 4096 cap is well within CPU budget               |
| Audio complexity              | Medium     | Low    | Defer audio to Phase 10; core game works without it           |
| Teleporter infinite loop      | Low        | High   | Per-grain cooldown flag (1 frame immunity after teleport)     |
| Mouse drawing gaps at speed   | Medium     | Medium | Interpolate intermediate stamp positions along drag path      |
| Flash physics accuracy        | Medium     | Low    | Approximate physics feel is sufficient — not a clone contest  |

---

## 11. Testing Strategy

### State Transition Tests (debug builds)

- `validate_transition(from, to)` asserts fire on illegal transitions
- Example: `PHASE_PLAYING → PHASE_TITLE` (Escape) is legal; `PHASE_TITLE → PHASE_LEVEL_COMPLETE` is illegal

### Collision Edge Cases

- **Grain lands exactly on line pixel boundary** — test: brush draws a horizontal line; verify grains rest on it at all x positions
- **Grain in corner** (two lines meeting) — must not clip through; verify rest-on-contact
- **Cup boundary** — grain spawned directly above cup; verify counted on first frame of contact

### Deterministic Replay Test

- Seed grain spawner with fixed random seed
- Record `(frame, mouse_event)` sequence
- Replay → verify identical grain positions at frame N
- Needed to confirm no per-frame floating-point divergence

### Performance Profiling Plan

- Instrument `game_update()` and `game_render()` with `platform_get_time()` delta
- Target: both functions combined < 2ms at 4096 active grains (leaves 14ms headroom at 60fps)
- If grain update exceeds budget: profile per-section with timestamps, optimize inner loop

---

## 12. Proposed Lesson Sequence

| Lesson | What Gets Built                                          | Student Sees                                           |
|--------|----------------------------------------------------------|--------------------------------------------------------|
| 01     | Window + backbuffer pipeline, white canvas               | White window, no crash                                 |
| 02     | Mouse input tracking, line drawing into bitmap           | Draw lines with mouse, persist on canvas               |
| 03     | Single grain spawning + gravity + screen-edge death      | Grain falls from top, disappears at bottom             |
| 04     | Grain vs line collision + slide resolution               | Grain slides down drawn ramp, piles up in bowl         |
| 05     | `GrainPool` (SoA upgrade), 4096 grains, emitter stream   | Sugar stream pours, fills bowl                         |
| 06     | Cup structs, fill counting, win condition                | Fill cup → counter hits 0 → "LEVEL COMPLETE"           |
| 07     | Level definition struct, 5 levels, level progression     | 5 puzzles playable end-to-end                          |
| 08     | Color system: grain color, color filters, colored cups   | Route through red filter → fills red cup only          |
| 09     | Special mechanics: gravity switch, cyclic, teleporters   | Flip gravity, wrap-around screen, teleport grains      |
| 10     | All 30 levels, title screen, level select                | Full game playable                                     |
| 11     | Rendering polish: font, progress bars, animations        | Visually resembles original                            |
| 12     | Audio: platform sound API, CC0 sounds embedded           | Dings on cup fill, music loop during play              |

---

## 13. File Structure

```
ai-llm-knowledge-dump/flash-games/sugar-sugar/
├── PLAN.md                    ← this file
└── course/
    ├── build_x11.sh
    ├── build_raylib.sh
    └── src/
        ├── platform.h         ← 4-function contract
        ├── game.h             ← types, structs, enums, public API
        ├── game.c             ← game_update(), game_render(), all logic
        ├── levels.c           ← static LevelDef levels[30] array
        ├── font.h             ← embedded 8×8 bitmap font
        ├── sounds.h           ← embedded CC0 PCM sound arrays (Phase 10)
        ├── main_x11.c         ← X11 + OpenGL platform backend
        └── main_raylib.c      ← Raylib platform backend
    └── lessons/
        ├── lesson-01.md
        ├── lesson-02.md
        ├── ...
        └── lesson-12.md
```

---

## 14. Key Design Decisions

### Why pixel-bitmap collision (not polygon collision)?

Sugar, Sugar's drawn lines are freehand, arbitrary-shape strokes. Storing them as polygon segments would require dynamic memory and complex intersection math. A flat `uint8_t[640*480]` bitmap gives O(1) collision per grain — just an array index — and is trivially reset by `memset(0)`. The entire bitmap is ~300KB, well within L2 cache on any modern CPU.

### Why SoA for GrainPool?

The grain update loop touches only `x`, `y`, `vx`, `vy`, `active` per grain. With Array-of-Structs, each grain's color and other fields would pad the cache line, evicting useful data. SoA ensures the CPU loads only the fields needed for the update pass.

### Why C arrays for levels (not a file format)?

No runtime file I/O, no parser, no failure modes. The level data is small (~30 structs × ~100 bytes = 3KB). Compile-time constants are zero-cost to access and impossible to corrupt at runtime.

### FSM instead of flag soup

Without explicit phases, the codebase would contain guards like:
- `if (all_cups_filled && celebration_timer > 0)` scattered across `game_update`
- `if (!is_playing && !is_title)` to prevent drawing during wrong states

A single `switch (state->phase)` with `change_phase()` makes illegal state combinations impossible to express. `validate_transition()` in debug builds catches bugs at the moment of introduction, not hours later.

---

## 15. Platform Contract (from platform.h)

```c
/* Platform provides these 4 functions. Game calls them. */
void   platform_init(const char *title, int width, int height);
double platform_get_time(void);          /* monotonic clock, seconds */
void   platform_get_input(GameInput *input);
void   platform_display_backbuffer(const GameBackbuffer *backbuffer);

/* Extensions needed for Sugar, Sugar (added to contract): */
void   platform_play_sound(int sound_id);  /* non-blocking, fire-and-forget */
void   platform_play_music(int music_id);  /* loops until replaced */
void   platform_stop_music(void);
```

Platforms that don't support audio implement `platform_play_sound` / `platform_play_music` as empty stubs. The game compiles and runs silently — audio is an enhancement, not a dependency.

---

*End of PLAN.md*
