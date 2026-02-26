# Lesson 11 — Food, Growth, Score, and Speed Scaling

## By the end of this lesson you will have:

A `snake_spawn_food` function that places food at a random empty cell, a `grow_pending` mechanism that grows the snake across multiple steps, a score counter, and a `move_interval` that decreases as the player scores points.

---

## Step 1 — `snake_spawn_food`

Food must appear inside the walls and not overlap the snake's body.

```c
static void snake_spawn_food(GameState *s) {
    int fx, fy, occupied;
    int idx, rem;

    /* Rejection sampling: pick random positions until one is empty */
    do {
        fx = 1 + rand() % (GRID_WIDTH  - 2);
        fy =     rand() % (GRID_HEIGHT - 1);
        occupied = 0;

        /* Check against every snake segment */
        idx = s->tail;
        rem = s->length;
        while (rem > 0) {
            if (s->segments[idx].x == fx && s->segments[idx].y == fy) {
                occupied = 1;
                break;
            }
            idx = (idx + 1) % MAX_SNAKE;
            rem--;
        }
    } while (occupied);

    s->food_x = fx;
    s->food_y = fy;
}
```

**New C concept — `rand()` and `%`:**
`rand()` returns a pseudo-random integer in `[0, RAND_MAX]`. `rand() % N` maps it to `[0, N-1]`. For food columns: `1 + rand() % (GRID_WIDTH - 2)` gives `[1, GRID_WIDTH-2]` — inside the left and right walls.

**Rejection sampling:**
The `do { ... } while (occupied)` loop retries until it finds an empty cell. For a short snake this converges immediately (high probability of empty cells). For a very long snake filling most of the grid, it may loop many times — but that's a rare edge case and the loop is cheap.

**Food row range:**
`fy = rand() % (GRID_HEIGHT - 1)` gives `[0, GRID_HEIGHT-2]`. The last row (`GRID_HEIGHT-1`) is the bottom wall — never put food there.

**`rand()` seeding:**
`srand(time(NULL))` is called once in `main`. Without seeding, `rand()` produces the same sequence every run.

---

## Step 2 — Food collision and `grow_pending`

Inside the "move step" after pushing the new head:

```c
/* Check food */
if (s->segments[s->head].x == s->food_x &&
    s->segments[s->head].y == s->food_y) {
    s->score++;
    s->grow_pending += GROW_AMOUNT;  /* e.g., 3 */

    /* Speed up every 3 points */
    if (s->score % 3 == 0 && s->move_interval > 0.05f)
        s->move_interval -= 0.01f;

    snake_spawn_food(s);
}
```

`grow_pending` accumulates how many additional cells the snake should grow. Each move step decrements it by 1 (and skips tail advancement). `GROW_AMOUNT=3` means eating one food item grows the snake 3 cells over 3 move steps.

**Score check timing:**
`s->segments[s->head].x == s->food_x` is checked after `s->head` is updated to the new position. This means "is the new head cell where the food is?" — correct.

---

## Step 3 — Speed scaling

```c
#define MOVE_INTERVAL_START  0.15f   /* 150ms = ~6.7 steps/sec */
#define MOVE_INTERVAL_MIN    0.05f   /* 50ms  = 20 steps/sec   */
#define MOVE_INTERVAL_STEP   0.01f   /* -10ms per 3 points     */
```

In `snake_init`:
```c
s->move_interval = MOVE_INTERVAL_START;
```

The speed formula:
- Score 3:  interval = 0.140 → 7.1 steps/sec
- Score 6:  interval = 0.130 → 7.7 steps/sec
- Score 30: interval = 0.050 → 20 steps/sec (maximum)

At 20 steps/sec with `CELL_SIZE=20`, the snake moves `20 * 20 = 400 pixels/sec` — covering the 1160-pixel-wide playfield in under 3 seconds. Maximum speed is challenging but not impossible.

---

## Step 4 — `best_score` persistence

`best_score` survives death. In `snake_init`:

```c
void snake_init(GameState *s) {
    int saved_best = s->best_score;  /* save before memset */
    memset(s, 0, sizeof(*s));
    s->best_score = saved_best;      /* restore after memset */

    /* ... rest of init ... */
}
```

`memset` zeros the entire `GameState` struct — including `best_score`. By saving and restoring it, the best score persists across game restarts without needing a separate variable outside the struct.

---

## Step 5 — Complete move step sequence

Putting it all together, the "move step" inside `snake_update` (after `move_timer >= move_interval`):

```c
/* 1. Apply direction (with 180° reversal guard) */
SNAKE_DIR opposite = (SNAKE_DIR)((s->current_dir + 2) % 4);
if (s->next_dir != opposite)
    s->current_dir = s->next_dir;

/* 2. Compute new head position */
int nx = s->segments[s->head].x;
int ny = s->segments[s->head].y;
switch (s->current_dir) {
    case SNAKE_DIR_UP:    ny--; break;
    case SNAKE_DIR_RIGHT: nx++; break;
    case SNAKE_DIR_DOWN:  ny++; break;
    case SNAKE_DIR_LEFT:  nx--; break;
}

/* 3. Wall collision */
if (nx <= 0 || nx >= GRID_WIDTH - 1 || ny < 0 || ny >= GRID_HEIGHT - 1) {
    if (s->score > s->best_score) s->best_score = s->score;
    s->game_over = 1;
    return;
}

/* 4. Self collision */
/* ... ring buffer traversal ... */
if (hit_self) { /* ... game_over ... */ return; }

/* 5. Push new head */
s->head = (s->head + 1) % MAX_SNAKE;
s->segments[s->head].x = nx;
s->segments[s->head].y = ny;

/* 6. Food check */
if (s->segments[s->head].x == s->food_x &&
    s->segments[s->head].y == s->food_y) {
    s->score++;
    s->grow_pending += GROW_AMOUNT;
    if (s->score % 3 == 0 && s->move_interval > MOVE_INTERVAL_MIN)
        s->move_interval -= MOVE_INTERVAL_STEP;
    snake_spawn_food(s);
}

/* 7. Advance tail (or grow) */
if (s->grow_pending > 0) {
    s->grow_pending--;
    s->length++;
} else {
    s->tail = (s->tail + 1) % MAX_SNAKE;
}
```

---

## Key Concepts

- `snake_spawn_food`: rejection sampling with `do { ... } while (occupied)` — retry until empty cell found
- `rand() % N` — uniform random integer in `[0, N-1]`; `srand(time(NULL))` seeds once in `main`
- `grow_pending`: accumulates growth; each step decrements it and skips tail advance
- Food collision: after pushing new head, compare head position to food position
- Speed scaling: `move_interval -= 0.01f` every 3 points; floor at `0.05f`
- `best_score`: saved before `memset`, restored after — persists across restarts

---

## Exercise

The current growth amount is constant (`GROW_AMOUNT=3`). Modify it so growth scales with score: first food = grow by 1, second = grow by 2, third = grow by 3, and so on. Where would you store the "current growth amount"? Would you put it in `GameState`? A local variable? A `#define`?
