/* src/main_raylib.c  —  Desktop Tower Defense | Raylib Platform Backend
 *
 * Implements the platform.h contract using Raylib 5.x.
 *
 * How the backbuffer reaches the screen:
 *   game_render() writes ARGB pixels into bb.pixels
 *   → R and B channels are swapped in-place (ARGB → ABGR ≡ Raylib RGBA)
 *   → UpdateTexture() uploads them to the GPU
 *   → DrawTextureEx() renders the texture with letterbox scaling
 *   → channels are swapped back so game state is unaffected
 *
 * Audio (AudioStream model):
 *   InitAudioDevice → SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE)
 *   → LoadAudioStream → PlayAudioStream
 *   Per-frame: if (IsAudioStreamProcessed) { game_get_audio_samples; UpdateAudioStream }
 *
 * Build:
 *   clang -Wall -Wextra -O2 -o build/game \
 *         src/main_raylib.c src/game.c src/levels.c src/audio.c \
 *         -lraylib -lm -lpthread -ldl
 */

#include "raylib.h"

#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* memset       */

#include "platform.h"

/* ===================================================================
 * PLATFORM GLOBALS
 * ===================================================================
 * g_scale / g_offset_x / g_offset_y are set each frame in
 * platform_display_backbuffer and read in platform_get_input to
 * transform window-space mouse coordinates into canvas-space coords.
 */
static Texture2D g_texture;

/* Letterbox globals — written by platform_display_backbuffer */
static float g_scale    = 1.0f;
static int   g_offset_x = 0;
static int   g_offset_y = 0;

/* Audio globals */
static AudioStream g_audio_stream;
static int         g_audio_init = 0;

/* Static sample buffer: stereo, 4× headroom */
static int16_t g_sample_buffer[AUDIO_CHUNK_SIZE * 4];

/* ===================================================================
 * PLATFORM IMPLEMENTATION
 * =================================================================== */

void platform_init(const char *title, int width, int height) {
    SetTraceLogLevel(LOG_WARNING); /* suppress verbose Raylib log spam */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(width, height, title);

    /* Create a texture with the same dimensions as the canvas.
     * PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 = 4 bytes per pixel, RGBA order.
     * We swap R↔B before each UpdateTexture call so our ARGB pixel data
     * lands in the correct channels. */
    Image img = {
        .data    = NULL,
        .width   = width,
        .height  = height,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    g_texture = LoadTextureFromImage(img);

    SetTargetFPS(60);
}

double platform_get_time(void) {
    return GetTime(); /* Raylib monotonic clock, seconds */
}

void platform_get_input(MouseState *mouse) {
    /* Mouse position — transform from window coords to canvas coords.
     *
     * GetMousePosition() returns window-space coords.  When the window
     * is resized the canvas is letterboxed: scaled to fit with black
     * borders.  We must undo that scale + offset to get canvas coords.
     *
     * g_scale and g_offset_* are updated in platform_display_backbuffer
     * (which runs before platform_get_input in the main loop). */
    Vector2 raw = GetMousePosition();

    int mouse_x = (int)((raw.x - (float)g_offset_x) / g_scale);
    int mouse_y = (int)((raw.y - (float)g_offset_y) / g_scale);

    /* Clamp to canvas bounds */
    if (mouse_x < 0)          mouse_x = 0;
    if (mouse_y < 0)          mouse_y = 0;
    if (mouse_x >= CANVAS_W)  mouse_x = CANVAS_W - 1;
    if (mouse_y >= CANVAS_H)  mouse_y = CANVAS_H - 1;

    /* Save previous position before updating current */
    mouse->prev_x = mouse->x;
    mouse->prev_y = mouse->y;
    mouse->x = mouse_x;
    mouse->y = mouse_y;

    /* Left mouse button — edge detection via UPDATE_BUTTON */
    int is_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    UPDATE_BUTTON(mouse->left_pressed, mouse->left_released, mouse->left_down, is_down);

    /* Right click — single-frame edge flag */
    mouse->right_pressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
}

void platform_display_backbuffer(const Backbuffer *bb) {
    /*
     * Pixel format fix: our pixels are 0xAARRGGBB (ARGB), stored
     * little-endian as bytes [B, G, R, A].  Raylib's RGBA format reads
     * those same bytes as [R=B, G=G, B=R, A=A] — swapping red and blue.
     *
     * We fix this with an in-place R↔B swap before the GPU upload and
     * a matching swap-back afterward so game state remains unaffected.
     */
    uint32_t *px    = bb->pixels;
    int        total = bb->width * bb->height;

    for (int i = 0; i < total; i++) {
        uint32_t c = px[i];
        px[i] = (c & 0xFF00FF00u)
              | ((c & 0x00FF0000u) >> 16)
              | ((c & 0x000000FFu) << 16);
    }

    UpdateTexture(g_texture, bb->pixels);

    /* Swap back so game state is unaffected */
    for (int i = 0; i < total; i++) {
        uint32_t c = px[i];
        px[i] = (c & 0xFF00FF00u)
              | ((c & 0x00FF0000u) >> 16)
              | ((c & 0x000000FFu) << 16);
    }

    /* Letterbox: scale the canvas to fit the current window while
     * preserving aspect ratio, centering it with black borders.
     * Store scale/offset in globals so platform_get_input can transform
     * mouse coordinates correctly. */
    int   win_w  = GetScreenWidth();
    int   win_h  = GetScreenHeight();
    float scaleX = (float)win_w / (float)CANVAS_W;
    float scaleY = (float)win_h / (float)CANVAS_H;
    g_scale    = (scaleX < scaleY) ? scaleX : scaleY;
    int dst_w  = (int)((float)CANVAS_W * g_scale);
    int dst_h  = (int)((float)CANVAS_H * g_scale);
    g_offset_x = (win_w - dst_w) / 2;
    g_offset_y = (win_h - dst_h) / 2;

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTextureEx(g_texture,
                  (Vector2){(float)g_offset_x, (float)g_offset_y},
                  0.0f,     /* rotation */
                  g_scale,
                  WHITE);
    EndDrawing();
}

/* -----------------------------------------------------------------------
 * Audio (AudioStream model)
 *
 * The platform asks the game for samples — game_get_audio_samples() fills
 * the output buffer.  We never call PlaySound/LoadSound; those hand
 * control to Raylib's scheduler, bypassing our PCM mixer.
 *
 * CRITICAL: SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE) MUST be
 * called BEFORE LoadAudioStream.  The count passed to UpdateAudioStream
 * MUST match AUDIO_CHUNK_SIZE exactly — a mismatch drifts the ring-buffer
 * write pointer, producing stutter and repeated audio.
 * ----------------------------------------------------------------------- */
void platform_audio_init(GameState *state, int samples_per_second) {
    (void)state; /* game_audio_init() is called inside game_init() */

    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        g_audio_init = 0;
        return;
    }

    /* CRITICAL: set buffer size BEFORE LoadAudioStream */
    SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE);

    g_audio_stream = LoadAudioStream(
        (unsigned int)samples_per_second,
        16,  /* bits per sample */
        2    /* stereo */
    );
    if (!IsAudioStreamValid(g_audio_stream)) {
        CloseAudioDevice();
        g_audio_init = 0;
        return;
    }

    PlayAudioStream(g_audio_stream);
    g_audio_init = 1;
}

void platform_audio_update(GameState *state) {
    if (!g_audio_init) return;
    if (!IsAudioStreamProcessed(g_audio_stream)) return;

    AudioOutputBuffer out = {
        .samples            = g_sample_buffer,
        .samples_per_second = AUDIO_SAMPLE_RATE,
        .sample_count       = AUDIO_CHUNK_SIZE,
    };
    game_get_audio_samples(state, &out);
    /* MUST pass AUDIO_CHUNK_SIZE — never a different value */
    UpdateAudioStream(g_audio_stream, g_sample_buffer, AUDIO_CHUNK_SIZE);
}

void platform_audio_shutdown(void) {
    if (!g_audio_init) return;
    StopAudioStream(g_audio_stream);
    UnloadAudioStream(g_audio_stream);
    CloseAudioDevice();
    g_audio_init = 0;
}

/* ===================================================================
 * MAIN LOOP
 * =================================================================== */

int main(void) {
    static GameState state;
    static Backbuffer bb;

    /* Allocate the pixel buffer (760 × 520 × 4 bytes ≈ 1.6 MB) */
    bb.pixels = (uint32_t *)malloc((size_t)(CANVAS_W * CANVAS_H) * sizeof(uint32_t));
    if (!bb.pixels) return 1;
    bb.width  = CANVAS_W;
    bb.height = CANVAS_H;
    bb.pitch  = CANVAS_W * 4; /* bytes per row */

    platform_init("Desktop Tower Defense", CANVAS_W, CANVAS_H);
    platform_audio_init(&state, AUDIO_SAMPLE_RATE);
    game_init(&state);

    double prev_time = platform_get_time();

    while (!WindowShouldClose() && !state.should_quit) {
        double curr_time  = platform_get_time();
        float  dt         = (float)(curr_time - prev_time);
        prev_time = curr_time;

        /* Cap delta-time: prevent physics explosion after debugger pauses */
        if (dt > 0.1f) dt = 0.1f;

        platform_get_input(&state.mouse);
        game_update(&state, dt);
        game_render(&state, &bb);
        platform_display_backbuffer(&bb);
        platform_audio_update(&state);
    }

    /* Cleanup */
    platform_audio_shutdown();
    UnloadTexture(g_texture);
    CloseWindow();
    free(bb.pixels);
    return 0;
}
