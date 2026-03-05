/* src/sprites.c — Sprite sheet system with async loading and error handling
 *
 * How it works:
 *   sprites_init(path)        — synchronous load (or placeholder if path==NULL)
 *   sprites_load_async(path)  — background pthread load; game keeps running
 *
 * On any load failure:
 *   - g_sprite_atlas.load_state = SPRITE_LOAD_ERROR
 *   - g_sprite_atlas.error_msg  = human-readable error
 *   - fprintf(stderr, "[SPRITES] ...")  — logged for debugging
 *   - draw_sprite() falls back to placeholder rects so the game remains playable
 *
 * PNG loading uses stb_image (vendor/stb_image.h — public domain).
 * STB_IMAGE_STATIC makes all stb functions static (file-scope), so they don't
 * conflict with Raylib's own internal copy of stb_image.
 *
 * To enable real sprites: set USE_SPRITES 1 in game.c and supply an atlas PNG.
 */

/* stb_image — keep symbols file-static so they don't clash with Raylib's
 * internal stb_image copy when linking against libraylib.a.
 * Pragmas suppress "unused function" warnings from the ~30 stb helpers that
 * exist in the header but are not called when only PNG support is compiled in. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#define STBI_ONLY_PNG           /* we only need PNG support */
#define STB_IMAGE_STATIC        /* all stbi_* functions become static */
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"
#pragma clang diagnostic pop

#include "sprites.h"
#include "utils/draw-shapes.h"
#include "utils/draw-text.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * GLOBALS
 * ========================================================================= */

SpriteAtlas g_sprite_atlas = {0};

/* Placeholder colors mirror the COLOR_* constants in game.h.
 * src_x / src_y describe where this sprite lives in a 256×256 atlas
 * arranged as a 16×16 grid of 16×16 tiles (rows = entity category). */
const SpriteDef SPRITE_DEFS[SPR_COUNT] = {
    /* id                       src_x src_y  w   h   placeholder_color                    label  */
    /* --- Towers (row 0, cols 0-6) --------------------------------------------- */
    [SPR_TOWER_PELLET]         = {   0,  0, 16, 16, GAME_RGB(0x88,0x88,0x88), "PLLT" },
    [SPR_TOWER_SQUIRT]         = {  16,  0, 16, 16, GAME_RGB(0x44,0x88,0xCC), "SQRT" },
    [SPR_TOWER_DART]           = {  32,  0, 16, 16, GAME_RGB(0x22,0x44,0x88), "DART" },
    [SPR_TOWER_SNAP]           = {  48,  0, 16, 16, GAME_RGB(0xCC,0x22,0x22), "SNAP" },
    [SPR_TOWER_SWARM]          = {  64,  0, 16, 16, GAME_RGB(0xFF,0x88,0x00), "SWRM" },
    [SPR_TOWER_FROST]          = {  80,  0, 16, 16, GAME_RGB(0x88,0xCC,0xFF), "FRST" },
    [SPR_TOWER_BASH]           = {  96,  0, 16, 16, GAME_RGB(0x88,0x66,0x44), "BASH" },
    /* --- Creeps (row 1, cols 0-5) ---------------------------------------------- */
    [SPR_CREEP_NORMAL]         = {   0, 16, 16, 16, GAME_RGB(0x88,0x88,0x88), "NRM"  },
    [SPR_CREEP_FAST]           = {  16, 16, 16, 16, GAME_RGB(0xFF,0xCC,0x00), "FST"  },
    [SPR_CREEP_FLYING]         = {  32, 16, 16, 16, GAME_RGB(0xFF,0xFF,0xFF), "FLY"  },
    [SPR_CREEP_ARMOURED]       = {  48, 16, 16, 16, GAME_RGB(0x44,0x44,0x44), "ARM"  },
    [SPR_CREEP_SPAWN]          = {  64, 16, 16, 16, GAME_RGB(0x44,0xCC,0x44), "SPN"  },
    [SPR_CREEP_BOSS]           = {  80, 16, 32, 32, GAME_RGB(0xCC,0x00,0x00), "BOSS" },
    /* --- Projectiles / FX (row 2) ---------------------------------------------- */
    [SPR_PROJECTILE]           = {   0, 32,  8,  8, GAME_RGB(0xFF,0xFF,0x00), "PRJ"  },
    [SPR_EXPLOSION]            = {   8, 32, 16, 16, GAME_RGB(0xFF,0x66,0x00), "EXP"  },
    /* --- Background tiles (row 3) ---------------------------------------------- */
    [SPR_BACKGROUND_TILE_EVEN] = {   0, 48, 20, 20, GAME_RGB(0xDD,0xDD,0xDD), ""     },
    [SPR_BACKGROUND_TILE_ODD]  = {  20, 48, 20, 20, GAME_RGB(0xC8,0xC8,0xC8), ""     },
    /* --- Missing-asset sentinel ------------------------------------------------- */
    [SPR_MISSING]              = {   0,  0, 16, 16, GAME_RGB(0xFF,0x00,0xFF), "???"  },
};

/* =========================================================================
 * ASYNC LOADING
 * ========================================================================= */

/* Path stored for the background thread — enough for any filesystem path */
static char g_async_path[512] = {0};

/* Marks when the background thread is finished (success or failure).
 * Written only by the thread; read only by the main thread after thread joins
 * or via sprites_is_ready(). Using volatile to avoid optimizer removing reads. */
static volatile int g_load_done = 0;
static pthread_t    g_load_thread;
static int          g_thread_started = 0;

/* Attempt to load the atlas PNG.  Runs on the background thread. */
static void *load_thread_fn(void *arg)
{
    const char *path = (const char *)arg;
    int w, h, channels;
    uint8_t *data = stbi_load(path, &w, &h, &channels, 4); /* force RGBA */

    if (!data) {
        const char *reason = stbi_failure_reason();
        snprintf(g_sprite_atlas.error_msg, sizeof(g_sprite_atlas.error_msg),
                 "stbi_load(\"%s\") failed: %s", path, reason ? reason : "unknown");
        fprintf(stderr, "[SPRITES] ERROR: %s\n", g_sprite_atlas.error_msg);
        g_sprite_atlas.load_state = SPRITE_LOAD_ERROR;
    } else {
        /* stb_image returns RGBA uint8_t; convert to 0xAARRGGBB uint32_t
         * (the same format the backbuffer and game.h colors use). */
        int pixel_count = w * h;
        uint32_t *pix = (uint32_t *)malloc((size_t)pixel_count * sizeof(uint32_t));
        if (!pix) {
            snprintf(g_sprite_atlas.error_msg, sizeof(g_sprite_atlas.error_msg),
                     "Out of memory allocating atlas (%d×%d)", w, h);
            fprintf(stderr, "[SPRITES] ERROR: %s\n", g_sprite_atlas.error_msg);
            stbi_image_free(data);
            g_sprite_atlas.load_state = SPRITE_LOAD_ERROR;
        } else {
            const uint8_t *src = data;
            for (int i = 0; i < pixel_count; i++, src += 4) {
                pix[i] = GAME_RGBA(src[0], src[1], src[2], src[3]);
            }
            stbi_image_free(data);

            g_sprite_atlas.pixels     = pix;
            g_sprite_atlas.width      = w;
            g_sprite_atlas.height     = h;
            g_sprite_atlas.loaded     = 1;
            g_sprite_atlas.load_state = SPRITE_LOAD_READY;
            fprintf(stderr, "[SPRITES] Loaded atlas: %s (%d×%d)\n", path, w, h);
        }
    }

    g_load_done = 1;
    return NULL;
}

/* =========================================================================
 * LIFECYCLE
 * ========================================================================= */

void sprites_init(const char *atlas_path)
{
    memset(&g_sprite_atlas, 0, sizeof(g_sprite_atlas));
    g_load_done      = 0;
    g_thread_started = 0;

    if (!atlas_path) {
        /* NULL → intentional placeholder mode; no file I/O needed */
        g_sprite_atlas.load_state = SPRITE_LOAD_IDLE;
        return;
    }

    /* Synchronous: call the same loading logic that the thread would use,
     * but from the main thread.  Simple and safe for startup. */
    strncpy(g_async_path, atlas_path, sizeof(g_async_path) - 1);
    load_thread_fn((void *)g_async_path);
    g_load_done = 1;
}

void sprites_load_async(const char *atlas_path)
{
    memset(&g_sprite_atlas, 0, sizeof(g_sprite_atlas));
    g_load_done      = 0;
    g_thread_started = 0;

    if (!atlas_path) {
        g_sprite_atlas.load_state = SPRITE_LOAD_IDLE;
        g_load_done = 1;
        return;
    }

    strncpy(g_async_path, atlas_path, sizeof(g_async_path) - 1);
    g_sprite_atlas.load_state = SPRITE_LOAD_LOADING;

    int rc = pthread_create(&g_load_thread, NULL, load_thread_fn, (void *)g_async_path);
    if (rc != 0) {
        snprintf(g_sprite_atlas.error_msg, sizeof(g_sprite_atlas.error_msg),
                 "pthread_create failed (rc=%d)", rc);
        fprintf(stderr, "[SPRITES] ERROR: %s\n", g_sprite_atlas.error_msg);
        g_sprite_atlas.load_state = SPRITE_LOAD_ERROR;
        g_load_done = 1;
        return;
    }
    g_thread_started = 1;
}

int sprites_is_ready(void)
{
    return g_load_done;
}

void sprites_shutdown(void)
{
    /* Wait for background thread before freeing */
    if (g_thread_started) {
        pthread_join(g_load_thread, NULL);
        g_thread_started = 0;
    }
    free(g_sprite_atlas.pixels);
    memset(&g_sprite_atlas, 0, sizeof(g_sprite_atlas));
    g_load_done = 0;
}

/* =========================================================================
 * INTERNAL: DRAW PLACEHOLDER
 * ========================================================================= */

/* Magenta checkerboard (Source Engine "missing texture" style).
 * Improved: 8×8 tiles, red border, label strip at bottom so learners
 * can immediately identify which sprite is missing without guessing. */
static void draw_missing_placeholder(Backbuffer *bb,
                                     int dst_x, int dst_y,
                                     int dst_w, int dst_h,
                                     const char *label)
{
    /* Larger 8×8 checkerboard tiles (was 4×4) — easier to see at glance */
    for (int py = 0; py < dst_h; py++) {
        for (int px = 0; px < dst_w; px++) {
            int bx = dst_x + px, by = dst_y + py;
            if (bx < 0 || bx >= bb->width || by < 0 || by >= bb->height) continue;
            uint32_t col = ((px / 8 + py / 8) & 1)
                           ? GAME_RGB(0xFF, 0x00, 0xFF)   /* magenta */
                           : GAME_RGB(0x11, 0x00, 0x11);  /* near-black */
            bb->pixels[by * (bb->pitch / 4) + bx] = col;
        }
    }

    /* Bright red 1-px border around the entire placeholder rect */
    if (dst_w > 2 && dst_h > 2) {
        uint32_t border = GAME_RGB(0xFF, 0x22, 0x22);
        for (int px = 0; px < dst_w; px++) {
            /* Top/bottom */
            int bxt = dst_x + px;
            if (bxt >= 0 && bxt < bb->width) {
                int byt = dst_y; int byb = dst_y + dst_h - 1;
                if (byt >= 0 && byt < bb->height) bb->pixels[byt * (bb->pitch / 4) + bxt] = border;
                if (byb >= 0 && byb < bb->height) bb->pixels[byb * (bb->pitch / 4) + bxt] = border;
            }
        }
        for (int py = 0; py < dst_h; py++) {
            /* Left/right */
            int byl = dst_y + py;
            if (byl >= 0 && byl < bb->height) {
                int bxl = dst_x; int bxr = dst_x + dst_w - 1;
                if (bxl >= 0 && bxl < bb->width) bb->pixels[byl * (bb->pitch / 4) + bxl] = border;
                if (bxr >= 0 && bxr < bb->width) bb->pixels[byl * (bb->pitch / 4) + bxr] = border;
            }
        }
    }

    /* Label strip: black band at bottom with white label text.
     * Only draw if there's enough vertical space (≥14 px). */
    const char *txt = label && label[0] ? label : "???";
    int strip_h = 10;
    if (dst_h >= 14) {
        int sy = dst_y + dst_h - strip_h;
        for (int py2 = 0; py2 < strip_h; py2++) {
            for (int px2 = 0; px2 < dst_w; px2++) {
                int bx2 = dst_x + px2, by2 = sy + py2;
                if (bx2 < 0 || bx2 >= bb->width || by2 < 0 || by2 >= bb->height) continue;
                bb->pixels[by2 * (bb->pitch / 4) + bx2] = GAME_RGB(0x11, 0x00, 0x11);
            }
        }
        int tw = text_width(txt, 1);
        int tx = dst_x + (dst_w - tw) / 2;
        draw_text(bb, tx, sy + 1, txt, GAME_RGB(0xFF, 0xAA, 0xFF), 1);
    } else {
        /* Too small for a strip: draw text centered on the full rect */
        int tw = text_width(txt, 1);
        int tx = dst_x + (dst_w - tw) / 2;
        int ty = dst_y + (dst_h - 8) / 2;
        draw_text(bb, tx, ty, txt, GAME_RGB(0xFF, 0xFF, 0xFF), 1);
    }
}

/* =========================================================================
 * INTERNAL: ATLAS BLIT (nearest-neighbour)
 * ========================================================================= */

static void blit_sprite(Backbuffer *bb, const SpriteDef *def,
                        int dst_x, int dst_y, int dst_w, int dst_h)
{
    int stride = g_sprite_atlas.width;
    for (int dy = 0; dy < dst_h; dy++) {
        int sy = def->src_y + dy * def->src_h / dst_h;
        if (sy < 0 || sy >= g_sprite_atlas.height) continue;
        for (int dx = 0; dx < dst_w; dx++) {
            int sx = def->src_x + dx * def->src_w / dst_w;
            if (sx < 0 || sx >= g_sprite_atlas.width) continue;
            uint32_t pixel = g_sprite_atlas.pixels[sy * stride + sx];
            int bx = dst_x + dx, by = dst_y + dy;
            if (bx < 0 || bx >= bb->width || by < 0 || by >= bb->height) continue;
            /* Skip fully transparent pixels */
            if ((pixel >> 24) == 0) continue;
            bb->pixels[by * (bb->pitch / 4) + bx] = pixel;
        }
    }
}

/* =========================================================================
 * DRAWING
 * ========================================================================= */

void draw_sprite(Backbuffer *bb, SpriteId id,
                 int dst_x, int dst_y, int dst_w, int dst_h)
{
    /* Out-of-range ID or atlas load error → magenta "???" */
    if (id < 0 || id >= SPR_COUNT || id == SPR_MISSING ||
        g_sprite_atlas.load_state == SPRITE_LOAD_ERROR) {
        const char *lbl = (id >= 0 && id < SPR_COUNT) ? SPRITE_DEFS[id].label : "???";
        draw_missing_placeholder(bb, dst_x, dst_y, dst_w, dst_h, lbl);
        return;
    }

    const SpriteDef *def = &SPRITE_DEFS[id];

    if (!g_sprite_atlas.loaded) {
        /* Placeholder mode or still loading → colored rect + label */
        draw_rect(bb, dst_x, dst_y, dst_w, dst_h, def->placeholder_color);
        if (def->label && def->label[0] != '\0') {
            int tw = text_width(def->label, 1);
            int tx = dst_x + (dst_w - tw) / 2;
            int ty = dst_y + (dst_h - 8) / 2;
            draw_text(bb, tx, ty, def->label, GAME_RGB(0x00, 0x00, 0x00), 1);
        }
        return;
    }

    /* Atlas is ready — blit the src rect */
    blit_sprite(bb, def, dst_x, dst_y, dst_w, dst_h);
}

void draw_sprite_frame(Backbuffer *bb, SpriteId id, int frame,
                       int dst_x, int dst_y, int dst_w, int dst_h)
{
    if (id < 0 || id >= SPR_COUNT || id == SPR_MISSING) {
        draw_missing_placeholder(bb, dst_x, dst_y, dst_w, dst_h, "???");
        return;
    }

    if (!g_sprite_atlas.loaded) {
        /* Placeholder: ignore frame number */
        (void)frame;
        draw_sprite(bb, id, dst_x, dst_y, dst_w, dst_h);
        return;
    }

    /* In atlas mode, each animation frame is one frame-width to the right */
    const SpriteDef *def = &SPRITE_DEFS[id];
    SpriteDef frame_def  = *def;
    frame_def.src_x     += frame * def->src_w;
    blit_sprite(bb, &frame_def, dst_x, dst_y, dst_w, dst_h);
}
