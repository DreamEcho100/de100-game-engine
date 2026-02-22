# Lesson 04 — Rotation: Making Pieces Spin

---

## By the end of this lesson you will have:

A Tetris piece on screen that **rotates when you press Z**. You will press Z four times
and watch the piece cycle through all four orientations and return to the original.
The rotation works for every piece — the I-piece goes horizontal then vertical,
the T-piece shows all four distinct shapes.

---

## The Problem

Open `src/main_x11.c` (or `main_raylib.c` — they're identical for this logic).

Right now your draw loop contains something like this:

```c
for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
        int pi = py * 4 + px;   /* <-- this is the ONLY rotation: 0° */
        if (TETROMINOES[current_piece][pi] == 'X') {
            /* draw a cell at (piece_col + px, piece_row + py) */
        }
    }
}
```

That formula `pi = py * 4 + px` reads the piece string top-to-bottom,
left-to-right. That's 0° — no rotation. We have no way to rotate yet.

In Tetris, pressing rotate should spin the piece 90° clockwise. We need to
support 0°, 90°, 180°, and 270°.

**The question:** Do we store four different strings for each piece?

**The answer:** No. We store one string and compute a *different index* into
it depending on the rotation. The same 16 characters, read in a different order.

---

## Step 1 — Understand the Grid Numbering

Every piece is stored as a 16-character string in a 4×4 grid.
Let's number every cell 0–15 so we can talk about positions precisely.

At **0° rotation**, the cells are numbered like reading a book — left to right,
top to bottom:

```
 col:  0   1   2   3
row 0: [0] [1] [2] [3]
row 1: [4] [5] [6] [7]
row 2: [8] [9] [10][11]
row 3:[12] [13][14][15]
```

The formula `pi = py * 4 + px` gives you this numbering.
- Cell (col=0, row=0): `0 * 4 + 0 = 0` ✓
- Cell (col=2, row=1): `1 * 4 + 2 = 6` ✓
- Cell (col=3, row=3): `3 * 4 + 3 = 15` ✓

Good. Now — what happens when we rotate the piece 90° clockwise?

---

## Step 2 — The Physical Rotation Trick

**Do this for real.** Take a piece of paper. Draw this grid:

```
 0  1  2  3
 4  5  6  7
 8  9 10 11
12 13 14 15
```

Now physically rotate the paper **90° clockwise** (turn it so the left edge
becomes the bottom edge).

Look at where cell **0** ended up. It's now at the **bottom-left**.
Look at where cell **3** ended up. It's now at the **top-left**.
Look at where cell **12** ended up. It's now at the **bottom-right**.

After 90° clockwise rotation, the grid looks like this:

```
 col:  0   1   2   3
row 0:[12] [8] [4] [0]
row 1:[13] [9] [5] [1]
row 2:[14] [10][6] [2]
row 3:[15] [11][7] [3]
```

The **column** in the rotated grid corresponds to the **row** we read *from the bottom*
in the original. This is the key insight.

We want a formula: given `(px, py)` in the rotated grid, what index do I look
up in the original string?

Reading the table above:
- (px=0, py=0) → index 12
- (px=0, py=1) → index 13
- (px=1, py=0) → index 8
- (px=3, py=0) → index 0

Try to spot the pattern:
- index = 12 + py - (px * 4)

Check it:
- (px=0, py=0): 12 + 0 - 0 = **12** ✓
- (px=0, py=1): 12 + 1 - 0 = **13** ✓
- (px=1, py=0): 12 + 0 - 4 = **8** ✓
- (px=3, py=0): 12 + 0 - 12 = **0** ✓

**90° clockwise formula: `index = 12 + py - (px * 4)`**

---

## Step 3 — Derive All Four Rotations

Let's do the same for 180° and 270°. You can verify these yourself with paper.

### 180° (upside-down)

Rotating 90° twice. The grid becomes:

```
 col:  0   1   2   3
row 0:[15] [14][13][12]
row 1:[11] [10][9] [8]
row 2: [7] [6] [5] [4]
row 3: [3] [2] [1] [0]
```

The formula: **`index = 15 - (py * 4) - px`**

Check it:
- (px=0, py=0): 15 - 0 - 0 = **15** ✓
- (px=3, py=0): 15 - 0 - 3 = **12** ✓
- (px=0, py=3): 15 - 12 - 0 = **3** ✓

### 270° clockwise (= 90° counter-clockwise)

```
 col:  0   1   2   3
row 0: [3] [7] [11][15]
row 1: [2] [6] [10][14]
row 2: [1] [5] [9] [13]
row 3: [0] [4] [8] [12]
```

The formula: **`index = 3 - py + (px * 4)`**

Check it:
- (px=0, py=0): 3 - 0 + 0 = **3** ✓
- (px=1, py=0): 3 - 0 + 4 = **7** ✓
- (px=0, py=3): 3 - 3 + 0 = **0** ✓

### Summary

| Rotation | Formula                    |
|----------|----------------------------|
| 0°       | `py * 4 + px`              |
| 90° CW   | `12 + py - (px * 4)`       |
| 180°     | `15 - (py * 4) - px`       |
| 270° CW  | `3 - py + (px * 4)`        |

---

## Step 4 — Write the `tetris_rotate()` Function

Add this function **above** your draw code in `main_x11.c`
(and the same in `main_raylib.c`):

```c
int tetris_rotate(int px, int py, int r)
{
    switch (r % 4) {
        case 0: return py * 4 + px;
        case 1: return 12 + py - (px * 4);
        case 2: return 15 - (py * 4) - px;
        case 3: return 3 - py + (px * 4);
    }
    return 0; /* unreachable, but the compiler wants a return here */
}
```

**Why `r % 4`?**

`r` is a counter that goes up every time you press Z. It will be 0, 1, 2, 3,
then 4, 5, 6, 7, and so on. But we only have 4 cases.

`r % 4` means "remainder when dividing by 4":
- r=0 → 0 % 4 = **0**
- r=4 → 4 % 4 = **0** (back to start)
- r=5 → 5 % 4 = **1**
- r=7 → 7 % 4 = **3**

Without `% 4`, once r reaches 4 the switch would fall through all cases and
return 0 — your piece would always show rotation 0 after the first full cycle.

---

## Step 5 — Add a `rotation` Variable

Near the top of `main()`, find where you declared your piece position.
Add a `rotation` variable:

```c
int piece_col   = 5;    /* starting column */
int piece_row   = 0;    /* starting row    */
int current_piece = 0;  /* which tetromino */
int rotation    = 0;    /* current rotation: 0, 1, 2, or 3 */
```

---

## Step 6 — Hook Rotation to the Z Key

In your event-handling code, add a case for the `z` key.

**For X11** — inside your `XNextEvent` loop, in the `KeyPress` branch:

```c
KeySym key = XLookupKeysym(&event.xkey, 0);
if (key == XK_z || key == XK_Z) {
    rotation++;
}
```

**For Raylib** — in your input section:

```c
if (IsKeyPressed(KEY_Z)) {
    rotation++;
}
```

---

## Step 7 — Use `tetris_rotate()` When Drawing

Find the draw loop that reads from the TETROMINOES string.
Replace the old index formula with a call to `tetris_rotate()`:

**Before:**
```c
int pi = py * 4 + px;
```

**After:**
```c
int pi = tetris_rotate(px, py, rotation);
```

That's the entire change to the draw loop. Everything else stays the same.

The full draw loop now looks like:

```c
for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
        int pi = tetris_rotate(px, py, rotation);
        if (TETROMINOES[current_piece][pi] == 'X') {
            int screen_x = (piece_col + px) * CELL_SIZE;
            int screen_y = (piece_row + py) * CELL_SIZE;
            /* draw cell at screen_x, screen_y */
        }
    }
}
```

---

## Step 8 — Build & Run

**X11:**
```bash
./build_x11.sh && ./tetris_x11
```

**Raylib:**
```bash
./build_raylib.sh && ./tetris_raylib
```

**What to verify:**

1. The piece appears on screen at startup — same as before.
2. Press **Z** once. The piece changes shape.
3. Press **Z** again. Different shape.
4. Press **Z** again. Different shape.
5. Press **Z** a fourth time. Back to the original shape.

**Test with specific pieces.** Edit `current_piece = 0` to try different values:

- **Piece 0 (I-piece):** Should alternate between a horizontal bar (4 wide)
  and a vertical bar (4 tall).
- **Piece 1 (square/O-piece):** Should look the same all 4 rotations —
  a 2×2 block never changes when rotated.
- **Piece 4 (T-piece):** Should show 4 clearly different T shapes pointing
  up, right, down, and left.

If the piece doesn't rotate, check that you replaced `py * 4 + px` with
`tetris_rotate(px, py, rotation)` and that `rotation++` is in your key handler.

---

## Mental Model

> **Rotation doesn't mean storing 4 versions of the piece.**
> It means reading the same 16-character string in a different order.
>
> The formula answers: "If I'm looking at grid cell (px, py) in the *rotated*
> view, which character in the *original* string do I need?"
>
> All four formulas are just different ways of walking a 4×4 grid.

Think of it like reading the same book — but sometimes you hold it normally,
sometimes upside-down, sometimes sideways. The words don't change; your
reading path does.

---

## Common Confusions

**"Why doesn't `rotation` reset to 0 after 3?"**

It doesn't have to. `r % 4` handles any value of `r`. `rotation` can grow
forever (0, 1, 2, 3, 4, 5...) and `% 4` always maps it to 0–3. Simple and safe.

**"The I-piece looks wrong in one rotation."**

Check the TETROMINOES string for the I-piece. Some Tetris implementations
offset pieces inside the 4×4 box. The rotation formula is correct; the visual
offset depends on where the 'X' cells are inside the string.

**"Can I rotate counter-clockwise?"**

Yes — use `rotation--` instead of `rotation++`. Since we're using `% 4` and C
handles negative modulo, you might want `rotation = (rotation + 3) % 4` for
counter-clockwise (adds 3 which is the same as subtracting 1 in mod-4 math).

---

## Exercise

Add **left and right arrow key** movement.

Add two more variables if you don't have them:

```c
int piece_col = 5;
int piece_row = 0;
```

Then in the key handler:
- Left arrow → `piece_col--`
- Right arrow → `piece_col++`

**For X11:**
```c
if (key == XK_Left)  piece_col--;
if (key == XK_Right) piece_col++;
```

**For Raylib:**
```c
if (IsKeyPressed(KEY_LEFT))  piece_col--;
if (IsKeyPressed(KEY_RIGHT)) piece_col++;
```

No collision detection yet — the piece can walk straight off the screen.
That's completely fine. The goal is to see movement working.

In the next lesson we'll split the code into proper files before adding more
game logic.
