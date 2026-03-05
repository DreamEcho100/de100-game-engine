# Lesson 08: State Machine and Economy

## What we're building

A `GamePhase` enum-driven state machine replaces the single implicit "always
updating" loop.  Each phase — `TITLE`, `PLACING`, `WAVE`, `WAVE_CLEAR`,
`GAME_OVER`, `VICTORY` — has its own update and render logic.  A side panel shows
`Wave: N/40`, `Lives: N` (red), and `Gold: N` (yellow) using an 8×8 bitmap font.
Overlay screens (title, game-over, victory) are drawn as dark semi-transparent
rectangles with centred text.

## What you'll learn

- How a `switch(state->phase)` dispatch cleanly separates per-phase logic
- How `change_phase()` centralises transition side-effects (reset timers, spawn
  first wave, etc.)
- How `draw_text()` and `snprintf()` compose to print dynamic HUD strings
- The `draw-text.c` 8×8 bitmap font implementation (characters A–Z, 0–9,
  punctuation)

## Prerequisites

- Lesson 07 complete: `spawn_creep()`, `compact_creeps()`, and the spawn timer
  all work
- `draw_rect()` and `draw_rect_blend()` in `utils/draw-shapes.h`

---

## Step 1: Confirm `GamePhase` enum in `game.h`

```c
/* src/game.h  —  GamePhase enum */
typedef enum {
    GAME_PHASE_TITLE = 0,
    GAME_PHASE_PLACING,     /* between waves: player places/sells towers */
    GAME_PHASE_WAVE,        /* wave in progress */
    GAME_PHASE_WAVE_CLEAR,  /* brief pause after all creeps are gone */
    GAME_PHASE_GAME_OVER,
    GAME_PHASE_VICTORY,
    GAME_PHASE_COUNT,
} GamePhase;
```

Also ensure `GameState` has:

```c
GamePhase  phase;
float      phase_timer;   /* general countdown used by WAVE_CLEAR */
int        current_wave;  /* 1-based; 0 = before first wave */
```

---

## Step 2: `change_phase()` — centralised transition handler

```c
/* src/game.c  —  change_phase() */

#define WAVE_CLEAR_DURATION 2.0f   /* seconds to linger in WAVE_CLEAR */

static void change_phase(GameState *state, GamePhase new_phase) {
    state->phase = new_phase;

    switch (new_phase) {
    case GAME_PHASE_TITLE:
        /* Re-use game_init logic handled by caller — nothing extra here */
        break;

    case GAME_PHASE_PLACING:
        /* Award interest in later lessons; for now, just reset timers */
        state->phase_timer = 0.0f;
        break;

    case GAME_PHASE_WAVE:
        state->current_wave++;
        /* Reset spawn bookkeeping for the new wave */
        state->wave_spawn_index = 0;
        state->wave_spawn_timer = 0.0f;
        /* wave_spawn_count will be set from the wave table in Lesson 12;
         * for now keep the demo value of 10 */
        state->wave_spawn_count = 10;
        break;

    case GAME_PHASE_WAVE_CLEAR:
        state->phase_timer = WAVE_CLEAR_DURATION;
        break;

    case GAME_PHASE_GAME_OVER:
        /* Stop all creep movement by clearing the pool */
        for (int i = 0; i < state->creep_count; i++)
            state->creeps[i].active = 0;
        state->creep_count = 0;
        break;

    case GAME_PHASE_VICTORY:
        state->phase_timer = 0.0f;
        break;

    default:
        break;
    }
}
```

---

## Step 3: Refactor `game_update()` into a phase switch

```c
/* src/game.c  —  game_update() (Lesson 08 version) */
void game_update(GameState *state, float dt) {
    if (dt > 0.1f) dt = 0.1f;

    switch (state->phase) {

    case GAME_PHASE_TITLE:
        /* Wait for mouse click — handled via input in Lesson 03/05 */
        if (state->mouse.left_pressed)
            change_phase(state, GAME_PHASE_PLACING);
        break;

    case GAME_PHASE_PLACING:
        /* Tower placement input already handled earlier in the frame.
         * "Send Wave" button click triggers transition: */
        if (state->mouse.left_pressed) {
            /* For this lesson: any click on the side panel starts the wave */
            if (state->mouse.x >= PANEL_X)
                change_phase(state, GAME_PHASE_WAVE);
        }
        break;

    case GAME_PHASE_WAVE: {
        /* Spawn timer */
        if (state->wave_spawn_index < state->wave_spawn_count) {
            state->wave_spawn_timer -= dt;
            if (state->wave_spawn_timer <= 0.0f) {
                spawn_creep(state, CREEP_NORMAL, 0.0f);
                state->wave_spawn_index++;
                state->wave_spawn_timer = 0.5f;
            }
        }

        /* Move creeps */
        for (int i = 0; i < state->creep_count; i++) {
            Creep *c = &state->creeps[i];
            if (c->active) creep_move_toward_exit(state, c, dt);
        }
        compact_creeps(state);

        /* Check game-over */
        if (state->player_lives <= 0)
            change_phase(state, GAME_PHASE_GAME_OVER);

        /* Check wave clear: all creeps spawned and pool is empty */
        else if (state->wave_spawn_index >= state->wave_spawn_count
                 && state->creep_count == 0) {
            if (state->current_wave >= WAVE_COUNT)
                change_phase(state, GAME_PHASE_VICTORY);
            else
                change_phase(state, GAME_PHASE_WAVE_CLEAR);
        }
        break;
    }

    case GAME_PHASE_WAVE_CLEAR:
        state->phase_timer -= dt;
        if (state->phase_timer <= 0.0f)
            change_phase(state, GAME_PHASE_PLACING);
        break;

    case GAME_PHASE_GAME_OVER:
        if (state->mouse.left_pressed) {
            game_init(state);   /* full reset */
        }
        break;

    case GAME_PHASE_VICTORY:
        /* Just show the overlay; player can restart */
        if (state->mouse.left_pressed)
            game_init(state);
        break;

    default:
        break;
    }
}
```

---

## Step 4: `draw-text.h` — public API

```c
/* src/utils/draw-text.h  —  Desktop Tower Defense | Bitmap font */
#ifndef DTD_DRAW_TEXT_H
#define DTD_DRAW_TEXT_H

#include <stdint.h>
#include "backbuffer.h"

/* Draw a single 8×8 character at pixel position (px, py).
 * Characters outside the supported set are drawn as blank. */
void draw_char(Backbuffer *bb, int px, int py, char ch, uint32_t color);

/* Draw a null-terminated string starting at (px, py).
 * Each character is 8 pixels wide with 1 pixel of horizontal gap = 9 px/char.
 * Newlines ('\n') advance py by 10 and reset px to the original x. */
void draw_text(Backbuffer *bb, int px, int py, const char *text, uint32_t color);

/* Convenience: draw text centred on (cx, cy). */
void draw_text_centered(Backbuffer *bb, int cx, int cy,
                        const char *text, uint32_t color);

#endif /* DTD_DRAW_TEXT_H */
```

---

## Step 5: `draw-text.c` — 8×8 bitmap font

Each character is stored as 8 bytes, one byte per row, MSB = left pixel.

```c
/* src/utils/draw-text.c  —  Desktop Tower Defense | 8×8 Bitmap Font */
#include "draw-text.h"
#include "backbuffer.h"
#include <string.h>  /* strlen */

/* -----------------------------------------------------------------------
 * 8×8 glyph table.
 * Index 0 = ASCII 0x20 (space).  Supported range: 0x20–0x5F (96 glyphs).
 * Unrecognised characters are drawn as blank (all zeros).
 * ----------------------------------------------------------------------- */
static const uint8_t FONT8x8[96][8] = {
    /* 0x20  ' ' */  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x21  '!' */  {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00},
    /* 0x22  '"' */  {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x23  '#' */  {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00},
    /* 0x24  '$' */  {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00},
    /* 0x25  '%' */  {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00},
    /* 0x26  '&' */  {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00},
    /* 0x27  '\''*/  {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00},
    /* 0x28  '(' */  {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00},
    /* 0x29  ')' */  {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00},
    /* 0x2A  '*' */  {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},
    /* 0x2B  '+' */  {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00},
    /* 0x2C  ',' */  {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06},
    /* 0x2D  '-' */  {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00},
    /* 0x2E  '.' */  {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00},
    /* 0x2F  '/' */  {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00},
    /* 0x30  '0' */  {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00},
    /* 0x31  '1' */  {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00},
    /* 0x32  '2' */  {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00},
    /* 0x33  '3' */  {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00},
    /* 0x34  '4' */  {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00},
    /* 0x35  '5' */  {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00},
    /* 0x36  '6' */  {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00},
    /* 0x37  '7' */  {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00},
    /* 0x38  '8' */  {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00},
    /* 0x39  '9' */  {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00},
    /* 0x3A  ':' */  {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00},
    /* 0x3B  ';' */  {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06},
    /* 0x3C  '<' */  {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00},
    /* 0x3D  '=' */  {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00},
    /* 0x3E  '>' */  {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},
    /* 0x3F  '?' */  {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00},
    /* 0x40  '@' */  {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00},
    /* 0x41  'A' */  {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00},
    /* 0x42  'B' */  {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00},
    /* 0x43  'C' */  {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00},
    /* 0x44  'D' */  {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00},
    /* 0x45  'E' */  {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00},
    /* 0x46  'F' */  {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00},
    /* 0x47  'G' */  {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00},
    /* 0x48  'H' */  {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00},
    /* 0x49  'I' */  {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    /* 0x4A  'J' */  {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00},
    /* 0x4B  'K' */  {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00},
    /* 0x4C  'L' */  {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00},
    /* 0x4D  'M' */  {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00},
    /* 0x4E  'N' */  {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00},
    /* 0x4F  'O' */  {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00},
    /* 0x50  'P' */  {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00},
    /* 0x51  'Q' */  {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00},
    /* 0x52  'R' */  {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00},
    /* 0x53  'S' */  {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00},
    /* 0x54  'T' */  {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    /* 0x55  'U' */  {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00},
    /* 0x56  'V' */  {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00},
    /* 0x57  'W' */  {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    /* 0x58  'X' */  {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00},
    /* 0x59  'Y' */  {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00},
    /* 0x5A  'Z' */  {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00},
    /* 0x5B  '[' */  {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00},
    /* 0x5C  '\\' */ {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00},
    /* 0x5D  ']' */  {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00},
    /* 0x5E  '^' */  {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00},
    /* 0x5F  '_' */  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
};

/* -----------------------------------------------------------------------
 * draw_char
 * ----------------------------------------------------------------------- */
void draw_char(Backbuffer *bb, int px, int py, char ch, uint32_t color) {
    int idx = (unsigned char)ch - 0x20;
    if (idx < 0 || idx >= 96) return; /* unrecognised — draw blank */

    const uint8_t *glyph = FONT8x8[idx];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                int sx = px + col;
                int sy = py + row;
                if (sx >= 0 && sx < bb->width && sy >= 0 && sy < bb->height)
                    bb->pixels[sy * bb->width + sx] = color;
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * draw_text
 * ----------------------------------------------------------------------- */
void draw_text(Backbuffer *bb, int px, int py, const char *text, uint32_t color) {
    int start_x = px;
    while (*text) {
        if (*text == '\n') {
            px  = start_x;
            py += 10; /* 8 px glyph + 2 px leading */
        } else {
            draw_char(bb, px, py, *text, color);
            px += 9; /* 8 px glyph + 1 px kerning gap */
        }
        text++;
    }
}

/* -----------------------------------------------------------------------
 * draw_text_centered
 * ----------------------------------------------------------------------- */
void draw_text_centered(Backbuffer *bb, int cx, int cy,
                        const char *text, uint32_t color) {
    int len = (int)strlen(text);
    int text_w = len * 9 - 1; /* subtract trailing gap */
    draw_text(bb, cx - text_w / 2, cy - 4, text, color);
}
```

**What's happening:**

- The font covers **ASCII 0x20–0x5F** (space through underscore), which gives
  us uppercase A–Z, digits 0–9, and common punctuation.  Lowercase letters and
  characters outside this range are silently ignored (drawn as blank).
- Each glyph row is a single byte.  Bit 7 (MSB) is the leftmost pixel.
  `0x80 >> col` isolates bit `col` from the left.
- `draw_text_centered` computes total pixel width as `len * 9 - 1` (9 px per
  character minus the trailing gap on the last character) and shifts left by
  half that width.

---

## Step 6: `draw_ui_panel()` — HUD side panel

```c
/* src/game.c  —  draw_ui_panel() */
#include <stdio.h>  /* snprintf */

static void draw_ui_panel(const GameState *state, Backbuffer *bb) {
    /* Panel background */
    draw_rect(bb, PANEL_X, 0, PANEL_W, CANVAS_H, COLOR_PANEL_BG);
    /* Left border line */
    for (int y = 0; y < CANVAS_H; y++)
        bb->pixels[y * bb->width + PANEL_X] = COLOR_PANEL_BORDER;

    char buf[32];
    int tx = PANEL_X + 8;  /* text left margin inside panel */
    int ty = 12;

    /* Wave counter */
    snprintf(buf, sizeof(buf), "WAVE %d/%d", state->current_wave, WAVE_COUNT);
    draw_text(bb, tx, ty, buf, COLOR_WHITE);
    ty += 20;

    /* Lives */
    snprintf(buf, sizeof(buf), "LIVES %d", state->player_lives);
    draw_text(bb, tx, ty, buf, COLOR_LIVES_TEXT);
    ty += 20;

    /* Gold */
    snprintf(buf, sizeof(buf), "GOLD  %d", state->player_gold);
    draw_text(bb, tx, ty, buf, COLOR_GOLD_TEXT);
}
```

---

## Step 7: Phase overlay rendering in `game_render()`

```c
/* src/game.c  —  game_render() overlay section */
void game_render(const GameState *state, Backbuffer *bb) {
    /* --- Grid, towers, creeps (existing) --- */
    /* ... */

    /* --- HUD panel --- */
    draw_ui_panel(state, bb);

    /* --- Phase overlays --- */
    switch (state->phase) {

    case GAME_PHASE_TITLE: {
        /* Dark semi-transparent overlay over grid */
        draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                        GAME_RGBA(0x00,0x00,0x00,0xB0));
        draw_text_centered(bb, GRID_PIXEL_W/2, GRID_PIXEL_H/2 - 14,
                           "DESKTOP TOWER DEFENSE", COLOR_WHITE);
        draw_text_centered(bb, GRID_PIXEL_W/2, GRID_PIXEL_H/2 + 6,
                           "CLICK TO PLAY", COLOR_GOLD_TEXT);
        break;
    }

    case GAME_PHASE_WAVE_CLEAR: {
        draw_text_centered(bb, GRID_PIXEL_W/2, 20,
                           "WAVE CLEAR!", COLOR_WHITE);
        break;
    }

    case GAME_PHASE_GAME_OVER: {
        draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                        GAME_RGBA(0x00,0x00,0x00,0xC0));
        draw_text_centered(bb, GRID_PIXEL_W/2, GRID_PIXEL_H/2 - 14,
                           "GAME OVER", COLOR_LIVES_TEXT);
        draw_text_centered(bb, GRID_PIXEL_W/2, GRID_PIXEL_H/2 + 6,
                           "CLICK TO RESTART", COLOR_WHITE);
        break;
    }

    case GAME_PHASE_VICTORY: {
        draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                        GAME_RGBA(0x00,0x00,0x00,0xC0));
        draw_text_centered(bb, GRID_PIXEL_W/2, GRID_PIXEL_H/2 - 14,
                           "YOU WIN!", COLOR_GOLD_TEXT);

        char buf[32];
        snprintf(buf, sizeof(buf), "WAVES: %d/%d", state->current_wave, WAVE_COUNT);
        draw_text_centered(bb, GRID_PIXEL_W/2, GRID_PIXEL_H/2 + 6,
                           buf, COLOR_WHITE);
        break;
    }

    default:
        break;
    }
}
```

---

## Build and run

```bash
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG \
      -fsanitize=address,undefined \
      -o build/dtd \
      src/main_raylib.c \
      src/game.c \
      src/utils/draw-shapes.c \
      src/utils/draw-text.c \
      -lraylib -lm -lpthread -ldl

./build/dtd
```

**Expected output:** A title screen with dark overlay and "DESKTOP TOWER DEFENSE /
CLICK TO PLAY" in the centre.  Click to enter PLACING mode.  Click the side panel
to start the wave.  Creeps spawn, navigate, and exit; lives decrement.  After all
10 creeps exit lives hit zero and "GAME OVER / CLICK TO RESTART" appears.  Click
to restart.

---

## Exercises

**Beginner:** Add a `"PLACING"` text label at the top of the side panel that only
appears during `GAME_PHASE_PLACING`.  Use `draw_text()` with `COLOR_WHITE`.

**Intermediate:** Add a `"SEND WAVE"` button to the panel (a `draw_rect` with a
text label) and detect a click inside its bounds to trigger
`change_phase(state, GAME_PHASE_WAVE)` instead of the whole-panel click used in
the lesson.

**Challenge:** Implement `draw_rect_blend` if it is not already in
`draw-shapes.c`.  It should read the existing pixel, blend the overlay RGBA using
`out = src_a/255 * overlay + (1 - src_a/255) * dst`, and write back.  Confirm the
title overlay is visibly translucent (background grid shows through).

---

## What's next

In Lesson 09 we add the **tower barrel** — a short line that rotates with
`atan2f()` to point at the nearest creep in range — and implement
`find_best_target()` using the BFS distance field to choose the creep closest to
the exit.
