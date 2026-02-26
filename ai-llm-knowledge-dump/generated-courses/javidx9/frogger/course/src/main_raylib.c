/* =============================================================================
   main_raylib.c — Raylib Platform Backend

   Implements the 4-function platform contract for frogger.
   Key changes from the old Raylib backend:

   1. Texture2D backbuffer pipeline (replaces DrawRectangle per sprite cell)
      frogger_render() writes to a uint32_t pixel array. This function uploads
      it to a Raylib Texture2D and draws it as a single textured quad per frame.

   2. Delta-time loop (replaces SetTargetFPS + frogger_run)
      GetTime() gives elapsed seconds. The loop computes dt and passes it to
      frogger_tick. VSync is handled by Raylib's EndDrawing.

   3. GameButtonState input (replaces IsKeyReleased)
      Uses IsKeyDown to update GameButtonState each frame, giving consistent
      "just pressed" detection matching the X11 backend.

   Build:
     clang -Wall -Wextra -Wpedantic -std=c99 -DASSETS_DIR=\"assets\" \
           -o build/frogger_raylib src/frogger.c src/main_raylib.c \
           -lraylib -lm
   ============================================================================= */

#include "frogger.h"
#include "platform.h"

#include <raylib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static int g_is_running = 0;

/* ─── platform_init ───────────────────────────────────────────────────────── */
void platform_init(const char *title, int width, int height) {
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(width, height, title);
    SetTargetFPS(0);  /* uncapped — VSync via EndDrawing */
    g_is_running = 1;
}

/* ─── platform_get_time ───────────────────────────────────────────────────── */
double platform_get_time(void) {
    return GetTime();
}

/* ─── platform_get_input ──────────────────────────────────────────────────── */
void platform_get_input(GameInput *input) {
    static const int KEYS_UP[2]    = {KEY_UP,    KEY_W};
    static const int KEYS_DOWN[2]  = {KEY_DOWN,  KEY_S};
    static const int KEYS_LEFT[2]  = {KEY_LEFT,  KEY_A};
    static const int KEYS_RIGHT[2] = {KEY_RIGHT, KEY_D};

    int up    = IsKeyDown(KEYS_UP[0])    || IsKeyDown(KEYS_UP[1]);
    int down  = IsKeyDown(KEYS_DOWN[0])  || IsKeyDown(KEYS_DOWN[1]);
    int left  = IsKeyDown(KEYS_LEFT[0])  || IsKeyDown(KEYS_LEFT[1]);
    int right = IsKeyDown(KEYS_RIGHT[0]) || IsKeyDown(KEYS_RIGHT[1]);

    UPDATE_BUTTON(input->move_up,    up);
    UPDATE_BUTTON(input->move_down,  down);
    UPDATE_BUTTON(input->move_left,  left);
    UPDATE_BUTTON(input->move_right, right);

    if (IsKeyPressed(KEY_ESCAPE) || WindowShouldClose()) {
        input->quit  = 1;
        g_is_running = 0;
    }
}

/* ─── main ────────────────────────────────────────────────────────────────── */
#ifndef ASSETS_DIR
#define ASSETS_DIR "assets"
#endif

int main(void) {
    GameState         state;
    FroggerBackbuffer bb;
    GameInput         input;

    platform_init("Frogger", SCREEN_PX_W, SCREEN_PX_H);

    bb.width  = SCREEN_PX_W;
    bb.height = SCREEN_PX_H;
    bb.pixels = (uint32_t *)malloc((size_t)(bb.width * bb.height) * sizeof(uint32_t));
    if (!bb.pixels) { fprintf(stderr, "FATAL: out of memory\n"); return 1; }

    /* Pre-fill with black so UpdateTexture has valid data from frame 0 */
    memset(bb.pixels, 0, (size_t)(bb.width * bb.height) * sizeof(uint32_t));

    frogger_init(&state, ASSETS_DIR);
    memset(&input, 0, sizeof(input));

    /* Create Texture2D for the backbuffer */
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

        frogger_tick(&state, &input, delta_time);
        frogger_render(&state, &bb);

        /* Upload backbuffer to GPU texture and draw fullscreen */
        UpdateTexture(tex, bb.pixels);
        BeginDrawing();
            DrawTexture(tex, 0, 0, WHITE);
        EndDrawing();
    }

    UnloadTexture(tex);
    free(bb.pixels);
    CloseWindow();
    return 0;
}
