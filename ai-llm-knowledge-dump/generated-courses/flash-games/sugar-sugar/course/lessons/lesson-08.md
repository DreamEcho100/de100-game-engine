# Lesson 08 — Color System: Filters and Colored Cups

**By the end of this lesson you will have:**
A level where white sugar pours from the emitter. A red-tinted band sits in the path. Grains passing through the band turn red. A red cup on the left accepts only red grains. A white cup on the right accepts only white grains. Both cups must be filled to win.

---

## Step 1 — `GRAIN_COLOR` enum: typed constants beat raw integers

In JavaScript you might write:

```js
const GRAIN_WHITE  = 0;
const GRAIN_RED    = 1;
const GRAIN_GREEN  = 2;
const GRAIN_ORANGE = 3;
```

In C, use an `enum` instead:

```c
// game.h — already written
typedef enum {
    GRAIN_WHITE  = 0,
    GRAIN_RED    = 1,
    GRAIN_GREEN  = 2,
    GRAIN_ORANGE = 3,
    GRAIN_COLOR_COUNT
} GRAIN_COLOR;
```

Why is this better than plain `int` constants?

1. The compiler knows `GRAIN_COLOR` is a restricted set. Pass `17` to a function expecting `GRAIN_COLOR` and you get a warning.
2. `GRAIN_COLOR_COUNT` always equals the number of enum members. Use it to size arrays — add a new color, the array grows automatically.
3. A debugger can show `GRAIN_RED` instead of `1`.

`GRAIN_COLOR_COUNT` is a common C idiom: put a sentinel at the end of an enum to count its members.

---

## Step 2 — `color[]` in `GrainPool`: one byte per grain

`GrainPool` uses Struct-of-Arrays layout (covered in Lesson 04). The color field is:

```c
uint8_t color[MAX_GRAINS]; // one byte per grain, value is a GRAIN_COLOR
```

`uint8_t` is an unsigned 8-bit integer (0–255). A `GRAIN_COLOR` value fits in one byte. Storing color separately from `x`, `y`, `vx`, `vy` means the physics loop never touches color memory — only the color-check loop does.

To assign a color when spawning a grain:

```c
// inside spawn_grains()
int slot = p->count++;
p->x[slot]      = (float)em->x;
p->y[slot]      = (float)em->y;
p->vx[slot]     = jitter;
p->vy[slot]     = 0.0f;
p->color[slot]  = GRAIN_WHITE;   // all grains start white
p->active[slot] = 1;
p->tpcd[slot]   = 0;
```

---

## Step 3 — `g_grain_colors[]`: `GRAIN_COLOR` → pixel color

Drawing a grain requires a `uint32_t` pixel color (0xAARRGGBB). A lookup table converts the enum to a pixel value in O(1):

```c
// game.c — already written
static const uint32_t g_grain_colors[GRAIN_COLOR_COUNT] = {
    [GRAIN_WHITE]  = COLOR_CREAM,    // 0xFFE8D5B7 — warm off-white
    [GRAIN_RED]    = COLOR_RED,      // 0xFFE53935
    [GRAIN_GREEN]  = COLOR_GREEN,    // 0xFF43A047
    [GRAIN_ORANGE] = COLOR_ORANGE,   // 0xFFFF8C00
};
```

Designated array initializers again: `[GRAIN_RED] = COLOR_RED` sets slot 1. Add a new enum member and a new entry — everything else is automatic.

Usage in `render_playing()`:

```c
for (int i = 0; i < p->count; i++) {
    if (!p->active[i]) continue;
    uint32_t col = g_grain_colors[p->color[i]];
    draw_pixel(bb, (int)p->x[i], (int)p->y[i], col);
}
```

`p->color[i]` is a `uint8_t` used as an array index. Valid because `GRAIN_COLOR_COUNT == 4` and the table has 4 slots.

---

## Step 4 — `ColorFilter` AABB check: change color, don't deactivate

A `ColorFilter` is a rectangle in the level:

```c
typedef struct {
    int         x, y, w, h;
    GRAIN_COLOR output_color;
} ColorFilter;
```

Each frame, `update_grains()` checks every active grain against every filter. The check is an **AABB** (Axis-Aligned Bounding Box) test — the same rectangle overlap check used for cups:

```c
// inside update_grains(), after moving the grain
int gx = (int)p->x[i];
int gy = (int)p->y[i];

for (int f = 0; f < lv->filter_count; f++) {
    ColorFilter *flt = &lv->filters[f];
    if (gx >= flt->x && gx < flt->x + flt->w &&
        gy >= flt->y && gy < flt->y + flt->h) {
        p->color[i] = flt->output_color;  // change color in-place
        // grain stays active — it keeps moving
    }
}
```

The grain is **not** removed. It continues through the filter and keeps falling. Only `color[i]` changes. This is different from the cup check, which deactivates the grain.

Concrete example with Level 9 filter `FILTER(60, 180, 100, 22, GRAIN_RED)`:
- A grain at `gx=100, gy=190`
- Check: `100 >= 60` ✓, `100 < 160` ✓, `190 >= 180` ✓, `190 < 202` ✓
- Result: `p->color[i] = GRAIN_RED`

---

## Step 5 — Cup color check: `GRAIN_WHITE` means "accept any"

A `Cup` has a `required_color` field:

```c
typedef struct {
    int         x, y, w, h;
    GRAIN_COLOR required_color; // GRAIN_WHITE = accept any
    int         required_count;
    int         collected;
} Cup;
```

The cup AABB check adds one extra condition:

```c
for (int c = 0; c < lv->cup_count; c++) {
    Cup *cup = &lv->cups[c];
    if (gx >= cup->x && gx < cup->x + cup->w &&
        gy >= cup->y && gy < cup->y + cup->h) {

        // color gate — GRAIN_WHITE means "I accept everything"
        int color_ok = (cup->required_color == GRAIN_WHITE) ||
                       (cup->required_color == p->color[i]);

        if (color_ok && cup->collected < cup->required_count) {
            cup->collected++;
            p->active[i] = 0;  // consume the grain
        }
        // wrong color: grain just falls through, stays active
    }
}
```

"Accept any" is encoded as `required_color == GRAIN_WHITE`. A level designer doesn't need a special `GRAIN_ANY` constant — they reuse `GRAIN_WHITE` since white grains haven't been transformed.

If the color is wrong the grain passes straight through the cup AABB and keeps falling. No special "reject" code is needed; the `if (color_ok)` branch simply doesn't execute.

---

## Step 6 — Level 9 definition: two cups, one filter

Open `src/levels.c` and find level 9:

```c
// levels.c — already written
[8] = {
    .index         = 8,
    .emitter_count = 1,
    .emitters      = { EMITTER(320, 20, 80) },
    .cup_count     = 2,
    .cups = {
        CUP( 60, 370, 85, 100, GRAIN_RED,   100),  // left: red only
        CUP(490, 370, 85, 100, GRAIN_WHITE,  100),  // right: any (white)
    },
    .filter_count = 1,
    .filters = { FILTER(60, 180, 100, 22, GRAIN_RED) },
},
```

The layout:

```
        [EMITTER at 320,20]
               |
               | white grains fall
               |
    [RED FILTER at 60,180]          ← 100px wide, 22px tall
    |||||||||||||||
         |                               |
    turns red                        still white
         |                               |
  [RED CUP 60,370]            [WHITE CUP 490,370]
```

The player must draw a line that splits the stream: some grains go left through the filter, some go right and miss it.

---

## Step 7 — `render_filters()`: semi-transparent colored bands

Filters are rendered as semi-transparent rectangles so the player can see through them:

```c
// game.c — render_playing()
static void render_filters(const GameState *state, GameBackbuffer *bb) {
    for (int f = 0; f < state->level.filter_count; f++) {
        ColorFilter *flt = &state->level.filters[f];

        // pick a visual color matching the output
        uint32_t col = g_grain_colors[flt->output_color];

        // alpha-blend at ~50% opacity so lines underneath show through
        draw_rect_blend(bb, flt->x, flt->y, flt->w, flt->h, col, 128);

        // solid border so the filter boundary is clearly visible
        draw_rect_outline(bb, flt->x, flt->y, flt->w, flt->h, col);
    }
}
```

`draw_rect_blend(bb, x, y, w, h, color, alpha)` blends each pixel:

```
out = (src * alpha + dst * (255 - alpha)) >> 8
```

With alpha = 128 (≈50%):
```
out = (src * 128 + dst * 127) >> 8
    ≈ (src + dst) / 2
```

This is the same formula as CSS `opacity: 0.5`, done manually per pixel.

---

## Step 8 — Multi-color levels: routing two streams

For levels with two emitters and two filters, the principle is identical — the level definition just adds more entries:

```c
// hypothetical level with two emitters, two filters, two colored cups
[XX] = {
    .emitter_count = 2,
    .emitters = {
        EMITTER(160, 20, 60),   // left emitter
        EMITTER(480, 20, 60),   // right emitter
    },
    .cup_count = 2,
    .cups = {
        CUP( 60, 370, 85, 100, GRAIN_RED,   100),
        CUP(490, 370, 85, 100, GRAIN_GREEN,  100),
    },
    .filter_count = 2,
    .filters = {
        FILTER( 60, 180, 100, 22, GRAIN_RED),
        FILTER(480, 180, 100, 22, GRAIN_GREEN),
    },
},
```

The `update_grains()` loop checks every grain against every filter on each frame. With 2 filters and 4096 possible grains that is 2 × 4096 = 8192 AABB tests per frame — trivial for a modern CPU.

---

## Build & Run

```bash
clang -Wall -Wextra -O2 -o sugar \
    src/main_x11.c src/game.c src/levels.c -lX11 -lm
./sugar
```

Navigate to level 9 from the title screen.

**What you should see:**
- White grains fall from the centre.
- A red-tinted horizontal band sits left of centre.
- Grains passing through the band turn red.
- The red cup (left) fills only when red grains fall in.
- The white cup (right) fills only when uncolored grains fall in.
- Level completes when both cups reach 100%.

---

## Key Concepts

- `GRAIN_COLOR` enum: a typed set of constants. `GRAIN_COLOR_COUNT` at the end gives the count automatically — use it to size lookup tables.
- `uint8_t color[MAX_GRAINS]`: one byte per grain in the SoA layout — cheap to read, never pollutes physics cache.
- `g_grain_colors[]`: a lookup table indexed by `GRAIN_COLOR`. O(1) conversion from enum to pixel color.
- **ColorFilter AABB**: same rectangle test as a cup, but changes `p->color[i]` instead of deactivating the grain.
- **Cup color gate**: `required_color == GRAIN_WHITE` accepts any grain color — "white" doubles as "unfiltered / accept all".
- `draw_rect_blend()`: alpha-blend formula `(src*a + dst*(255-a)) >> 8`. Use alpha 128 for ~50% transparency.
- Wrong-color grains fall through cups silently — no explicit rejection code; the `if (color_ok)` branch just doesn't run.

---

## Exercise

Add a second filter to level 9 at position `FILTER(480, 250, 100, 22, GRAIN_GREEN)` and change the right cup to `CUP(490, 370, 85, 100, GRAIN_GREEN, 100)`. Verify that now you must route one stream through the red filter and another through the green filter to win.
