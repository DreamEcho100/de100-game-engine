# Lesson 8 — Score Display, Speed, Death Screen, and Restart

**By the end of this lesson you will have:** Score and speed shown in the header, a death screen displaying the final score with "Press Space to restart", and a working restart that resets the whole game without recreating the window.

---

## What we're building

Four connected features:

1. **Score in the header** — display the live score using `snprintf` + the text-draw call you already use for borders.
2. **Speed progression** — every 3 food eaten, the `speed` counter decreases (= faster ticks).
3. **Death screen** — "GAME OVER" + final score + restart instruction, drawn over the play area.
4. **Restart** — pressing Space/Enter resets all game variables and rebuilds the initial snake.

None of these require new platform code. They're all pure game-logic and render additions.

---

## Step 1 — Score in the header

You already draw two border bars and possibly a title in the header. Add a score string next to them.

**X11:**
```c
char buf[64];
snprintf(buf, sizeof(buf), "SCORE: %d", score);
XSetForeground(display, gc, 0xFFFFFF);
XDrawString(display, window, gc,
            10, CELL_SIZE * 2 - 4,   /* row 2 of header, a few px up from baseline */
            buf, strlen(buf));
```

**Raylib:**
```c
char buf[64];
snprintf(buf, sizeof(buf), "SCORE: %d", score);
DrawText(buf, 10, CELL_SIZE, 14, WHITE);
```

**Why `snprintf` and not `sprintf`?**

`snprintf(buf, sizeof(buf), ...)` guarantees it will never write more than `sizeof(buf)` characters. `sprintf` has no such limit and can silently overflow the buffer — a classic C bug. Always use `snprintf`.

---

## Step 2 — Speed progression

Add a `speed` variable (you should already have it from the movement system). If not, add:

```c
int speed = 8;   /* starts at 8; lower = faster */
```

In the food-eaten block (where you do `score++` and `grow_pending += 5`), add:

```c
if (score % 3 == 0 && speed > 2)
    speed--;
```

**Speed table:**

```
speed value    ticks per move    ms per move (BASE_TICK_MS=150)
    8              8                  1200ms    (starting speed)
    7              7                  1050ms
    6              6                   900ms
    5              5                   750ms
    4              4                   600ms
    3              3                   450ms
    2              2                   300ms    (max speed, never goes lower)
```

Lower `speed` = fewer ticks before moving = faster snake.

---

## Step 3 — Speed in the header

Add a second string to the header:

```c
snprintf(buf, sizeof(buf), "SPEED: %d", 9 - speed);
```

This inverts the display: `speed=8` shows as "SPEED: 1" (slow), `speed=2` shows as "SPEED: 7" (fast). Players expect higher numbers to mean faster, so we display the complement.

Place it to the right of the score, e.g. at x=200 in X11 or x=200 in Raylib.

---

## Step 4 — Death screen overlay

Replace (or augment) the simple "GAME OVER" box from lesson 6 with a richer overlay.

Compute the center of the **play area** (not the whole window — offset by the header):

```c
int play_top = HEADER_ROWS * CELL_SIZE;          /* pixel y where play area starts */
int cx = (GRID_WIDTH  * CELL_SIZE) / 2;          /* horizontal center */
int cy = play_top + (GRID_HEIGHT * CELL_SIZE) / 2; /* vertical center of play area */
```

**X11:**
```c
if (game_over) {
    int bw = 260, bh = 110;
    int bx = cx - bw / 2, by = cy - bh / 2;

    XSetForeground(display, gc, 0x111111);
    XFillRectangle(display, window, gc, bx, by, bw, bh);

    /* Border */
    XSetForeground(display, gc, 0xFF3333);
    XDrawRectangle(display, window, gc, bx, by, bw, bh);

    /* Title */
    XSetForeground(display, gc, 0xFF3333);
    XDrawString(display, window, gc, bx + 85, by + 28, "GAME OVER", 9);

    /* Final score */
    char sbuf[32];
    snprintf(sbuf, sizeof(sbuf), "Score: %d", score);
    XSetForeground(display, gc, 0xFFFFFF);
    XDrawString(display, window, gc, bx + 85, by + 52, sbuf, strlen(sbuf));

    /* Restart hint */
    XDrawString(display, window, gc,
                bx + 38, by + 80,
                "Space / Enter to restart", 24);
}
```

**Raylib:**
```c
if (game_over) {
    int bw = 260, bh = 110;
    int bx = cx - bw / 2, by = cy - bh / 2;

    DrawRectangle(bx, by, bw, bh, (Color){17, 17, 17, 230});
    DrawRectangleLines(bx, by, bw, bh, RED);
    DrawText("GAME OVER",      bx + 72, by + 18, 22, RED);

    char sbuf[32];
    snprintf(sbuf, sizeof(sbuf), "Score: %d", score);
    DrawText(sbuf,                 bx + 90, by + 50, 16, WHITE);
    DrawText("Space / Enter to restart",
                                   bx + 28, by + 78, 13, LIGHTGRAY);
}
```

---

## Step 5 — Restart variables and key flag

Add to your variable declarations:

```c
int restart_pressed = 0;
```

This is a "latch" flag — we set it to 1 when the key is pressed, read it once in the game loop, then clear it. This prevents the restart from firing multiple times per keypress.

**X11 — in the `KeyPress` handler:**
```c
if (key == XK_space || key == XK_Return)
    restart_pressed = 1;
```

**Raylib — in the input section of your main loop:**
```c
if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))
    restart_pressed = 1;
```

---

## Step 6 — Restart logic

In the main loop, before the movement/render logic, add:

```c
if (game_over && restart_pressed) {
    /* --- Reset game state --- */
    game_over      = 0;
    score          = 0;
    speed          = 8;
    tick_count     = 0;
    grow_pending   = 0;
    direction      = 1;          /* start moving RIGHT */
    next_direction = 1;

    /* --- Rebuild initial snake (10 segments, horizontal, mid-screen) --- */
    memset(segments, 0, sizeof(segments));
    head = 9; tail = 0; length = 10;
    for (int i = 0; i < 10; i++) {
        segments[i].x = 10 + i;   /* cells 10..19 on x-axis */
        segments[i].y = GRID_HEIGHT / 2;
    }

    /* --- Place new food --- */
    spawn_food(segments, tail, length, &food_x, &food_y);

    restart_pressed = 0;
}
```

**Why does this work?**

Restarting a game is just resetting variables. The X11 connection, the window, the GC — none of that needs to be recreated. Only the game state changes. This is why keeping platform code separate from game logic matters: you can nuke the game state without touching the platform.

---

## Step 7 — Best score (bonus, sets up the exercise)

Add:

```c
int best_score = 0;
```

In the game-over detection (where you set `game_over = 1`):

```c
if (score > best_score) best_score = score;
```

Display it in the header:

```c
snprintf(buf, sizeof(buf), "BEST: %d", best_score);
```

`best_score` is NOT reset in the restart block — it persists across games.

---

## Build & Run

**X11:**
```sh
gcc -o snake_x11 src/main_x11.c -lX11 && ./snake_x11
```

**Raylib:**
```sh
gcc -o snake_raylib src/main_raylib.c -lraylib -lm && ./snake_raylib
```

**Full test checklist:**
- [ ] Header shows "SCORE: 0" at start
- [ ] Header shows "SPEED: 1" at start
- [ ] Eating 3 food increases displayed speed to "SPEED: 2"
- [ ] Snake visibly moves faster at higher speed
- [ ] Death shows "GAME OVER" + correct score + restart hint
- [ ] Pressing Space resets score to 0 and snake to starting position
- [ ] Best score survives a restart
- [ ] Pressing Space when NOT dead does nothing

---

## Mental Model

> **Restart is just resetting variables. The platform is unchanged. Only the game data resets.**

```
Game variables:            Platform:
  score     ← reset 0       X11 Display    ← untouched
  speed     ← reset 8       X11 Window     ← untouched
  snake     ← rebuilt       X11 GC         ← untouched
  food      ← respawned
  game_over ← reset 0
```

This is a preview of the full platform/game split you'll do in lesson 9. The window is not the game. The game is just data. Data can be reset without recreating the window.

**The latch pattern:**

```
Key pressed →  restart_pressed = 1
Main loop:     if (game_over && restart_pressed)
                   do restart
                   restart_pressed = 0   ← clear after handling
```

Without the latch, the game would restart on every frame the key is held down, which could skip through multiple restarts in a fraction of a second.

---

## Exercise

Add a persistent high score that survives restarts:

1. You already have `best_score` from Step 7. Confirm it updates correctly.

2. Display it in the header alongside score and speed:
   ```c
   snprintf(buf, sizeof(buf), "SCORE: %d  SPEED: %d  BEST: %d",
            score, 9 - speed, best_score);
   ```

3. **Extension:** Save the best score to a file. In the restart logic:
   ```c
   FILE *f = fopen("best.txt", "w");
   if (f) { fprintf(f, "%d\n", best_score); fclose(f); }
   ```
   At program start, load it back:
   ```c
   FILE *f = fopen("best.txt", "r");
   if (f) { fscanf(f, "%d", &best_score); fclose(f); }
   ```
   Now best score persists even if you close the program entirely. This previews the file I/O extension from lesson 10.
