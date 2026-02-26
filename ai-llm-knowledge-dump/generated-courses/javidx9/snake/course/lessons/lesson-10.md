# Lesson 10 — Collision Detection: Walls + Self

## By the end of this lesson you will have:

Wall collision and self-collision checks inside `snake_update`, with `game_over` set when the snake's new head position hits a border or any of its own body segments.

---

## When Collision is Checked

Collision is checked after the snake has committed to its new head position — not before. The sequence inside the "move step" (after `move_timer >= move_interval`):

```
1. Apply turn: current_dir = next_dir
2. Compute new head position: nx, ny
3. Check wall collision    → if hit, game_over = 1, return
4. Check self collision    → if hit, game_over = 1, return
5. Check food collision    → grow or advance tail
6. Push new head to ring buffer
```

---

## Step 1 — Apply direction and compute new head

```c
/* Apply queued direction */
SNAKE_DIR opposite = (SNAKE_DIR)((s->current_dir + 2) % 4);
if (s->next_dir != opposite)
    s->current_dir = s->next_dir;

/* Compute new head position */
int nx = s->segments[s->head].x;
int ny = s->segments[s->head].y;
switch (s->current_dir) {
    case SNAKE_DIR_UP:    ny--; break;
    case SNAKE_DIR_RIGHT: nx++; break;
    case SNAKE_DIR_DOWN:  ny++; break;
    case SNAKE_DIR_LEFT:  nx--; break;
}
```

The 180° reversal guard: `opposite = (current_dir + 2) % 4`. If `next_dir == opposite`, ignore it (keep `current_dir`). A player who tries to go back into their own body is protected.

---

## Step 2 — Wall collision

The play field is bounded by wall cells on all sides. The playable interior is:

```
col: 1 to GRID_WIDTH - 2  (walls on col 0 and GRID_WIDTH-1)
row: 0 to GRID_HEIGHT - 2  (wall on row GRID_HEIGHT-1, header above row 0)
```

```c
/* Wall collision: hit wall cells (col 0, col GRID_WIDTH-1, row GRID_HEIGHT-1) */
if (nx <= 0 || nx >= GRID_WIDTH - 1 ||
    ny < 0  || ny >= GRID_HEIGHT - 1) {
    s->game_over = 1;
    if (s->score > s->best_score)
        s->best_score = s->score;
    return;
}
```

**Why `ny < 0`?** The snake starts near the middle of the field. Moving up from row 0 would give `ny = -1`. There's no visible wall at the top — the header acts as the boundary. Allowing `ny=-1` would wrap into the header visually, so we treat `ny < 0` as a wall hit.

**`best_score` update on death:**
Both wall and self collision update `best_score`. It's cleaner to update in one place (the end of `snake_update`, or right before setting `game_over = 1`). In our code we update it in both collision branches to be explicit, or factor it out:

```c
static void trigger_game_over(GameState *s) {
    if (s->score > s->best_score)
        s->best_score = s->score;
    s->game_over = 1;
}
```

---

## Step 3 — Self collision

Traverse every segment except the tail. The tail is excluded because it will move away on this step — if the snake is growing, the tail stays, but the head can't actually land on the tail cell because the body would fill that position before the head gets there in practice.

```c
/* Self collision: check new head against all body segments */
int idx = s->tail;
int rem = s->length;
int hit_self = 0;
while (rem > 0) {
    if (s->segments[idx].x == nx && s->segments[idx].y == ny) {
        hit_self = 1;
        break;
    }
    idx = (idx + 1) % MAX_SNAKE;
    rem--;
}
if (hit_self) {
    trigger_game_over(s);
    return;
}
```

**Ring buffer traversal (same pattern as rendering):**
```c
idx = s->tail;
rem = s->length;
while (rem > 0) {
    /* use segments[idx] */
    idx = (idx + 1) % MAX_SNAKE;
    rem--;
}
```

Start at `tail`, count `length` segments, wrap with `% MAX_SNAKE`. This always visits every occupied cell in order from oldest (tail) to newest (head).

**New C concept — `break` in a `while` loop:**
`break` exits the innermost loop immediately. Here it stops the traversal as soon as a collision is found — no need to keep checking once we know the snake is dead.

---

## Step 4 — Ring buffer: head and tail pointers

```c
/* Push new head */
s->head = (s->head + 1) % MAX_SNAKE;
s->segments[s->head].x = nx;
s->segments[s->head].y = ny;

/* Advance tail (unless growing) */
if (s->grow_pending > 0) {
    s->grow_pending--;
    s->length++;
} else {
    s->tail = (s->tail + 1) % MAX_SNAKE;
}
```

The ring buffer has `MAX_SNAKE` slots. `head` is the index of the newest segment; `tail` is the oldest. Both wrap with `% MAX_SNAKE`.

When `grow_pending > 0`:
- **Don't** advance `tail` → the old tail cell stays in the buffer → snake appears one longer
- Decrement `grow_pending` and increment `length`

When not growing:
- Advance `tail` → oldest cell leaves the buffer → tail shrinks → snake length stays constant

---

## Visualizing the Ring Buffer

At length=4, `MAX_SNAKE=8`:
```
Slots:   [0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]
          H    .    .    .    T    .    .    .
         newest                oldest
```
`head=0, tail=4, length=4`.

After one step (no growth):
```
New head at slot 1. Tail advances to slot 5.
Slots:   [0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]
               H    .    .         T    .    .
```

After one step with growth:
```
New head at slot 1. Tail stays at slot 4.
Slots:   [0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]
               H    .    .    T    .    .    .
         length=5 now
```

---

## Key Concepts

- Collision checked after computing new head position (`nx`, `ny`)
- Wall bounds: `nx <= 0 || nx >= GRID_WIDTH-1 || ny < 0 || ny >= GRID_HEIGHT-1`
- Self collision: traverse ring buffer from `tail`, `length` steps, compare each `(x,y)` to `(nx,ny)`
- Ring buffer traversal: `idx=tail; rem=length; while(rem>0) { use[idx]; idx=(idx+1)%MAX; rem--; }`
- `break` exits the loop on first collision found
- On collision: `best_score = max(score, best_score)`, then `game_over = 1`
- Tail not advanced when `grow_pending > 0` → snake grows by keeping old tail cell

---

## Exercise

The self-collision check visits all `length` segments including the tail. In theory, when the snake is not growing, the tail will vacate its current cell this step — so the head could safely move into the tail's old position. Modify the traversal to skip the tail (`rem = s->length - 1` if not growing). Does this change the gameplay feel? When would the player notice the difference?
