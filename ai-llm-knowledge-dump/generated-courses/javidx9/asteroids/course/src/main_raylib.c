/* =============================================================================
   main_raylib.c — Raylib Platform Backend for Asteroids

   Implements the 4-function platform contract using Raylib 5.x.

   Pipeline per frame:
     1. platform_get_input  — IsKeyDown/IsKeyPressed → UPDATE_BUTTON
     2. asteroids_update    — game logic (called by main)
     3. asteroids_render    — writes to bb.pixels[] (called by main)
     4. UpdateTexture + DrawTexture — upload bb.pixels to GPU, present frame

   VSync: Raylib's EndDrawing handles it automatically (SetTargetFPS(0) = uncapped,
   VSync still active via the default swap interval).
   ============================================================================= */

#include "asteroids.h"
#include "platform.h"

#include <raylib.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int g_is_running = 0;

/* ─── platform_init ───────────────────────────────────────────────────────── */
void platform_init(const char *title, int width, int height) {
    SetTraceLogLevel(LOG_WARNING); /* suppress verbose Raylib startup logs */
    InitWindow(width, height, title);
    SetTargetFPS(0); /* uncapped — VSync fires in EndDrawing */
    g_is_running = 1;
}

/* ─── platform_get_time ───────────────────────────────────────────────────── */
double platform_get_time(void) {
    return GetTime(); /* seconds since InitWindow, double precision */
}

/* ─── platform_get_input ──────────────────────────────────────────────────── */
void platform_get_input(GameInput *input) {
    /* IsKeyDown returns 1 every frame the key is held.
     * UPDATE_BUTTON only records a transition when the state changes,
     * so calling it every frame with IsKeyDown is correct and efficient. */
    UPDATE_BUTTON(input->left,  IsKeyDown(KEY_LEFT));
    UPDATE_BUTTON(input->right, IsKeyDown(KEY_RIGHT));
    UPDATE_BUTTON(input->up,    IsKeyDown(KEY_UP));
    UPDATE_BUTTON(input->fire,  IsKeyDown(KEY_SPACE));

    if (IsKeyPressed(KEY_ESCAPE) || WindowShouldClose()) {
        input->quit  = 1;
        g_is_running = 0;
    }
}

/* ─── main ────────────────────────────────────────────────────────────────── */
int main(void) {
    GameState           state;
    AsteroidsBackbuffer bb;
    GameInput           input;

    platform_init("Asteroids", SCREEN_W, SCREEN_H);

    bb.width  = SCREEN_W;
    bb.height = SCREEN_H;
    bb.pixels = (uint32_t *)malloc((size_t)(bb.width * bb.height) * sizeof(uint32_t));
    if (!bb.pixels) { fprintf(stderr, "FATAL: out of memory\n"); return 1; }
    memset(bb.pixels, 0, (size_t)(bb.width * bb.height) * sizeof(uint32_t));

    asteroids_init(&state);
    memset(&input, 0, sizeof(input));

    /* Create a Texture2D that matches our backbuffer dimensions and format.
     * PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 reads bytes in memory order R,G,B,A
     * — which matches our [RR,GG,BB,AA] byte layout exactly.               */
    Image img = {
        .data    = bb.pixels,
        .width   = bb.width,
        .height  = bb.height,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    Texture2D tex = LoadTextureFromImage(img);

    double prev_time = platform_get_time();

    while (g_is_running && !WindowShouldClose()) {
        double curr_time  = platform_get_time();
        float  delta_time = (float)(curr_time - prev_time);
        prev_time = curr_time;
        if (delta_time > 0.1f) delta_time = 0.1f;

        prepare_input_frame(&input);
        platform_get_input(&input);
        if (input.quit) break;

        asteroids_update(&state, &input, delta_time);
        asteroids_render(&state, &bb);

        /* Upload bb.pixels to the GPU texture, then draw it fullscreen */
        UpdateTexture(tex, bb.pixels);
        BeginDrawing();
            DrawTexture(tex, 0, 0, WHITE);
        EndDrawing(); /* VSync blocks here */
    }

    UnloadTexture(tex);
    free(bb.pixels);
    CloseWindow();
    return 0;
}
