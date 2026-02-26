/*
 * main_x11.c  —  Sugar, Sugar | X11 Platform Backend
 *
 * Implements the platform.h contract using:
 *   - Xlib        (window creation, event polling)
 *   - XImage      (software pixel blit to display)
 *   - clock_gettime (monotonic timer)
 *
 * Build:
 *   clang -Wall -Wextra -O2 -o sugar src/main_x11.c src/game.c src/levels.c -lX11 -lm
 *
 * Debug build:
 *   clang -Wall -Wextra -O0 -g -fsanitize=address,undefined \
 *         -o sugar_dbg src/main_x11.c src/game.c src/levels.c -lX11 -lm
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <time.h>    /* clock_gettime  */
#include <stdlib.h>  /* malloc, free   */
#include <string.h>  /* memset         */

#include "platform.h"
#include "sounds.h"

/* ===================================================================
 * PLATFORM GLOBALS
 * These live in this file only — game.c never sees them.
 * =================================================================== */
static Display *g_display;
static Window   g_window;
static GC       g_gc;
static XImage  *g_ximage;
static int      g_screen;
static int      g_should_quit;

/* We keep a pointer to the shared backbuffer pixel array so XImage
 * can reference it without a copy. */
static uint32_t *g_pixel_data;
static int       g_pixel_w;
static int       g_pixel_h;

/* ===================================================================
 * PLATFORM IMPLEMENTATION
 * =================================================================== */

void platform_init(const char *title, int width, int height) {
    g_display = XOpenDisplay(NULL); /* connect to the X server */
    if (!g_display) {
        /* If XOpenDisplay fails, we can't render anything — hard exit. */
        return;
    }
    g_screen = DefaultScreen(g_display);

    /* Create a simple window */
    g_window = XCreateSimpleWindow(
        g_display,
        RootWindow(g_display, g_screen),
        0, 0,              /* x, y — window manager will reposition */
        (unsigned)width,
        (unsigned)height,
        1,                 /* border width */
        BlackPixel(g_display, g_screen),
        WhitePixel(g_display, g_screen)
    );

    /* Register for the events we care about */
    XSelectInput(g_display, g_window,
                 ExposureMask       |  /* window needs a repaint        */
                 KeyPressMask       |  /* key pressed                   */
                 KeyReleaseMask     |  /* key released                  */
                 ButtonPressMask    |  /* mouse button pressed          */
                 ButtonReleaseMask  |  /* mouse button released         */
                 PointerMotionMask  |  /* mouse moved while button held */
                 Button1MotionMask);   /* motion with left button held  */

    /* Tell the window manager we want window-close events */
    Atom wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &wm_delete, 1);

    /* Set the window title */
    XStoreName(g_display, g_window, title);

    /* Show the window */
    XMapWindow(g_display, g_window);
    XFlush(g_display);

    /* Create a graphics context for XPutImage */
    g_gc = XCreateGC(g_display, g_window, 0, NULL);
}

double platform_get_time(void) {
    /* CLOCK_MONOTONIC never jumps backward (unlike gettimeofday).
     * This is the correct clock to use for delta-time calculations. */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

void platform_get_input(GameInput *input) {
    /* XPending returns the number of events in the queue without blocking. */
    Atom wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);

    /* ---- Save start-of-frame mouse position BEFORE processing events ----
     *
     * X11 can queue MANY MotionNotify events between frames (e.g. 10+
     * events when the mouse moves fast).  If we update prev_x inside the
     * event loop, by the time game_update runs, prev_x/y only holds the
     * position from the second-to-last event — all earlier segments of the
     * mouse path are lost.  draw_brush_line(prev_x, prev_y, x, y) then
     * draws only the tiny last segment, leaving visible gaps.
     *
     * Fix: snapshot the position once here, before the event loop.
     * After all events are consumed, x/y = end-of-frame position and
     * prev_x/y = start-of-frame position.  Bresenham in draw_brush_line
     * interpolates the full path in one call, covering every pixel the
     * mouse crossed regardless of how many events were queued. */
    input->mouse.prev_x = input->mouse.x;
    input->mouse.prev_y = input->mouse.y;

    while (XPending(g_display)) {
        XEvent event;
        XNextEvent(g_display, &event);

        switch (event.type) {

        case MotionNotify:
            /* Update current position only — prev is already saved above. */
            input->mouse.x = event.xmotion.x;
            input->mouse.y = event.xmotion.y;
            break;

        case ButtonPress:
            if (event.xbutton.button == Button1) {
                /* Also update position so prev matches current on click */
                input->mouse.prev_x = event.xbutton.x;
                input->mouse.prev_y = event.xbutton.y;
                input->mouse.x      = event.xbutton.x;
                input->mouse.y      = event.xbutton.y;
                UPDATE_BUTTON(input->mouse.left, 1);
            }
            break;

        case ButtonRelease:
            if (event.xbutton.button == Button1)
                UPDATE_BUTTON(input->mouse.left, 0);
            break;

        case KeyPress: {
            KeySym sym = XLookupKeysym(&event.xkey, 0);
            if (sym == XK_Escape)            UPDATE_BUTTON(input->escape,  1);
            if (sym == XK_r || sym == XK_R)  UPDATE_BUTTON(input->reset,   1);
            if (sym == XK_g || sym == XK_G)  UPDATE_BUTTON(input->gravity, 1);
            if (sym == XK_Return || sym == XK_space) UPDATE_BUTTON(input->enter, 1);
            break;
        }

        case KeyRelease: {
            KeySym sym = XLookupKeysym(&event.xkey, 0);
            if (sym == XK_Escape)            UPDATE_BUTTON(input->escape,  0);
            if (sym == XK_r || sym == XK_R)  UPDATE_BUTTON(input->reset,   0);
            if (sym == XK_g || sym == XK_G)  UPDATE_BUTTON(input->gravity, 0);
            if (sym == XK_Return || sym == XK_space) UPDATE_BUTTON(input->enter, 0);
            break;
        }

        case ClientMessage:
            /* Window close button (the ✕) */
            if ((Atom)event.xclient.data.l[0] == wm_delete)
                g_should_quit = 1;
            break;

        case Expose:
            /* The window was uncovered — we'll redraw next frame anyway */
            break;
        }
    }
}

void platform_display_backbuffer(const GameBackbuffer *backbuffer) {
    /*
     * On the first call, create the XImage that wraps our pixel buffer.
     * XCreateImage does NOT copy the data — it references g_pixel_data
     * directly.  So every frame, XPutImage reads from the same memory
     * that game_render() just wrote to.
     */
    if (!g_ximage) {
        g_pixel_data = backbuffer->pixels;
        g_pixel_w    = backbuffer->width;
        g_pixel_h    = backbuffer->height;

        /* Create the XImage.  The pixel format 0xAARRGGBB stored
         * little-endian as [B,G,R,A] maps correctly to X11's default
         * TrueColor visual on x86 Linux (blue_mask = 0xFF, red_mask = 0xFF0000). */
        g_ximage = XCreateImage(
            g_display,
            DefaultVisual(g_display, g_screen),
            (unsigned)DefaultDepth(g_display, g_screen),
            ZPixmap,
            0,                        /* offset */
            (char *)backbuffer->pixels,
            (unsigned)backbuffer->width,
            (unsigned)backbuffer->height,
            32,                       /* bitmap_pad: 32-bit aligned */
            backbuffer->pitch         /* bytes per row */
        );
        /* Explicitly declare byte order so X11 doesn't misinterpret
         * our 0xAARRGGBB pixels on big-endian hosts or unusual displays. */
        if (g_ximage) g_ximage->byte_order = LSBFirst;
    }

    /* Upload our pixel buffer to the X11 window */
    XPutImage(g_display, g_window, g_gc, g_ximage,
              0, 0,  /* src x, y */
              0, 0,  /* dst x, y */
              (unsigned)backbuffer->width,
              (unsigned)backbuffer->height);
    XFlush(g_display);
}

/* -----------------------------------------------------------------------
 * Audio stubs  — X11 backend does not implement audio in this course.
 * To add audio, replace these with ALSA (libasound) calls.
 * ----------------------------------------------------------------------- */
void platform_play_sound(int sound_id) { (void)sound_id; }
void platform_play_music(int music_id) { (void)music_id; }
void platform_stop_music(void)         {}

/* ===================================================================
 * MAIN LOOP
 * =================================================================== */

int main(void) {
    /* Declare as static so the ~460 KB of arrays in GameState and
     * the 2.5 MB backbuffer pixel array are placed in the BSS segment
     * (zero-initialised at startup) rather than on the stack.
     *
     * JS analogy: this is like module-level variables — they exist for
     * the lifetime of the program, not just one function call. */
    static GameState     state;
    static GameBackbuffer bb;

    /* Allocate the pixel buffer on the heap.
     * 640 × 480 × 4 bytes = 1,228,800 bytes ≈ 1.2 MB. */
    bb.pixels = (uint32_t *)malloc((size_t)(CANVAS_W * CANVAS_H) * sizeof(uint32_t));
    if (!bb.pixels) return 1; /* out of memory */

    /* Open the window (must happen before game_init so bb dimensions work) */
    platform_init("Sugar, Sugar", CANVAS_W, CANVAS_H);

    /* Initialise the game: zero state, load level 0, set backbuffer dims */
    game_init(&state, &bb);

    GameInput input;
    memset(&input, 0, sizeof(input));

    /* Delta-time game loop */
    double prev_time = platform_get_time();

    while (!g_should_quit && !state.should_quit) {
        double curr_time  = platform_get_time();
        float  delta_time = (float)(curr_time - prev_time);
        prev_time = curr_time;

        /* Cap delta-time: if we were paused in a debugger for 10 seconds,
         * we don't want the physics to explode. */
        if (delta_time > 0.1f) delta_time = 0.1f;

        /* Step 1: clear per-frame input counters */
        prepare_input_frame(&input);

        /* Step 2: read OS events → fill input struct */
        platform_get_input(&input);

        /* Step 3: advance the simulation */
        game_update(&state, &input, delta_time);

        /* Step 4: draw the current frame into the pixel buffer */
        game_render(&state, &bb);

        /* Step 5: send the pixel buffer to the X11 window */
        platform_display_backbuffer(&bb);

        /* ---- Frame rate cap: target 60 fps ----
         * Without a cap the game runs at thousands of fps.  dt is then
         * microscopic (< 0.0001 s), grains move < 0.005 px per frame, and
         * the displacement-based settle check immediately marks every grain
         * as "still" — causing them to bake and vanish before they are even
         * rendered once.
         *
         * nanosleep sleeps for the remainder of the ~16.7 ms budget.
         * If the frame took longer than 16.7 ms we skip the sleep. */
        {
            double frame_end     = platform_get_time();
            double elapsed       = frame_end - curr_time;
            double target        = 1.0 / 60.0;
            if (elapsed < target) {
                double sleep_s = target - elapsed;
                struct timespec ts;
                ts.tv_sec  = (time_t)sleep_s;
                ts.tv_nsec = (long)((sleep_s - (double)ts.tv_sec) * 1e9);
                nanosleep(&ts, NULL);
            }
        }
    }

    /* Cleanup */
    if (g_ximage) {
        /* Prevent XDestroyImage from freeing our pixel data
         * (we'll free it ourselves below). */
        g_ximage->data = NULL;
        XDestroyImage(g_ximage);
    }
    if (g_gc)      XFreeGC(g_display, g_gc);
    if (g_window)  XDestroyWindow(g_display, g_window);
    if (g_display) XCloseDisplay(g_display);
    free(bb.pixels);

    return 0;
}
