/* ============================================================
 * main_raylib.c — Raylib Platform Backend
 *
 * Implements the platform API (platform.h) using Raylib.
 *
 * Compare this file side-by-side with main_x11.c.
 * The game logic is IDENTICAL — only the platform calls differ.
 *
 * Raylib abstracts away:
 *   - Display server connection (X11 XOpenDisplay)
 *   - Window creation (XCreateSimpleWindow)
 *   - Graphics context (XCreateGC)
 *   - Color allocation (XAllocNamedColor)
 *   - Event loop (XPending / XNextEvent)
 *   - Frame timing (nanosleep)
 *   - Screen buffer flush (XFlush)
 *
 * The result: ~3x less code for identical visual output.
 * Learning X11 first reveals exactly what Raylib is hiding.
 *
 * Build:
 *   gcc -o tetris_raylib src/tetris.c src/main_raylib.c \
 *       -lraylib -lm -lpthread -ldl -Wall -Wextra -g
 * ============================================================ */

#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tetris.h"
#include "platform.h"

/* ─── Window Layout ────────────────────────────────────────────
 *  Same proportions as the X11 version for visual consistency.
 * ─────────────────────────────────────────────────────────── */
#define SIDEBAR_WIDTH  (6 * CELL_SIZE)
#define WINDOW_WIDTH   (FIELD_WIDTH  * CELL_SIZE + SIDEBAR_WIDTH)
#define WINDOW_HEIGHT  (FIELD_HEIGHT * CELL_SIZE)

/* ─── Piece Colors ─────────────────────────────────────────────
 *
 *  Indexed by field cell value (0-9), same semantic mapping as X11.
 *  Raylib uses (Color){R, G, B, A} structs instead of pixel values.
 *
 *  In X11: unsigned long pixel = XAllocNamedColor("cyan")
 *  In Raylib: Color c = {0, 255, 255, 255}
 *
 *  Both produce the same visual result, but Raylib's approach is
 *  portable (works on any GPU/display server).
 * ─────────────────────────────────────────────────────────── */
static const Color PIECE_COLORS[10] = {
    {  0,   0,   0, 255},  /* 0: empty  — black   */
    {  0, 255, 255, 255},  /* 1: I      — cyan    */
    {  0, 255,   0, 255},  /* 2: S      — green   */
    {255,   0,   0, 255},  /* 3: Z      — red     */
    {255,   0, 255, 255},  /* 4: T      — magenta */
    {  0,   0, 255, 255},  /* 5: J      — blue    */
    {255, 165,   0, 255},  /* 6: L      — orange  */
    {255, 255,   0, 255},  /* 7: O      — yellow  */
    {255, 255, 255, 255},  /* 8: flash  — white   */
    {128, 128, 128, 255},  /* 9: wall   — gray    */
};

/* ─── platform_init ────────────────────────────────────────────
 *
 *  vs. X11: XOpenDisplay + XCreateSimpleWindow + XMapWindow + XFlush
 *  Raylib: one call. All complexity hidden inside.
 * ─────────────────────────────────────────────────────────── */
void platform_init(void) {
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Tetris — Raylib");
    SetTargetFPS(60);  /* Raylib caps renders at 60fps; game ticks are still 50ms */
}

/* ─── platform_get_input ───────────────────────────────────────
 *
 *  Raylib uses direct polling — no event queue to drain.
 *
 *  IsKeyDown()    — true every frame while key is physically held
 *  IsKeyPressed() — true only on the FIRST frame of a new press
 *
 *  IsKeyPressed() naturally handles the rotate latch without any
 *  extra boolean flag (compare to X11 g_key_rotate latch).
 *
 *  vs. X11: 30+ lines of event queue draining + flag management
 *  Raylib:  6 lines of direct poll
 * ─────────────────────────────────────────────────────────── */
void platform_get_input(PlatformInput *input) {
    input->move_left  = IsKeyDown(KEY_LEFT)    || IsKeyDown(KEY_A);
    input->move_right = IsKeyDown(KEY_RIGHT)   || IsKeyDown(KEY_D);
    input->move_down  = IsKeyDown(KEY_DOWN)    || IsKeyDown(KEY_S);
    input->rotate     = IsKeyPressed(KEY_Z)    || IsKeyPressed(KEY_X);
    input->restart    = IsKeyPressed(KEY_R);
    input->quit       = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q);
}

/* ─── draw_cell ────────────────────────────────────────────────
 *
 *  Draw one grid cell at (col, row) with a 1px gap for grid lines.
 *
 *  vs. X11: XSetForeground() + XFillRectangle()
 *  Raylib:  DrawRectangle() with color as parameter (no GC state)
 * ─────────────────────────────────────────────────────────── */
static void draw_cell(int col, int row, Color color) {
    DrawRectangle(
        col * CELL_SIZE + 1,
        row * CELL_SIZE + 1,
        CELL_SIZE - 2,
        CELL_SIZE - 2,
        color
    );
}

/* ─── platform_render ──────────────────────────────────────────
 *
 *  🔴 HOT PATH — called ~20 times/second.
 *
 *  BeginDrawing() / EndDrawing() bracket is REQUIRED.
 *  All draw calls must happen between them.
 *
 *  vs. X11: manual XSetForeground before every draw call
 *  Raylib:  color is a parameter — no separate "set color" step
 * ─────────────────────────────────────────────────────────── */
void platform_render(const GameState *state) {
    int x, y, px, py;
    char buf[64];

    BeginDrawing();
    ClearBackground(BLACK);

    /* ── Draw locked field cells (walls + placed pieces) ── */
    for (x = 0; x < FIELD_WIDTH; x++) {
        for (y = 0; y < FIELD_HEIGHT; y++) {
            unsigned char cell = state->field[y * FIELD_WIDTH + x];
            if (cell > 0)
                draw_cell(x, y, PIECE_COLORS[cell]);
        }
    }

    /* ── Draw current falling piece ── */
    if (!state->game_over) {
        for (px = 0; px < 4; px++) {
            for (py = 0; py < 4; py++) {
                int pi = tetris_rotate(px, py, state->current_rotation);
                if (TETROMINOES[state->current_piece][pi] != '.') {
                    draw_cell(state->current_x + px,
                              state->current_y + py,
                              PIECE_COLORS[state->current_piece + 1]);
                }
            }
        }
    }

    /* ── Sidebar ── */
    int sx = FIELD_WIDTH * CELL_SIZE + 10;

    DrawText("SCORE", sx, 10, 16, WHITE);
    snprintf(buf, sizeof(buf), "%d", state->score);
    DrawText(buf, sx, 30, 20, YELLOW);

    int level = (20 - state->speed) / 2;
    DrawText("LEVEL", sx, 60, 16, WHITE);
    snprintf(buf, sizeof(buf), "%d", level);
    DrawText(buf, sx, 80, 20, GREEN);

    DrawText("NEXT", sx, 115, 16, WHITE);
    int prev_col = FIELD_WIDTH + 1;
    int prev_row = 5;
    for (px = 0; px < 4; px++) {
        for (py = 0; py < 4; py++) {
            int pi = tetris_rotate(px, py, 0);
            if (TETROMINOES[state->next_piece][pi] != '.')
                draw_cell(prev_col + px, prev_row + py,
                          PIECE_COLORS[state->next_piece + 1]);
        }
    }

    /* Controls hint */
    DrawText("Controls:",     sx, WINDOW_HEIGHT - 100, 12, GRAY);
    DrawText("← →    Move",   sx, WINDOW_HEIGHT -  84, 12, GRAY);
    DrawText("↓      Drop",   sx, WINDOW_HEIGHT -  68, 12, GRAY);
    DrawText("Z/X  Rotate",   sx, WINDOW_HEIGHT -  52, 12, GRAY);
    DrawText("R   Restart",   sx, WINDOW_HEIGHT -  36, 12, GRAY);
    DrawText("Q/Esc  Quit",   sx, WINDOW_HEIGHT -  20, 12, GRAY);

    /* ── Game over overlay ── */
    if (state->game_over) {
        int cx = FIELD_WIDTH * CELL_SIZE / 2;
        int cy = FIELD_HEIGHT * CELL_SIZE / 2;
        DrawRectangle(cx - 70, cy - 36, 140, 72, (Color){0, 0, 0, 200});
        DrawText("GAME OVER",        cx - 52, cy - 22, 24, RED);
        DrawText("R = Restart",      cx - 46, cy +  4, 14, WHITE);
        DrawText("Q/Esc = Quit",     cx - 46, cy + 22, 14, WHITE);
    }

    EndDrawing();
}

/* ─── platform_sleep_ms ────────────────────────────────────────
 *
 *  Raylib's WaitTime() suspends the thread for fractional seconds.
 *  We still call this to maintain the 50ms tick pace even though
 *  SetTargetFPS(60) caps rendering. The tick logic is separate
 *  from the render frame rate.
 * ─────────────────────────────────────────────────────────── */
void platform_sleep_ms(int ms) {
    WaitTime((double)ms / 1000.0);
}

/* ─── platform_should_quit ─────────────────────────────────────*/
int platform_should_quit(void) {
    return WindowShouldClose();
}

/* ─── platform_shutdown ────────────────────────────────────────
 *
 *  vs. X11: XFreeGC + XDestroyWindow + XCloseDisplay (3 calls)
 *  Raylib:  CloseWindow() — one call
 * ─────────────────────────────────────────────────────────── */
void platform_shutdown(void) {
    CloseWindow();
}

/* ─── main ─────────────────────────────────────────────────────
 *
 *  Identical to main_x11.c — same structure, same loop.
 *  Only the platform_* function implementations differ.
 * ─────────────────────────────────────────────────────────── */
int main(void) {
    srand((unsigned int)time(NULL));

    GameState     state;
    PlatformInput input;

    platform_init();
    tetris_init(&state);

    while (!platform_should_quit()) {
        platform_sleep_ms(50);
        platform_get_input(&input);

        if (input.quit)
            break;

        if (state.game_over && input.restart)
            tetris_init(&state);
        else
            tetris_tick(&state, &input);

        platform_render(&state);
    }

    platform_shutdown();
    printf("Thanks for playing! Final score: %d\n", state.score);
    return 0;
}
