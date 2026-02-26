# 🎮 Tetris Course Plan: X11 & Raylib Editions

> **Port:** `OneLoneCoder_Tetris.cpp` (Windows/Console, C++) → Linux C (X11 + Raylib)
> **Style:** Handmade Hero philosophy — platform abstraction layer, manual memory, no unnecessary abstraction.
> **Audience:** DreamEcho100 — Full-stack web dev learning systems/C programming from first principles.

---

## 🗺️ Big Picture Architecture

The course is structured around a **platform abstraction layer** pattern (as Casey does in Handmade Hero):

```
┌─────────────────────────────────────────────────────────────┐
│                     Game Logic Layer                         │
│  tetrominos, field, rotation, collision, scoring, timing    │
│              (Pure C, NO platform dependencies)             │
└───────────────────────┬─────────────────────────────────────┘
                        │ calls
        ┌───────────────┴───────────────┐
        ▼                               ▼
┌───────────────┐               ┌───────────────┐
│  X11 Backend  │               │ Raylib Backend│
│  - Window     │               │  - Window     │
│  - Input      │               │  - Input      │
│  - Rendering  │               │  - Rendering  │
└───────────────┘               └───────────────┘
```

Both backends implement the **same** platform API contract.  
The game loop and all game logic compile identically for both.

---

## 📁 Planned File Structure

```
OneLoneCoder_Tetris/
├── OneLoneCoder_Tetris.cpp     ← Original reference (do not touch)
├── PLAN.md                     ← This file
├── src/
│   ├── tetris.h                ← Game types, constants, shared structs
│   ├── tetris.c                ← Platform-independent game logic
│   ├── platform.h              ← Platform API contract (the interface)
│   ├── main_x11.c              ← X11 backend entry point
│   └── main_raylib.c           ← Raylib backend entry point
├── build_x11.sh                ← Build script for X11 version
└── build_raylib.sh             ← Build script for Raylib version
```

---

## 📚 Course Lessons

---

### Lesson 1 — Project Setup & C vs C++ Differences

**Goal:** Get a compilable skeleton running on Linux. Understand why we rewrite in C.

**What the original does:**

- Uses `#include <Windows.h>` — console screen buffer API
- Uses `wstring`, `vector`, STL containers — not available in pure C
- Uses `using namespace std` — C doesn't have namespaces

**Key Topics:**

1. Setting up `build_x11.sh` and `build_raylib.sh` using `gcc`
2. C vs C++: no STL, no `string`, no `vector`, manual arrays
3. Replacing `wstring tetromino[7]` with `const char* tetromino[7]`
4. Replacing `vector<int> vLines` with a fixed-size `int vLines[4]` + `int nLines`
5. Replacing `new / delete` with `malloc() / free()`
6. Understanding: _why_ manual memory is better for games

**Web Dev Analogy:**

> STL containers are like `Array` in JS with GC — convenient but you don't control allocation.  
> In C, you own the memory like `new ArrayBuffer()` in JS — explicit, predictable, fast.

**Mental Model — Data Layout:**

```
Original C++:             Our C version:
wstring tetromino[7]  →  const char* tetromino[7]  (pointer to literal string)
vector<int> vLines    →  int vLines[4]; int nLines  (stack-allocated, size bounded by 4 rows/piece)
wchar_t* screen       →  no screen buffer needed (X11/Raylib draw directly)
```

**Exercise:**
Write a small C program that prints all 7 tetromino shapes using a `const char*` array
and a nested loop (no printf format strings — use `putchar()`).

---

### Lesson 2 — Tetromino Representation & The Rotation Algorithm

**Goal:** Understand and implement `Rotate(px, py, r)` in C. Build deep intuition for the math.

**What the original does:**

```cpp
int Rotate(int px, int py, int r) {
    switch (r % 4) {
    case 0: pi = py * 4 + px;          // 0°
    case 1: pi = 12 + py - (px * 4);   // 90°
    case 2: pi = 15 - (py * 4) - px;   // 180°
    case 3: pi = 3 - py + (px * 4);    // 270°
    }
}
```

**Key Topics:**

1. Tetromino stored as a 4×4 grid in a 16-character string (row-major layout)
2. Understanding 2D-to-1D index mapping: `index = row * width + col`
3. Deriving each rotation formula from a coordinate transform
4. Verifying with a hand-drawn grid

**ASCII Visual — Index Layout:**

```
4x4 Grid flat indices:
 0  1  2  3
 4  5  6  7
 8  9 10 11
12 13 14 15

I-piece (vertical): "..X...X...X...X."
Indices with 'X':  2, 6, 10, 14

After 90° rotation, maps to: 8, 9, 10, 11 (horizontal)
```

**Web Dev Analogy:**

> A 2D array in C is like a flat `Float32Array` for a canvas — you compute `[y * width + x]` yourself. No magic.

**Exercise:**
Draw all 4 rotations of the S-piece (`".X...XX...X....."`) on graph paper, then verify each
index against the `Rotate()` formula.

---

### Lesson 3 — Collision Detection: `DoesPieceFit()`

**Goal:** Implement and fully understand collision detection logic. Learn bounds checking.

**What the original does:**

- Iterates the 4×4 bounding box of the piece
- For each cell: look up in tetromino string, look up in field array
- Only checks cells that are inside field bounds (handles the I-piece edge case)
- Returns `false` on first solid cell that overlaps a non-zero field cell

**Key Topics:**

1. The field as a 1D array: `pField[y * nFieldWidth + x]`
2. Why `>0` means "occupied" (field values: 0=empty, 1-7=piece color, 8=completed line, 9=wall)
3. Why we skip out-of-bounds cells instead of returning false
4. The "long piece hanging off the edge" problem explained

**ASCII Visual — Field Encoding:**

```
Field cell values:
  0  = empty space
  1-7 = locked piece (color = piece type + 1)
  8  = completed line (flashes before clearing)
  9  = wall/boundary

Display: " ABCDEFG=#"
           0123456789
```

**Field boundary visualization:**

```
nFieldWidth=12, nFieldHeight=18:
9 . . . . . . . . . . 9   ← row 0
9 . . . . . . . . . . 9
...
9 . . . . . . . . . . 9   ← row 16
9 9 9 9 9 9 9 9 9 9 9 9   ← row 17 (floor)
```

**Exercise:**
Step through `DoesPieceFit()` manually for the T-piece at position (5, 0) rotation 0.
Which cells are checked? Which are skipped (out of bounds)?

---

### Lesson 4 — Platform Abstraction Layer Design

**Goal:** Design `platform.h` — the contract between game logic and platform backends.

**What the original does (implicitly):**

- Rendering: `WriteConsoleOutputCharacter()` — dumps char buffer to terminal
- Input: `GetAsyncKeyState()` — polls key state
- Timing: `this_thread::sleep_for(50ms)` — fixed tick sleep

**Our Platform API Contract (`platform.h`):**

```c
// Input: which keys are pressed this tick
typedef struct {
    int move_left;
    int move_right;
    int move_down;
    int rotate;
    int quit;
} PlatformInput;

// Render: platform receives game state and draws it
typedef struct {
    unsigned char *field;       // [nFieldWidth * nFieldHeight]
    int field_width;
    int field_height;
    int current_piece;
    int current_rotation;
    int current_x;
    int current_y;
    int score;
    int game_over;
} GameState;

// Functions implemented by each backend:
void platform_init(int width, int height);
void platform_get_input(PlatformInput *input);
void platform_render(const GameState *state);
void platform_sleep_ms(int ms);
int  platform_should_quit(void);
void platform_shutdown(void);
```

**Key Topics:**

1. What a "platform layer" is and why it matters (Handmade Hero philosophy)
2. Dependency inversion: game logic depends on the API, not the OS
3. Resource lifetimes: window/display = Wave 1 (whole-program lifetime)
4. Why we pass `GameState*` not individual variables (data-oriented)

**Web Dev Analogy:**

> This is like writing a `Renderer` interface in TypeScript with two implementations:  
> `X11Renderer` and `RaylibRenderer`. The game code only calls `render(state)`, not the OS directly.

**Exercise:**
Write `platform.h` from scratch (no peeking) based only on what the game loop needs.
Then compare with the version above.

---

### Lesson 5 — X11 Backend: Window Creation & Rendering

**Goal:** Open an X11 window, draw colored rectangles for each cell.

**Key Topics:**

1. X11 connection: `XOpenDisplay()` — like opening a WebSocket to the compositor
2. Window creation: `XCreateSimpleWindow()`
3. Selecting events: `XSelectInput()` — `KeyPressMask | ExposureMask`
4. Graphics context: `XCreateGC()` — like `canvas.getContext('2d')`
5. Drawing cells: `XFillRectangle()` with a color per piece type
6. `XFlush()` — flush the command buffer (like flushing a network write buffer)

**X11 Setup Flow:**

```
display = XOpenDisplay(NULL)
    ↓
screen = DefaultScreen(display)
    ↓
window = XCreateSimpleWindow(display, root, x, y, w, h, ...)
    ↓
XSelectInput(display, window, KeyPressMask | ExposureMask)
    ↓
XMapWindow(display, window)   ← make it visible
    ↓
XFlush(display)
```

**Color Table (piece → X11 color name):**

```
Piece 0 (I) → "cyan"
Piece 1 (S) → "green"
Piece 2 (Z) → "red"
Piece 3 (T) → "magenta"
Piece 4 (J) → "blue"
Piece 5 (L) → "orange"
Piece 6 (O) → "yellow"
Wall (9)    → "gray"
```

**Common Mistakes:**

- ❌ Forgetting `XSelectInput()` — events silently won't arrive
- ❌ Drawing without a GC — segfault
- ❌ Never calling `XFlush()` — window stays blank

**Exercise:**
Draw just the field boundary (the walls — value 9) as gray rectangles.
Verify it looks like the playfield outline before adding any game logic.

---

### Lesson 6 — Raylib Backend: Window Creation & Rendering

**Goal:** Open a Raylib window and draw the same game state.

**Key Topics:**

1. `InitWindow(width, height, title)` + `SetTargetFPS(60)`
2. `BeginDrawing()` / `EndDrawing()` — the render frame bracket
3. `DrawRectangle()` with `Color` struct (R,G,B,A)
4. `WindowShouldClose()` — the quit signal
5. Comparison: Raylib is ~5 lines vs X11's ~30 lines for the same result

**Raylib Render Loop:**

```c
while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);
    // draw field cells...
    // draw current piece...
    // draw score text...
    EndDrawing();
}
```

**X11 vs Raylib Side-by-Side:**

```
X11:                          Raylib:
XOpenDisplay()                InitWindow()
XCreateSimpleWindow()         (handled by InitWindow)
XCreateGC()                   (built-in)
XSetForeground()              DrawRectangle(..., color)
XFillRectangle()              DrawRectangle()
XFlush()                      EndDrawing()
XCloseDisplay()               CloseWindow()
```

**Key Insight:**

> Raylib hides the OS complexity. X11 exposes it.  
> Learning X11 first means you understand _what_ Raylib is abstracting away.

**Exercise:**
Render the field boundary and a hardcoded I-piece at position (5, 0) in both backends.
Confirm both look visually identical before wiring up the game logic.

---

### Lesson 7 — Input Handling: X11 Events vs Raylib Polling

**Goal:** Implement `platform_get_input()` for both backends.

**What the original does:**

```cpp
bKey[k] = (0x8000 & GetAsyncKeyState((unsigned char)("\x27\x25\x28Z"[k]))) != 0;
// Keys: Right=0x27, Left=0x25, Down=0x28, Z=rotate
```

This is **polling** — it asks "is this key pressed right now?" — not event-driven.

**X11 Input — Event-Driven:**

```c
// Check the X11 event queue each tick
while (XPending(display)) {
    XEvent event;
    XNextEvent(display, &event);
    if (event.type == KeyPress) {
        KeySym key = XLookupKeysym(&event.xkey, 0);
        if (key == XK_Left)  input->move_left = 1;
        if (key == XK_Right) input->move_right = 1;
        if (key == XK_Down)  input->move_down = 1;
        if (key == XK_z)     input->rotate = 1;
        if (key == XK_Escape) input->quit = 1;
    }
    if (event.type == KeyRelease) { /* clear flags */ }
}
```

**Raylib Input — Polling:**

```c
input->move_left  = IsKeyDown(KEY_LEFT);
input->move_right = IsKeyDown(KEY_RIGHT);
input->move_down  = IsKeyDown(KEY_DOWN);
input->rotate     = IsKeyPressed(KEY_Z);   // pressed = single frame only
input->quit       = IsKeyPressed(KEY_ESCAPE);
```

**Key Insight — Rotate Latch:**
The original uses `bRotateHold` to prevent the piece from spinning continuously.  
In Raylib this is handled by `IsKeyPressed()` (fires once per press).  
In X11 we must track it ourselves with a boolean flag.

**Exercise:**
Log each key event to `stdout` in both backends. Confirm Left/Right/Down fire continuously
when held, and Z fires only once per press.

---

### Lesson 8 — Game Loop & Tick-Based Timing

**Goal:** Implement the platform-independent game loop with fixed-tick timing.

**What the original does:**

```cpp
this_thread::sleep_for(50ms);   // 1 tick = 50ms = ~20 ticks/second
nSpeedCount++;
bForceDown = (nSpeedCount == nSpeed);  // nSpeed starts at 20 (1 drop/second)
```

**Our C game loop structure:**

```c
// In tetris.c (platform-independent)
void tetris_tick(GameState *state, const PlatformInput *input) {
    // 1. Process input
    // 2. Update speed counter
    // 3. Handle force-down
    // 4. Check line completion
    // 5. Update score
}

// In main_x11.c / main_raylib.c
while (!platform_should_quit()) {
    platform_sleep_ms(50);          // 1 tick
    platform_get_input(&input);
    tetris_tick(&state, &input);
    platform_render(&state);
}
```

**Timing Concepts:**

```
nSpeed = 20  → piece drops every 20 ticks × 50ms = 1000ms = 1 second
nSpeed = 10  → piece drops every 10 ticks × 50ms =  500ms = 0.5 seconds

Every 50 pieces: nSpeed-- (if nSpeed >= 10)
Max speed: 10 ticks between drops (500ms)
```

**🔴 HOT PATH annotation:**

```c
// 🔴 HOT PATH: ~20 times/second
void tetris_tick(GameState *state, const PlatformInput *input) {
    // All game logic here. NO malloc(). NO file I/O.
}
```

**Exercise:**
Add a `printf` that prints the current tick count every second.
Verify it fires exactly 20 times per second.

---

### Lesson 9 — Piece Movement, Locking & Spawning

**Goal:** Implement the core piece lifecycle: spawn → move → lock → spawn.

**What the original does:**

```cpp
// Move right if key pressed AND fits
nCurrentX += (bKey[0] && DoesPieceFit(..., nCurrentX+1, nCurrentY)) ? 1 : 0;

// Lock piece: write piece cells into field array
for (int px = 0; px < 4; px++)
    for (int py = 0; py < 4; py++)
        if (tetromino[nCurrentPiece][Rotate(px,py,nRot)] != '.')
            pField[(nCurrentY+py)*nFieldWidth + (nCurrentX+px)] = nCurrentPiece + 1;

// Spawn new piece
nCurrentX = nFieldWidth / 2;
nCurrentY = 0;
nCurrentRotation = 0;
nCurrentPiece = rand() % 7;
```

**Key Topics:**

1. Why `nCurrentPiece + 1` (not just `nCurrentPiece`) as the field value — 0 means empty
2. Spawn position: center of field, top row
3. Game over detection: if new piece doesn't fit immediately → game over
4. `rand() % 7` — simple RNG, not great distribution but fine for this project

**Piece Lifecycle State Machine:**

```
SPAWN
  │
  ▼
FALLING ←──────────────────────────┐
  │  (input: left/right/down/rotate)│
  │  (auto: force down each nSpeed ticks)
  │
  ▼ (can't move down)
LOCKING
  │  (write cells into pField)
  │  (check for complete lines)
  ▼
SPAWN (new piece)
  │
  ▼ (new piece doesn't fit)
GAME OVER
```

**Exercise:**
Add a "ghost piece" that shows where the current piece will land (draw it with a different color).
This requires running `DoesPieceFit()` downward until it fails.

---

### Lesson 10 — Line Detection, Clearing & Gravity

**Goal:** Implement line completion: detect, flash, collapse rows, award points.

**What the original does:**

```cpp
// Detect complete lines in the piece's 4-row band
for (int py = 0; py < 4; py++) {
    bool bLine = true;
    for (int px = 1; px < nFieldWidth - 1; px++)
        bLine &= (pField[(nCurrentY+py)*nFieldWidth + px]) != 0;
    if (bLine) {
        // Mark line as '8' (flashes as '=')
        for (int px = 1; px < nFieldWidth-1; px++)
            pField[(nCurrentY+py)*nFieldWidth+px] = 8;
        vLines.push_back(nCurrentY + py);
    }
}

// Animate (display with '=' chars, sleep 400ms)
// Then collapse: shift all rows above down by 1
for (auto &v : vLines)
    for (int px = 1; px < nFieldWidth-1; px++)
        for (int py = v; py > 0; py--)
            pField[py*nFieldWidth+px] = pField[(py-1)*nFieldWidth+px];
```

**Key Topics:**

1. Why only check rows `nCurrentY` to `nCurrentY+3` — piece can only complete lines in its band
2. Why mark with `8` before collapsing — flash animation opportunity
3. Gravity: copy each row from above (shift down), clear top row
4. Order matters: process lines from **bottom to top** to avoid shifting already-shifted rows

**Line Collapse Visualization:**

```
Before (line 5 complete):        After collapse:
Row 3: . . A . . . . . . . .    Row 3: . . . . . . . . . . .  (new empty)
Row 4: B . A . C . . . . . .    Row 4: . . A . . . . . . . .  (was row 3)
Row 5: B B B B B B B B B B B ←  Row 5: B . A . C . . . . . .  (was row 4)
Row 6: . . . . D . . E . . .    Row 6: . . . . D . . E . . .  (unchanged)
```

**Exercise:**
Test with a manually pre-filled field that has 2 complete lines.
Verify both lines clear and all rows above shift down correctly.

---

### Lesson 11 — Scoring, Speed Progression & HUD

**Goal:** Implement the score formula and render HUD text in both backends.

**Scoring Formula:**

```c
score += 25;   // per piece locked
if (nLines > 0)
    score += (1 << nLines) * 100;
// 1 line:  200 pts
// 2 lines: 400 pts
// 3 lines: 800 pts
// 4 lines: 1600 pts (Tetris!)
```

**Speed Progression:**

```
Every 50 pieces: if (nSpeed >= 10) nSpeed--;
nSpeed range: 20 (slow) → 10 (fast)
Tick interval: always 50ms
Drop interval: nSpeed × 50ms (1000ms → 500ms)
```

**X11 Text Rendering:**

```c
char score_text[32];
snprintf(score_text, sizeof(score_text), "SCORE: %d", state->score);
XDrawString(display, window, gc, x, y, score_text, strlen(score_text));
```

**Raylib Text Rendering:**

```c
char score_text[32];
snprintf(score_text, sizeof(score_text), "SCORE: %d", state->score);
DrawText(score_text, x, y, font_size, WHITE);
```

**Exercise:**
Display score, piece count, and current speed level on-screen.
Add a "Level" indicator: `level = (20 - nSpeed) / 2` (0–5).

---

### Lesson 12 — Game Over, Restart & Final Integration

**Goal:** Handle game over screen, optional restart, and clean shutdown.

**What the original does:**

```cpp
bGameOver = !DoesPieceFit(nCurrentPiece, 0, nFieldWidth/2, 0);
// ...
CloseHandle(hConsole);
cout << "Game Over!! Score:" << nScore << endl;
system("pause");
```

**Our version:**

1. Show "GAME OVER" text overlay with final score
2. Wait for R (restart) or Q (quit) key
3. On restart: `memset(pField, 0, ...)`, reset all state vars, re-draw boundary
4. On quit: call `platform_shutdown()`

**Clean Shutdown Checklist:**

```
X11:                          Raylib:
XFreeGC(display, gc)          CloseWindow()
XDestroyWindow(display, win)
XCloseDisplay(display)
free(pField)
```

**Resource Lifetime Review (Wave 1 = whole program):**

```
Wave 1 (never freed early):
  - X11 display connection
  - X11 window
  - Raylib window
  - pField buffer

Wave 2 (per-game):
  - score, piece state, speed — just ints, reset on restart
```

**Final Integration Checklist:**

- [ ] Build `build_x11.sh` produces `tetris_x11` binary
- [ ] Build `build_raylib.sh` produces `tetris_raylib` binary
- [ ] Both versions: pieces fall, input works, lines clear, score updates
- [ ] Game over triggers correctly, restart works
- [ ] No memory leaks (test with `valgrind ./tetris_x11`)

**Exercise (Capstone):**
Add a "Next Piece" preview panel beside the field.
Requires: pre-generating next piece index, rendering a 4×4 preview box.

---

## 🔑 Key Concepts Covered Across All Lessons

| Concept                               | Lesson(s) |
| ------------------------------------- | --------- |
| C vs C++ (no STL, manual arrays)      | 1         |
| 2D-to-1D array indexing               | 2, 3      |
| Rotation math (coordinate transforms) | 2         |
| Collision detection + bounds checking | 3         |
| Platform abstraction layer design     | 4         |
| X11: display, window, GC, draw        | 5         |
| Raylib: init, draw loop, colors       | 6         |
| X11 events vs polling input           | 7         |
| Fixed-tick game loop, timing          | 8         |
| Piece state machine (spawn/fall/lock) | 9         |
| Line clearing + gravity               | 10        |
| Bit-shift scoring formula             | 11        |
| Clean shutdown, memory lifetimes      | 12        |

---

## 🔨 Build Scripts (Planned)

**`build_x11.sh`:**

```sh
#!/bin/bash
gcc -o tetris_x11 src/tetris.c src/main_x11.c -lX11 -Wall -Wextra -g
```

**`build_raylib.sh`:**

```sh
#!/bin/bash
gcc -o tetris_raylib src/tetris.c src/main_raylib.c -lraylib -lm -Wall -Wextra -g
```

---

## 📌 Notes

- The original C++ uses `wchar_t` / `wstring` for Unicode console rendering. Our C version uses `char*` — no need for wide characters when rendering with X11 rectangles or Raylib quads.
- The original's `screen` buffer (80×30 char array) is the console abstraction. In our version this is eliminated — we draw directly to the window each frame.
- `rand() % 7` produces slightly biased distribution (RAND_MAX not divisible by 7). Acceptable for a learning project; note it as a known limitation.
- Both backends should produce bit-identical game logic results — same RNG seed = same game.
