/* =============================================================================
   main_x11.c — X11 Platform Backend
   =============================================================================
   Three important differences from a naive first attempt:

   1. DOUBLE BUFFERING (no flicker)
      We draw into an offscreen Pixmap (g_backbuffer), then copy the finished
      frame to the window in one XCopyArea call. The user never sees a partial
      frame.

   2. KEY REPEAT FIX (responsive input)
      X11 auto-repeat sends a fake KeyRelease+KeyPress pair every ~30ms while
      a key is held. Without detection, each pair fires "released", causing the
      frog to hop repeatedly from one tap. We peek ahead and skip fake releases.

   3. PIXEL-SMOOTH SCROLL (no sprite jumping)
      We use lane_scroll() from frogger.h which works in PIXEL units and handles
      negative speeds correctly. See the comment in frogger.h for details.

   Build:
     gcc -Wall -Wextra -std=c11 -o frogger_x11 src/frogger.c src/main_x11.c -lX11
   ============================================================================= */

#define _POSIX_C_SOURCE 200809L

#include "frogger.h"
#include "platform.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* -------------------------------------------------------------------------
   X11 module state
   ------------------------------------------------------------------------- */
static Display *g_display   = NULL;
static Window   g_window    = 0;
static Pixmap   g_backbuffer = 0;   /* ← offscreen buffer for double-buffering */
static GC       g_gc        = 0;
static Atom     g_wm_delete;
static int      g_quit      = 0;
static int      g_win_w     = 0;
static int      g_win_h     = 0;

/* Key held state — 0=up, 1=down. Index: 0=UP 1=DOWN 2=LEFT 3=RIGHT */
static int g_key_down[4] = {0};

/* -------------------------------------------------------------------------
   Helper: RGB bytes → X11 pixel value (0xRRGGBB on TrueColor display)
   ------------------------------------------------------------------------- */
static unsigned long rgb_to_pixel(uint8_t r, uint8_t g, uint8_t b) {
    return ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b;
}

/* -------------------------------------------------------------------------
   draw_sprite_partial — draw a region of a sprite to g_backbuffer.

   NOTE: We draw to g_backbuffer (offscreen), not g_window.
   XCopyArea in platform_render copies the finished frame to the window.

   🔴 HOT PATH: sets foreground + fills rect per sprite cell.
   For this game (~180 tiles × 64 cells each = ~11,520 cells/frame) it is
   fast enough. A production game would batch into one XImage per frame.
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

            if (gl == 0x0020) continue; /* space = transparent */

            int ci = c & 0x0F;
            unsigned long pixel = rgb_to_pixel(
                CONSOLE_PALETTE[ci][0],
                CONSOLE_PALETTE[ci][1],
                CONSOLE_PALETTE[ci][2]);

            XSetForeground(g_display, g_gc, pixel);
            XFillRectangle(g_display, g_backbuffer, g_gc,   /* ← backbuffer */
                dest_px_x + sx * CELL_PX,
                dest_px_y + sy * CELL_PX,
                CELL_PX, CELL_PX);
        }
    }
}

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

/* =========================================================================
   PLATFORM FUNCTION IMPLEMENTATIONS
   ========================================================================= */

void platform_init(int width, int height, const char *title) {
    g_win_w = width;
    g_win_h = height;

    g_display = XOpenDisplay(NULL);
    if (!g_display) {
        fprintf(stderr, "FATAL: Cannot open X display\n");
        return;
    }

    int screen    = DefaultScreen(g_display);
    Window root   = RootWindow(g_display, screen);
    int depth     = DefaultDepth(g_display, screen);

    g_window = XCreateSimpleWindow(
        g_display, root, 0, 0, width, height, 0, 0, 0);

    XSelectInput(g_display, g_window,
        KeyPressMask | KeyReleaseMask | ExposureMask | StructureNotifyMask);

    g_wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &g_wm_delete, 1);
    XStoreName(g_display, g_window, title);

    /* Create GC before the pixmap (pixmap init needs it for the clear) */
    g_gc = XCreateGC(g_display, g_window, 0, NULL);

    /* -----------------------------------------------------------------------
       DOUBLE BUFFER: Create an offscreen Pixmap the same size as the window.
       All rendering draws here; XCopyArea blits it to screen at the end.
       This eliminates flickering because the window never shows a partial frame.
       JS analogy: like drawing to an offscreen <canvas>, then ctx.drawImage.
       ----------------------------------------------------------------------- */
    g_backbuffer = XCreatePixmap(g_display, g_window, width, height, depth);

    /* Clear backbuffer to black */
    XSetForeground(g_display, g_gc, 0x000000);
    XFillRectangle(g_display, g_backbuffer, g_gc, 0, 0, width, height);

    XMapWindow(g_display, g_window);
    XFlush(g_display);
}

void platform_get_input(InputState *input) {
    memset(input, 0, sizeof(InputState));

    while (XPending(g_display)) {
        XEvent e;
        XNextEvent(g_display, &e);

        if (e.type == ClientMessage) {
            if ((Atom)e.xclient.data.l[0] == g_wm_delete)
                g_quit = 1;
        }

        if (e.type == KeyPress) {
            KeySym sym = XLookupKeysym(&e.xkey, 0);
            if (sym == XK_Up    || sym == XK_w) g_key_down[0] = 1;
            if (sym == XK_Down  || sym == XK_s) g_key_down[1] = 1;
            if (sym == XK_Left  || sym == XK_a) g_key_down[2] = 1;
            if (sym == XK_Right || sym == XK_d) g_key_down[3] = 1;
            if (sym == XK_Escape) g_quit = 1;
        }

        if (e.type == KeyRelease) {
            /* ---------------------------------------------------------------
               KEY REPEAT DETECTION
               When a key is held, X11 auto-repeat fires at ~30 Hz:
                 KeyRelease  (fake)
                 KeyPress    (fake, same keycode, same timestamp)
                 KeyRelease  (fake)
                 KeyPress    (fake) ...

               We detect this by peeking at the next event. If it is a
               KeyPress for the same keycode at the same timestamp, it is
               an auto-repeat pair — consume the KeyPress and ignore both.
               A real release has NO matching KeyPress immediately after.
               --------------------------------------------------------------- */
            int is_repeat = 0;
            if (XEventsQueued(g_display, QueuedAfterReading)) {
                XEvent ahead;
                XPeekEvent(g_display, &ahead);
                if (ahead.type == KeyPress &&
                    ahead.xkey.keycode == e.xkey.keycode &&
                    ahead.xkey.time    == e.xkey.time) {
                    XNextEvent(g_display, &ahead); /* consume the fake KeyPress */
                    is_repeat = 1;
                }
            }

            if (!is_repeat) {
                KeySym sym = XLookupKeysym(&e.xkey, 0);
                if (sym == XK_Up    || sym == XK_w) { input->up_released    = 1; g_key_down[0] = 0; }
                if (sym == XK_Down  || sym == XK_s) { input->down_released  = 1; g_key_down[1] = 0; }
                if (sym == XK_Left  || sym == XK_a) { input->left_released  = 1; g_key_down[2] = 0; }
                if (sym == XK_Right || sym == XK_d) { input->right_released = 1; g_key_down[3] = 0; }
            }
        }
    }
}

void platform_render(const GameState *state) {
    /* Step 1: clear the BACKBUFFER to black (not the window) */
    XSetForeground(g_display, g_gc, 0x000000);
    XFillRectangle(g_display, g_backbuffer, g_gc, 0, 0, g_win_w, g_win_h);

    /* Step 2: draw lanes to backbuffer */
    for (int y = 0; y < NUM_LANES; y++) {
        int tile_start, px_offset;
        /* lane_scroll gives pixel-accurate offset — no 8-pixel snapping */
        lane_scroll(state->time, RENDER_LANE_SPEEDS[y], &tile_start, &px_offset);

        int dest_py = y * TILE_PX;  /* pixel Y for this row */

        for (int i = 0; i < LANE_WIDTH; i++) {
            char c    = RENDER_LANE_PATTERNS[y][(tile_start + i) % LANE_PATTERN_LEN];
            int src_x = 0;
            int spr   = local_tile_to_sprite(c, &src_x);

            /* Tile X in pixels — sub-tile smooth because px_offset is in pixels */
            int dest_px = (-1 + i) * TILE_PX - px_offset;

            if (spr >= 0) {
                draw_sprite_partial(&state->sprites, spr,
                    src_x, 0, TILE_CELLS, TILE_CELLS,
                    dest_px, dest_py);
            }
            /* '.' (road) stays black — already cleared */
        }
    }

    /* Step 3: draw frog */
    int show_frog = 1;
    if (state->dead) {
        int flash = (int)(state->dead_timer / 0.05f);
        show_frog = (flash % 2 == 0);
    }
    if (show_frog) {
        /* Pixel-accurate position — no intermediate TILE_CELLS cast.
           Old: (int)(frog_x * TILE_CELLS) * CELL_PX  → snaps to 8px grid.
           New: (int)(frog_x * TILE_PX)               → 1px precision.
           At river speed=3, frog moves 3px/frame. Old formula froze for
           ~2 frames then jumped 8px (8px / 3px = 2.6 frames). Now smooth. */
        int frog_px = (int)(state->frog_x * (float)TILE_PX);
        int frog_py = (int)(state->frog_y * (float)TILE_PX);
        draw_sprite_partial(&state->sprites, SPR_FROG,
            0, 0,
            state->sprites.widths[SPR_FROG],
            state->sprites.heights[SPR_FROG],
            frog_px, frog_py);
    }

    /* Step 4: HUD text */
    char score_buf[64];
    snprintf(score_buf, sizeof(score_buf), "Homes: %d", state->homes_reached);
    XSetForeground(g_display, g_gc, 0xFFFFFF);
    XDrawString(g_display, g_backbuffer, g_gc,   /* ← backbuffer */
        8, 20, score_buf, (int)strlen(score_buf));

    if (state->dead) {
        XSetForeground(g_display, g_gc, 0xFF0000);
        const char *msg = "DEAD!";
        XDrawString(g_display, g_backbuffer, g_gc,
            SCREEN_PX_W/2 - 20, SCREEN_PX_H/2,
            msg, (int)strlen(msg));
    }

    if (state->homes_reached >= 3) {
        XSetForeground(g_display, g_gc, 0xFFFF00);
        const char *win = "YOU WIN!";
        XDrawString(g_display, g_backbuffer, g_gc,
            SCREEN_PX_W/2 - 24, SCREEN_PX_H/2 - 20,
            win, (int)strlen(win));
    }

    /* Step 5: BLIT — copy finished frame from backbuffer to window in ONE operation.
       This is the key to flicker-free rendering. The user only ever sees the
       complete, fully-drawn frame.                                           */
    XCopyArea(g_display, g_backbuffer, g_window, g_gc,
              0, 0, g_win_w, g_win_h, 0, 0);

    XFlush(g_display);
}

void platform_sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int platform_should_quit(void) {
    return g_quit;
}

void platform_shutdown(void) {
    if (g_backbuffer) XFreePixmap(g_display, g_backbuffer);
    if (g_gc)         XFreeGC(g_display, g_gc);
    if (g_display)    XCloseDisplay(g_display);
}

#ifndef ASSETS_DIR
#define ASSETS_DIR "assets"
#endif

int main(void) {
    frogger_run(ASSETS_DIR);
    return 0;
}
