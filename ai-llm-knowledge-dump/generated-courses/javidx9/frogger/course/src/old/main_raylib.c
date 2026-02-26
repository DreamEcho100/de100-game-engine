/* =============================================================================
   main_raylib.c — Raylib Platform Backend
   =============================================================================
   Raylib already double-buffers via BeginDrawing / EndDrawing so no
   flickering fix is needed here.

   Key fix: lane_scroll() replaces the old broken scroll math. See frogger.h.

   Build:
     gcc -Wall -Wextra -std=c11 -o frogger_raylib \
         src/frogger.c src/main_raylib.c -lraylib -lm
   ============================================================================= */

#include "frogger.h"
#include "platform.h"

#include <raylib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
   Raylib module state
   ------------------------------------------------------------------------- */
static int   g_quit   = 0;
static int   g_win_w  = 0;
static int   g_win_h  = 0;

/* Last-frame key state for "released this frame" detection */
static int g_was_held[4] = {0};  /* 0=UP 1=DOWN 2=LEFT 3=RIGHT */

/* Lane data — local copy so platform_render is self-contained */
static const float RENDER_LANE_SPEEDS[NUM_LANES] = {
     0.0f, -3.0f, 3.0f, 2.0f, 0.0f,
    -3.0f,  3.0f,-4.0f, 2.0f, 0.0f,
};
static const char RENDER_LANE_PATTERNS[NUM_LANES][LANE_PATTERN_LEN + 1] = {
    "wwwhhwwwhhwwwhhwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww",
    ",,,jllk,,jllllk,,,,,,,jllk,,,,,jk,,,jlllk,,,,jllllk,,,,jlllk,,,,",
    ",,,,jllk,,,,,jllk,,,,jllk,,,,,,,,,jllk,,,,,jk,,,,,,jllllk,,,,,,,",
    ",,jlk,,,,,jlk,,,,,jk,,,,,jlk,,,jlk,,,,jk,,,,jllk,,,,jk,,,,,,jk,,",
    "pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp",
    "....asdf.......asdf....asdf..........asdf........asdf....asdf....",
    ".....ty..ty....ty....ty.....ty........ty..ty.ty......ty.......ty.",
    "..zx.....zx.........zx..zx........zx...zx...zx....zx...zx...zx..",
    "..ty.....ty.......ty.....ty......ty..ty.ty.......ty....ty........",
    "pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp",
};

static int local_tile_to_sprite(char c, int *src_x) {
    *src_x = 0;
    switch (c) {
        case 'w': return SPR_WALL;
        case 'h': return SPR_HOME;
        case ',': return SPR_WATER;
        case 'p': return SPR_PAVEMENT;
        case 'j': *src_x =  0; return SPR_LOG;
        case 'l': *src_x =  8; return SPR_LOG;
        case 'k': *src_x = 16; return SPR_LOG;
        case 'z': *src_x =  0; return SPR_CAR1;
        case 'x': *src_x =  8; return SPR_CAR1;
        case 't': *src_x =  0; return SPR_CAR2;
        case 'y': *src_x =  8; return SPR_CAR2;
        case 'a': *src_x =  0; return SPR_BUS;
        case 's': *src_x =  8; return SPR_BUS;
        case 'd': *src_x = 16; return SPR_BUS;
        case 'f': *src_x = 24; return SPR_BUS;
        default:  return -1;
    }
}

/* -------------------------------------------------------------------------
   Helper: draw a sub-region of a sprite at (dest_px_x, dest_px_y)
   ------------------------------------------------------------------------- */
static void draw_sprite_partial(
    const SpriteBank *bank, int spr_id,
    int src_x, int src_y, int src_w, int src_h,
    int dest_px_x, int dest_px_y)
{
    int sheet_w = bank->widths[spr_id];
    int offset  = bank->offsets[spr_id];

    for (int sy = 0; sy < src_h; sy++) {
        for (int sx = 0; sx < src_w; sx++) {
            int idx    = offset + (src_y + sy) * sheet_w + (src_x + sx);
            int16_t c  = bank->colors[idx];
            int16_t gl = bank->glyphs[idx];

            if (gl == 0x0020) continue; /* transparent */

            int ci = c & 0x0F;
            Color col = {
                CONSOLE_PALETTE[ci][0],
                CONSOLE_PALETTE[ci][1],
                CONSOLE_PALETTE[ci][2],
                255
            };

            DrawRectangle(
                dest_px_x + sx * CELL_PX,
                dest_px_y + sy * CELL_PX,
                CELL_PX, CELL_PX, col);
        }
    }
}

/* =========================================================================
   PLATFORM FUNCTION IMPLEMENTATIONS
   ========================================================================= */

void platform_init(int width, int height, const char *title) {
    g_win_w = width;
    g_win_h = height;
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(width, height, title);
    SetTargetFPS(60);
}

void platform_get_input(InputState *input) {
    memset(input, 0, sizeof(InputState));

    /* Raylib provides IsKeyReleased which is naturally one-shot per frame. */
    static const int KEYS[4][2] = {
        {KEY_UP,    KEY_W},
        {KEY_DOWN,  KEY_S},
        {KEY_LEFT,  KEY_A},
        {KEY_RIGHT, KEY_D},
    };

    int *released[4] = {
        &input->up_released,
        &input->down_released,
        &input->left_released,
        &input->right_released,
    };

    for (int i = 0; i < 4; i++) {
        *released[i] = IsKeyReleased(KEYS[i][0]) || IsKeyReleased(KEYS[i][1]);
    }

    if (IsKeyPressed(KEY_ESCAPE) || WindowShouldClose())
        g_quit = 1;

    (void)g_was_held;
}

void platform_render(const GameState *state) {
    BeginDrawing();
    ClearBackground(BLACK);

    /* Draw lanes */
    for (int y = 0; y < NUM_LANES; y++) {
        int tile_start, px_offset;
        lane_scroll(state->time, RENDER_LANE_SPEEDS[y], &tile_start, &px_offset);

        int dest_py = y * TILE_PX;

        for (int i = 0; i < LANE_WIDTH; i++) {
            char c    = RENDER_LANE_PATTERNS[y][(tile_start + i) % LANE_PATTERN_LEN];
            int src_x = 0;
            int spr   = local_tile_to_sprite(c, &src_x);

            /* Sub-pixel smooth horizontal position */
            int dest_px = (-1 + i) * TILE_PX - px_offset;

            if (spr >= 0) {
                draw_sprite_partial(&state->sprites, spr,
                    src_x, 0, TILE_CELLS, TILE_CELLS,
                    dest_px, dest_py);
            }
        }
    }

    /* Draw frog */
    int show_frog = 1;
    if (state->dead) {
        int flash = (int)(state->dead_timer / 0.05f);
        show_frog = (flash % 2 == 0);
    }
    if (show_frog) {
        int frog_px = (int)(state->frog_x * (float)TILE_PX);
        int frog_py = (int)(state->frog_y * (float)TILE_PX);
        draw_sprite_partial(&state->sprites, SPR_FROG,
            0, 0,
            state->sprites.widths[SPR_FROG],
            state->sprites.heights[SPR_FROG],
            frog_px, frog_py);
    }

    /* HUD */
    char score_buf[64];
    snprintf(score_buf, sizeof(score_buf), "Homes: %d", state->homes_reached);
    DrawText(score_buf, 8, 8, 14, WHITE);

    if (state->dead)
        DrawText("DEAD!", SCREEN_PX_W/2 - 20, SCREEN_PX_H/2, 24, RED);

    if (state->homes_reached >= 3)
        DrawText("YOU WIN!", SCREEN_PX_W/2 - 30, SCREEN_PX_H/2 - 30, 24, YELLOW);

    EndDrawing();
}

void platform_sleep_ms(int ms) {
    /* Raylib's EndDrawing handles frame timing via SetTargetFPS(60).
       Sleep is a no-op here to avoid double-throttling.            */
    (void)ms;
}

int platform_should_quit(void) {
    return g_quit || WindowShouldClose();
}

void platform_shutdown(void) {
    CloseWindow();
}

#ifndef ASSETS_DIR
#define ASSETS_DIR "assets"
#endif

int main(void) {
    frogger_run(ASSETS_DIR);
    return 0;
}
