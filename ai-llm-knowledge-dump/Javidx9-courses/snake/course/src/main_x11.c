/* ============================================================
 * main_x11.c — X11 Platform Backend for Snake
 *
 * Implements the 6-function platform API (platform.h) using Xlib.
 * Also contains main() — the game loop.
 *
 * Build:
 *   gcc -Wall -Wextra -g -o snake_x11 src/snake.c src/main_x11.c -lX11
 *
 * Controls:
 *   Left / A   — turn left
 *   Right / D  — turn right
 *   R / Space  — restart (after game over)
 *   Q / Escape — quit
 * ============================================================ */

#include <X11/Xlib.h>    /* XOpenDisplay, XCreateWindow, XDrawString … */
#include <X11/keysym.h>  /* XK_Left, XK_Right, XK_Escape … */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>        /* nanosleep, struct timespec */

#include "snake.h"
#include "platform.h"

/* ─── Global X11 State ─────────────────────────────────────────
 *
 *  These live for the entire program lifetime.
 *  Using globals is fine in a single-file platform backend.
 * ─────────────────────────────────────────────────────────── */
static Display      *g_display;    /* connection to the X server */
static Window        g_window;     /* our application window */
static GC            g_gc;         /* Graphics Context — holds drawing settings */
static int           g_screen;     /* default screen index */

/* Pre-allocated pixel colors. Allocating at startup avoids per-frame allocs. */
static unsigned long g_col[8];
/*  g_col[0] = black  (background)
    g_col[1] = green  (border / walls)
    g_col[2] = yellow (snake body)
    g_col[3] = white  (snake head — alive)
    g_col[4] = red    (food)
    g_col[5] = gray50 (header background)
    g_col[6] = white  (text)
    g_col[7] = dark red (dead snake) */

/* ─── Key Latch State ──────────────────────────────────────────
 *
 *  X11 sends repeated KeyPress events when a key is held.
 *  We only want ONCE-per-press for game actions.
 *
 *  Latch pattern:
 *    On KeyPress: set flag to 1.
 *    In platform_get_input: copy flag → input, then CLEAR flag.
 *    Result: the flag is 1 for exactly one call, then 0.
 *
 *  Exception: quit/turn flags stay set while key is held (not latched).
 * ─────────────────────────────────────────────────────────── */
static int g_should_quit  = 0;
static int g_turn_left    = 0;   /* Right arrow or D (NOT latched — held OK) */
static int g_turn_right   = 0;   /* Left arrow or A */
static int g_restart_latch = 0;  /* R / Space — fires once then clears */

/* ─── alloc_color ──────────────────────────────────────────────
 *
 *  Ask the X server to allocate a named color (e.g. "red", "lime green").
 *  Returns a pixel value we can pass to XSetForeground.
 *  Falls back to white if the color isn't found (shouldn't happen).
 * ─────────────────────────────────────────────────────────── */
static unsigned long alloc_color(const char *name) {
    XColor screen_color, exact_color;
    Colormap cmap = DefaultColormap(g_display, g_screen);
    if (XAllocNamedColor(g_display, cmap, name, &screen_color, &exact_color))
        return screen_color.pixel;
    return WhitePixel(g_display, g_screen);  /* fallback */
}

/* ─── platform_init ────────────────────────────────────────────
 *
 *  Open display, create window, set up event masks, allocate colors.
 *  XSelectInput is REQUIRED — without it no events arrive.
 * ─────────────────────────────────────────────────────────── */
void platform_init(void) {
    Atom wm_delete;

    g_display = XOpenDisplay(NULL);  /* NULL = use DISPLAY env var */
    if (!g_display) {
        fprintf(stderr, "Cannot open X display. Is $DISPLAY set?\n");
        exit(1);
    }

    g_screen = DefaultScreen(g_display);

    /* Create a simple window: pos(100,100), size, 1px border, black fg/bg */
    g_window = XCreateSimpleWindow(
        g_display,
        RootWindow(g_display, g_screen),
        100, 100,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        1,
        BlackPixel(g_display, g_screen),
        BlackPixel(g_display, g_screen)
    );

    /* Tell X which events to deliver to us */
    XSelectInput(g_display, g_window,
                 KeyPressMask | KeyReleaseMask | ExposureMask);

    /* Intercept the window-close button (WM_DELETE_WINDOW protocol) */
    wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &wm_delete, 1);

    XStoreName(g_display, g_window, "Snake \xe2\x80\x94 X11");
    g_gc = XCreateGC(g_display, g_window, 0, NULL);

    /* Allocate all colors at startup — never alloc per frame */
    g_col[0] = BlackPixel(g_display, g_screen);
    g_col[1] = alloc_color("lime green");
    g_col[2] = alloc_color("yellow");
    g_col[3] = WhitePixel(g_display, g_screen);
    g_col[4] = alloc_color("red");
    g_col[5] = alloc_color("gray50");
    g_col[6] = WhitePixel(g_display, g_screen);
    g_col[7] = alloc_color("dark red");

    XMapWindow(g_display, g_window);  /* make window visible */
    XFlush(g_display);                /* flush pending commands to server */
}

/* ─── platform_get_input ───────────────────────────────────────
 *
 *  Drain the X event queue and translate key events to our PlatformInput.
 *
 *  WHY drain the entire queue?
 *  If we only handle one event per tick, the queue grows unboundedly
 *  when the game is running slower than the key-repeat rate.
 * ─────────────────────────────────────────────────────────── */
void platform_get_input(PlatformInput *input) {
    memset(input, 0, sizeof(PlatformInput));

    /* Drain all pending events */
    while (XPending(g_display)) {
        XEvent ev;
        XNextEvent(g_display, &ev);

        if (ev.type == KeyPress) {
            KeySym key = XLookupKeysym(&ev.xkey, 0);
            if (key == XK_Left  || key == XK_a) g_turn_left    = 1;
            if (key == XK_Right || key == XK_d) g_turn_right   = 1;
            if (key == XK_r     || key == XK_space) g_restart_latch = 1;
            if (key == XK_q     || key == XK_Escape) g_should_quit = 1;
        }
        if (ev.type == KeyRelease) {
            KeySym key = XLookupKeysym(&ev.xkey, 0);
            /* Release resets the turn flags so holding doesn't repeat turn */
            if (key == XK_Left  || key == XK_a) g_turn_left  = 0;
            if (key == XK_Right || key == XK_d) g_turn_right = 0;
        }
        if (ev.type == ClientMessage) {
            /* User clicked the window-close button */
            g_should_quit = 1;
        }
    }

    /* Copy current state to output struct */
    input->turn_left  = g_turn_left;
    input->turn_right = g_turn_right;
    input->restart    = g_restart_latch;
    input->quit       = g_should_quit;

    /* Clear latched flags — they fire exactly once */
    g_restart_latch = 0;
}

/* ─── draw_cell ────────────────────────────────────────────────
 *
 *  Draw a 1-cell filled rectangle at grid position (col, row).
 *
 *  Row 0 is the TOP of the window (header area).
 *  The play field starts at pixel y = 0 (rows 0..FIELD_HEIGHT-1 combined).
 *
 *  We inset by 1 pixel on each side (CELL_SIZE-2) to leave a grid gap.
 * ─────────────────────────────────────────────────────────── */
static void draw_cell(int col, int row, unsigned long color) {
    XSetForeground(g_display, g_gc, color);
    XFillRectangle(g_display, g_window, g_gc,
                   col * CELL_SIZE + 1,      /* x pixel */
                   row * CELL_SIZE + 1,      /* y pixel */
                   CELL_SIZE - 2,            /* width */
                   CELL_SIZE - 2);           /* height */
}

/* ─── platform_render ──────────────────────────────────────────
 *
 *  Draw the complete game frame. Called once per tick.
 *  Rendering order: background → header → walls → food → body → head → overlay.
 *  We redraw everything every frame (no dirty-region tracking needed at this scale).
 * ─────────────────────────────────────────────────────────── */
void platform_render(const GameState *state) {
    char buf[64];
    int col, row, idx, rem;
    unsigned long body_color, head_color;

    /* 1. Clear the whole window to black */
    XSetForeground(g_display, g_gc, g_col[0]);
    XFillRectangle(g_display, g_window, g_gc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    /* 2. Draw header background (rows 0-2) in gray */
    XSetForeground(g_display, g_gc, g_col[5]);
    XFillRectangle(g_display, g_window, g_gc,
                   0, 0, WINDOW_WIDTH, HEADER_ROWS * CELL_SIZE);

    /* 3. Header bottom border line (row HEADER_ROWS-1, full width, green) */
    XSetForeground(g_display, g_gc, g_col[1]);
    XFillRectangle(g_display, g_window, g_gc,
                   0, (HEADER_ROWS - 1) * CELL_SIZE, WINDOW_WIDTH, 2);

    /* 4. "SNAKE" title in header */
    XSetForeground(g_display, g_gc, g_col[6]);
    XDrawString(g_display, g_window, g_gc,
                WINDOW_WIDTH / 2 - 20,         /* centered-ish */
                CELL_SIZE + CELL_SIZE / 2,     /* vertically centered in header */
                "SNAKE", 5);

    /* 5. Score and best score in header */
    snprintf(buf, sizeof(buf), "Score: %d", state->score);
    XDrawString(g_display, g_window, g_gc, 10, CELL_SIZE + CELL_SIZE / 2,
                buf, (int)strlen(buf));
    snprintf(buf, sizeof(buf), "Best: %d", state->best_score);
    XDrawString(g_display, g_window, g_gc, WINDOW_WIDTH - 80, CELL_SIZE + CELL_SIZE / 2,
                buf, (int)strlen(buf));

    /* 6. Draw border walls in green
     *    Left col=0, right col=GRID_WIDTH-1, bottom row=GRID_HEIGHT+HEADER_ROWS-1 */
    for (row = HEADER_ROWS; row < GRID_HEIGHT + HEADER_ROWS; row++) {
        draw_cell(0,              row, g_col[1]);  /* left wall */
        draw_cell(GRID_WIDTH - 1, row, g_col[1]);  /* right wall */
    }
    for (col = 0; col < GRID_WIDTH; col++) {
        draw_cell(col, GRID_HEIGHT + HEADER_ROWS - 1, g_col[1]); /* bottom wall */
    }

    /* 7. Draw food (red) */
    draw_cell(state->food_x, state->food_y, g_col[4]);

    /* 8. Draw snake body (tail to head-1) and head */
    body_color = state->game_over ? g_col[7] : g_col[2];
    head_color = state->game_over ? g_col[7] : g_col[3];

    idx = state->tail;
    rem = state->length - 1;  /* all except head */
    while (rem > 0) {
        draw_cell(state->segments[idx].x, state->segments[idx].y, body_color);
        idx = (idx + 1) % MAX_SNAKE;
        rem--;
    }

    /* 9. Draw snake head */
    draw_cell(state->segments[state->head].x, state->segments[state->head].y, head_color);

    /* 10. Game over overlay — centered box with text */
    if (state->game_over) {
        int cx = WINDOW_WIDTH  / 2;
        int cy = WINDOW_HEIGHT / 2;

        /* Dark box background */
        XSetForeground(g_display, g_gc, g_col[5]);
        XFillRectangle(g_display, g_window, g_gc, cx - 80, cy - 30, 160, 60);

        /* "GAME OVER" in red */
        XSetForeground(g_display, g_gc, g_col[4]);
        XDrawString(g_display, g_window, g_gc, cx - 32, cy - 10, "GAME OVER", 9);

        /* Score line */
        XSetForeground(g_display, g_gc, g_col[6]);
        snprintf(buf, sizeof(buf), "Score: %d", state->score);
        XDrawString(g_display, g_window, g_gc, cx - 24, cy + 8,
                    buf, (int)strlen(buf));

        /* Restart hint */
        XDrawString(g_display, g_window, g_gc, cx - 54, cy + 22,
                    "Press R to restart", 18);
    }

    /* Push all drawing commands to the X server */
    XFlush(g_display);
}

/* ─── platform_sleep_ms ────────────────────────────────────────
 *
 *  Sleep for ms milliseconds using nanosleep (POSIX, Linux).
 *  More precise than usleep for sub-second intervals.
 * ─────────────────────────────────────────────────────────── */
void platform_sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* ─── platform_should_quit / platform_shutdown ─────────────────*/
int platform_should_quit(void) { return g_should_quit; }

void platform_shutdown(void) {
    XFreeGC(g_display, g_gc);
    XDestroyWindow(g_display, g_window);
    XCloseDisplay(g_display);
}

/* ─── main ─────────────────────────────────────────────────────
 *
 *  The game loop. Dead simple:
 *    sleep → get input → tick → render → repeat.
 *
 *  We sleep FIRST so the first frame is drawn immediately after init,
 *  and subsequent frames are paced by state->speed_ms.
 * ─────────────────────────────────────────────────────────── */
int main(void) {
    GameState     state;
    PlatformInput input;

    platform_init();
    snake_init(&state);   /* srand is called inside snake_init */

    while (!platform_should_quit()) {
        platform_sleep_ms(BASE_TICK_MS);  /* pace the loop */
        platform_get_input(&input);

        if (input.quit) break;

        snake_tick(&state, input);

        platform_render(&state);
    }

    platform_shutdown();
    printf("Final score: %d\n", state.score);
    return 0;
}
