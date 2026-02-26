/* =============================================================================
   main_x11.c — X11/OpenGL Platform Backend for Asteroids

   Implements the 4-function platform contract.

   Pipeline per frame:
     1. platform_get_input  — drain X11 event queue, update GameInput
     2. asteroids_update    — game logic (called by main)
     3. asteroids_render    — writes to bb.pixels[] (called by main)
     4. platform_display_backbuffer — upload pixels to GL texture, swap buffers

   Key techniques:
     - GLX with double-buffered RGBA visual — no XPixmap, no XCopyArea
     - glTexImage2D(GL_RGBA) uploads bb.pixels each frame (one GPU call)
     - GL_NEAREST filter: pixel-exact, no blur
     - GLX_EXT_swap_control: VSync drives frame timing, no sleep_ms
     - XkbSetDetectableAutoRepeat: suppress fake KeyRelease/KeyPress during hold
   ============================================================================= */

#define _POSIX_C_SOURCE 200809L

#include "asteroids.h"
#include "platform.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ─── Module state ────────────────────────────────────────────────────────── */
static Display   *g_display   = NULL;
static Window     g_window    = 0;
static GLXContext g_glctx     = NULL;
static Atom       g_wm_delete;
static int        g_is_running = 0;
static GLuint     g_texture   = 0;

/* ─── VSync setup ─────────────────────────────────────────────────────────── */
static void setup_vsync(void) {
    const char *exts = glXQueryExtensionsString(g_display,
                                                DefaultScreen(g_display));
    if (exts && strstr(exts, "GLX_EXT_swap_control")) {
        typedef void (*PFNGLXSWAPINTERVALEXT)(Display *, GLXDrawable, int);
        PFNGLXSWAPINTERVALEXT fn =
            (PFNGLXSWAPINTERVALEXT)(void *)glXGetProcAddressARB(
                (const GLubyte *)"glXSwapIntervalEXT");
        if (fn) {
            fn(g_display, glXGetCurrentDrawable(), 1);
            fprintf(stderr, "✓ VSync enabled (GLX_EXT_swap_control)\n");
            return;
        }
    }
    fprintf(stderr, "! VSync not available\n");
}

/* ─── platform_init ───────────────────────────────────────────────────────── */
void platform_init(const char *title, int width, int height) {
    g_display = XOpenDisplay(NULL);
    if (!g_display) { fprintf(stderr, "FATAL: XOpenDisplay\n"); return; }

    int    screen = DefaultScreen(g_display);
    Window root   = RootWindow(g_display, screen);

    /* Request double-buffered RGBA framebuffer from GLX */
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

    /* XCreateWindow (not XCreateSimpleWindow) — required for a custom GLX visual */
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

    /* Suppress fake KeyRelease+KeyPress pairs sent while a key is held.
     * Without this, UPDATE_BUTTON sees repeated press events → unintended
     * "just pressed" fires on every fake event during a key hold.          */
    Bool supported;
    XkbSetDetectableAutoRepeat(g_display, True, &supported);
    fprintf(stderr, "✓ Auto-repeat detection: %s\n",
            supported ? "enabled" : "not supported");

    /* Allocate a GL texture — reused every frame for the backbuffer upload */
    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    /* Orthographic projection: (0,0) top-left, (1,1) bottom-right */
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
            if (sym == XK_Left)   UPDATE_BUTTON(input->left,  1);
            if (sym == XK_Right)  UPDATE_BUTTON(input->right, 1);
            if (sym == XK_Up)     UPDATE_BUTTON(input->up,    1);
            if (sym == XK_space)  UPDATE_BUTTON(input->fire,  1);
            if (sym == XK_Escape) { g_is_running = 0; input->quit = 1; }
        }
        if (e.type == KeyRelease) {
            KeySym sym = XkbKeycodeToKeysym(g_display, e.xkey.keycode, 0, 0);
            if (sym == XK_Left)  UPDATE_BUTTON(input->left,  0);
            if (sym == XK_Right) UPDATE_BUTTON(input->right, 0);
            if (sym == XK_Up)    UPDATE_BUTTON(input->up,    0);
            if (sym == XK_space) UPDATE_BUTTON(input->fire,  0);
        }
    }
}

/* ─── platform_display_backbuffer (static — not exported) ────────────────── */
static void platform_display_backbuffer(const AsteroidsBackbuffer *bb) {
    glBindTexture(GL_TEXTURE_2D, g_texture);
    /* GL_RGBA matches our byte layout [RR,GG,BB,AA] in memory */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 bb->width, bb->height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);

    /* Fullscreen quad: texture coord (0,0)=top-left, (1,1)=bottom-right */
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(0, 0);
        glTexCoord2f(1, 0); glVertex2f(1, 0);
        glTexCoord2f(1, 1); glVertex2f(1, 1);
        glTexCoord2f(0, 1); glVertex2f(0, 1);
    glEnd();

    glXSwapBuffers(g_display, g_window); /* blocks until VSync */
}

/* ─── main ────────────────────────────────────────────────────────────────── */
int main(void) {
    GameState          state;
    AsteroidsBackbuffer bb;
    GameInput          input;

    platform_init("Asteroids", SCREEN_W, SCREEN_H);

    bb.width  = SCREEN_W;
    bb.height = SCREEN_H;
    bb.pixels = (uint32_t *)malloc((size_t)(bb.width * bb.height) * sizeof(uint32_t));
    if (!bb.pixels) { fprintf(stderr, "FATAL: out of memory\n"); return 1; }

    asteroids_init(&state);
    memset(&input, 0, sizeof(input));

    double prev_time = platform_get_time();

    while (g_is_running) {
        double curr_time  = platform_get_time();
        float  delta_time = (float)(curr_time - prev_time);
        prev_time = curr_time;
        if (delta_time > 0.1f) delta_time = 0.1f; /* cap for debugger pauses */

        prepare_input_frame(&input);
        platform_get_input(&input);
        if (input.quit) break;

        asteroids_update(&state, &input, delta_time);
        asteroids_render(&state, &bb);
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
