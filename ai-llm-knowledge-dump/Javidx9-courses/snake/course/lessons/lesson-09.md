# Lesson 9 — Splitting into Files: snake.h, snake.c, platform.h

**By the end of this lesson you will have:** The code split into `snake.h`, `snake.c`, and `platform.h`, with both X11 and Raylib backends compiling cleanly and behaving exactly as before.

---

## Why this matters

Right now all 300+ lines live in `main_x11.c`. If you want a Raylib version, you'd copy-paste all the game logic (movement, collision, food, scoring). Then you'd have two copies of the same logic. Fix a bug in one, forget the other — now you have a ghost bug.

The solution is to split by **responsibility**:

| File | Responsibility | Knows about X11/Raylib? |
|------|---------------|-------------------------|
| `snake.h` | Types, constants, function declarations | No |
| `snake.c` | All game logic | No |
| `platform.h` | Contract the backends must implement | No |
| `main_x11.c` | X11 setup + render + input | Yes (X11 only) |
| `main_raylib.c` | Raylib setup + render + input | Yes (Raylib only) |

`snake.c` is the **brain**. `main_x11.c` and `main_raylib.c` are the **bodies**. The brain doesn't know which body it's in. This is exactly like a TypeScript interface with two implementations — one for production, one for tests.

---

## Step 1 — Create `src/snake.h`

This file contains: constants, data type definitions, and function declarations. No logic, no platform calls.

```c
/* src/snake.h */
#ifndef SNAKE_H
#define SNAKE_H

#include <stdlib.h>   /* rand(), srand() */
#include <string.h>   /* memset() */
#include <time.h>     /* time() */

/* ── Grid constants ─────────────────────────────── */
#define GRID_WIDTH    60
#define GRID_HEIGHT   20
#define CELL_SIZE     14
#define HEADER_ROWS   3
#define MAX_SNAKE     1200
#define BASE_TICK_MS  150

/* ── Window size helpers ─────────────────────────── */
#define WINDOW_WIDTH  (GRID_WIDTH  * CELL_SIZE)
#define WINDOW_HEIGHT ((GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE)

/* ── Direction constants ─────────────────────────── */
#define DIR_UP    0
#define DIR_RIGHT 1
#define DIR_DOWN  2
#define DIR_LEFT  3

/* ── Data types ──────────────────────────────────── */
typedef struct {
    int x, y;
} Segment;

typedef struct {
    int turn_left;    /* 1 if left-turn key pressed this frame */
    int turn_right;   /* 1 if right-turn key pressed this frame */
    int restart;      /* 1 if space/enter pressed this frame */
} PlatformInput;

typedef struct {
    /* Ring buffer */
    Segment segments[MAX_SNAKE];
    int     head;
    int     tail;
    int     length;

    /* Movement */
    int direction;
    int next_direction;
    int tick_count;
    int speed;         /* ticks per move; lower = faster */

    /* Growth */
    int grow_pending;

    /* Food */
    int food_x;
    int food_y;

    /* Scoring */
    int score;
    int best_score;

    /* State */
    int game_over;
} GameState;

/* ── Function declarations ───────────────────────── */
void snake_init(GameState *s);
void snake_tick(GameState *s, PlatformInput input);
void snake_spawn_food(GameState *s);

#endif /* SNAKE_H */
```

**Header guards explained:** `#ifndef SNAKE_H` / `#define SNAKE_H` / `#endif` prevent the file from being included twice in the same translation unit. Like TypeScript's `export {}` preventing duplicate declarations — but required in C because there's no module system.

**`typedef struct`:** Lets you write `GameState` instead of `struct GameState`. In plain C (not C++) you must always write `struct GameState` unless you typedef it. Most C code you'll encounter uses typedef.

---

## Step 2 — Create `src/snake.c`

This file implements all game logic. Zero platform calls. Zero X11. Zero Raylib.

```c
/* src/snake.c */
#include "snake.h"

/* Direction deltas: UP RIGHT DOWN LEFT */
static const int dx[4] = { 0,  1,  0, -1 };
static const int dy[4] = {-1,  0,  1,  0 };

void snake_spawn_food(GameState *s) {
    int ok;
    do {
        s->food_x = rand() % GRID_WIDTH;
        s->food_y = rand() % GRID_HEIGHT;
        ok = 1;
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
    } while (!ok);
}

void snake_init(GameState *s) {
    memset(s, 0, sizeof(*s));

    /* Initial snake: 10 segments, horizontal, centre of grid */
    s->head   = 9;
    s->tail   = 0;
    s->length = 10;
    for (int i = 0; i < 10; i++) {
        s->segments[i].x = 10 + i;
        s->segments[i].y = GRID_HEIGHT / 2;
    }

    s->direction      = DIR_RIGHT;
    s->next_direction = DIR_RIGHT;
    s->speed          = 8;
    s->tick_count     = 0;
    s->grow_pending   = 0;
    s->score          = 0;
    s->game_over      = 0;
    /* best_score intentionally NOT reset — persists across restarts */

    srand((unsigned)time(NULL));
    snake_spawn_food(s);
}

void snake_tick(GameState *s, PlatformInput input) {
    /* ── Restart ───────────────────────────────────── */
    if (s->game_over && input.restart) {
        int saved_best = s->best_score;
        snake_init(s);
        s->best_score = saved_best;
        return;
    }

    if (s->game_over) return;

    /* ── Direction input ───────────────────────────── */
    /* CW/CCW turns; (direction + 1) % 4 = CW, (direction + 3) % 4 = CCW */
    if (input.turn_right)
        s->next_direction = (s->direction + 1) % 4;
    if (input.turn_left)
        s->next_direction = (s->direction + 3) % 4;

    /* ── Speed counter ─────────────────────────────── */
    s->tick_count++;
    if (s->tick_count < s->speed) return;
    s->tick_count = 0;

    /* Apply direction (reversal already prevented: CW/CCW can't reverse) */
    s->direction = s->next_direction;

    /* ── Compute new head position ─────────────────── */
    int new_x = s->segments[s->head].x + dx[s->direction];
    int new_y = s->segments[s->head].y + dy[s->direction];

    /* ── Wall collision ────────────────────────────── */
    if (new_x < 0 || new_x >= GRID_WIDTH ||
        new_y < 0 || new_y >= GRID_HEIGHT) {
        if (s->score > s->best_score) s->best_score = s->score;
        s->game_over = 1;
        return;
    }

    /* ── Self collision ────────────────────────────── */
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

    /* ── Advance head ──────────────────────────────── */
    s->head = (s->head + 1) % MAX_SNAKE;
    s->segments[s->head].x = new_x;
    s->segments[s->head].y = new_y;
    s->length++;

    /* ── Food collision ────────────────────────────── */
    if (new_x == s->food_x && new_y == s->food_y) {
        s->score++;
        s->grow_pending += 5;
        if (s->score % 3 == 0 && s->speed > 2)
            s->speed--;
        snake_spawn_food(s);
    }

    /* ── Advance tail (or grow) ────────────────────── */
    if (s->grow_pending > 0) {
        s->grow_pending--;
    } else {
        s->tail = (s->tail + 1) % MAX_SNAKE;
        s->length--;
    }
}
```

**`static const`:** The `dx`/`dy` arrays are `static` (only visible in this .c file) and `const` (never modified). This is good practice — it tells the compiler and future readers that these values never change.

---

## Step 3 — Create `src/platform.h`

This declares the six functions that every platform backend must implement:

```c
/* src/platform.h */
#ifndef PLATFORM_H
#define PLATFORM_H

#include "snake.h"

/*
 * Every platform backend (main_x11.c, main_raylib.c) must implement
 * all six of these functions.
 *
 * The game loop in main() calls them in this order every frame:
 *   platform_get_input → snake_tick → platform_render
 */

/* Open the window. Returns 0 on success, non-zero on failure. */
int  platform_init(const char *title, int width, int height);

/* Return 1 if the user requested quit (close button, Alt-F4, etc.) */
int  platform_should_quit(void);

/* Fill *input with the current frame's key state. */
void platform_get_input(PlatformInput *input);

/* Draw the current game state. */
void platform_render(const GameState *state);

/* Sleep for ms milliseconds. */
void platform_sleep_ms(int ms);

/* Destroy the window and free resources. */
void platform_shutdown(void);

#endif /* PLATFORM_H */
```

This is the **contract**. `snake.c` never calls any of these. `main()` calls them. The backend implements them. The game logic (`snake.c`) is completely isolated from both sides.

---

## Step 4 — Trim `src/main_x11.c`

Remove all game logic. Keep only:

1. X11 headers and setup (`Display`, `Window`, `GC`, `XOpenDisplay`, etc.)
2. `platform_init()` — open display, create window, create GC, allocate colors
3. `platform_should_quit()` — check for close events
4. `platform_get_input()` — poll X11 key events, fill `PlatformInput`
5. `platform_render()` — clear window, draw header, draw snake from `state->segments`, draw food, draw game-over overlay if `state->game_over`
6. `platform_sleep_ms()` — `usleep(ms * 1000)` (needs `#include <unistd.h>`)
7. `platform_shutdown()` — `XCloseDisplay(display)`
8. `main()`:

```c
/* src/main_x11.c — main() only */
#include "platform.h"

int main(void) {
    if (platform_init("Snake", WINDOW_WIDTH, WINDOW_HEIGHT) != 0)
        return 1;

    GameState state;
    snake_init(&state);

    while (!platform_should_quit()) {
        PlatformInput input = {0};
        platform_get_input(&input);
        snake_tick(&state, input);
        platform_render(&state);
        platform_sleep_ms(BASE_TICK_MS);
    }

    platform_shutdown();
    return 0;
}
```

`main()` is now 12 lines. It knows nothing about X11 or Raylib. It just runs the loop.

---

## Step 5 — Do the same for `src/main_raylib.c`

Same structure. `platform_render()` uses Raylib draw calls instead of X11 calls. `platform_sleep_ms()` can be `WaitTime(ms / 1000.0)` or just an empty stub since Raylib's `BeginDrawing`/`EndDrawing` handles timing differently (you may want to use `SetTargetFPS` instead).

---

## Step 6 — Update the build commands

The game logic is now in `snake.c`, so include it in both compilations:

**X11:**
```sh
gcc -o snake_x11 src/main_x11.c src/snake.c -lX11
```

**Raylib:**
```sh
gcc -o snake_raylib src/main_raylib.c src/snake.c -lraylib -lm
```

If you have a `Makefile`, add `snake.c` to the object list:
```makefile
OBJ_X11    = main_x11.o snake.o
OBJ_RAYLIB = main_raylib.o snake.o
```

---

## Build & Run

```sh
gcc -o snake_x11 src/main_x11.c src/snake.c -lX11 && ./snake_x11
```

```sh
gcc -o snake_raylib src/main_raylib.c src/snake.c -lraylib -lm && ./snake_raylib
```

**The game should behave identically to before.** Nothing visible changed — only the code organization changed.

---

## Mental Model

```
         ┌──────────────────────────────────────┐
         │           snake.c (the brain)         │
         │  snake_init, snake_tick, snake_spawn  │
         │  No X11. No Raylib. Pure C.           │
         └────────────────┬─────────────────────┘
                          │  reads/writes GameState
              ┌───────────┴──────────────┐
              │                          │
   ┌──────────▼────────┐      ┌─────────▼────────┐
   │  main_x11.c       │      │  main_raylib.c    │
   │  (X11 body)       │      │  (Raylib body)    │
   │  platform_init    │      │  platform_init    │
   │  platform_render  │      │  platform_render  │
   │  platform_input   │      │  platform_input   │
   └───────────────────┘      └──────────────────┘
```

> **Like a TypeScript interface with two implementations.** `platform.h` is the interface. `main_x11.c` and `main_raylib.c` are the implementations. `snake.c` only depends on the types in `snake.h` — it's completely portable.

---

## Exercise

Verify the initial snake position by printing it to stdout from inside `snake_init()`:

```c
void snake_init(GameState *s) {
    /* ... existing init code ... */

    /* Debug: print first 5 segments */
    for (int i = 0; i < 5; i++) {
        printf("[%d] x=%d y=%d\n", i, s->segments[i].x, s->segments[i].y);
    }
}
```

You'll need `#include <stdio.h>` at the top of `snake.c`.

Expected output:
```
[0] x=10 y=10
[1] x=11 y=10
[2] x=12 y=10
[3] x=13 y=10
[4] x=14 y=10
```

(Assuming `GRID_HEIGHT/2 = 10` for a 20-row grid.)

Remove the printf after verifying. Production code shouldn't print to stdout on every init — but using printf to verify your data structures during development is a core C debugging technique.
