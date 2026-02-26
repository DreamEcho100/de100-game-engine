# Lesson 05 — Vec2 + 2D Rotation Matrix

## By the end of this lesson you will have:
A complete understanding of *why* the two lines at the heart of `draw_wireframe` rotate a point correctly. You will be able to verify the formula by hand and understand where `cos` and `sin` come from, rather than treating the rotation matrix as a magic incantation.

---

## Step 1 — The unit circle

Draw a circle of radius 1 centered at the origin. Pick any angle `θ` measured counterclockwise from the positive x-axis. The point on the circle at that angle has coordinates:

```
x = cos(θ)
y = sin(θ)
```

Let's verify with three exact angles:

```
θ = 0°    (pointing right)
  cos(0°) = 1,  sin(0°) = 0   → point (1, 0)   ✓ right

θ = 90°   (pointing up in math coords, DOWN in y-down screen coords)
  cos(90°) = 0, sin(90°) = 1  → point (0, 1)   ✓ top of circle

θ = 180°  (pointing left)
  cos(180°) = -1, sin(180°) = 0 → point (-1, 0) ✓ left
```

This is the *definition* of cosine and sine — they are the x and y coordinates of a unit-circle point at angle θ.

---

## Step 2 — Rotating the x-axis unit vector

Start with the vector that points along the x-axis: `{1, 0}`. What point does it map to after rotating by angle `r`?

By the unit circle: `{cos(r), sin(r)}`.

So a rotation by `r` sends `{1, 0}` → `{cos(r), sin(r)}`.

---

## Step 3 — Rotating the y-axis unit vector

Now start with `{0, 1}` (the y-axis). After rotating by `r`:

By the unit circle, the y-axis is 90° ahead of the x-axis. Rotating it by `r` puts it at angle `90° + r`.

Using the angle-addition identities:
```
cos(90° + r) = -sin(r)
sin(90° + r) =  cos(r)
```

So a rotation by `r` sends `{0, 1}` → `{-sin(r), cos(r)}`.

---

## Step 4 — Rotating any vector

Any vector `{x, y}` can be written as:

```
{x, y} = x * {1, 0}  +  y * {0, 1}
```

It's `x` copies of the x-unit-vector plus `y` copies of the y-unit-vector. After rotating both:

```
x * {cos(r), sin(r)}  +  y * {-sin(r), cos(r)}
= {x*cos(r) - y*sin(r),  x*sin(r) + y*cos(r)}
```

This gives the 2D rotation formula:

```
x' = x*cos(r) - y*sin(r)
y' = x*sin(r) + y*cos(r)
```

**Worked example — rotating {0, -5} by 90° (π/2 radians):**

This is the ship's nose vertex, currently pointing upward (y = -5 in y-down screen coordinates). Rotating by 90° should point it to the right.

```
r = π/2
cos(π/2) = 0
sin(π/2) = 1

x' = 0 * 0   - (-5) * 1  = 0 + 5  = 5.0
y' = 0 * 1   + (-5) * 0  = 0 + 0  = 0.0

Result: {5.0, 0.0}  → pointing right ✓
```

**Worked example — rotating {-2.5, +2.5} (ship's bottom-left vertex) by 90°:**

```
x' = (-2.5)*0 - (2.5)*1  = 0 - 2.5  = -2.5
y' = (-2.5)*1 + (2.5)*0  = -2.5 + 0 = -2.5

Result: {-2.5, -2.5}  → moved to upper-left ✓
```

After rotating all three ship vertices by 90°, the triangle now points right instead of up.

---

## Step 5 — The code that does this

In `draw_wireframe`:

```c
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
```

`rx = model[i].x * c - model[i].y * s` — this is `x' = x*cos - y*sin`.  
`ry = model[i].x * s + model[i].y * c` — this is `y' = x*sin + y*cos`.

Exactly the formula derived in Step 4.

---

## Step 6 — Why compute c and s once, before the loop

`cosf` and `sinf` are transcendental functions. On a modern CPU they take approximately 20–50 clock cycles each, versus 1–4 cycles for multiply/add. For a 20-vertex asteroid:

```
Inside the loop:  2 trig calls × 20 verts = 40 trig calls per asteroid
Outside the loop: 2 trig calls total, regardless of vertex count
```

With 7 asteroids on screen at once, that would be 280 trig calls vs 14. The difference is:

```
280 × 35 cycles = 9,800 cycles  (inside)
 14 × 35 cycles =   490 cycles  (outside)
```

At 3 GHz, 9,310 extra cycles is 3 microseconds — negligible today, but Bresenham made the same argument in 1965. The principle matters: **do expensive work outside loops whenever the result doesn't change per iteration**.

**JS analogy:**
```js
// Bad — Math.cos called 20 times:
for (let i = 0; i < verts.length; i++) {
    const rx = verts[i].x * Math.cos(angle) - verts[i].y * Math.sin(angle);
}

// Good — computed once:
const c = Math.cos(angle), s = Math.sin(angle);
for (let i = 0; i < verts.length; i++) {
    const rx = verts[i].x * c - verts[i].y * s;
}
```

---

## Step 7 — The Vec2 type

```c
typedef struct { float x, y; } Vec2;
```

This is the entire Vec2 definition. It's a struct with two `float` fields named `x` and `y`.

**JS analogy:** In TypeScript: `type Vec2 = { x: number; y: number }`.

In C, you can initialize a struct literal inline:
```c
state->ship_model[0] = (Vec2){  0.0f, -5.0f };
```

This is called a **compound literal** — it creates a temporary `Vec2` and assigns it. The `(Vec2)` cast tells the compiler the type of the brace-initializer.

**JS analogy:** `{ x: 0.0, y: -5.0 }` — same idea, but in C you must name the type.

The `f` suffix on `0.0f` and `-5.0f` is important: without it, `0.0` is a `double` (64-bit float). `0.0f` is a `float` (32-bit float). Since `Vec2::x` is `float`, mixing in a `double` would silently promote and then truncate. The `f` suffix keeps everything consistently 32-bit.

---

## Build & Run

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11 -lX11 -lxkbcommon -lGL -lGLX -lm
./build/asteroids_x11
```

**What you should see:** The ship rotates correctly when you press left/right arrows — the nose of the triangle always points in the direction the ship is heading. The rotation formula from this lesson is what makes that work.

---

## Key Concepts

- `cos(θ)` and `sin(θ)` are the x and y coordinates of a point on the unit circle at angle θ.
- Rotation formula: `x' = x*cos(r) - y*sin(r)`, `y' = x*sin(r) + y*cos(r)`.
- Derive it by: rotating the two basis vectors `{1,0}` and `{0,1}` and combining linearly.
- **Compute `c = cosf(angle)`, `s = sinf(angle)` once, before the vertex loop.** Trig is expensive; the values don't change per vertex.
- `Vec2` is `typedef struct { float x, y; } Vec2;` — initialized with compound literals `(Vec2){ x, y }`.
- `0.0f` not `0.0` — the `f` suffix gives a `float` literal, matching the struct field type.
- y-down screen coordinates: angle 0 points upward (negative y), angle π/2 points right (positive x).

## Exercise

Manually compute the rotation of all three ship vertices when `angle = π` (180°, pointing down):

```
cos(π) = -1,  sin(π) = 0

Vertex 0: { 0.0, -5.0 }
  x' = 0.0*(-1) - (-5.0)*0  = 0.0
  y' = 0.0*0    + (-5.0)*(-1) = 5.0
  Result: { 0.0, 5.0 }  → now pointing DOWN ✓

Vertex 1: { -2.5, 2.5 }
Vertex 2: { +2.5, 2.5 }
```

Compute vertices 1 and 2 yourself. Then verify by running the game, rotating the ship exactly 180° (watch the score HUD and keep rotating), and confirming the nose points down.
