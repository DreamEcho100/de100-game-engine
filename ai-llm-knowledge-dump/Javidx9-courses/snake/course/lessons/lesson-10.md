# Lesson 10 — Polish, Integration, and What to Build Next

**By the end of this lesson you will have:** A complete, polished Snake game; both backends verified against a full integration checklist; zero memory leaks confirmed with Valgrind; and a clear map of six extensions you can build next.

---

## What we're doing

No new features. This lesson is about:

1. Verifying everything works end-to-end
2. Understanding the final architecture
3. Running Valgrind to check for memory leaks
4. Comparing our C version to the original C++ code
5. Mapping the road forward

This is the kind of work that separates "code that works on my machine once" from "code I'm confident in."

---

## Step 1 — Tune the speed feel

The current progression:

```
score   speed value   ms per move
0–2         8          1200ms   (very slow start)
3–5         7          1050ms
6–8         6           900ms
9–11        5           750ms
12–14       4           600ms
15–17       3           450ms
18+         2           300ms   (max speed)
```

**Play to score 20+.** Does speed 2 (300ms) feel appropriately challenging? If not, tune `BASE_TICK_MS` in `snake.h`:

- **120ms** base: max speed becomes 240ms — noticeably harder
- **100ms** base: max speed becomes 200ms — quite intense
- **180ms** base: max speed becomes 360ms — more forgiving

This is the only constant you need to change. Because it's in `snake.h`, it affects both backends simultaneously. That's the payoff for having a shared header.

---

## Step 2 — Final source map

Here's every file and what it does:

```
src/
├── snake.h          Constants, Segment, PlatformInput, GameState, declarations
├── snake.c          snake_init(), snake_tick(), snake_spawn_food()
├── platform.h       6-function contract (platform_init through platform_shutdown)
├── main_x11.c       X11 body: window, events, render, main()
└── main_raylib.c    Raylib body: window, events, render, main()
```

**`snake_tick()` control flow** (the heart of the game):

```
snake_tick(state, input)
    │
    ├─ restart requested AND game_over? → snake_init(), return
    ├─ game_over? → return (frozen)
    │
    ├─ Apply turn input (CW/CCW only)
    │
    ├─ tick_count++; if tick_count < speed → return (not time to move yet)
    │   tick_count = 0
    │
    ├─ Commit direction
    ├─ Compute new_x, new_y
    │
    ├─ Wall check → game_over = 1, return
    ├─ Self check → game_over = 1, return
    │
    ├─ Advance head (write new_x/new_y, increment head index, length++)
    │
    ├─ Food check → score++, grow_pending += 5, maybe speed--, spawn_food()
    │
    └─ Grow or advance tail:
           grow_pending > 0 → grow_pending--  (length stays increased)
           else             → tail++, length--
```

Every branch is visible. Nothing is hidden in a class or callback.

---

## Step 3 — Integration checklist

Build both backends and work through this list:

```
BOTH BACKENDS:
[ ] Window opens at correct size (60×14 + 3 header rows × 14 = 882×406 px)
[ ] Arena header shows two horizontal border bars
[ ] Header shows "SCORE: 0" and "SPEED: 1" at start
[ ] Snake is visible at game start (10 green segments, horizontal)
[ ] Snake starts moving immediately (no key press required)
[ ] Left/right arrows turn the snake (clockwise/counter-clockwise)
[ ] Attempting a 180° reversal has no effect
[ ] Steering into the top wall → game over
[ ] Steering into the left wall → game over
[ ] Steering into the right wall → game over
[ ] Steering into the bottom wall → game over
[ ] Steering into own body → game over
[ ] Red food cell is visible
[ ] Eating food: snake grows, food moves, score increments
[ ] Speed increases every 3 food eaten (SPEED display increments)
[ ] Death overlay: "GAME OVER", correct score, restart instruction
[ ] Pressing Space restarts: score=0, snake rebuilt, food respawned
[ ] Pressing Space when alive does nothing
[ ] Best score survives a restart
```

Fix anything that fails before continuing.

---

## Step 4 — Run Valgrind (X11 backend)

```sh
valgrind --leak-check=full ./snake_x11
```

Play for a few seconds, eat some food, die, restart, then close the window normally.

**Reading Valgrind output:**

```
LEAK SUMMARY:
   definitely lost: 0 bytes in 0 blocks    ← MUST be 0
   indirectly lost: 0 bytes in 0 blocks    ← MUST be 0
     possibly lost: 0 bytes in 0 blocks    ← aim for 0
   still reachable: N bytes in M blocks    ← ACCEPTABLE for X11
```

- **"definitely lost"** = you called `malloc` and never `free`d it. This is a real bug.
- **"still reachable"** = memory that was allocated but freed at process exit (common for X11 internals). Not a bug.
- **"indirectly lost"** = freed a pointer to a struct but not a pointer inside the struct. Fix these.

Our game uses no `malloc` directly (all state is on the stack or in static arrays), so you should see 0 definitely lost. The X11 library itself will have "still reachable" — that's expected and not your fault.

---

## Step 5 — C vs C++ comparison

Here's how our implementation differs from the original OLC C++ Snake:

```
Original C++ (OneLoneCoder)        Our C version
─────────────────────────────────  ──────────────────────────────────
std::list<sSnakeSegment>       →   Ring buffer: Segment segments[1200]
push_front() / pop_back()      →   head = (head+1)%MAX; tail = (tail+1)%MAX
Windows.h console buffer       →   X11 window / Raylib window
GetAsyncKeyState()             →   X11 KeyPress events / IsKeyPressed()
std::chrono 120ms/200ms        →   Fixed BASE_TICK_MS = 150
srand() in global scope        →   srand(time(NULL)) in snake_init()
grow: push 5 duplicate entries →   grow_pending += 5 counter
No file split                  →   snake.h / snake.c / platform.h split
```

The biggest architectural difference: the original uses a linked list (`std::list`) where the head is always at the front and the tail is always at the back. Our ring buffer achieves the same logical behavior with a fixed-size array — no allocation, no pointer chasing, cache-friendly.

The `grow_pending` counter is our design. The original literally pushed 5 copies of the head into the list. Our version does the same thing logically (the tail stays put for 5 ticks) but doesn't waste memory.

---

## Step 6 — What to build next

Six concrete extensions, roughly in order of difficulty:

### 1. Wrap-around walls (easy)
Instead of dying at edges, wrap to the opposite side:

```c
/* In snake_tick(), replace wall check with: */
if (new_x < 0)           new_x = GRID_WIDTH - 1;
if (new_x >= GRID_WIDTH)  new_x = 0;
if (new_y < 0)           new_y = GRID_HEIGHT - 1;
if (new_y >= GRID_HEIGHT) new_y = 0;
/* Remove the game_over = 1 wall check entirely */
```

Pac-Man style. Adds a very different feel to the game.

### 2. Occupancy grid (medium — improves performance)
Replace the O(n) self-collision loop with an O(1) lookup:

```c
/* In GameState, add: */
int occupied[GRID_HEIGHT][GRID_WIDTH];

/* Update on head advance: */
state->occupied[new_y][new_x] = 1;

/* Update on tail advance: */
state->occupied[state->segments[state->tail].y]
               [state->segments[state->tail].x] = 0;

/* Self-collision check becomes: */
if (state->occupied[new_y][new_x]) { game_over... }
```

Initialize `occupied` in `snake_init()` to mark the starting snake segments as 1.

### 3. Multiple food items (medium)
Store food as an array:

```c
#define MAX_FOOD 3
int food_x[MAX_FOOD], food_y[MAX_FOOD];
```

In render, draw all `MAX_FOOD` cells. In `spawn_food`, fill all slots. In food collision, loop over all food positions. Eating any one respawns just that one food cell.

### 4. Obstacle walls (medium)
Add random wall cells placed at game start. Store them in the occupancy grid as value `2` (snake=1, wall=2). Add them to `platform_render()` as gray cells. Add a wall collision check in `snake_tick()` that checks `occupied[new_y][new_x] == 2`.

### 5. Persistent high score file (easy once you know fopen)
```c
/* Save after death: */
FILE *f = fopen("highscore.txt", "w");
if (f) { fprintf(f, "%d\n", state->best_score); fclose(f); }

/* Load at snake_init(): */
FILE *f = fopen("highscore.txt", "r");
if (f) { fscanf(f, "%d", &state->best_score); fclose(f); }
```

`fopen`, `fprintf`, `fscanf`, `fclose` — four functions. That's all file I/O in C.

### 6. Sound with Raylib (easy with Raylib)
```c
/* In platform_init(): */
InitAudioDevice();
Sound eat_sound  = LoadSound("assets/eat.wav");
Sound die_sound  = LoadSound("assets/die.wav");

/* In platform_render() or a new platform_play_sound() call: */
if (just_ate)  PlaySound(eat_sound);
if (just_died) PlaySound(die_sound);

/* In platform_shutdown(): */
UnloadSound(eat_sound);
UnloadSound(die_sound);
CloseAudioDevice();
```

Raylib ships with audio support. Free `.wav` files are available at freesound.org. X11 has no built-in audio — for X11 sound you'd use a library like `libasound` (ALSA).

---

## Build & Run — Final

```sh
# Build X11
gcc -o snake_x11 src/main_x11.c src/snake.c -lX11

# Build Raylib
gcc -o snake_raylib src/main_raylib.c src/snake.c -lraylib -lm

# Valgrind check
valgrind --leak-check=full ./snake_x11
```

---

## Mental Model — The Complete Picture

```
snake.h     ← shared vocabulary (types, constants)
    │
    ├── snake.c         ← the game (platform-free)
    │       snake_init()
    │       snake_tick()
    │       snake_spawn_food()
    │
    └── platform.h      ← the contract (6 functions)
            │
            ├── main_x11.c      implements platform.h using X11
            │       main() calls: platform_init
            │                     game loop:
            │                       platform_get_input
            │                       snake_tick
            │                       platform_render
            │                     platform_shutdown
            │
            └── main_raylib.c   implements platform.h using Raylib
                    (identical main(), different platform_* bodies)
```

> **Two complete games that share one brain. The brain has zero platform dependencies.**

You now understand:
- C memory model (stack arrays, no heap)
- Ring buffers (fixed array + two moving indices)
- Collision detection (bounds check + buffer scan)
- Game state as a plain struct (no classes, no hidden state)
- Header files as contracts (like TypeScript `.d.ts`)
- Platform abstraction (like dependency injection)
- Valgrind (memory safety verification)

That's a solid foundation for any C systems project.
