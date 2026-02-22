#ifndef SNAKE_H
#define SNAKE_H

/* ============================================================
 * snake.h — Core game types, constants, and public API
 *
 * Platform-independent. Included by snake.c, main_x11.c, main_raylib.c.
 * Nothing in here is OS-specific.
 *
 * Port of OneLoneCoder Snake (javidx9) from C++/Windows to C/Linux.
 * ============================================================ */

#include <stdlib.h>   /* rand(), srand() */
#include <string.h>   /* memset() */
#include <time.h>     /* time() */

/* ─── Grid & Display ─────────────────────────────────────────── */
#define GRID_WIDTH    60
#define GRID_HEIGHT   20
#define CELL_SIZE     14
#define HEADER_ROWS   3
#define MAX_SNAKE     1200
#define BASE_TICK_MS  150

/* Window pixel dimensions — used by both backends */
#define WINDOW_WIDTH  (GRID_WIDTH  * CELL_SIZE)
#define WINDOW_HEIGHT ((GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE)

/* ─── Direction Constants ────────────────────────────────────── */
#define DIR_UP    0
#define DIR_RIGHT 1
#define DIR_DOWN  2
#define DIR_LEFT  3

/* ─── Segment ───────────────────────────────────────────────── */
typedef struct {
    int x, y;
} Segment;

/* ─── PlatformInput ──────────────────────────────────────────── */
typedef struct {
    int turn_left;   /* CCW turn: Left arrow or A */
    int turn_right;  /* CW  turn: Right arrow or D */
    int restart;     /* R or Space */
    int quit;        /* Q or Escape */
} PlatformInput;

/* ─── GameState (ring buffer) ────────────────────────────────── */
typedef struct {
    Segment segments[MAX_SNAKE];
    int     head;
    int     tail;
    int     length;
    int     direction;
    int     next_direction;
    int     tick_count;
    int     speed;         /* ticks per move; lower = faster */
    int     grow_pending;
    int     food_x;
    int     food_y;
    int     score;
    int     best_score;
    int     game_over;
} GameState;

/* ─── Public Game API ────────────────────────────────────────── */
void snake_init(GameState *s);
void snake_spawn_food(GameState *s);
void snake_tick(GameState *s, PlatformInput input);

#endif /* SNAKE_H */
