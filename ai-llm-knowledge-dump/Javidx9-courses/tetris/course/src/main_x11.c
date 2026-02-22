/* ============================================================
 * main_x11.c — X11 Platform Backend
 *
 * Implements the platform API (platform.h) using Xlib (libX11).
 *
 * Key X11 concepts:
 *   Display  — connection to the X server  (like a WebSocket to the compositor)
 *   Screen   — which physical monitor to use
 *   Window   — an on-screen rectangle       (like a DOM element)
 *   GC       — Graphics Context: drawing state (like canvas.getContext('2d'))
 *   XEvent   — input/expose/destroy event   (like a DOM event)
 *   XFlush() — flush pending commands to server (like flushing a write buffer)
 *
 * Build:
 *   gcc -o tetris_x11 src/tetris.c src/main_x11.c -lX11 -Wall -Wextra -g
 * ============================================================ */

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tetris.h"
#include "platform.h"

/* ─── Window Layout ────────────────────────────────────────────
 *
 *  +──────────────────+──────────────+
 *  │                  │  SCORE:      │
 *  │   Play Field     │  12345       │
 *  │   12 cells wide  │              │
 *  │   18 cells tall  │  NEXT:       │
 *  │                  │  [preview]   │
 *  │                  │              │
 *  │                  │  Controls:   │
 *  │                  │  ← → ↓  Move │
 *  │                  │  Z   Rotate  │
 *  +──────────────────+──────────────+
 *
 * ─────────────────────────────────────────────────────────── */
#define SIDEBAR_WIDTH  (6 * CELL_SIZE)
#define WINDOW_WIDTH   (FIELD_WIDTH  * CELL_SIZE + SIDEBAR_WIDTH)
#define WINDOW_HEIGHT  (FIELD_HEIGHT * CELL_SIZE)

/* ─── Global X11 State ─────────────────────────────────────────
 *
 *  Wave 1 resources — live for the entire program lifetime.
 *  We never free them early. platform_shutdown() releases them all.
 * ─────────────────────────────────────────────────────────── */
static Display       *g_display;    /* connection to X server */
static Window         g_window;     /* our application window */
static GC             g_gc;         /* graphics context (drawing state) */
static int            g_screen;     /* which screen/monitor */
static unsigned long  g_colors[10]; /* pre-allocated colors indexed 0-9 */
static int            g_should_quit = 0;

/* ─── Key State ────────────────────────────────────────────────
 *
 *  X11 sends KeyPress/KeyRelease events. We maintain boolean flags
 *  updated from the event queue each tick.
 *
 *  move_* keys: held = 1, released = 0 (continuous while held)
 *  rotate/restart: "latched" — set on KeyPress, cleared after one tick
 * ─────────────────────────────────────────────────────────── */
static int g_key_left    = 0;
static int g_key_right   = 0;
static int g_key_down    = 0;
static int g_key_rotate  = 0;  /* latch: cleared after platform_get_input() */
static int g_key_restart = 0;  /* latch */

/* ─── Color Helper ─────────────────────────────────────────────
 *
 *  XAllocNamedColor looks up a color by CSS-style name in the
 *  server's color database and returns a pixel value we can
 *  pass to XSetForeground().
 * ─────────────────────────────────────────────────────────── */
static unsigned long alloc_color(const char *name) {
    XColor color, exact;
    Colormap cmap = DefaultColormap(g_display, g_screen);
    if (XAllocNamedColor(g_display, cmap, name, &color, &exact))
        return color.pixel;
    return WhitePixel(g_display, g_screen); /* fallback on failure */
}

/* ─── platform_init ────────────────────────────────────────────*/
void platform_init(void) {
    /* 1. Open connection to X server.
     *    NULL means use the DISPLAY environment variable. */
    g_display = XOpenDisplay(NULL);
    if (!g_display) {
        fprintf(stderr, "Error: Cannot open X display.\n"
                        "  Is the DISPLAY environment variable set?\n"
                        "  Are you running in a graphical session?\n");
        exit(1);
    }

    g_screen = DefaultScreen(g_display);

    /* 2. Create window.
     *    XCreateSimpleWindow is a convenience wrapper over XCreateWindow. */
    g_window = XCreateSimpleWindow(
        g_display,
        RootWindow(g_display, g_screen),  /* parent: the desktop root */
        100, 100,                          /* initial x, y position */
        WINDOW_WIDTH, WINDOW_HEIGHT,       /* width, height in pixels */
        1,                                 /* border width */
        BlackPixel(g_display, g_screen),   /* border color */
        BlackPixel(g_display, g_screen)    /* background color */
    );

    /* 3. Register which events we want.
     *    ❌ COMMON MISTAKE: forgetting XSelectInput → events never arrive! */
    XSelectInput(g_display, g_window,
                 KeyPressMask | KeyReleaseMask | ExposureMask);

    /* 4. Handle window close button (WM_DELETE_WINDOW protocol) */
    Atom wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &wm_delete, 1);

    /* 5. Set title bar */
    XStoreName(g_display, g_window, "Tetris — X11");

    /* 6. Create a Graphics Context.
     *    Like getting ctx = canvas.getContext('2d').
     *    GC holds: current color, line width, font, etc. */
    g_gc = XCreateGC(g_display, g_window, 0, NULL);

    /* 7. Pre-allocate piece colors (indexed by field cell value).
     *    0=empty  1=I  2=S  3=Z  4=T  5=J  6=L  7=O  8=flash  9=wall */
    g_colors[0] = BlackPixel(g_display, g_screen);
    g_colors[1] = alloc_color("cyan");
    g_colors[2] = alloc_color("green");
    g_colors[3] = alloc_color("red");
    g_colors[4] = alloc_color("magenta");
    g_colors[5] = alloc_color("blue");
    g_colors[6] = alloc_color("orange");
    g_colors[7] = alloc_color("yellow");
    g_colors[8] = alloc_color("white");
    g_colors[9] = alloc_color("gray50");

    /* 8. Show the window and flush commands to the server */
    XMapWindow(g_display, g_window);
    XFlush(g_display);
}

/* ─── platform_get_input ───────────────────────────────────────
 *
 *  Drains the X11 event queue, updates key state flags,
 *  then copies the current flags into *input.
 *
 *  Latched keys (rotate, restart) are cleared AFTER copying —
 *  this ensures they fire exactly once per press.
 * ─────────────────────────────────────────────────────────── */
void platform_get_input(PlatformInput *input) {
    memset(input, 0, sizeof(PlatformInput));

    /* Drain all events that have arrived since last tick */
    while (XPending(g_display)) {
        XEvent event;
        XNextEvent(g_display, &event);

        if (event.type == KeyPress) {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_Left  || key == XK_a) g_key_left    = 1;
            if (key == XK_Right || key == XK_d) g_key_right   = 1;
            if (key == XK_Down  || key == XK_s) g_key_down    = 1;
            if (key == XK_z     || key == XK_x) g_key_rotate  = 1; /* latch */
            if (key == XK_r)                    g_key_restart = 1; /* latch */
            if (key == XK_Escape || key == XK_q) g_should_quit = 1;
        }

        if (event.type == KeyRelease) {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_Left  || key == XK_a) g_key_left  = 0;
            if (key == XK_Right || key == XK_d) g_key_right = 0;
            if (key == XK_Down  || key == XK_s) g_key_down  = 0;
            /* rotate/restart latches are cleared below, not on KeyRelease */
        }

        /* Window manager sent "close window" message */
        if (event.type == ClientMessage)
            g_should_quit = 1;
    }

    /* Copy current state into the input struct */
    input->move_left  = g_key_left;
    input->move_right = g_key_right;
    input->move_down  = g_key_down;
    input->rotate     = g_key_rotate;
    input->restart    = g_key_restart;
    input->quit       = g_should_quit;

    /* Clear latched keys so they fire exactly once */
    g_key_rotate  = 0;
    g_key_restart = 0;
}

/* ─── draw_cell ────────────────────────────────────────────────
 *
 *  Draw a single cell at grid coordinates (col, row).
 *  Leaves a 1-pixel gap around each cell for a grid-line effect.
 * ─────────────────────────────────────────────────────────── */
static void draw_cell(int col, int row, unsigned long color) {
    XSetForeground(g_display, g_gc, color);
    XFillRectangle(g_display, g_window, g_gc,
                   col * CELL_SIZE + 1,
                   row * CELL_SIZE + 1,
                   CELL_SIZE - 2,
                   CELL_SIZE - 2);
}

/* ─── platform_render ──────────────────────────────────────────
 *
 *  🔴 HOT PATH — called ~20 times/second.
 *  No dynamic allocation. Just X11 draw calls.
 *
 *  Render order:
 *    1. Clear background
 *    2. Draw locked field cells
 *    3. Draw current falling piece
 *    4. Draw sidebar (score + next-piece preview + controls)
 *    5. Draw game-over overlay if needed
 *    6. XFlush() — send all draw commands to X server
 * ─────────────────────────────────────────────────────────── */
void platform_render(const GameState *state) {
    int x, y, px, py;
    char buf[64];

    /* 1. Clear background */
    XSetForeground(g_display, g_gc, BlackPixel(g_display, g_screen));
    XFillRectangle(g_display, g_window, g_gc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    /* 2. Draw locked field cells (includes walls at value=9) */
    for (x = 0; x < FIELD_WIDTH; x++) {
        for (y = 0; y < FIELD_HEIGHT; y++) {
            unsigned char cell = state->field[y * FIELD_WIDTH + x];
            if (cell > 0)
                draw_cell(x, y, g_colors[cell]);
        }
    }

    /* 3. Draw the current falling piece */
    if (!state->game_over) {
        for (px = 0; px < 4; px++) {
            for (py = 0; py < 4; py++) {
                int pi = tetris_rotate(px, py, state->current_rotation);
                if (TETROMINOES[state->current_piece][pi] != '.') {
                    draw_cell(state->current_x + px,
                              state->current_y + py,
                              g_colors[state->current_piece + 1]);
                }
            }
        }
    }

    /* 4. Sidebar */
    int sx = FIELD_WIDTH * CELL_SIZE + 10;
    XSetForeground(g_display, g_gc, WhitePixel(g_display, g_screen));

    /* Score */
    XDrawString(g_display, g_window, g_gc, sx, 20, "SCORE", 5);
    snprintf(buf, sizeof(buf), "%d", state->score);
    XDrawString(g_display, g_window, g_gc, sx, 36, buf, (int)strlen(buf));

    /* Level */
    int level = (20 - state->speed) / 2;
    XDrawString(g_display, g_window, g_gc, sx, 58, "LEVEL", 5);
    snprintf(buf, sizeof(buf), "%d", level);
    XDrawString(g_display, g_window, g_gc, sx, 74, buf, (int)strlen(buf));

    /* Next piece preview */
    XDrawString(g_display, g_window, g_gc, sx, 100, "NEXT", 4);
    int prev_col = FIELD_WIDTH + 1;
    int prev_row = 4;
    for (px = 0; px < 4; px++) {
        for (py = 0; py < 4; py++) {
            int pi = tetris_rotate(px, py, 0);
            if (TETROMINOES[state->next_piece][pi] != '.')
                draw_cell(prev_col + px, prev_row + py,
                          g_colors[state->next_piece + 1]);
        }
    }

    /* Controls hint */
    XSetForeground(g_display, g_gc, alloc_color("gray50"));
    XDrawString(g_display, g_window, g_gc, sx, WINDOW_HEIGHT - 90, "Controls:", 9);
    XDrawString(g_display, g_window, g_gc, sx, WINDOW_HEIGHT - 74, "← →  Move",  9);
    XDrawString(g_display, g_window, g_gc, sx, WINDOW_HEIGHT - 58, "↓   Drop",    8);
    XDrawString(g_display, g_window, g_gc, sx, WINDOW_HEIGHT - 42, "Z   Rotate", 10);
    XDrawString(g_display, g_window, g_gc, sx, WINDOW_HEIGHT - 26, "R   Restart",11);
    XDrawString(g_display, g_window, g_gc, sx, WINDOW_HEIGHT - 10, "Q   Quit",    8);

    /* 5. Game over overlay */
    if (state->game_over) {
        int cx = FIELD_WIDTH * CELL_SIZE / 2;
        int cy = FIELD_HEIGHT * CELL_SIZE / 2;
        XSetForeground(g_display, g_gc, BlackPixel(g_display, g_screen));
        XFillRectangle(g_display, g_window, g_gc, cx - 60, cy - 30, 120, 60);
        XSetForeground(g_display, g_gc, alloc_color("red"));
        XDrawString(g_display, g_window, g_gc, cx - 28, cy - 8, "GAME OVER", 9);
        XSetForeground(g_display, g_gc, WhitePixel(g_display, g_screen));
        XDrawString(g_display, g_window, g_gc, cx - 42, cy + 12, "R=Restart  Q=Quit", 17);
    }

    /* 6. Flush all pending draw commands to the X server */
    XFlush(g_display);
}

/* ─── platform_sleep_ms ────────────────────────────────────────*/
void platform_sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* ─── platform_should_quit ─────────────────────────────────────*/
int platform_should_quit(void) {
    return g_should_quit;
}

/* ─── platform_shutdown ────────────────────────────────────────
 *
 *  Release all X11 resources in reverse-creation order.
 *  ❌ COMMON MISTAKE: forgetting these → resource leak
 * ─────────────────────────────────────────────────────────── */
void platform_shutdown(void) {
    XFreeGC(g_display, g_gc);
    XDestroyWindow(g_display, g_window);
    XCloseDisplay(g_display);
}

/* ─── main ─────────────────────────────────────────────────────
 *
 *  The main loop: tick, input, update, render — 20 times/second.
 *
 *  Note: we seed rand() with time() so each game is different.
 *  Limitation: rand() % 7 has slight bias (RAND_MAX not divisible
 *  by 7), acceptable for a learning project.
 * ─────────────────────────────────────────────────────────── */
int main(void) {
    srand((unsigned int)time(NULL));

    GameState     state;
    PlatformInput input;

    platform_init();
    tetris_init(&state);

    while (!platform_should_quit()) {
        platform_sleep_ms(50);        /* 1 tick = 50ms */
        platform_get_input(&input);

        if (input.quit)
            break;

        if (state.game_over && input.restart)
            tetris_init(&state);
        else
            tetris_tick(&state, &input);

        platform_render(&state);
    }

    platform_shutdown();
    printf("Thanks for playing! Final score: %d\n", state.score);
    return 0;
}
