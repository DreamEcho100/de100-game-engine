/* =============================================================================
   main_x11.c — X11/OpenGL Platform Backend

   Implements the 4-function platform contract for frogger.
   Key differences from the old X11 backend:

   1. OpenGL backbuffer (replaces XPixmap)
      frogger_render() writes to a uint32_t pixel array. This function uploads
      it to a GL texture and draws a fullscreen quad each frame. One glTexImage2D
      call per frame — no per-sprite X11 drawing calls.

   2. VSync (GLX_EXT_swap_control)
      glXSwapBuffers replaces platform_sleep_ms(16). The driver blocks until the
      next monitor refresh, giving smooth 60fps with no spin-wait.

   3. XkbSetDetectableAutoRepeat (replaces peek-ahead)
      When a key is held, X11 normally sends fake KeyRelease+KeyPress pairs.
      This call tells the server to suppress those — only real events arrive.
      GameButtonState then correctly tracks "just pressed" vs "held".

   Build:
     clang -Wall -Wextra -Wpedantic -std=c99 -DASSETS_DIR=\"assets\" \
           -o build/frogger_x11 src/frogger.c src/main_x11.c \
           -lX11 -lxkbcommon -lGL -lGLX
   ============================================================================= */

#define _POSIX_C_SOURCE 200809L

#include "frogger.h"
#include "platform.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ─── X11/GLX module state ────────────────────────────────────────────────── */
static Display    *g_display  = NULL;
static Window      g_window   = 0;
static GLXContext  g_glctx    = NULL;
static Atom        g_wm_delete;
static int         g_is_running = 0;
static int         g_win_w = 0;
static int         g_win_h = 0;
static GLuint      g_texture = 0;

/* ─── VSync setup ─────────────────────────────────────────────────────────── */
static void setup_vsync(void) {
    const char *exts = glXQueryExtensionsString(g_display,
                                                DefaultScreen(g_display));
    if (exts && strstr(exts, "GLX_EXT_swap_control")) {
        typedef void (*PFNGLXSWAPINTERVALEXT)(Display *, GLXDrawable, int);
        PFNGLXSWAPINTERVALEXT glXSwapIntervalEXT =
            (PFNGLXSWAPINTERVALEXT)(void *)glXGetProcAddressARB(
                (const GLubyte *)"glXSwapIntervalEXT");
        if (glXSwapIntervalEXT) {
            glXSwapIntervalEXT(g_display, glXGetCurrentDrawable(), 1);
            fprintf(stderr, "✓ VSync enabled (GLX_EXT_swap_control)\n");
            return;
        }
    }
    fprintf(stderr, "! VSync not available\n");
}

/* ─── platform_init ───────────────────────────────────────────────────────── */
void platform_init(const char *title, int width, int height) {
    g_win_w = width;
    g_win_h = height;

    g_display = XOpenDisplay(NULL);
    if (!g_display) { fprintf(stderr, "FATAL: XOpenDisplay\n"); return; }

    int screen = DefaultScreen(g_display);
    Window root = RootWindow(g_display, screen);

    /* GLX framebuffer config */
    static const int fb_attribs[] = {
        GLX_RENDER_TYPE,   GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER,  True,
        GLX_RED_SIZE,   8, GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE,  8, GLX_ALPHA_SIZE, 8,
        None
    };
    int n_configs = 0;
    GLXFBConfig *fbc = glXChooseFBConfig(g_display, screen, fb_attribs, &n_configs);
    if (!fbc || n_configs == 0) { fprintf(stderr, "FATAL: glXChooseFBConfig\n"); return; }

    XVisualInfo *vi = glXGetVisualFromFBConfig(g_display, fbc[0]);

    XSetWindowAttributes swa;
    swa.colormap   = XCreateColormap(g_display, root, vi->visual, AllocNone);
    swa.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask;

    g_window = XCreateWindow(g_display, root,
        0, 0, (unsigned)width, (unsigned)height, 0,
        vi->depth, InputOutput, vi->visual,
        CWColormap | CWEventMask, &swa);

    g_wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &g_wm_delete, 1);
    XStoreName(g_display, g_window, title);

    g_glctx = glXCreateContext(g_display, vi, NULL, GL_TRUE);
    XFree(vi);
    XFree(fbc);

    XMapWindow(g_display, g_window);
    glXMakeCurrent(g_display, g_window, g_glctx);

    setup_vsync();

    /* Auto-repeat: suppress fake KeyRelease+KeyPress pairs while key is held */
    Bool supported;
    XkbSetDetectableAutoRepeat(g_display, True, &supported);
    fprintf(stderr, "✓ Auto-repeat detection: %s\n",
            supported ? "enabled" : "not supported");

    /* Allocate GL texture for backbuffer */
    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    /* Orthographic 2D projection */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1, 1, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);

    fprintf(stderr, "✓ OpenGL initialized: %s\n", glGetString(GL_VERSION));
    g_is_running = 1;
}

/* ─── platform_get_time ───────────────────────────────────────────────────── */
double platform_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* ─── platform_get_input ──────────────────────────────────────────────────── */
void platform_get_input(GameInput *input) {
    while (XPending(g_display)) {
        XEvent e;
        XNextEvent(g_display, &e);

        if (e.type == ClientMessage &&
            (Atom)e.xclient.data.l[0] == g_wm_delete) {
            g_is_running = 0;
            input->quit  = 1;
        }

        if (e.type == KeyPress) {
            KeySym sym = XkbKeycodeToKeysym(g_display, e.xkey.keycode, 0, 0);
            if (sym == XK_Up    || sym == XK_w) UPDATE_BUTTON(input->move_up,    1);
            if (sym == XK_Down  || sym == XK_s) UPDATE_BUTTON(input->move_down,  1);
            if (sym == XK_Left  || sym == XK_a) UPDATE_BUTTON(input->move_left,  1);
            if (sym == XK_Right || sym == XK_d) UPDATE_BUTTON(input->move_right, 1);
            if (sym == XK_Escape) { g_is_running = 0; input->quit = 1; }
        }
        if (e.type == KeyRelease) {
            KeySym sym = XkbKeycodeToKeysym(g_display, e.xkey.keycode, 0, 0);
            if (sym == XK_Up    || sym == XK_w) UPDATE_BUTTON(input->move_up,    0);
            if (sym == XK_Down  || sym == XK_s) UPDATE_BUTTON(input->move_down,  0);
            if (sym == XK_Left  || sym == XK_a) UPDATE_BUTTON(input->move_left,  0);
            if (sym == XK_Right || sym == XK_d) UPDATE_BUTTON(input->move_right, 0);
        }
    }
}

/* ─── platform_display_backbuffer (static — not exported) ────────────────── */
static void platform_display_backbuffer(const FroggerBackbuffer *bb) {
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 bb->width, bb->height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);

    glBegin(GL_QUADS);
        glTexCoord2f(0,0); glVertex2f(0,0);
        glTexCoord2f(1,0); glVertex2f(1,0);
        glTexCoord2f(1,1); glVertex2f(1,1);
        glTexCoord2f(0,1); glVertex2f(0,1);
    glEnd();

    glXSwapBuffers(g_display, g_window);
}

/* ─── main ────────────────────────────────────────────────────────────────── */
#ifndef ASSETS_DIR
#define ASSETS_DIR "assets"
#endif

int main(void) {
    GameState       state;
    FroggerBackbuffer bb;
    GameInput       input;

    platform_init("Frogger", SCREEN_PX_W, SCREEN_PX_H);

    bb.width  = SCREEN_PX_W;
    bb.height = SCREEN_PX_H;
    bb.pixels = (uint32_t *)malloc((size_t)(bb.width * bb.height) * sizeof(uint32_t));
    if (!bb.pixels) { fprintf(stderr, "FATAL: out of memory\n"); return 1; }

    frogger_init(&state, ASSETS_DIR);
    memset(&input, 0, sizeof(input));

    double prev_time = platform_get_time();

    while (g_is_running) {
        double curr_time  = platform_get_time();
        float  delta_time = (float)(curr_time - prev_time);
        prev_time = curr_time;
        if (delta_time > 0.1f) delta_time = 0.1f;

        prepare_input_frame(&input);
        platform_get_input(&input);
        if (input.quit) break;

        frogger_tick(&state, &input, delta_time);
        frogger_render(&state, &bb);
        platform_display_backbuffer(&bb);
    }

    free(bb.pixels);
    glDeleteTextures(1, &g_texture);
    glXMakeCurrent(g_display, None, NULL);
    glXDestroyContext(g_display, g_glctx);
    XDestroyWindow(g_display, g_window);
    XCloseDisplay(g_display);
    return 0;
}
