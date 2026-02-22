/* ============================================================
 * main_raylib.c — Raylib Platform Backend for Snake
 *
 * Implements the 6-function platform API (platform.h) using Raylib.
 * Also contains main() — the game loop.
 *
 * Build:
 *   gcc -Wall -Wextra -g -o snake_raylib src/snake.c src/main_raylib.c \
 *       -lraylib -lm
 *
 * Controls:
 *   Left / A    — turn left
 *   Right / D   — turn right
 *   R / Space   — restart (after game over)
 *   Q / Escape  — quit
 *
 * Raylib handles all the OS windowing and input for us.
 * IsKeyPressed() is single-frame (true only on the frame the key is pressed),
 * so we don't need manual latch logic unlike the X11 backend.
 * ============================================================ */

#include "raylib.h"  /* Raylib — must come before our headers on some setups */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "snake.h"
#include "platform.h"

/* ─── Color Palette ────────────────────────────────────────────
 *
 *  Indexed to match the same logical layout as the X11 backend:
 *    COLORS[0] = background
 *    COLORS[1] = walls
 *    COLORS[2] = snake body
 *    COLORS[3] = snake head (alive)
 *    COLORS[4] = food
 *    COLORS[5] = header background
 *    COLORS[6] = text
 *    COLORS[7] = dead snake
 *
 *  Raylib Color = { r, g, b, a } (all 0-255).
 * ─────────────────────────────────────────────────────────── */
static const Color COLORS[8] = {
    {   0,   0,   0, 255 },  /* [0] black — background */
    {  50, 205,  50, 255 },  /* [1] lime green — walls */
    { 255, 215,   0, 255 },  /* [2] yellow — snake body */
    { 255, 255, 255, 255 },  /* [3] white — snake head (alive) */
    { 220,  50,  50, 255 },  /* [4] red — food */
    {  80,  80,  80, 255 },  /* [5] dark gray — header background */
    { 255, 255, 255, 255 },  /* [6] white — text */
    { 139,   0,   0, 255 },  /* [7] dark red — dead snake */
};

/* ─── platform_init ────────────────────────────────────────────
 *
 *  Open the Raylib window. SetTargetFPS ensures Raylib's internal
 *  frame cap doesn't interfere with our manual sleep-based pacing.
 *  We set it high (120) so BeginDrawing/EndDrawing never block.
 * ─────────────────────────────────────────────────────────── */
void platform_init(void) {
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Snake \xe2\x80\x94 Raylib");
    SetTargetFPS(120);  /* high cap so our sleep controls timing, not Raylib */
}

/* ─── platform_get_input ───────────────────────────────────────
 *
 *  Raylib's IsKeyPressed() returns true only on the first frame
 *  a key is down — exactly the latch behavior we need.
 *  No manual latch state required (contrast with main_x11.c).
 * ─────────────────────────────────────────────────────────── */
void platform_get_input(PlatformInput *input) {
    input->turn_left  = IsKeyPressed(KEY_LEFT)   || IsKeyPressed(KEY_A);
    input->turn_right = IsKeyPressed(KEY_RIGHT)  || IsKeyPressed(KEY_D);
    input->restart    = IsKeyPressed(KEY_R)      || IsKeyPressed(KEY_SPACE);
    input->quit       = IsKeyPressed(KEY_Q)      || IsKeyPressed(KEY_ESCAPE);
}

/* ─── draw_cell ────────────────────────────────────────────────
 *
 *  Draw one grid cell at (col, row) inset by 1px so a dark gap
 *  shows between cells — same visual style as the X11 backend.
 * ─────────────────────────────────────────────────────────── */
static void draw_cell(int col, int row, Color color) {
    DrawRectangle(
        col * CELL_SIZE + 1,   /* x pixel */
        row * CELL_SIZE + 1,   /* y pixel */
        CELL_SIZE - 2,         /* width */
        CELL_SIZE - 2,         /* height */
        color
    );
}

/* ─── platform_render ──────────────────────────────────────────
 *
 *  Raylib requires every frame to be wrapped in BeginDrawing/EndDrawing.
 *  All draw calls must occur between them. ClearBackground handles
 *  the full-window fill (equivalent to XFillRectangle with black in X11).
 * ─────────────────────────────────────────────────────────── */
void platform_render(const GameState *state) {
    char buf[64];
    int col, row, idx, rem;
    Color body_color, head_color;

    BeginDrawing();

    /* 1. Clear to black */
    ClearBackground(COLORS[0]);

    /* 2. Header background (rows 0 .. HEADER_ROWS-1) */
    DrawRectangle(0, 0, WINDOW_WIDTH, HEADER_ROWS * CELL_SIZE, COLORS[5]);

    /* 3. Header bottom border line */
    DrawRectangle(0, (HEADER_ROWS - 1) * CELL_SIZE, WINDOW_WIDTH, 2, COLORS[1]);

    /* 4. Title and score in header */
    DrawText("SNAKE",
             WINDOW_WIDTH / 2 - 28,
             CELL_SIZE / 2,
             CELL_SIZE,
             COLORS[6]);

    snprintf(buf, sizeof(buf), "Score: %d", state->score);
    DrawText(buf, 10, CELL_SIZE / 2, CELL_SIZE, COLORS[6]);

    snprintf(buf, sizeof(buf), "Best: %d", state->best_score);
    DrawText(buf, WINDOW_WIDTH - 100, CELL_SIZE / 2, CELL_SIZE, COLORS[6]);

    /* 5. Border walls */
    for (row = HEADER_ROWS; row < GRID_HEIGHT + HEADER_ROWS; row++) {
        draw_cell(0,              row, COLORS[1]);  /* left wall */
        draw_cell(GRID_WIDTH - 1, row, COLORS[1]);  /* right wall */
    }
    for (col = 0; col < GRID_WIDTH; col++) {
        draw_cell(col, GRID_HEIGHT + HEADER_ROWS - 1, COLORS[1]); /* bottom wall */
    }

    /* 6. Food */
    draw_cell(state->food_x, state->food_y, COLORS[4]);

    /* 7. Snake body (tail to head-1) and head */
    body_color = state->game_over ? COLORS[7] : COLORS[2];
    head_color = state->game_over ? COLORS[7] : COLORS[3];

    idx = state->tail;
    rem = state->length - 1;  /* all except head */
    while (rem > 0) {
        draw_cell(state->segments[idx].x, state->segments[idx].y, body_color);
        idx = (idx + 1) % MAX_SNAKE;
        rem--;
    }

    /* 8. Snake head */
    draw_cell(state->segments[state->head].x, state->segments[state->head].y, head_color);

    /* 9. Game over overlay */
    if (state->game_over) {
        int cx = WINDOW_WIDTH  / 2;
        int cy = WINDOW_HEIGHT / 2;

        /* Semi-transparent dark box */
        DrawRectangle(cx - 90, cy - 36, 180, 74, (Color){0, 0, 0, 200});
        DrawRectangleLines(cx - 90, cy - 36, 180, 74, COLORS[1]);

        DrawText("GAME OVER", cx - 58, cy - 26, 24, COLORS[4]);

        snprintf(buf, sizeof(buf), "Score: %d", state->score);
        DrawText(buf, cx - 34, cy + 4, 18, COLORS[6]);

        DrawText("Press R to restart", cx - 68, cy + 26, 14, LIGHTGRAY);
    }

    EndDrawing();
}

/* ─── platform_sleep_ms ────────────────────────────────────────
 *
 *  WaitTime pauses Raylib's polling for the given seconds.
 *  We convert ms → seconds for Raylib's API.
 *  This is equivalent to nanosleep in the X11 backend.
 * ─────────────────────────────────────────────────────────── */
void platform_sleep_ms(int ms) {
    WaitTime((double)ms / 1000.0);
}

/* ─── platform_should_quit / platform_shutdown ─────────────────
 *
 *  WindowShouldClose() returns true when the user closes the window
 *  or presses Escape (Raylib default). We also set quit via get_input.
 * ─────────────────────────────────────────────────────────── */
int platform_should_quit(void) {
    return WindowShouldClose();
}

void platform_shutdown(void) {
    CloseWindow();
}

/* ─── main ─────────────────────────────────────────────────────
 *
 *  Identical structure to main_x11.c — the game loop is the same.
 *  Only the platform_* calls differ between backends.
 * ─────────────────────────────────────────────────────────── */
int main(void) {
    GameState     state;
    PlatformInput input;

    platform_init();
    snake_init(&state);   /* srand is called inside snake_init */

    while (!platform_should_quit()) {
        platform_sleep_ms(BASE_TICK_MS);
        platform_get_input(&input);

        if (input.quit) break;

        snake_tick(&state, input);

        platform_render(&state);
    }

    platform_shutdown();
    printf("Final score: %d\n", state.score);
    return 0;
}
