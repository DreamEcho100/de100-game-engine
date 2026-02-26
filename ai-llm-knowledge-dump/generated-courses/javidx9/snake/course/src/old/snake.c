/* ============================================================
 * snake.c — Platform-independent game logic (ring buffer)
 *
 * No X11. No Raylib. No platform headers at all.
 * Compiles identically whether we link X11 or Raylib.
 *
 * Port of OneLoneCoder Snake (javidx9) from C++/Windows to C/Linux.
 * Original: https://github.com/OneLoneCoder/videos
 * ============================================================ */

#include "snake.h"

static const int dx[4] = { 0,  1,  0, -1 };
static const int dy[4] = {-1,  0,  1,  0 };

void snake_spawn_food(GameState *s) {
    int ok;
    do {
        s->food_x = 1 + rand() % (GRID_WIDTH  - 2);
        s->food_y = 1 + rand() % (GRID_HEIGHT - 2);
        ok = 1;
        {
            int idx = s->tail;
            int rem  = s->length;
            while (rem > 0) {
                if (s->segments[idx].x == s->food_x &&
                    s->segments[idx].y == s->food_y) {
                    ok = 0;
                    break;
                }
                idx = (idx + 1) % MAX_SNAKE;
                rem--;
            }
        }
    } while (!ok);
}

void snake_init(GameState *s) {
    int saved_best = s->best_score;
    memset(s, 0, sizeof(*s));
    s->best_score = saved_best;

    s->head   = 9;
    s->tail   = 0;
    s->length = 10;
    {
        int i;
        for (i = 0; i < 10; i++) {
            s->segments[i].x = 10 + i;
            s->segments[i].y = GRID_HEIGHT / 2;
        }
    }

    s->direction      = DIR_RIGHT;
    s->next_direction = DIR_RIGHT;
    s->speed          = 8;
    s->tick_count     = 0;
    s->grow_pending   = 0;
    s->score          = 0;
    s->game_over      = 0;

    srand((unsigned)time(NULL));
    snake_spawn_food(s);
}

void snake_tick(GameState *s, PlatformInput input) {
    int new_x, new_y;

    if (s->game_over && input.restart) {
        snake_init(s);
        return;
    }
    if (s->game_over) return;

    if (input.turn_right)
        s->next_direction = (s->direction + 1) % 4;
    if (input.turn_left)
        s->next_direction = (s->direction + 3) % 4;

    s->tick_count++;
    if (s->tick_count < s->speed) return;
    s->tick_count = 0;

    s->direction = s->next_direction;

    new_x = s->segments[s->head].x + dx[s->direction];
    new_y = s->segments[s->head].y + dy[s->direction];

    /* Wall collision */
    if (new_x < 0 || new_x >= GRID_WIDTH ||
        new_y < 0 || new_y >= GRID_HEIGHT) {
        if (s->score > s->best_score) s->best_score = s->score;
        s->game_over = 1;
        return;
    }

    /* Self collision */
    {
        int idx = s->tail;
        int rem  = s->length;
        while (rem > 0) {
            if (s->segments[idx].x == new_x &&
                s->segments[idx].y == new_y) {
                if (s->score > s->best_score) s->best_score = s->score;
                s->game_over = 1;
                return;
            }
            idx = (idx + 1) % MAX_SNAKE;
            rem--;
        }
    }

    /* Advance head */
    s->head = (s->head + 1) % MAX_SNAKE;
    s->segments[s->head].x = new_x;
    s->segments[s->head].y = new_y;
    s->length++;

    /* Food collision */
    if (new_x == s->food_x && new_y == s->food_y) {
        s->score++;
        s->grow_pending += 5;
        if (s->score % 3 == 0 && s->speed > 2)
            s->speed--;
        snake_spawn_food(s);
    }

    /* Advance tail (or grow) */
    if (s->grow_pending > 0) {
        s->grow_pending--;
    } else {
        s->tail = (s->tail + 1) % MAX_SNAKE;
        s->length--;
    }
}
