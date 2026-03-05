# Lesson 04: BFS Distance Field

## What we're building

A **BFS distance field** — a number stored in every grid cell that tells a creep
exactly how many cells it must traverse to reach the exit.  Once computed, any
number of creeps can find the optimal path at zero additional cost: just always
move to the neighbour with the lowest distance.

We also add a **debug visualization**: cells near the exit are bright green, cells
far away shade toward dark red, giving an instant visual readout of the flow field.

## What you'll learn

- The **BFS (Breadth-First Search) algorithm** and why it is perfect for tower defense
- How to implement a **queue** using a flat C array (no `malloc`, no linked list)
- The **flow field / distance map** concept: one BFS run serves all creeps

## Prerequisites

- Lesson 03 complete and building on both backends

---

## Why BFS from the exit? — Concept first

Before writing any code, understand the design decision.

### Forward vs backward BFS

There are two directions you could run BFS:

| Direction | Start | What you get |
|---|---|---|
| Forward (naïve) | Each creep's current cell | Shortest path for one creep |
| **Backward (our approach)** | Exit cell | Distance-to-exit for every cell |

**Backward BFS from the exit** is the right choice because:
1. The exit is the **same destination for every creep**.
2. We compute the distance field **once** whenever the grid changes (a tower is
   placed or sold).
3. Every creep in the field can then navigate by gradient descent: move to the
   neighbour cell with the smallest `dist[]` value — that is always the shortest
   path to the exit.
4. 256 creeps would need 256 separate forward BFS runs; backward BFS costs one.

### Why not A*?

A* finds a path from one source to one target.  It needs a known target for each
pathfinder.  Our target (the exit) is the same for all creeps — BFS is simpler,
equally optimal, and produces the useful side effect of the full distance map.

---

## BFS wave diagram

Starting from the exit cell `(29, 9)`:

```
  Step 0 — seed the queue with the exit cell:
    dist[29][9] = 0

  Step 1 — expand to unvisited neighbours of dist=0 cells:
    dist[28][9] = 1   dist[29][8] = 1   dist[29][10] = 1

  Step 2 — expand to unvisited neighbours of dist=1 cells:
    dist[27][9] = 2   dist[28][8] = 2   dist[28][10] = 2
    dist[29][7] = 2   dist[29][11] = 2
    ...

  Step N — wave continues until every reachable cell has a distance.
  Cells blocked by towers get dist = -1 (unreachable).
```

The wave propagates like ripples in a pond.  Each "ring" of cells is exactly one
step farther from the exit than the previous ring.  Creeps follow this field by
always stepping to the cell with the smallest dist value.

---

## Step 1: `bfs_fill_distance` — complete implementation

Add this function to `src/game.c`.  Every line is commented.

```c
/* src/game.c  —  bfs_fill_distance (Lesson 04)
 *
 * Fills state->dist[] with the BFS distance from each reachable EMPTY (or
 * ENTRY) cell to the EXIT cell.
 *
 * Algorithm: backward BFS from the exit.
 *   - dist[exit] = 0
 *   - For each cell dequeued, examine its 4 orthogonal neighbours.
 *   - If a neighbour is passable and not yet visited, assign dist+1 and enqueue.
 *   - CELL_TOWER cells are walls — skip them.
 *   - Cells that BFS never reaches keep dist = -1 (no path to exit).
 *
 * Queue implementation: a flat int array used as a ring buffer.
 *   head = index of the next cell to dequeue.
 *   tail = index where the next cell will be enqueued.
 *   When tail wraps past QUEUE_CAP it restarts at 0.
 *   The queue never needs more than GRID_ROWS * GRID_COLS entries.
 *
 * Time complexity: O(GRID_ROWS * GRID_COLS) — each cell is enqueued at most once.
 * Space complexity: same — the queue array is stack-allocated.
 */

#define QUEUE_CAP (GRID_ROWS * GRID_COLS)

void bfs_fill_distance(GameState *state) {
    /* Step 1: initialise every cell to -1 ("not yet visited / unreachable"). */
    for (int i = 0; i < GRID_ROWS * GRID_COLS; i++)
        state->dist[i] = -1;

    /* Step 2: seed the queue with the exit cell.
     * The exit itself has distance 0 — it IS the destination. */
    int exit_idx = EXIT_ROW * GRID_COLS + EXIT_COL;
    state->dist[exit_idx] = 0;

    /* Flat array queue: stores cell indices (row * GRID_COLS + col). */
    int queue[QUEUE_CAP];
    int head = 0; /* next index to dequeue */
    int tail = 0; /* next index to enqueue into */

    /* Enqueue the exit cell. */
    queue[tail++] = exit_idx;

    /* Step 3: BFS main loop — run until the queue is empty. */
    while (head != tail) {
        /* Dequeue the front cell. */
        int idx = queue[head++];
        if (head >= QUEUE_CAP) head = 0; /* wrap around ring buffer */

        /* Convert flat index back to (row, col). */
        int row = idx / GRID_COLS;
        int col = idx % GRID_COLS;

        /* Current distance — neighbours get this + 1. */
        int d = state->dist[idx];

        /* Step 4: examine 4 orthogonal neighbours (up, down, left, right).
         * For each neighbour:
         *   a) Check grid bounds — don't step outside the array.
         *   b) Check if the cell is passable (not CELL_TOWER).
         *   c) Check if not yet visited (dist == -1).
         *   If all three pass: assign dist[neighbour] = d + 1 and enqueue. */
        int nr, nc, nidx;

        /* Up */
        nr = row - 1; nc = col;
        if (nr >= 0) {
            nidx = nr * GRID_COLS + nc;
            if ((CellState)state->grid[nidx] != CELL_TOWER &&
                state->dist[nidx] == -1) {
                state->dist[nidx] = d + 1;
                queue[tail++] = nidx;
                if (tail >= QUEUE_CAP) tail = 0;
            }
        }

        /* Down */
        nr = row + 1; nc = col;
        if (nr < GRID_ROWS) {
            nidx = nr * GRID_COLS + nc;
            if ((CellState)state->grid[nidx] != CELL_TOWER &&
                state->dist[nidx] == -1) {
                state->dist[nidx] = d + 1;
                queue[tail++] = nidx;
                if (tail >= QUEUE_CAP) tail = 0;
            }
        }

        /* Left */
        nr = row; nc = col - 1;
        if (nc >= 0) {
            nidx = nr * GRID_COLS + nc;
            if ((CellState)state->grid[nidx] != CELL_TOWER &&
                state->dist[nidx] == -1) {
                state->dist[nidx] = d + 1;
                queue[tail++] = nidx;
                if (tail >= QUEUE_CAP) tail = 0;
            }
        }

        /* Right */
        nr = row; nc = col + 1;
        if (nc < GRID_COLS) {
            nidx = nr * GRID_COLS + nc;
            if ((CellState)state->grid[nidx] != CELL_TOWER &&
                state->dist[nidx] == -1) {
                state->dist[nidx] = d + 1;
                queue[tail++] = nidx;
                if (tail >= QUEUE_CAP) tail = 0;
            }
        }
    }
    /* After the loop, every reachable cell has dist >= 0.
     * Every unreachable cell (behind a wall of towers) keeps dist == -1. */
}
```

**Queue implementation — why a flat array ring buffer?**

In JavaScript you'd write `queue.push(item)` / `queue.shift()`.  In C:
- `queue.shift()` (dequeue from the front) is O(N) if you slide the array.
- Using a **head/tail ring buffer** makes both enqueue and dequeue O(1).
- We know the maximum queue size up front (`GRID_ROWS * GRID_COLS` = 600) so a
  stack-allocated array is safe and requires zero heap allocation.

```
                 head                   tail
                  ↓                      ↓
  queue: [ ... | 42 | 77 | 91 | 130 |   |  ... ]
                       ↑
                  next to dequeue

  Dequeue: read queue[head], head++
  Enqueue: write queue[tail], tail++
  Both wrap around at QUEUE_CAP.
```

---

## Step 2: Call `bfs_fill_distance` from `game_init`

```c
void game_init(GameState *state) {
    memset(state, 0, sizeof(*state));
    state->hover_col = -1;
    state->hover_row = -1;
    state->grid[ENTRY_ROW * GRID_COLS + ENTRY_COL] = CELL_ENTRY;
    state->grid[EXIT_ROW  * GRID_COLS + EXIT_COL ] = CELL_EXIT;

    /* Compute the initial distance field.
     * At this point the grid is empty so all cells are reachable. */
    bfs_fill_distance(state);
}
```

In Lesson 05 we will also call `bfs_fill_distance` whenever a tower is placed or
removed.  For now, calling it once in `game_init` is sufficient to see the field.

---

## Step 3: Debug distance visualization in `game_render`

Add this static helper and call it inside `game_render`:

```c
/* draw_dist_debug: tint each cell by its BFS distance value.
 *
 * Color mapping:
 *   dist =  0  → pure bright green  (the exit)
 *   dist = max → dark red           (farthest from exit)
 *   dist = -1  → dark grey          (unreachable / tower)
 *
 * We lerp from green (0,255,0) to red (255,0,0) based on normalized distance:
 *   t = dist / max_dist   (0.0 = at exit, 1.0 = farthest)
 *   r = (int)(t * 255)
 *   g = (int)((1.0f - t) * 255)
 *
 * This gives each cell a unique hue that shows the BFS gradient at a glance.
 *
 * Call this AFTER draw_grid so the tint overlays the checkerboard. */
static void draw_dist_debug(const GameState *state, Backbuffer *bb) {
    /* Find the maximum distance (for normalization). */
    int max_dist = 1;
    for (int i = 0; i < GRID_ROWS * GRID_COLS; i++)
        if (state->dist[i] > max_dist)
            max_dist = state->dist[i];

    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            int idx  = row * GRID_COLS + col;
            int dist = state->dist[idx];
            int px   = col * CELL_SIZE;
            int py   = row * CELL_SIZE;

            if (dist < 0) {
                /* Unreachable — draw dark grey, 50% blend */
                draw_rect_blend(bb, px, py, CELL_SIZE, CELL_SIZE,
                                GAME_RGB(0x44, 0x44, 0x44), 180);
            } else {
                /* Normalize: t = 0.0 at exit, 1.0 at farthest cell. */
                float t = (float)dist / (float)max_dist;
                int   r = (int)(t * 200.0f);
                int   g = (int)((1.0f - t) * 200.0f) + 55;
                draw_rect_blend(bb, px, py, CELL_SIZE, CELL_SIZE,
                                GAME_RGB(r, g, 0x00), 160);
            }
        }
    }
}

void game_render(const GameState *state, Backbuffer *bb) {
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BG);
    draw_grid(state, bb);
    draw_dist_debug(state, bb);  /* ← NEW: distance field overlay */
    draw_rect(bb, GRID_PIXEL_W, 0, bb->width - GRID_PIXEL_W, bb->height, COLOR_PANEL_BG);
}
```

---

## Step 4: Verify the distance values

Add a temporary `printf` to `game_init` to print the entry cell's distance:

```c
/* Temporary debug print — remove after verifying BFS is correct. */
#ifdef DEBUG
{
    int entry_idx  = ENTRY_ROW * GRID_COLS + ENTRY_COL;
    int exit_idx   = EXIT_ROW  * GRID_COLS + EXIT_COL;
    printf("BFS: dist[entry]=%-3d  dist[exit]=%d\n",
           state->dist[entry_idx], state->dist[exit_idx]);
    /* Expected: dist[exit]=0, dist[entry]=29 (29 columns apart, same row) */
}
#endif
```

---

## Build and run

```bash
# Raylib
./build-dev.sh --backend=raylib -r

# X11
./build-dev.sh --backend=x11 -r
```

**Expected output:**
- Terminal: `BFS: dist[entry]=29  dist[exit]=0`
- Window: The exit cell (column 29, row 9) has a bright green tint.  Moving left
  from the exit, cells gradually shade toward dark red.  The entry cell
  (column 0, row 9) should be the darkest red — it is 29 steps from the exit.
- Cells off the central row are also colored: they have slightly larger distances
  because they must travel diagonally around to the straight path.

---

## Exercises

1. **Beginner:** `printf` the distance of the cell at `(col=14, row=9)` (the centre
   of the row).  Verify it is 15 (14 steps from exit on the direct path through row 9,
   plus the exit cell itself at 0).

2. **Intermediate:** Manually set `state->grid[9 * GRID_COLS + 15] = CELL_TOWER` in
   `game_init`, then call `bfs_fill_distance` again.  Observe how the visualization
   changes — the wave must route around the blocked cell.  Check that `dist[entry]`
   is still reachable (> 0) even with one tower blocking the direct path.

3. **Challenge:** The current BFS only supports 4-directional movement (up, down,
   left, right).  Add diagonal movement (8 directions) as an experiment.  What changes
   in the BFS expansion loop?  What changes in the distance values?  Why might
   8-directional BFS be a worse choice for a tower-defense grid game?

---

## What's next

In Lesson 05 we add tower placement: clicking an empty cell places a tower, the BFS
is recomputed, and a legality check prevents players from fully blocking the creep
path.  We also add sell functionality with a 70% gold refund.
