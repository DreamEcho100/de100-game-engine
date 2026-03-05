/* src/main_x11.c  —  Desktop Tower Defense | X11 Platform Backend
 *
 * Implements the platform.h contract using:
 *   - Xlib        (window creation, event polling)
 *   - XImage      (software pixel blit to display)
 *   - clock_gettime (monotonic timer)
 *   - ALSA        (audio output, optional — build with -DALSA_AVAILABLE -lasound)
 *
 * The window is kept at a fixed 760×520 — X11 has no built-in scaling for
 * XImage blits, so we lock the size via XSizeHints rather than attempting
 * software scaling.  The Raylib backend supports letterbox scaling.
 *
 * Build:
 *   clang -Wall -Wextra -O2 -o build/game_x11 \
 *         src/main_x11.c src/game.c src/levels.c src/audio.c -lX11 -lm
 *
 * With audio:
 *   clang -Wall -Wextra -O2 -DALSA_AVAILABLE -o build/game_x11 \
 *         src/main_x11.c src/game.c src/levels.c src/audio.c -lX11 -lasound -lm
 */

/* Expose POSIX extensions: clock_gettime, nanosleep, struct timespec */
#define _POSIX_C_SOURCE 199309L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>  /* XK_Escape and other keysym constants */

#include <time.h>    /* clock_gettime, nanosleep */
#include <stdlib.h>  /* malloc, free             */
#include <string.h>  /* memset                   */

#include "platform.h"

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

/* Pointer to the backbuffer pixel array (XImage references it without copy) */
static uint32_t *g_pixel_data;
static int       g_pixel_w;
static int       g_pixel_h;

/* Raw X11 left-button tracking (persists across frames) */
static int g_mouse_left_down = 0;

/* WM_DELETE_WINDOW atom — cached once in platform_init, used every frame.
 * Re-interning every frame via XInternAtom() was an O(round-trip) per frame. */
static Atom g_wm_delete = 0;

/* ===================================================================
 * ALSA AUDIO GLOBALS (compiled out when ALSA_AVAILABLE is not defined)
 * =================================================================== */
#ifdef ALSA_AVAILABLE
#include <alloca.h>           /* required for snd_pcm_*_alloca macros with clang */
#include <alsa/asoundlib.h>

static snd_pcm_t *g_pcm        = NULL;
static int        g_audio_init = 0;
#endif

/* ===================================================================
 * PLATFORM IMPLEMENTATION
 * =================================================================== */

void platform_init(const char *title, int width, int height) {
    g_display = XOpenDisplay(NULL); /* connect to the X server */
    if (!g_display) return;

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

    /* Lock window size: X11 cannot scale XImage blits so we fix dimensions.
     * The Raylib backend handles letterbox scaling instead. */
    XSizeHints *hints = XAllocSizeHints();
    if (hints) {
        hints->flags      = PMinSize | PMaxSize;
        hints->min_width  = hints->max_width  = width;
        hints->min_height = hints->max_height = height;
        XSetWMNormalHints(g_display, g_window, hints);
        XFree(hints);
    }

    /* Register for events we care about — KeyPressMask is required for
     * keyboard input (Escape to quit); without it X11 never delivers KeyPress events. */
    XSelectInput(g_display, g_window,
                 ExposureMask        |
                 ButtonPressMask     |
                 ButtonReleaseMask   |
                 PointerMotionMask   |
                 Button1MotionMask   |
                 KeyPressMask        |
                 KeyReleaseMask      |
                 StructureNotifyMask);

    /* Tell the window manager we want window-close events.
     * Cache the atom — looking it up every frame was an unnecessary round-trip. */
    g_wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &g_wm_delete, 1);

    XStoreName(g_display, g_window, title);
    XMapWindow(g_display, g_window);
    XFlush(g_display);

    /* Create a graphics context for XPutImage */
    g_gc = XCreateGC(g_display, g_window, 0, NULL);
}

double platform_get_time(void) {
    /* CLOCK_MONOTONIC never jumps backward — correct for delta-time. */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

void platform_get_input(MouseState *mouse) {
    /* Reset single-frame edge flags before processing new events.
     * CRITICAL: left_pressed and left_released are edge-triggered — they must
     * be cleared every frame so a click on one screen doesn't carry over
     * into the next (e.g. PLAY button click must not auto-select a mod). */
    mouse->left_pressed  = 0;
    mouse->left_released = 0;
    mouse->right_pressed = 0;

    /* Save start-of-frame position so we capture full mouse path */
    mouse->prev_x = mouse->x;
    mouse->prev_y = mouse->y;

    while (XPending(g_display)) {
        XEvent event;
        XNextEvent(g_display, &event);

        switch (event.type) {

        case MotionNotify:
            mouse->x = event.xmotion.x;
            mouse->y = event.xmotion.y;
            break;

        case ButtonPress:
            if (event.xbutton.button == Button1) {
                /* Sync position on click so prev matches current */
                mouse->prev_x = event.xbutton.x;
                mouse->prev_y = event.xbutton.y;
                mouse->x      = event.xbutton.x;
                mouse->y      = event.xbutton.y;
                g_mouse_left_down = 1;
                UPDATE_BUTTON(mouse->left_pressed, mouse->left_released,
                              mouse->left_down, g_mouse_left_down);
            }
            if (event.xbutton.button == Button3) {
                mouse->right_pressed = 1;
            }
            break;

        case ButtonRelease:
            if (event.xbutton.button == Button1) {
                g_mouse_left_down = 0;
                UPDATE_BUTTON(mouse->left_pressed, mouse->left_released,
                              mouse->left_down, g_mouse_left_down);
            }
            break;

        case ClientMessage:
            /* Window close button (the ✕) — uses the cached g_wm_delete atom */
            if ((Atom)event.xclient.data.l[0] == g_wm_delete)
                g_should_quit = 1;
            break;

        case KeyPress: {
            /* Escape key → quit immediately (works even mid-wave) */
            KeySym ks = XLookupKeysym(&event.xkey, 0);
            if (ks == XK_Escape) g_should_quit = 1;
            break;
        }

        case Expose:
        case ConfigureNotify:
            /* Window uncovered or resized (locked size) — handle to keep queue clean */
            break;
        }
    }
}

void platform_display_backbuffer(const Backbuffer *bb) {
    /*
     * On the first call, create the XImage that wraps our pixel buffer.
     * XCreateImage does NOT copy the data — it references g_pixel_data
     * directly.  So every frame, XPutImage reads from the same memory
     * that game_render() just wrote to.
     *
     * The pixel format 0xAARRGGBB stored little-endian as [B,G,R,A] maps
     * correctly to X11's default TrueColor visual on x86 Linux
     * (blue_mask = 0xFF, red_mask = 0xFF0000). No R↔B swap needed.
     */
    if (!g_ximage) {
        g_pixel_data = bb->pixels;
        g_pixel_w    = bb->width;
        g_pixel_h    = bb->height;

        g_ximage = XCreateImage(
            g_display,
            DefaultVisual(g_display, g_screen),
            (unsigned)DefaultDepth(g_display, g_screen),
            ZPixmap,
            0,                      /* offset */
            (char *)bb->pixels,
            (unsigned)bb->width,
            (unsigned)bb->height,
            32,                     /* bitmap_pad: 32-bit aligned */
            bb->pitch               /* bytes per row */
        );
        if (g_ximage) g_ximage->byte_order = LSBFirst;
    }

    XPutImage(g_display, g_window, g_gc, g_ximage,
              0, 0,  /* src x, y */
              0, 0,  /* dst x, y */
              (unsigned)bb->width,
              (unsigned)bb->height);
    XFlush(g_display);
}

/* -----------------------------------------------------------------------
 * Audio (ALSA) — chunk-based model
 *
 * Each frame we push exactly AUDIO_CHUNK_SIZE frames to ALSA.
 * The sw_params start threshold of 1 means playback begins on the
 * first write — no pre-fill needed.
 *
 * Build: -DALSA_AVAILABLE -lasound
 * ----------------------------------------------------------------------- */

#ifdef ALSA_AVAILABLE

void platform_audio_init(GameState *state, int hz) {
    (void)state; /* game_audio_init called by game_init */

    if (snd_pcm_open(&g_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        g_pcm = NULL;
        return;
    }

    /* ── Hardware parameters ────────────────────────────────────────── */
    snd_pcm_hw_params_t *hw;
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(g_pcm, hw);
    snd_pcm_hw_params_set_access(g_pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(g_pcm, hw, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(g_pcm, hw, 2);

    unsigned int rate = (unsigned int)hz;
    snd_pcm_hw_params_set_rate_near(g_pcm, hw, &rate, 0);

    snd_pcm_uframes_t period = AUDIO_CHUNK_SIZE;
    snd_pcm_hw_params_set_period_size_near(g_pcm, hw, &period, 0);

    if (snd_pcm_hw_params(g_pcm, hw) < 0) {
        snd_pcm_close(g_pcm);
        g_pcm = NULL;
        return;
    }

    /* ── Software parameters: low-latency start ─────────────────────── */
    snd_pcm_sw_params_t *sw;
    snd_pcm_sw_params_alloca(&sw);
    snd_pcm_sw_params_current(g_pcm, sw);
    snd_pcm_sw_params_set_start_threshold(g_pcm, sw, 1);
    snd_pcm_sw_params(g_pcm, sw);

    g_audio_init = 1;
}

void platform_audio_update(GameState *state) {
    if (!g_audio_init || !g_pcm) return;

    /* Only write if the ALSA ring buffer has room for a full chunk.
     * snd_pcm_writei() blocks by default — skipping when the buffer is
     * nearly full prevents a ~46 ms stall that would delay window-close events. */
    snd_pcm_sframes_t avail = snd_pcm_avail_update(g_pcm);
    if (avail < (snd_pcm_sframes_t)AUDIO_CHUNK_SIZE) return;

    static int16_t buf[AUDIO_CHUNK_SIZE * 2]; /* stereo interleaved */
    AudioOutputBuffer out = {
        .samples            = buf,
        .samples_per_second = AUDIO_SAMPLE_RATE,
        .sample_count       = AUDIO_CHUNK_SIZE,
    };
    game_get_audio_samples(state, &out);

    snd_pcm_sframes_t written = snd_pcm_writei(g_pcm, buf, (snd_pcm_uframes_t)AUDIO_CHUNK_SIZE);
    if (written < 0) {
        /* Recover from underrun or suspend */
        snd_pcm_recover(g_pcm, (int)written, 0);
    }
}

void platform_audio_shutdown(void) {
    if (!g_audio_init || !g_pcm) return;
    /* snd_pcm_drop() discards buffered audio immediately.
     * snd_pcm_drain() would block until all queued audio plays out (~seconds). */
    snd_pcm_drop(g_pcm);
    snd_pcm_close(g_pcm);
    g_pcm = NULL;
    g_audio_init = 0;
}

#else /* !ALSA_AVAILABLE — silent stubs so the build always succeeds */

void platform_audio_init(GameState *state, int hz) { (void)state; (void)hz; }
void platform_audio_update(GameState *state)        { (void)state; }
void platform_audio_shutdown(void)                  {}

#endif /* ALSA_AVAILABLE */

/* ===================================================================
 * MAIN LOOP
 * =================================================================== */

int main(void) {
    static GameState  state;
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

    while (!g_should_quit && !state.should_quit) {
        double curr_time = platform_get_time();
        float  dt        = (float)(curr_time - prev_time);
        prev_time = curr_time;

        /* Cap delta-time: prevent physics explosion after debugger pauses */
        if (dt > 0.1f) dt = 0.1f;

        platform_get_input(&state.mouse);
        game_update(&state, dt);
        game_render(&state, &bb);
        platform_display_backbuffer(&bb);
        platform_audio_update(&state);

        /* ---- Frame rate cap: target 60 fps ----
         * nanosleep for the remainder of the ~16.7 ms frame budget.
         * If the frame already took longer we skip the sleep. */
        {
            double frame_end = platform_get_time();
            double elapsed   = frame_end - curr_time;
            double target    = 1.0 / 60.0;
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
    platform_audio_shutdown();
    if (g_ximage) {
        /* Prevent XDestroyImage from freeing our pixel data
         * (we free it ourselves below). */
        g_ximage->data = NULL;
        XDestroyImage(g_ximage);
    }
    if (g_gc)      XFreeGC(g_display, g_gc);
    if (g_window)  XDestroyWindow(g_display, g_window);
    if (g_display) XCloseDisplay(g_display);
    free(bb.pixels);

    return 0;
}
