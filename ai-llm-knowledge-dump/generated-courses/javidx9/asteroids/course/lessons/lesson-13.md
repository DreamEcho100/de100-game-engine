# Lesson 13 — Level Progression + Score + Bitmap Font HUD

## By the end of this lesson you will have:
Infinite waves: when all asteroids are destroyed, the game awards a 1000-point bonus and immediately spawns two fresh large asteroids 90° to the left and right of the player, flying toward where the player is pointing. The score is displayed in the top-left corner in white text at scale 2.

---

## Step 1 — Wave-clear detection

**Why:** We need to know when the player has destroyed every asteroid. The simplest signal is `asteroid_count == 0` after `compact_pool` has run. At this point in `asteroids_update`, all dead asteroids have been removed and all newly spawned children (from splits earlier in the same frame) have been added. An empty pool means the wave is genuinely over.

```c
if (state->asteroid_count == 0) {
    state->score += 1000; /* bonus for clearing the wave */
    /* ... spawn next wave ... */
}
```

This runs at the very end of `asteroids_update`, after collision detection and `compact_pool`. The `PHASE_DEAD` check at the top of the function means this code is unreachable during a death animation — no false "wave cleared" triggers.

---

## Step 2 — Spawning the next wave: 90° offset from player heading

**Why 90°?** Spawning directly in front of the player would be instantly fatal. Spawning exactly behind would be unfair in the other direction. Placing asteroids 90° to the left and right gives the player time to react while keeping the threat close enough to feel exciting.

```c
float pa   = state->player.angle;
float px   = state->player.x;
float py   = state->player.y;
float left_x  = 150.0f * sinf(pa - 3.14159265f / 2.0f) + px;
float left_y  = 150.0f * cosf(pa - 3.14159265f / 2.0f) + py;
float right_x = 150.0f * sinf(pa + 3.14159265f / 2.0f) + px;
float right_y = 150.0f * cosf(pa + 3.14159265f / 2.0f) + py;
add_asteroid(state, left_x,  left_y,
             50.0f * sinf(pa),  50.0f * cosf(pa),  LARGE_SIZE);
add_asteroid(state, right_x, right_y,
             50.0f * sinf(-pa), 50.0f * cosf(-pa), LARGE_SIZE);
```

**Worked example — player at (400, 300), facing up (angle = 0):**

Left asteroid position (`pa - π/2 = -π/2`):
```
left_x = 150 * sin(-π/2) + 400 = 150 * -1 + 400 = 250   (150px to the left)
left_y = 150 * cos(-π/2) + 300 = 150 *  0 + 300 = 300
```

Right asteroid position (`pa + π/2 = +π/2`):
```
right_x = 150 * sin(+π/2) + 400 = 150 * 1 + 400 = 550   (150px to the right)
right_y = 150 * cos(+π/2) + 300 = 150 * 0 + 300 = 300
```

Both are 150 pixels away — safe spawn distance. Their velocities are:
```
Left:  dx = 50 * sin(0)  = 0,  dy = 50 * cos(0)  = 50   (moving downward toward player)
Right: dx = 50 * sin(0)  = 0,  dy = 50 * cos(-0) = 50   (same — both converge)
```

This means on wave 2, two large asteroids close in from the sides. The player must move or shoot quickly.

---

## Step 3 — The bitmap font: 5-column encoding

**Why a custom font?** We have no OS font API (this is a raw pixel backbuffer, not a windowed text renderer). Instead we embed a minimal 5×7-pixel bitmap font directly in `asteroids.c` as a static array.

Each character is stored as **5 bytes** — one byte per column:

```c
static const uint8_t font_glyphs[96][5] = {
    ...
    {0x3E,0x51,0x49,0x45,0x3E}, /* '0' */
    {0x00,0x42,0x7F,0x40,0x00}, /* '1' */
    ...
};
```

The index is `ch - 0x20` because the table starts at ASCII 32 (space, `0x20`). There are 96 entries covering all printable ASCII from space (32) to DEL (127).

**Decoding a glyph — the digit '0', column 0, byte `0x3E`:**

```
0x3E in binary = 0b00111110

Bit 0 (top row):    0  → pixel OFF
Bit 1:              1  → pixel ON
Bit 2:              1  → pixel ON
Bit 3:              1  → pixel ON
Bit 4:              1  → pixel ON
Bit 5:              1  → pixel ON
Bit 6 (bottom row): 0  → pixel OFF
```

Column 0 of '0' draws a vertical bar with the top and bottom pixels off — the left side of the oval shape of the digit zero.

The `draw_char` function reads each glyph:

```c
static void draw_char(AsteroidsBackbuffer *bb, int x, int y,
                      char c, int scale, uint32_t color)
{
    if (c < 0x20 || c > 0x7F) return;
    const uint8_t *glyph = font_glyphs[(unsigned char)c - 0x20];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if (!(glyph[col] & (1 << row))) continue;
            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    int px = x + col * scale + sx;
                    int py = y + row * scale + sy;
                    if (px >= 0 && px < bb->width &&
                        py >= 0 && py < bb->height)
                        bb->pixels[py * bb->width + px] = color;
                }
            }
        }
    }
}
```

The `scale` parameter is a pixel multiplier. At `scale = 1` each font pixel is 1 screen pixel (5×7 pixels per character). At `scale = 2` each font pixel becomes a 2×2 block (10×14 pixels per character) — much more readable.

**JS analogy:** This is like a hand-rolled bitmap font sprite sheet, except the "sprite" is encoded as bitmasks rather than PNG data. `(1 << row)` tests one bit at a time, like `image.getPixelAt(col, row)`.

---

## Step 4 — `draw_text`: advancing x between characters

```c
static void draw_text(AsteroidsBackbuffer *bb, int x, int y,
                      const char *s, int scale, uint32_t color)
{
    for (; *s; s++, x += (5 + 1) * scale)
        draw_char(bb, x, y, *s, scale, color);
}
```

Each character advances `x` by `(5 + 1) * scale`:
- 5 pixel columns for the glyph
- 1 pixel column of spacing between characters

At `scale = 2`: each character advances `(5 + 1) * 2 = 12` pixels. "SCORE: 100" is 10 characters → 120 pixels wide.

The loop condition `*s` is C's idiomatic way to iterate a null-terminated string — it reads as "while the current character is not the null terminator `\0`." In JS you'd write `for (let i = 0; i < s.length; i++)`.

---

## Step 5 — `snprintf` for the HUD string

We can't use `+` to concatenate strings in C. Instead we use `snprintf` — a safe string formatter that writes into a fixed-size buffer, never exceeding `sizeof(buf)` bytes:

```c
char buf[32];
snprintf(buf, sizeof(buf), "SCORE: %d", state->score);
draw_text(bb, 8, 8, buf, 2, COLOR_WHITE);
```

`%d` formats `state->score` as a decimal integer — the same as `${state.score}` in a JS template literal.

`buf[32]` is a local array on the stack — no `malloc`, no `free`. `"SCORE: 99999"` is 12 characters + null terminator = 13 bytes; 32 bytes is comfortably large.

**Why `snprintf` and not `sprintf`?** `sprintf` has no length limit — it will overflow `buf` if the score somehow grows beyond 24 digits. `snprintf(buf, sizeof(buf), ...)` stops writing at 31 characters + null terminator, guaranteeing no buffer overrun. Always use `snprintf`.

---

## Build & Run

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11 -lX11 -lxkbcommon -lGL -lGLX -lm
./build/asteroids_x11
```

**What you should see:** "SCORE: 0" appears in white in the top-left corner (rendered as 10×14-pixel characters at scale 2). Shoot all asteroids — you hear the clear, see the score jump by 1000 points, and two large asteroids immediately spawn to the sides. The game continues indefinitely. Each new wave adds 1000 to your score on clear.

---

## Key Concepts

- **Wave-clear detection** — `asteroid_count == 0` after `compact_pool` is the simplest correct signal.
- **90° spawn offset** — `angle ± π/2` places new threats far enough from the player to be survivable.
- **`sinf(a ± π/2)` simplification** — `sin(a - π/2) = -cos(a)` and `sin(a + π/2) = cos(a)`, which is what the geometry works out to; the code uses the explicit formula for clarity.
- **5-column bitmap font** — each character is 5 bytes; each byte is 7 bits; `glyph[col] & (1 << row)` tests one pixel.
- **`scale` multiplier** — each font pixel becomes a `scale × scale` block; `scale = 2` gives crisp large text with no floating-point math.
- **`snprintf(buf, sizeof(buf), ...)` pattern** — safe integer-to-string formatting with stack allocation; no heap, no `sprintf` overflow risk.
- **`const char *s` iteration** — `for (; *s; s++)` walks a null-terminated C string one character at a time; the loop stops when `*s == '\0'`.

---

## Exercise

Add a "LIVES: 3" display in the top-right corner. You'll need:
1. A `lives` field in `GameState` (add it to the struct in `asteroids.h`).
2. Decrement `lives` on death instead of immediately calling `reset_game`; only call `reset_game` (and actually `asteroids_init`-level re-init) when `lives == 0`.
3. A second `snprintf` + `draw_text` call placing the lives string at `x = SCREEN_W - 80, y = 8`.

Hint: `snprintf(buf, sizeof(buf), "LIVES: %d", state->lives)`.
