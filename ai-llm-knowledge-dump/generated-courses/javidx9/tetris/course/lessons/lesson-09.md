# Lesson 09 — Bitmap Font: 5×7 Glyphs, draw_char, draw_text

## By the end of this lesson you will have:

A complete bitmap font system in `tetris.c` — digit and letter glyph tables, a special character lookup table, and `draw_text` that renders any ASCII string into the backbuffer at any position, color, and scale.

---

## Fonts as Data

The original course used platform font rendering:
- X11: `XDrawString(display, window, gc, x, y, text, len)`
- Raylib: `DrawText(text, x, y, fontSize, color)`

Both are platform-specific. They use OS-installed fonts with their own layout engines.

**The new approach:** font data lives in `tetris.c` as static byte arrays. Each character is 5 pixels wide and 7 pixels tall — a 5×7 bitmap glyph. To draw a character, you look up its glyph table entry and call `draw_rect` once per lit pixel.

No OS dependency. No font file. No text API. Just data and a loop.

---

## Step 1 — The glyph format

Each glyph is stored as 7 bytes — one byte per row. Within each byte, **bits 4 down to 0** (the lowest 5 bits) represent the 5 pixels left to right:

```
Bit 4 = leftmost pixel
Bit 3 = second pixel
Bit 2 = middle pixel
Bit 1 = fourth pixel
Bit 0 = rightmost pixel
```

Example — letter 'A':

```c
{0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}
```

Decoding row by row:

```
0x0E = 0b01110 →  . # # # .
0x11 = 0b10001 →  # . . . #
0x11 = 0b10001 →  # . . . #
0x1F = 0b11111 →  # # # # #
0x11 = 0b10001 →  # . . . #
0x11 = 0b10001 →  # . . . #
0x11 = 0b10001 →  # . . . #
```

**Bit extraction:** `bitmap[row] & (0x10 >> col)`

`0x10` is `0b10000` — the value with only bit 4 set (leftmost pixel). Shifting right by `col` moves the mask one column to the right:
- col=0: `0x10 >> 0` = `0b10000` — tests bit 4
- col=1: `0x10 >> 1` = `0b01000` — tests bit 3
- col=2: `0x10 >> 2` = `0b00100` — tests bit 2
- col=3: `0x10 >> 3` = `0b00010` — tests bit 1
- col=4: `0x10 >> 4` = `0b00001` — tests bit 0

If the result is nonzero, that pixel is lit.

---

## Step 2 — Digit table

```c
static const unsigned char FONT_DIGITS[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, /* 0 */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 1 */
    {0x0E, 0x11, 0x01, 0x0E, 0x10, 0x10, 0x1F}, /* 2 */
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E}, /* 3 */
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, /* 4 */
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, /* 5 */
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, /* 6 */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, /* 8 */
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, /* 9 */
};
```

Index `c - '0'` gives the glyph for digit character `c`.

---

## Step 3 — Letter table

```c
static const unsigned char FONT_LETTERS[26][7] = {
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* A */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, /* B */
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, /* C */
    /* ... all 26 letters ... */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, /* Z */
};
```

Index `c - 'A'` for uppercase, `c - 'a'` for lowercase (same glyphs — our font is case-insensitive).

---

## Step 4 — Special characters table

```c
typedef struct {
    char          character;
    unsigned char bitmap[7];
} FontSpecialChar;

static const FontSpecialChar FONT_SPECIAL[] = {
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},  /* period */
    {':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}},  /* colon */
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},  /* dash */
    /* ... more special chars ... */
    /* Arrow symbols using non-printing ASCII positions */
    {'^', {0x00, 0x04, 0x0E, 0x15, 0x04, 0x04, 0x00}}, /* up arrow ↑ */
    {'v', {0x00, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00}}, /* down arrow ↓ */
    {'{', {0x00, 0x04, 0x08, 0x1E, 0x08, 0x04, 0x00}}, /* left arrow ← */
    {'}', {0x00, 0x04, 0x02, 0x0F, 0x02, 0x04, 0x00}}, /* right arrow → */
    {0, {0}}  /* terminator */
};
```

**Custom arrow mappings:**  
The characters `{`, `}`, `^`, `v` are repurposed as directional arrows. In the controls hint (`draw_text(bb, x, y, "{} MOVE LEFT/RIGHT", ...)`), `{` renders as ← and `}` renders as →. This avoids Unicode and keeps the font a simple ASCII lookup.

**Terminator entry `{0, {0}}`:**  
The table ends with a sentinel where `character == 0`. The search loop `for (...; FONT_SPECIAL[i].character != 0; i++)` stops here — the same pattern as C null-terminated strings.

```c
static const unsigned char *find_special_char(char c) {
    for (int i = 0; FONT_SPECIAL[i].character != 0; i++) {
        if (FONT_SPECIAL[i].character == c)
            return FONT_SPECIAL[i].bitmap;
    }
    return NULL;
}
```

---

## Step 5 — `draw_char`

```c
static void draw_char(TetrisBackbuffer *bb, int x, int y, char c,
                      uint32_t color, int scale) {
    const unsigned char *bitmap = NULL;

    if (c >= '0' && c <= '9') {
        bitmap = FONT_DIGITS[c - '0'];
    } else if (c >= 'A' && c <= 'Z') {
        bitmap = FONT_LETTERS[c - 'A'];
    } else if (c >= 'a' && c <= 'z') {
        bitmap = FONT_LETTERS[c - 'a'];  /* lowercase maps to uppercase glyphs */
    } else if (c == ' ') {
        return;  /* space: just advance cursor, draw nothing */
    } else {
        bitmap = find_special_char(c);
        if (!bitmap) return;  /* unknown character: skip silently */
    }

    /* Render the 5×7 glyph */
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (bitmap[row] & (0x10 >> col)) {
                draw_rect(bb,
                          x + col * scale,
                          y + row * scale,
                          scale, scale,
                          color);
            }
        }
    }
}
```

**Scale parameter:**  
At `scale=1`, each glyph pixel is 1×1 screen pixel. At `scale=2`, each glyph pixel is a 2×2 screen block — doubling the text size. The `draw_rect` call writes a `scale × scale` rectangle per lit pixel.

---

## Step 6 — `draw_text`

```c
void draw_text(TetrisBackbuffer *bb, int x, int y, const char *text,
               uint32_t color, int scale) {
    int cursor_x = x;
    while (*text) {
        draw_char(bb, cursor_x, y, *text, color, scale);
        cursor_x += 6 * scale;  /* 5px glyph + 1px spacing */
        text++;
    }
}
```

**Cursor advancement:** `6 * scale`. A glyph is 5 pixels wide. Adding 1 pixel of spacing between characters gives `5 + 1 = 6` pixels per character slot. At scale=2: 12 pixels per character.

**New C concept — `while (*text)`:**  
`text` is a `const char *` — a pointer to the first character of the string. `*text` dereferences it to get the current character. For a null-terminated string, the null byte `'\0'` at the end has value 0, which is falsy — so `while (*text)` loops until the null terminator.

---

## Step 7 — Test in `tetris_render`

```c
/* Add to tetris_render, in the HUD section: */
char buf[32];
int sx = FIELD_WIDTH * CELL_SIZE + 10;

draw_text(backbuffer, sx, 10, "SCORE", COLOR_WHITE, 2);
snprintf(buf, sizeof(buf), "%d", state->score);
draw_text(backbuffer, sx, 26, buf, COLOR_YELLOW, 2);
```

`snprintf(buf, sizeof(buf), "%d", value)` formats an integer as a decimal string into `buf`. `sizeof(buf)` prevents buffer overflow — `snprintf` never writes more than that many bytes.

Build and run — you should see "SCORE" and the score value in the sidebar.

---

## Visual: Glyph Rendering at scale=2

```
Character '5' glyph:
Row 0: 0x1F = 11111 → ██ ██ ██ ██ ██
Row 1: 0x10 = 10000 → ██ .. .. .. ..
Row 2: 0x1E = 11110 → ██ ██ ██ ██ ..
Row 3: 0x01 = 00001 → .. .. .. .. ██
Row 4: 0x01 = 00001 → .. .. .. .. ██
Row 5: 0x11 = 10001 → ██ .. .. .. ██
Row 6: 0x0E = 01110 → .. ██ ██ ██ ..

At scale=2: each ██ is a 2×2 block, glyph is 10×14 pixels
```

---

## Key Concepts

- Font as data: byte arrays where each byte encodes one row of 5 pixels
- Bit extraction: `bitmap[row] & (0x10 >> col)` — `0x10` is the leftmost bit mask
- `FONT_DIGITS[c - '0']` and `FONT_LETTERS[c - 'A']` — O(1) lookup by character offset
- `FONT_SPECIAL` with sentinel `{0, {0}}` — linear scan with null-terminator pattern
- Custom arrow mappings: `{`, `}`, `^`, `v` render as directional arrows
- `scale` multiplier: 1=small, 2=normal readable text, 3=large headings
- `cursor_x += 6 * scale` — 5px glyph + 1px gap between characters

---

## Exercise

Add a space character `' '` to `FONT_SPECIAL` instead of handling it as a special case in `draw_char`. What value would its bitmap be? (Hint: a space has no lit pixels.) Alternatively, keep the special case and explain why it's slightly faster.
