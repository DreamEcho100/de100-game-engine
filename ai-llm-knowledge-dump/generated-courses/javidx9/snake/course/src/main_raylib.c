/* ============================================================
 * main_raylib.c — Raylib Platform Backend for Snake
 *
 * Implements platform.h using Raylib.
 * Also contains main() — the game loop.
 *
 * Build:
 *   clang snake.c main_raylib.c -o snake_raylib -lraylib -lm
 *
 * Controls:
 *   Left / A   — turn left (CCW)
 *   Right / D  — turn right (CW)
 *   R / Space  — restart (after game over)
 *   Q / Escape — quit
 * ============================================================ */

#include "platform.h"
#include "snake.h"

#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>

/* ─── platform_init ──────────────────────────────────────────── */

void platform_init(const char *title, int width, int height) {
    InitWindow(width, height, title);
    SetTargetFPS(60);  /* VSync-equivalent pacing via Raylib */
}

/* ─── platform_get_time ──────────────────────────────────────── */

double platform_get_time(void) {
    return GetTime();  /* seconds since window opened */
}

/* ─── platform_get_input ─────────────────────────────────────── */

void platform_get_input(GameInput *input) {
    /* Quit */
    if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE)) {
        input->quit = 1;
    }

    /* Restart */
    if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_SPACE)) {
        input->restart = 1;
    }

    /* Turn left (CCW): just-pressed maps to half_transition_count=1, ended_down=1
     * Turn keys fire once per press — IsKeyPressed returns true only on the
     * frame the key goes down, which matches the "just pressed" behavior. */
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
        UPDATE_BUTTON(input->turn_left, 1);
    } else if (IsKeyReleased(KEY_LEFT) || IsKeyReleased(KEY_A)) {
        UPDATE_BUTTON(input->turn_left, 0);
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
        UPDATE_BUTTON(input->turn_right, 1);
    } else if (IsKeyReleased(KEY_RIGHT) || IsKeyReleased(KEY_D)) {
        UPDATE_BUTTON(input->turn_right, 0);
    }
}

/* ─── platform_display_backbuffer ────────────────────────────── */
/* Note: display is done inline in main() below using Raylib's
 * UpdateTexture + DrawTexture — no separate function needed. */

/* ─── main ───────────────────────────────────────────────────── */

int main(void) {
    SnakeBackbuffer backbuffer;
    GameInput       input = {0};
    GameState       state = {0};

    platform_init("Snake", WINDOW_WIDTH, WINDOW_HEIGHT);

    backbuffer.width  = WINDOW_WIDTH;
    backbuffer.height = WINDOW_HEIGHT;
    backbuffer.pixels = (uint32_t *)malloc(
        WINDOW_WIDTH * WINDOW_HEIGHT * sizeof(uint32_t));
    if (!backbuffer.pixels) {
        fprintf(stderr, "Failed to allocate backbuffer\n");
        CloseWindow();
        return 1;
    }

    /* Create a Raylib texture to display the backbuffer.
     * Image struct wraps the pixel pointer — no copy is made here. */
    Image img = {
        .data    = backbuffer.pixels,
        .width   = WINDOW_WIDTH,
        .height  = WINDOW_HEIGHT,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    Texture2D texture = LoadTextureFromImage(img);

    snake_init(&state);

    while (!WindowShouldClose() && !input.quit) {
        float delta_time = GetFrameTime();
        if (delta_time > 0.1f) delta_time = 0.1f;

        prepare_input_frame(&input);
        platform_get_input(&input);

        if (input.quit) break;

        snake_update(&state, &input, delta_time);
        snake_render(&state, &backbuffer);

        /* Upload CPU backbuffer to GPU texture, then draw it */
        UpdateTexture(texture, backbuffer.pixels);

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexture(texture, 0, 0, WHITE);
        EndDrawing();
    }

    UnloadTexture(texture);
    free(backbuffer.pixels);
    CloseWindow();
    return 0;
}
