/*
 * main_raylib.c  —  Sugar, Sugar | Raylib Platform Backend
 *
 * Implements the platform.h contract using Raylib 5.x.
 *
 * How the backbuffer reaches the screen:
 *   game_render() writes ARGB pixels into bb.pixels
 *   → we wrap them in a Raylib Image
 *   → UpdateTexture() uploads them to the GPU as a texture
 *   → DrawTexture() renders the texture fullscreen
 *
 * Build:
 *   clang -Wall -Wextra -O2 -o sugar \
 *         src/main_raylib.c src/game.c src/levels.c \
 *         -lraylib -lm -lpthread -ldl
 *
 * Debug build:
 *   clang -Wall -Wextra -O0 -g -fsanitize=address,undefined \
 *         -o sugar_dbg \
 *         src/main_raylib.c src/game.c src/levels.c \
 *         -lraylib -lm -lpthread -ldl
 */

#include "raylib.h"

#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* memset       */

#include "platform.h"
#include "sounds.h"

/* ===================================================================
 * PLATFORM GLOBALS
 * =================================================================== */
static Texture2D g_texture;  /* GPU texture we upload pixels into */
static int       g_tex_w;
static int       g_tex_h;

/* ===================================================================
 * PLATFORM IMPLEMENTATION
 * =================================================================== */

void platform_init(const char *title, int width, int height) {
    SetTraceLogLevel(LOG_WARNING); /* suppress verbose Raylib log spam */
    InitWindow(width, height, title);

    /* Create a texture with the same dimensions as the canvas.
     * PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 = 4 bytes per pixel, in RGBA order.
     *
     * Note: Raylib expects RGBA, but our backbuffer is ARGB (0xAARRGGBB).
     * On little-endian, 0xAARRGGBB is stored as bytes [BB,GG,RR,AA].
     * Raylib reads as RGBA: R=BB, G=GG, B=RR, A=AA — colours are swapped!
     *
     * Fix: use PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 with swapped channel
     * interpretation, OR swap B and R in game_render.
     *
     * We choose the simpler approach: use BGRA format constant so Raylib
     * reads our bytes in the correct order.  If your Raylib version doesn't
     * have BGRA, swap R and B in the GAME_RGB macros in game.h instead.
     */
    Image img = {
        .data    = NULL,
        .width   = width,
        .height  = height,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    g_texture = LoadTextureFromImage(img); /* GPU upload of blank image */
    g_tex_w   = width;
    g_tex_h   = height;

    SetTargetFPS(60);
}

double platform_get_time(void) {
    return GetTime(); /* Raylib monotonic clock, seconds */
}

void platform_get_input(GameInput *input) {
    /* Mouse position */
    Vector2 pos = GetMousePosition();
    input->mouse.prev_x = input->mouse.x;
    input->mouse.prev_y = input->mouse.y;
    input->mouse.x      = (int)pos.x;
    input->mouse.y      = (int)pos.y;

    /* Left mouse button */
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        UPDATE_BUTTON(input->mouse.left, 1);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        UPDATE_BUTTON(input->mouse.left, 0);

    /* Keys */
    if (IsKeyPressed(KEY_ESCAPE))  UPDATE_BUTTON(input->escape,  1);
    if (IsKeyReleased(KEY_ESCAPE)) UPDATE_BUTTON(input->escape,  0);
    if (IsKeyPressed(KEY_R))       UPDATE_BUTTON(input->reset,   1);
    if (IsKeyReleased(KEY_R))      UPDATE_BUTTON(input->reset,   0);
    if (IsKeyPressed(KEY_G))       UPDATE_BUTTON(input->gravity, 1);
    if (IsKeyReleased(KEY_G))      UPDATE_BUTTON(input->gravity, 0);
}

void platform_display_backbuffer(const GameBackbuffer *backbuffer) {
    /*
     * UpdateTexture() copies our pixel data from CPU memory to the GPU.
     * This is the key moment where software rendering "lands" on the GPU.
     *
     * Note on pixel format:
     *   Our pixels are 0xAARRGGBB (ARGB), stored little-endian as [B,G,R,A].
     *   Raylib's RGBA format reads [R,G,B,A].
     *   So our B maps to Raylib's R, our R maps to Raylib's B.
     *   This swaps red and blue channels!
     *
     *   Quick fix for now: swap R and B in the colour macros.
     *   A cleaner fix (Lesson 11): use a BGRA pixel format or do a one-time
     *   channel swap at the end of game_render().
     */
    UpdateTexture(g_texture, backbuffer->pixels);

    BeginDrawing();
    ClearBackground(BLACK);
    /* Draw texture at (0,0) with no tint */
    DrawTexture(g_texture, 0, 0, WHITE);
    EndDrawing();
}

/* -----------------------------------------------------------------------
 * Audio stubs — Raylib has full audio support but we add it in Lesson 12.
 * Swap these out with InitAudioDevice() + LoadSound() + PlaySound() calls.
 * ----------------------------------------------------------------------- */
void platform_play_sound(int sound_id) { (void)sound_id; }
void platform_play_music(int music_id) { (void)music_id; }
void platform_stop_music(void)         {}

/* ===================================================================
 * MAIN LOOP
 * =================================================================== */

int main(void) {
    static GameState      state;
    static GameBackbuffer bb;

    /* Allocate the pixel buffer */
    bb.pixels = (uint32_t *)malloc((size_t)(CANVAS_W * CANVAS_H) * sizeof(uint32_t));
    if (!bb.pixels) return 1;

    platform_init("Sugar, Sugar", CANVAS_W, CANVAS_H);
    game_init(&state, &bb);

    GameInput input;
    memset(&input, 0, sizeof(input));

    double prev_time = platform_get_time();

    while (!WindowShouldClose() && !state.should_quit) {
        double curr_time  = platform_get_time();
        float  delta_time = (float)(curr_time - prev_time);
        prev_time = curr_time;
        if (delta_time > 0.1f) delta_time = 0.1f;

        prepare_input_frame(&input);
        platform_get_input(&input);
        game_update(&state, &input, delta_time);
        game_render(&state, &bb);
        platform_display_backbuffer(&bb);
    }

    UnloadTexture(g_texture);
    CloseWindow();
    free(bb.pixels);
    return 0;
}
