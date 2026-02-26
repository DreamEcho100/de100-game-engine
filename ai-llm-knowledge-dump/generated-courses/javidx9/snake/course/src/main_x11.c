#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

/* ============================================================
 * main_x11.c — X11/GLX Platform Backend for Snake
 *
 * Implements platform.h using Xlib + OpenGL.
 * Also contains main() — the game loop.
 *
 * Build:
 *   clang snake.c main_x11.c -o snake_x11 -lX11 -lxkbcommon -lGL -lGLX
 *
 * Controls:
 *   Left / A   — turn left (CCW)
 *   Right / D  — turn right (CW)
 *   R / Space  — restart (after game over)
 *   Q / Escape — quit
 * ============================================================ */

#include "platform.h"
#include "snake.h"

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* GLX VSync extension function pointer type */
typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display *, GLXDrawable, int);

/* ─── Platform State ─────────────────────────────────────────── */

typedef struct {
    Display    *display;
    Window      window;
    GLXContext  gl_context;
    GLuint      texture_id;
    int         screen;
    int         width;
    int         height;
    int         vsync_enabled;
} X11_State;

static int       g_is_running = 1;
static X11_State g_x11 = {0};

/* ─── Timing ─────────────────────────────────────────────────── */

double platform_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* ─── VSync Setup ────────────────────────────────────────────── */

static void setup_vsync(void) {
    const char *ext = glXQueryExtensionsString(g_x11.display, g_x11.screen);

    if (ext && strstr(ext, "GLX_EXT_swap_control")) {
        PFNGLXSWAPINTERVALEXTPROC fn =
            (PFNGLXSWAPINTERVALEXTPROC)
                glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalEXT");
        if (fn) {
            fn(g_x11.display, g_x11.window, 1);
            g_x11.vsync_enabled = 1;
            printf("✓ VSync enabled (GLX_EXT_swap_control)\n");
            return;
        }
    }

    if (ext && strstr(ext, "GLX_MESA_swap_control")) {
        PFNGLXSWAPINTERVALMESAPROC fn =
            (PFNGLXSWAPINTERVALMESAPROC)
                glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalMESA");
        if (fn) {
            fn(1);
            g_x11.vsync_enabled = 1;
            printf("✓ VSync enabled (GLX_MESA_swap_control)\n");
            return;
        }
    }

    g_x11.vsync_enabled = 0;
    printf("⚠ VSync not available — using sleep fallback\n");
}

/* ─── platform_init ──────────────────────────────────────────── */

void platform_init(const char *title, int width, int height) {
    Bool supported;
    XVisualInfo *visual;
    Colormap colormap;
    XSetWindowAttributes attrs;
    int visual_attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };

    g_x11.display = XOpenDisplay(NULL);
    if (!g_x11.display) {
        fprintf(stderr, "Cannot open X display. Is $DISPLAY set?\n");
        exit(1);
    }

    /* Suppress synthetic key-repeat events — we only want real press/release */
    XkbSetDetectableAutoRepeat(g_x11.display, True, &supported);
    printf("✓ Auto-repeat detection: %s\n", supported ? "enabled" : "not supported");

    g_x11.screen = DefaultScreen(g_x11.display);
    g_x11.width  = width;
    g_x11.height = height;

    /* Choose a visual that supports GLX + double-buffering */
    visual = glXChooseVisual(g_x11.display, g_x11.screen, visual_attribs);
    if (!visual) {
        fprintf(stderr, "Failed to choose GLX visual\n");
        exit(1);
    }

    /* XCreateWindow (not XCreateSimpleWindow) is required when specifying
     * a custom visual for GLX */
    colormap = XCreateColormap(g_x11.display,
                               RootWindow(g_x11.display, g_x11.screen),
                               visual->visual, AllocNone);
    attrs.colormap   = colormap;
    attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                       StructureNotifyMask;

    g_x11.window = XCreateWindow(
        g_x11.display,
        RootWindow(g_x11.display, g_x11.screen),
        100, 100, width, height, 0,
        visual->depth, InputOutput, visual->visual,
        CWColormap | CWEventMask, &attrs);

    /* GLX context — handles OpenGL state */
    g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
    if (!g_x11.gl_context) {
        fprintf(stderr, "Failed to create GLX context\n");
        exit(1);
    }
    XFree(visual);

    XStoreName(g_x11.display, g_x11.window, title);
    XMapWindow(g_x11.display, g_x11.window);
    XFlush(g_x11.display);

    glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

    /* 2D orthographic projection — (0,0) is top-left */
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    /* Create the texture that will hold the backbuffer pixels */
    glGenTextures(1, &g_x11.texture_id);
    glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    setup_vsync();
    printf("✓ OpenGL initialized: %s\n", glGetString(GL_VERSION));
}

/* ─── platform_get_input ─────────────────────────────────────── */

void platform_get_input(GameInput *input) {
    XEvent event;

    while (XPending(g_x11.display) > 0) {
        XNextEvent(g_x11.display, &event);

        switch (event.type) {
        case KeyPress: {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            switch (key) {
            case XK_q: case XK_Q: case XK_Escape:
                g_is_running = 0;
                input->quit  = 1;
                break;
            case XK_r: case XK_R: case XK_space:
                input->restart = 1;
                break;
            case XK_Left:  case XK_a: case XK_A:
                UPDATE_BUTTON(input->turn_left, 1);
                break;
            case XK_Right: case XK_d: case XK_D:
                UPDATE_BUTTON(input->turn_right, 1);
                break;
            }
            break;
        }
        case KeyRelease: {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            switch (key) {
            case XK_Left:  case XK_a: case XK_A:
                UPDATE_BUTTON(input->turn_left, 0);
                break;
            case XK_Right: case XK_d: case XK_D:
                UPDATE_BUTTON(input->turn_right, 0);
                break;
            }
            break;
        }
        case ClientMessage:
            g_is_running = 0;
            input->quit  = 1;
            break;

        case ConfigureNotify:
            if (event.xconfigure.width  != g_x11.width ||
                event.xconfigure.height != g_x11.height) {
                g_x11.width  = event.xconfigure.width;
                g_x11.height = event.xconfigure.height;
                glViewport(0, 0, g_x11.width, g_x11.height);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glOrtho(0, g_x11.width, g_x11.height, 0, -1, 1);
                glMatrixMode(GL_MODELVIEW);
            }
            break;
        }
    }
}

/* ─── platform_display_backbuffer ────────────────────────────── */

static void platform_display_backbuffer(const SnakeBackbuffer *bb) {
    glClear(GL_COLOR_BUFFER_BIT);

    /* Upload CPU pixel buffer to GPU texture */
    glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 bb->width, bb->height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);

    /* Draw a fullscreen quad textured with the backbuffer */
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0,         0);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(bb->width, 0);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(bb->width, bb->height);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0,         bb->height);
    glEnd();

    /* Swap front/back buffers — VSync blocks here at monitor refresh */
    glXSwapBuffers(g_x11.display, g_x11.window);
}

/* ─── Cleanup ────────────────────────────────────────────────── */

static void platform_shutdown(void) {
    if (g_x11.texture_id)  glDeleteTextures(1, &g_x11.texture_id);
    if (g_x11.gl_context) {
        glXMakeCurrent(g_x11.display, None, NULL);
        glXDestroyContext(g_x11.display, g_x11.gl_context);
    }
    if (g_x11.window)  XDestroyWindow(g_x11.display, g_x11.window);
    if (g_x11.display) XCloseDisplay(g_x11.display);
}

/* ─── main ───────────────────────────────────────────────────── */

int main(void) {
    static const double TARGET_FRAME = 1.0 / 60.0;
    SnakeBackbuffer backbuffer;
    GameInput       input   = {0};
    GameState       state   = {0};
    double          prev_time;

    platform_init("Snake", WINDOW_WIDTH, WINDOW_HEIGHT);

    backbuffer.width  = WINDOW_WIDTH;
    backbuffer.height = WINDOW_HEIGHT;
    backbuffer.pixels = (uint32_t *)malloc(
        WINDOW_WIDTH * WINDOW_HEIGHT * sizeof(uint32_t));
    if (!backbuffer.pixels) {
        fprintf(stderr, "Failed to allocate backbuffer\n");
        platform_shutdown();
        return 1;
    }

    snake_init(&state);
    prev_time = platform_get_time();

    while (g_is_running) {
        double curr_time  = platform_get_time();
        float  delta_time = (float)(curr_time - prev_time);
        prev_time = curr_time;

        /* Cap delta_time to avoid giant jumps (e.g., debugger pauses) */
        if (delta_time > 0.1f) delta_time = 0.1f;

        prepare_input_frame(&input);
        platform_get_input(&input);

        if (input.quit) break;

        snake_update(&state, &input, delta_time);
        snake_render(&state, &backbuffer);
        platform_display_backbuffer(&backbuffer);

        /* Frame cap fallback when VSync is unavailable */
        if (!g_x11.vsync_enabled) {
            double elapsed = platform_get_time() - curr_time;
            if (elapsed < TARGET_FRAME)
                sleep_ms((int)((TARGET_FRAME - elapsed) * 1000.0));
        }
    }

    free(backbuffer.pixels);
    platform_shutdown();
    return 0;
}
