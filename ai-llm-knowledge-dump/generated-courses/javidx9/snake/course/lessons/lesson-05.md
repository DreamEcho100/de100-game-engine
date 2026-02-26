# Lesson 05 — GameState + Ring Buffer + `snake_init`

## By the end of this lesson you will have:

A fully initialized `GameState` with a 10-segment snake placed in the middle of the grid, a ring buffer data structure explained with diagrams, and `snake_init` that preserves the best score across resets.

---

## Why a Ring Buffer?

The original C++ code used `std::list<sSnakeSegment>` — a doubly-linked list with `push_front` (add head) and `pop_back` (remove tail).

A linked list requires a `malloc` for each new node — memory allocated every time the snake grows. The ring buffer approach uses a single fixed array allocated once at startup. All operations are constant time with no dynamic allocation.

```
Ring buffer visualization (MAX_SNAKE=10, shown small for clarity):

Array:  [_, _, S2, S1, H, _, _, _, T, S4]
Index:   0  1   2   3  4  5  6  7  8   9
                              ↑           ↑
                             head        tail

H  = head (most recent position — where snake is now)
T  = tail (oldest position — end of the snake's body)
S1-S4 = body segments between head and tail
_  = unused slots
```

The snake occupies slots from `tail` up to `head` (wrapping around if needed).

---

## Step 1 — `GameState` struct

```c
typedef struct {
    Segment  segments[MAX_SNAKE];  /* fixed array — no malloc per segment */
    int      head;                 /* index of most recent head segment */
    int      tail;                 /* index of oldest (tail-tip) segment */
    int      length;               /* count of valid segments */

    SNAKE_DIR direction;           /* current moving direction */
    SNAKE_DIR next_direction;      /* queued turn, applied on next step */

    float    move_timer;           /* accumulates delta_time each frame */
    float    move_interval;        /* seconds between steps (decreases with score) */

    int      grow_pending;         /* segments still to add before tail advances */
    int      food_x;
    int      food_y;
    int      score;
    int      best_score;           /* preserved across snake_init */
    int      game_over;
} GameState;
```

**`move_timer` / `move_interval`** replace the old `tick_count` / `speed` fields:
- Old: `if (++tick_count >= speed) { move; tick_count = 0; }` — tied to frame rate
- New: `move_timer += delta_time; if (move_timer >= move_interval) { move; move_timer -= move_interval; }` — tied to real time, frame-rate independent

**`best_score`** must survive `snake_init`. We'll save and restore it manually.

---

## Step 2 — `snake_init`

```c
void snake_init(GameState *s) {
    int saved_best = s->best_score;  /* preserve across reset */
    int i;

    memset(s, 0, sizeof(*s));        /* zero all fields */
    s->best_score = saved_best;      /* restore */

    /* Place 10-segment snake horizontally, pointing right */
    s->head   = 9;   /* most recent position at index 9 */
    s->tail   = 0;   /* oldest (tail-tip) at index 0 */
    s->length = 10;
    for (i = 0; i < 10; i++) {
        s->segments[i].x = 10 + i;
        s->segments[i].y = GRID_HEIGHT / 2;
    }

    s->direction      = SNAKE_DIR_RIGHT;
    s->next_direction = SNAKE_DIR_RIGHT;
    s->move_timer     = 0.0f;
    s->move_interval  = 0.15f;   /* 150ms per step */
    s->grow_pending   = 0;
    s->score          = 0;
    s->game_over      = 0;

    srand((unsigned)time(NULL));
    snake_spawn_food(s);
}
```

**New C concept — `memset(s, 0, sizeof(*s))`:**
Sets every byte in the struct to 0. Safe for zeroing all integer and pointer fields. `sizeof(*s)` = "the size of whatever s points to" = `sizeof(GameState)`. This is the standard C "reset a struct" idiom.

**Why `s->head = 9` and `s->tail = 0` for 10 segments?**

```
Indices: 0  1  2  3  4  5  6  7  8  9
         x x  x  x  x  x  x  x  x  H
         ↑                           ↑
        tail                        head
```

Segments 0–9 hold x positions 10–19 (a horizontal snake). `tail=0` is the leftmost (oldest) tip, `head=9` is the rightmost (newest) tip. The snake moves right, so the head is at the right end.

---

## Step 3 — `snake_spawn_food`

```c
void snake_spawn_food(GameState *s) {
    int ok, idx, rem;
    do {
        s->food_x = 1 + rand() % (GRID_WIDTH  - 2);  /* avoid border walls */
        s->food_y = 1 + rand() % (GRID_HEIGHT - 2);
        ok = 1;
        idx = s->tail;
        rem = s->length;
        while (rem > 0) {
            if (s->segments[idx].x == s->food_x &&
                s->segments[idx].y == s->food_y) {
                ok = 0;  /* food landed on snake — retry */
                break;
            }
            idx = (idx + 1) % MAX_SNAKE;
            rem--;
        }
    } while (!ok);
}
```

**New C concept — `do { } while (!ok)`:**
A do-while loop always executes the body at least once, then checks the condition. Here we always pick a random position, then check validity — if invalid, pick again. This is the standard "rejection sampling" pattern.

**Ring buffer traversal:**
```c
idx = s->tail;
rem = s->length;
while (rem > 0) {
    /* visit s->segments[idx] */
    idx = (idx + 1) % MAX_SNAKE;
    rem--;
}
```
Start at `tail`, advance with `(idx+1) % MAX_SNAKE` to wrap around the end of the array, stop after `length` steps. This correctly visits every segment regardless of where head/tail sit in the array.

---

## Key Concepts

- Ring buffer: fixed array + head/tail indices — no per-step `malloc`
- `head`: most recently written slot; `tail`: oldest slot
- Advance: `head = (head + 1) % MAX_SNAKE`; trim: `tail = (tail + 1) % MAX_SNAKE`
- `memset(s, 0, sizeof(*s))` — zero-initialize the entire struct
- Save `best_score` before `memset`, restore after — simple pattern for persistent fields
- `move_timer` / `move_interval` — real-time based movement, frame-rate independent
- Ring buffer traversal: start at `tail`, advance `length` times with `% MAX_SNAKE`

---

## Exercise

What happens if `snake_init` is called when `s->best_score` is 0 (very first run)? Does `memset` zero it again unnecessarily? Is that a problem? Now think about the food spawning loop: what would happen if the snake filled the entire grid (all 1200 cells)? Is `snake_spawn_food` safe in that case?
