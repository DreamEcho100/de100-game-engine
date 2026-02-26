# Lesson 04 — draw_wireframe

## By the end of this lesson you will have:
The white triangle ship and yellow jagged asteroid circles visible on screen. `draw_wireframe` takes a model (an array of points defined around the origin) and transforms each point — rotate, then scale, then translate — before connecting them with lines. The asteroid slowly rotates as it drifts across the screen.

---

## Step 1 — What a "model" is

A model is an array of `Vec2` points defined in **model space** — coordinates relative to the object's own center, with the center at (0,0). The ship model has three vertices:

```c
state->ship_model[0] = (Vec2){  0.0f, -5.0f };   /* tip: straight up */
state->ship_model[1] = (Vec2){ -2.5f, +2.5f };   /* bottom-left     */
state->ship_model[2] = (Vec2){ +2.5f, +2.5f };   /* bottom-right    */
```

These numbers are in "model units" — small values near the origin. The ship is about 5 units tall and 5 units wide. We multiply by `SHIP_SCALE = 10.0f` when drawing to make it 50 pixels tall.

**JS analogy:** A `Vec2` model is like an SVG `<path>` defined relative to its own bounding box origin. The transformation pipeline that follows is like SVG's `transform="rotate(45) scale(10) translate(400,300)"`.

---

## Step 2 — The three-step transformation pipeline

To draw the ship at world position (400, 300), rotated 45°, scaled by 10, we apply transformations in this order:

```
1. Rotate the model around the origin (changes direction the ship faces)
2. Scale  (change from model units to pixel size)
3. Translate (move from origin to world position)
```

**Why this order?** Because rotation and scaling work correctly around the **origin**. If you translate first (moving the center to (400,300)) and then rotate, the rotation would spin the model around (0,0), not around the ship's center.

Worked example with ship tip vertex `{0, -5}`, angle=0.5 rad, scale=10, world pos (400, 300):

```
Step 1 — Rotate by 0.5 radians:
  cos(0.5) ≈ 0.8776,  sin(0.5) ≈ 0.4794
  rx = 0.0 * 0.8776 - (-5.0) * 0.4794 = 0 + 2.397 = 2.397
  ry = 0.0 * 0.4794 + (-5.0) * 0.8776 = 0 - 4.388 = -4.388

Step 2 — Scale by 10:
  rx = 2.397 * 10 = 23.97
  ry = -4.388 * 10 = -43.88

Step 3 — Translate to (400, 300):
  tx = 23.97 + 400 = 423.97  ≈ pixel x=423
  ty = -43.88 + 300 = 256.12 ≈ pixel y=256
```

So the ship tip ends up at pixel (423, 256) when the ship is rotated 0.5 rad and sitting at screen center.

---

## Step 3 — The exact code

```c
static void draw_wireframe(AsteroidsBackbuffer *bb,
                           const Vec2 *model, int n_verts,
                           float ox, float oy,       /* world position */
                           float angle, float scale, /* orientation + size */
                           uint32_t color)
{
    Vec2 t[64];
    float c = cosf(angle), s = sinf(angle);

    for (int i = 0; i < n_verts; i++) {
        /* Step 1: rotate */
        float rx = model[i].x * c - model[i].y * s;
        float ry = model[i].x * s + model[i].y * c;
        /* Step 2: scale */
        rx *= scale;
        ry *= scale;
        /* Step 3: translate to world position */
        t[i].x = rx + ox;
        t[i].y = ry + oy;
    }

    /* Draw closed polygon: connect each vertex to the next, and last to first */
    for (int i = 0; i < n_verts; i++) {
        int j = (i + 1) % n_verts;
        draw_line(bb,
                  (int)t[i].x, (int)t[i].y,
                  (int)t[j].x, (int)t[j].y,
                  color);
    }
}
```

**`float c = cosf(angle), s = sinf(angle);` — before the loop:**  
`cosf` and `sinf` are expensive (~20–50 clock cycles). With 20 asteroid vertices and 3 ship vertices, calling them inside the loop would waste ~460–1100 extra cycles per frame for no reason. We compute them once.

**`Vec2 t[64]` — stack-allocated transformed vertex buffer:**  
`ASTEROID_VERTS = 20`, `SHIP_VERTS = 3` — both well under 64. This array lives on the stack (no `malloc`). It's temporary: filled during the transform loop, consumed during the draw loop, then forgotten when `draw_wireframe` returns.

---

## Step 4 — The closed polygon loop: `(i + 1) % n_verts`

```c
for (int i = 0; i < n_verts; i++) {
    int j = (i + 1) % n_verts;
    draw_line(bb, (int)t[i].x, (int)t[i].y,
                  (int)t[j].x, (int)t[j].y, color);
}
```

Let's trace it for the ship with 3 vertices (indices 0, 1, 2):

```
i=0: j = (0+1) % 3 = 1 → draw line from t[0] to t[1]  (tip to bottom-left)
i=1: j = (1+1) % 3 = 2 → draw line from t[1] to t[2]  (bottom-left to bottom-right)
i=2: j = (2+1) % 3 = 0 → draw line from t[2] to t[0]  (bottom-right back to tip)
```

The `% n_verts` wraps the last index back to 0, closing the polygon. Without it, you'd draw `n-1` edges and leave a gap between the last vertex and the first.

**JS analogy:**
```js
for (let i = 0; i < verts.length; i++) {
    const j = (i + 1) % verts.length;
    ctx.moveTo(verts[i].x, verts[i].y);
    ctx.lineTo(verts[j].x, verts[j].y);
}
```
Exactly the same pattern.

---

## Step 5 — Drawing the ship and asteroids with draw_wireframe

In `asteroids_render`:

```c
/* Draw ship */
draw_wireframe(bb,
               state->ship_model, SHIP_VERTS,
               state->player.x, state->player.y,
               state->player.angle, SHIP_SCALE,
               COLOR_WHITE);

/* Draw asteroids */
for (i = 0; i < state->asteroid_count; i++) {
    const SpaceObject *a = &state->asteroids[i];
    if (!a->active) continue;
    draw_wireframe(bb,
                   state->asteroid_model, ASTEROID_VERTS,
                   a->x, a->y, a->angle, (float)a->size,
                   COLOR_YELLOW);
}
```

For the ship: `scale = SHIP_SCALE = 10.0f`, so model vertices (max 5 units) become up to 50 pixels.  
For asteroids: `scale = a->size` (16, 32, or 64), so the same unit-circle model renders at 3 different sizes.

This is the key reuse: **one model, many sizes**. The same 20-vertex jagged circle is drawn at LARGE_SIZE=64, MEDIUM_SIZE=32, and SMALL_SIZE=16 just by passing a different scale value.

---

## Step 6 — Building the asteroid model in asteroids_init

```c
for (int i = 0; i < ASTEROID_VERTS; i++) {
    float noise = (float)rand() / (float)RAND_MAX * 0.4f + 0.8f;
    float t     = ((float)i / (float)ASTEROID_VERTS) * 2.0f * 3.14159265f;
    state->asteroid_model[i].x = noise * sinf(t);
    state->asteroid_model[i].y = noise * cosf(t);
}
```

For vertex `i=0` of 20:
```
t = (0/20) * 2π = 0 radians
noise ∈ [0.8, 1.2]   (example: 0.95)
x = 0.95 * sin(0)  = 0.95 * 0.0   = 0.0
y = 0.95 * cos(0)  = 0.95 * 1.0   = 0.95
```

For vertex `i=5` of 20:
```
t = (5/20) * 2π = 0.5π = 1.5708 radians
noise = 1.1  (example)
x = 1.1 * sin(π/2) = 1.1 * 1.0 = 1.1
y = 1.1 * cos(π/2) = 1.1 * 0.0 = 0.0
```

The 20 vertices form a rough circle of radius ~1, jagged because each `noise` value is random. When scaled by `a->size`, the radius becomes approximately `a->size` pixels.

**Why built once in `asteroids_init` and not per asteroid?** All asteroids share the same shape — we only need one model. If we regenerated it per asteroid, every asteroid would have the same random shape (seeded at the same time) or we'd need a per-asteroid model array. Sharing one model costs nothing and is simpler.

---

## Build & Run

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11 -lX11 -lxkbcommon -lGL -lGLX -lm
./build/asteroids_x11
```

**What you should see:** A white triangle (ship) in the center of the screen, and two large yellow jagged circles (asteroids) slowly rotating and drifting. Use arrow keys to rotate and thrust the ship. The shapes remain crisp as they move.

---

## Key Concepts

- A model is an array of `Vec2` points defined around the origin in model units.
- Transform order is always: **Rotate → Scale → Translate**. Doing it in the wrong order gives incorrect results.
- `cosf`/`sinf` are computed once before the vertex loop to avoid 20× repeated calls.
- `(i + 1) % n_verts` closes the polygon by connecting the last vertex back to the first.
- One asteroid model, three sizes: `scale = a->size` (16, 32, or 64) reuses the same 20 vertices.
- Stack-allocated `Vec2 t[64]` — no heap allocation for temporary transformed vertices.

## Exercise

Change the ship model to a 4-vertex diamond shape. Add a fourth vertex at `{0.0f, 2.5f}` (bottom-center point) and set `SHIP_VERTS` to 4 in `asteroids.h`. You will need to change:

```c
#define SHIP_VERTS  4

state->ship_model[0] = (Vec2){  0.0f, -5.0f };
state->ship_model[1] = (Vec2){ -2.5f,  0.0f };
state->ship_model[2] = (Vec2){  0.0f, +2.5f };
state->ship_model[3] = (Vec2){ +2.5f,  0.0f };
```

Rebuild and confirm the diamond shape appears. Then revert `SHIP_VERTS` to 3 and restore the original triangle vertices before moving on.
