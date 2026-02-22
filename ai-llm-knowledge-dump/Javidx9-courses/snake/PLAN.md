# Snake Course Plan

> **Source:** `OneLoneCoder_Snake.cpp` (C++, Windows console, by javidx9)
> **Output:** C port with X11 and Raylib backends, 10 build-along lessons
> **Student:** DreamEcho100 — React/TS dev, C beginner, not strong at math

---

## What the Program Does

Classic Snake. A snake moves around a grid. Left/right arrows turn it CW/CCW.
Eat food → grow by 5 segments. Hit a wall or yourself → game over. Space to restart.

```
┌──── SCORE: 3 ──────────────────────────────────┐
├────────────────────────────────────────────────┤
│                                                │
│              %                                 │  ← food
│                                                │
│        @OOOOOOO                                │  ← head @ + body O
│                                                │
└────────────────────────────────────────────────┘
```

---

## Original Code Key Points

- `list<sSnakeSegment>` for snake body → we replace with a **ring buffer**
- `push_front` = add new head, `pop_back` = remove tail (unless growing)
- Direction 0=UP 1=RIGHT 2=DOWN 3=LEFT. Right key: dir+1. Left key: dir+3 (≡ dir-1 mod 4)
- 120ms/200ms timing (aspect ratio trick) → we use fixed 150ms (pixel cells are square)
- Food placement: random position not occupied by snake
- Grow: push 5 duplicate tail segments on eat

---

## C Port: Ring Buffer for the Snake

Instead of `std::list`, we use a circular buffer (fixed array + head/tail indices).

```
segments[MAX_SNAKE]  — fixed array big enough for a full-grid snake (60×20=1200)
head                 — index where the most recent head was written
tail                 — index of the oldest segment (the tail tip)
length               — current number of segments

Each tick:
  1. head = (head + 1) % MAX_SNAKE       ← advance head pointer
  2. segments[head] = new_head_position  ← write new head
  3. if NOT growing: tail = (tail+1)%MAX_SNAKE  ← advance tail (pop)
  4. if growing: grow_pending--          ← skip the pop, snake gets longer
```

Direction system:
```
0=UP  (dx=0, dy=-1)    Turn right (CW):  dir = (dir+1)%4
1=RIGHT (dx=1, dy=0)   Turn left  (CCW): dir = (dir+3)%4
2=DOWN (dx=0, dy=1)    Can't reverse:    |new-old|≠2
3=LEFT (dx=-1, dy=0)
```

---

## File Structure

```
course/
├── build_x11.sh
├── build_raylib.sh
├── src/
│   ├── snake.h        ← GameState, Segment, PlatformInput, declarations
│   ├── snake.c        ← game logic (pure C, no OS)
│   ├── platform.h     ← 6-function platform contract
│   ├── main_x11.c     ← X11 backend
│   └── main_raylib.c  ← Raylib backend
└── lessons/
    ├── lesson-01.md … lesson-10.md
```

---

## Lesson Sequence

| # | What gets built | What you see |
|---|---|---|
| 01 | Open a window (X11 + Raylib) | Black window, Q to quit |
| 02 | Draw the arena | Two border lines, score label |
| 03 | Snake ring buffer — draw static snake | 10-segment snake in the arena |
| 04 | Movement — snake advances each tick | Snake glides across the screen |
| 05 | Input — turn left/right | Arrow keys change direction |
| 06 | Collision — walls + self | Snake dies; GAME OVER overlay |
| 07 | Food — spawn, eat, grow | Food appears, eating grows the snake |
| 08 | Score + death screen + restart | Score updates; Space restarts |
| 09 | File split: snake.h/c + platform.h | Clean structure, same behavior |
| 10 | Speed scaling + polish + Valgrind | Faster as score grows; fully complete |

---

## GameState Struct (final design)

```c
#define GRID_WIDTH   60
#define GRID_HEIGHT  20
#define CELL_SIZE    20
#define MAX_SNAKE    (GRID_WIDTH * GRID_HEIGHT)
#define HEADER_ROWS   3

typedef struct { int x; int y; } Segment;

typedef struct {
    Segment segments[MAX_SNAKE];
    int head;          /* ring buffer head index */
    int tail;          /* ring buffer tail index */
    int length;

    int grow_pending;  /* segments yet to be added */
    int direction;     /* 0=UP 1=RIGHT 2=DOWN 3=LEFT */
    int next_direction;/* buffered input turn */

    int food_x;
    int food_y;

    int score;
    int speed;         /* ticks per move (starts high, decreases) */
    int tick_count;    /* accumulator → triggers move when == speed */
    int game_over;
} GameState;
```
