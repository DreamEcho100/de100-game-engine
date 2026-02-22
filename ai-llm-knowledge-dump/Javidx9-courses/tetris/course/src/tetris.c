/* ============================================================
 * tetris.c — Platform-independent game logic
 *
 * NO OS headers. NO X11. NO Raylib. NO Windows.h.
 * This file compiles identically whether we link against X11 or Raylib.
 *
 * Port of OneLoneCoder Tetris (javidx9) from C++/Windows to C/Linux.
 * Original: https://github.com/OneLoneCoder/videos
 * ============================================================ */

#include "tetris.h"

#include <stdlib.h>   /* rand() */
#include <string.h>   /* memset() */

/* ─── Tetromino Shapes ─────────────────────────────────────────
 *
 *  7 pieces, each stored as a 16-char string (4x4 bounding box).
 *  Layout: row-major, index = row*4 + col
 *
 *  Piece  Shape    Color
 *  ─────  ──────   ──────
 *    0    I        cyan
 *    1    S        green
 *    2    Z        red
 *    3    T        magenta
 *    4    J        blue
 *    5    L        orange
 *    6    O        yellow
 *
 *  In the original C++ code these were wstring (wide strings).
 *  In C we use const char* — no need for wide chars since we
 *  render with colored rectangles, not terminal characters.
 * ─────────────────────────────────────────────────────────── */
const char *TETROMINOES[7] = {
    "..X...X...X...X.",  /* 0: I  */
    "..X..XX...X.....",  /* 1: S  */
    ".....XX..XX.....",  /* 2: Z  */
    "..X..XX..X......",  /* 3: T  */
    ".X...XX...X.....",  /* 4: J  */
    ".X...X...XX.....",  /* 5: L  */
    "..X...X..XX.....",  /* 6: O  */
};

/* ─── Rotation ─────────────────────────────────────────────────
 *
 *  Maps (col px, row py) in a 4x4 grid to a flat index after r*90°.
 *
 *  Original 4x4 layout (r=0, no rotation):
 *    index = py * 4 + px
 *
 *   0  1  2  3
 *   4  5  6  7
 *   8  9 10 11
 *  12 13 14 15
 *
 *  After 90° clockwise (r=1):
 *    New position of (px, py) → (3-py, px)
 *    flat index = new_row*4 + new_col = px*4 + (3-py) = 12+py - px*4
 *
 *  After 180° (r=2):
 *    (px, py) → (3-px, 3-py)
 *    flat index = (3-py)*4 + (3-px) = 15 - py*4 - px
 *
 *  After 270° clockwise (r=3):
 *    (px, py) → (py, 3-px)
 *    flat index = (3-px)*4 + py = 12 - px*4 + py = 3-py + px*4
 *    (rewritten as original: 3 - py + (px * 4))
 * ─────────────────────────────────────────────────────────── */
int tetris_rotate(int px, int py, int r) {
    switch (r % 4) {
    case 0: return py * 4 + px;           /* 0°   — identity       */
    case 1: return 12 + py - (px * 4);    /* 90°  — clockwise      */
    case 2: return 15 - (py * 4) - px;    /* 180° — upside down    */
    case 3: return 3 - py + (px * 4);     /* 270° — counter-clock  */
    }
    return 0; /* unreachable, satisfies compiler */
}

/* ─── Collision Detection ──────────────────────────────────────
 *
 *  Iterates all 16 cells of the piece's 4x4 bounding box.
 *  For each solid cell ('X'):
 *    - If out of field bounds → SKIP (not a failure).
 *      This allows the I-piece to hang off an edge during spawn
 *      without triggering a false "doesn't fit" result.
 *    - If in bounds AND field cell != 0 → FAIL (overlap).
 *
 *  Returns 1 = fits, 0 = doesn't fit.
 * ─────────────────────────────────────────────────────────── */
int tetris_does_piece_fit(const GameState *state, int piece, int rotation,
                          int pos_x, int pos_y) {
    int px, py;
    for (px = 0; px < 4; px++) {
        for (py = 0; py < 4; py++) {
            int pi = tetris_rotate(px, py, rotation);
            int fi = (pos_y + py) * FIELD_WIDTH + (pos_x + px);

            if (pos_x + px >= 0 && pos_x + px < FIELD_WIDTH) {
                if (pos_y + py >= 0 && pos_y + py < FIELD_HEIGHT) {
                    /* In bounds — check for overlap */
                    if (TETROMINOES[piece][pi] != '.' && state->field[fi] != 0)
                        return 0;
                }
            }
        }
    }
    return 1;
}

/* ─── Initialization ───────────────────────────────────────────
 *
 *  Resets all game state. Called once at startup and on restart.
 *  Uses memset() to zero everything first (clean slate),
 *  then explicitly sets non-zero initial values.
 * ─────────────────────────────────────────────────────────── */
void tetris_init(GameState *state) {
    int x, y;
    memset(state, 0, sizeof(GameState));

    /* Build boundary: left wall (x=0), right wall (x=WIDTH-1), floor (y=HEIGHT-1).
     * All other cells remain 0 (empty) from the memset above. */
    for (x = 0; x < FIELD_WIDTH; x++) {
        for (y = 0; y < FIELD_HEIGHT; y++) {
            state->field[y * FIELD_WIDTH + x] =
                (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) ? 9 : 0;
        }
    }

    state->current_piece    = rand() % 7;
    state->next_piece       = rand() % 7;
    state->current_x        = FIELD_WIDTH / 2;
    state->current_y        = 0;
    state->current_rotation = 0;

    state->speed            = 20;  /* 20 ticks * 50ms = 1 second per drop */
    state->speed_count      = 0;
    state->piece_count      = 0;
    state->score            = 0;
    state->line_count       = 0;
    state->game_over        = 0;
    state->flash_timer      = 0;
}

/* ─── Game Tick ────────────────────────────────────────────────
 *
 *  🔴 HOT PATH — called every 50ms.
 *  NO malloc(). NO file I/O. NO system calls.
 *  Just pure arithmetic and array indexing.
 *
 *  Execution order each tick:
 *    1. If flash_timer active → count down and (when done) collapse lines
 *    2. Process player input (left/right/down/rotate)
 *    3. Tick speed counter → if reached speed limit, force drop
 *    4. If piece can't drop → lock it, detect lines, spawn next piece
 * ─────────────────────────────────────────────────────────── */
void tetris_tick(GameState *state, const PlatformInput *input) {
    int px, py, i;

    if (state->game_over) return;

    /* ── 1. Flash animation ──
     * While lines are flashing (value=8 in field), we freeze movement.
     * When the timer expires, collapse the completed rows. */
    if (state->flash_timer > 0) {
        state->flash_timer--;
        if (state->flash_timer == 0) {
            /* Collapse: for each completed line, shift all rows above it down */
            for (i = 0; i < state->line_count; i++) {
                int v = state->lines[i];
                for (px = 1; px < FIELD_WIDTH - 1; px++) {
                    /* Shift each cell from v up to row 1 downward by 1 */
                    for (py = v; py > 0; py--)
                        state->field[py * FIELD_WIDTH + px] =
                            state->field[(py - 1) * FIELD_WIDTH + px];
                    state->field[px] = 0; /* clear the newly exposed top row */
                }
            }
            state->line_count = 0;
        }
        return; /* skip input and movement during flash */
    }

    /* ── 2. Player input ──
     * Each move is gated by DoesPieceFit — the piece only moves
     * if the destination is valid. No movement queuing needed. */

    /* Move left: decrease X by 1 if it fits */
    if (input->move_left &&
        tetris_does_piece_fit(state, state->current_piece, state->current_rotation,
                              state->current_x - 1, state->current_y))
        state->current_x--;

    /* Move right: increase X by 1 if it fits */
    if (input->move_right &&
        tetris_does_piece_fit(state, state->current_piece, state->current_rotation,
                              state->current_x + 1, state->current_y))
        state->current_x++;

    /* Soft drop: increase Y by 1 if it fits */
    if (input->move_down &&
        tetris_does_piece_fit(state, state->current_piece, state->current_rotation,
                              state->current_x, state->current_y + 1))
        state->current_y++;

    /* Rotate: 'rotate' fires exactly once per key press (platform guarantees this).
     * We try rotation+1; if it doesn't fit, we simply don't rotate. */
    if (input->rotate &&
        tetris_does_piece_fit(state, state->current_piece, state->current_rotation + 1,
                              state->current_x, state->current_y))
        state->current_rotation++;

    /* ── 3. Forced drop ──
     * Every `speed` ticks, the piece is pushed down one row automatically. */
    state->speed_count++;
    if (state->speed_count < state->speed)
        return; /* not yet time to drop */

    state->speed_count = 0;
    state->piece_count++;

    /* Difficulty: every 50 pieces placed, decrease drop interval by 1 tick */
    if (state->piece_count % 50 == 0 && state->speed > 10)
        state->speed--;

    /* ── 4. Can piece drop? ── */
    if (tetris_does_piece_fit(state, state->current_piece, state->current_rotation,
                              state->current_x, state->current_y + 1)) {
        state->current_y++;   /* yes — drop it */
        return;
    }

    /* ── Piece can't drop → LOCK IT ──
     * Write the piece's solid cells into the field array permanently. */
    for (px = 0; px < 4; px++) {
        for (py = 0; py < 4; py++) {
            int pi = tetris_rotate(px, py, state->current_rotation);
            if (TETROMINOES[state->current_piece][pi] != '.') {
                state->field[(state->current_y + py) * FIELD_WIDTH +
                             (state->current_x + px)] = state->current_piece + 1;
            }
        }
    }

    /* ── Check for completed lines in the piece's 4-row band ──
     * Only rows nCurrentY..nCurrentY+3 can possibly be completed
     * by this piece, so we only check those 4 rows. */
    state->line_count = 0;
    for (py = 0; py < 4; py++) {
        if (state->current_y + py >= FIELD_HEIGHT - 1) continue; /* skip floor row */

        int line_complete = 1;
        for (px = 1; px < FIELD_WIDTH - 1; px++) {
            if (state->field[(state->current_y + py) * FIELD_WIDTH + px] == 0) {
                line_complete = 0;
                break;
            }
        }

        if (line_complete) {
            /* Mark cells as 8 — platform_render() draws these as "flash" color */
            for (px = 1; px < FIELD_WIDTH - 1; px++)
                state->field[(state->current_y + py) * FIELD_WIDTH + px] = 8;
            state->lines[state->line_count++] = state->current_y + py;
        }
    }

    /* ── Score ──
     * 25 pts per piece locked.
     * Line bonus uses bit-shift: 1<<n gives 2, 4, 8, 16 for n=1..4.
     * Multiplied by 100: 200, 400, 800, 1600 for 1-4 lines (classic Tetris). */
    state->score += 25;
    if (state->line_count > 0)
        state->score += (1 << state->line_count) * 100;

    /* ── Start flash animation ── */
    if (state->line_count > 0)
        state->flash_timer = 8; /* 8 ticks * 50ms = 400ms */

    /* ── Spawn next piece ── */
    state->current_piece    = state->next_piece;
    state->next_piece       = rand() % 7;
    state->current_x        = FIELD_WIDTH / 2;
    state->current_y        = 0;
    state->current_rotation = 0;

    /* ── Game over check ──
     * If the newly spawned piece doesn't fit at the top center, the board is full. */
    if (!tetris_does_piece_fit(state, state->current_piece, state->current_rotation,
                               state->current_x, state->current_y))
        state->game_over = 1;
}
