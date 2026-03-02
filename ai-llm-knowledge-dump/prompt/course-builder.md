# Course Builder Prompt

Use this file with the Copilot CLI agent to convert a source file or project into a build-along course.

**Reference Implementation:** `games/tetris/` — A complete example following these patterns.

---

## How to use

Attach this file + the source file(s) or directory you want to convert, then say:

> "Build a course from @path/to/source based on @ai-llm-knowledge-dump/PROMPT.md and output it to @path/to/course/"

---

## What to build

### Step 1 — Analyze & Plan

1. Read the source file(s) thoroughly.
2. Read `@ai-llm-knowledge-dump/llm.txt` to understand the student's background.
3. Identify the natural **build progression**: what is the smallest runnable first step? What do you add next? What are the logical milestones?
4. Write a `PLAN.md` next to the source file(s) covering:
   - What the program does overall
   - The proposed lesson sequence (one line per lesson: lesson number + what gets built + what the student sees)
   - The planned file structure for the course output
5. **Do not start writing lessons until PLAN.md exists.**

---

### Step 2 — Build the course files (Recommended file layout)

```
game-name/
├── build-dev.sh              # Build script with backend selection
├── src/
│   ├── utils/
│   │   ├── backbuffer.h      # Backbuffer type and color macros
│   │   ├── draw-shapes.c     # Rectangle, line primitives
│   │   ├── draw-shapes.h
│   │   ├── draw-text.c       # Bitmap font rendering
│   │   ├── draw-text.h
│   │   └── math.h            # MIN, MAX, CLAMP macros
│   ├── game.c                # All game logic
│   ├── game.h                # Game types, state, API
│   ├── platform.h            # Platform contract
│   ├── main_x11.c            # X11/OpenGL backend
│   └── main_raylib.c         # Raylib backend
└── build/                    # Build output (gitignored)
```

**Why `utils/`?** Drawing primitives and math helpers are reusable across games. Keep them separate from game-specific logic.

**Reference:** See `games/tetris/` for the complete structure.

---

## Build script

Use a build script with backend selection and run flags.

**Reference:** `games/tetris/build-dev.sh`

**Key features to include:**

- `--backend=x11|raylib` flag for selecting platform backend
- `-r` or `--run` flag to run after building
- `-d` or `--debug` flag for debug builds with sanitizers
- Separate `SOURCES` variable for common game files
- Backend-specific source and library additions via `case` statement

**Usage pattern:**

```bash
./build-dev.sh                      # Build with default backend (raylib)
./build-dev.sh --backend=x11        # Build with X11
./build-dev.sh --backend=raylib -r  # Build and run
./build-dev.sh -d -r                # Debug build and run
```

---

## Source file rules

The source code must be:

- Written in **C** (not C++), even if the original is C++
- Split into three layers:
  - **Game logic**: `game.c` — all update, render, and draw calls; zero platform API
  - **Shared header**: `game.h` — types, structs, enums, public function declarations
  - **Platform backends**: `main_x11.c`, `main_raylib.c` — one file per platform
- Connected via a **`platform.h` contract**
- Uses a **backbuffer pipeline**: game writes ARGB pixels into a `uint32_t *` array; platform uploads that array to the GPU each frame
- Buildable with `clang` using the provided build scripts
- Heavily commented — every non-obvious line gets a comment explaining the _why_

### Platform contract

**Reference:** `games/tetris/src/platform.h`

The minimal contract requires these functions:

```c
int    platform_init(PlatformGameProps *props);
double platform_get_time(void);
void   platform_get_input(GameInput *input, PlatformGameProps *props);
void   platform_shutdown(void);
```

For engine integration, wrap initialization state in `PlatformGameProps`:

- Window title, width, height
- Backbuffer struct
- `is_running` flag

**Look for in the reference:**

- `platform_game_props_init()` — allocates backbuffer, sets dimensions
- `platform_game_props_free()` — frees backbuffer
- How each backend implements the contract differently

**Principle:** Keep the contract minimal for simple games. Expand for engine integration. The game layer should never call platform-specific APIs directly.

---

## Architectural patterns

### Backbuffer architecture (mandatory)

**Reference:** `games/tetris/src/utils/backbuffer.h`

```
┌──────────────────────────────────────────┐
│  main_x11.c / main_raylib.c              │
│  platform_init → loop:                   │
│    platform_get_input                    │
│    game_update(state, input, dt)         │
│    game_render(backbuffer, state)        │
│    upload backbuffer to GPU              │
└──────────────────────────────────────────┘
                  ↓ Backbuffer *
┌──────────────────────────────────────────┐
│  game.c                                  │
│  game_render writes ARGB pixels          │
│  draw_rect / draw_rect_blend             │
│  draw_text / draw_char (bitmap font)     │
└──────────────────────────────────────────┘
```

**Look for in the reference:**

- `Backbuffer` struct with `pixels`, `width`, `height`, `pitch`
- `GAME_RGB()` and `GAME_RGBA()` macros for color packing
- How `game_render()` writes pixels, platform uploads them

**Why?** Game logic is 100% platform-independent. Adding a new backend only requires a new `main_*.c`.

### Color system (mandatory)

**Reference:** `games/tetris/src/utils/backbuffer.h` for macros, `games/tetris/src/game.h` for constants

**Pattern:**

```c
/* 0xAARRGGBB — alpha in high byte, blue in low byte */
#define GAME_RGBA(r, g, b, a) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define GAME_RGB(r, g, b) GAME_RGBA(r, g, b, 255)
```

Pre-define named color constants in `game.h`:

- `COLOR_BLACK`, `COLOR_WHITE`, `COLOR_RED`, etc.
- Game-specific colors (e.g., `COLOR_CYAN` for Tetris I-piece)

### Delta-time game loop (mandatory)

**Reference:** `games/tetris/src/main_x11.c` (lines ~280-310), `games/tetris/src/main_raylib.c` (main loop)

**Pattern:**

```c
double prev_time = platform_get_time();
while (running) {
    double curr_time = platform_get_time();
    float delta_time = (float)(curr_time - prev_time);
    prev_time = curr_time;
    if (delta_time > 0.1f) delta_time = 0.1f;  /* cap for debugger pauses */

    prepare_input_frame(&input);
    platform_get_input(&input, &props);
    game_update(&state, &input, delta_time);
    game_render(&backbuffer, &state);
    /* platform uploads backbuffer */
}
```

**Game-specific considerations:**

- Tetris: Uses `delta_time` for drop timers, auto-repeat
- Physics games: May need fixed timestep with accumulator
- Turn-based: `delta_time` still useful for animations

### VSync with manual fallback

**Reference:** `games/tetris/src/main_x11.c` — `setup_vsync()` function

**Look for:**

- VSync extension detection (`GLX_EXT_swap_control`, `GLX_MESA_swap_control`)
- Fallback flag: `g_x11.vsync_enabled`
- Manual frame limiting when VSync unavailable:
  ```c
  if (!g_x11.vsync_enabled) {
      double frame_time = get_time() - current_time;
      if (frame_time < g_target_frame_time) {
          sleep_ms((int)((g_target_frame_time - frame_time) * 1000.0));
      }
  }
  ```

**Raylib:** Handles this internally with `SetTargetFPS(60)`.

### Input system (mandatory)

**Reference:** `games/tetris/src/game.h` (input types), `games/tetris/src/game.c` (`prepare_input_frame`, `handle_action_with_repeat`)

**Core types:**

```c
typedef struct {
    int half_transition_count;  /* state changes this frame */
    int ended_down;             /* current state: 1=pressed, 0=released */
} GameButtonState;

typedef struct {
    float timer;      /* accumulates delta time while held */
    float interval;   /* time between auto-repeats */
} AutoRepeatInterval;

typedef struct {
    GameButtonState button;
    AutoRepeatInterval repeat;
} GameActionWithRepeat;
```

**Look for in the reference:**

- `UPDATE_BUTTON()` macro — call on key press/release events
- `prepare_input_frame()` — resets `half_transition_count`, keeps `ended_down` and timers
- `handle_action_with_repeat()` — returns whether action should trigger this frame

**Game-specific considerations:**

- Tetris: Auto-repeat for movement (left/right/down), single-shot for rotation
- Platformer: Auto-repeat not needed for jump, useful for run
- Shooter: Auto-repeat for firing, single-shot for reload

**Key insight:** Subtract interval instead of resetting timer for timing accuracy:

```c
action->repeat.timer -= action->repeat.interval;  /* preserves remainder */
```

### Typed enums (mandatory)

**Reference:** `games/tetris/src/game.h` — all `typedef enum` declarations

**Pattern:**

```c
typedef enum { DIR_NONE, DIR_LEFT, DIR_RIGHT } MOVE_DIR;
void piece_move(GameState *state, MOVE_DIR dir);  /* self-documenting */
```

**Look for in the reference:**

- `TETROMINO_BY_IDX` — which piece type
- `TETRIS_FIELD_CELL` — cell contents (empty, wall, piece types)
- `TETROMINO_R_DIR` — rotation states
- `TETROMINO_ROTATE_X_VALUE` — rotation input direction

**Why?** Prevents passing wrong integer values. Makes function signatures readable.

---

### Debug trap macro (mandatory for debug builds)

**Why:** When an assertion fails, you want the debugger to break at the exact line. A null pointer dereference (`*(int*)0 = 0`) works but is undefined behavior. Platform-specific traps are cleaner.

**Pattern:**

```c
/* In a shared header like game.h or utils/debug.h */
#ifdef DEBUG
  #if defined(_MSC_VER)
    #define DEBUG_TRAP() __debugbreak()
  #elif defined(__GNUC__) || defined(__clang__)
    #define DEBUG_TRAP() __builtin_trap()
  #else
    #define DEBUG_TRAP() (*(volatile int *)0 = 0)
  #endif

  #define ASSERT(expr) do { if (!(expr)) { DEBUG_TRAP(); } } while(0)
  #define ASSERT_MSG(expr, msg) do { if (!(expr)) { fprintf(stderr, "ASSERT: %s\n", msg); DEBUG_TRAP(); } } while(0)
#else
  #define DEBUG_TRAP()
  #define ASSERT(expr)
  #define ASSERT_MSG(expr, msg)
#endif
```

**Usage:**

```c
ASSERT(player_x >= 0);
ASSERT_MSG(buffer != NULL, "Buffer was not allocated");
```

**Add `-DDEBUG` to debug builds in your build script.**

## Coordinate and unit systems

### When to use which system

| Game Type                | Coordinate System      | Unit System                                 |
| ------------------------ | ---------------------- | ------------------------------------------- |
| Tetris, Pong, Breakout   | Y-down (matches grid)  | Tiles/cells directly                        |
| Platformer, physics game | Y-up (matches math)    | World units (meters)                        |
| RPG with scrolling       | Y-up + camera-relative | World units + hierarchical for large worlds |
| Infinite/procedural      | Y-up + hierarchical    | Chunk + offset                              |

### Y-up vs Y-down

**Current Tetris uses Y-down** because:

- Grid rows are naturally numbered top-to-bottom
- Pieces "fall down" = positive Y movement
- Screen coordinates match game coordinates

**For physics/platformer games, use Y-up:**

```c
/* In game logic: Y-up (0,0 is bottom-left) */
player.pos.y += velocity.y * dt;  /* positive = up */

/* In game_render(): convert to screen coords */
int screen_y = (backbuffer->height - 1) - (int)world_y;
```

**Rule:** Pick one convention and stick with it. Document your choice.

### Unit system

**Tetris uses tiles directly** — `CELL_SIZE` is pixels, positions are tile indices.

**For resolution-independent games:**

```c
#define PIXELS_PER_UNIT 32.0f  /* 1 world unit = 32 pixels */

/* Game logic uses world units */
float player_x = 5.5f;  /* 5.5 meters */

/* Convert at render time only */
int px_x = (int)(player_x * PIXELS_PER_UNIT);
```

**Rule:** All `GameState` fields use world units. All `draw_*` functions receive pixels. Conversion happens in `game_render()`.

### Hierarchical coordinates (for large worlds)

**When needed:** World larger than ~10,000 units from origin (float precision degrades).

**Pattern:**

```c
typedef struct {
    int32_t chunk_x, chunk_y;   /* coarse: which chunk */
    float offset_x, offset_y;   /* fine: position within chunk (always small) */
} WorldPosition;
```

**Reference:** See `ai-llm-knowledge-dump/world-coordinate-system.md` for full implementation.

**Games that need this:** Open-world RPGs, infinite runners, procedural worlds.
**Games that don't:** Tetris, Pong, single-screen platformers.

### Camera-relative rendering (for scrolling games)

**When needed:** World larger than screen.

**Pattern:**

```c
int screen_x = (int)((entity.x - camera.x) * PIXELS_PER_UNIT) + screen_width / 2;
int screen_y = screen_height / 2 - (int)((entity.y - camera.y) * PIXELS_PER_UNIT);
```

**Tetris doesn't use a camera** — the entire field fits on screen.

---

## Drawing utilities

### Math utilities

**Reference:** `games/tetris/src/utils/math.h`

**Look for:**

- `MIN(a, b)`, `MAX(a, b)` macros
- Use for clipping, bounds checking

**Consider adding for other games:**

- `CLAMP(x, lo, hi)`
- `ABS(x)`
- `WRAP(x, max)` — for wrapping positions

### Rectangle drawing

**Reference:** `games/tetris/src/utils/draw-shapes.c`

**Look for:**

- `draw_rect()` — solid rectangle with clipping
- `draw_rect_blend()` — alpha blending with fast paths

**Key optimization in `draw_rect_blend()`:**

```c
if (alpha == 255) { draw_rect(...); return; }  /* fully opaque */
if (alpha == 0) return;                         /* fully transparent */
/* Only blend when actually needed */
```

### Bitmap font rendering

**Reference:** `games/tetris/src/utils/draw-text.c`, `games/tetris/src/utils/draw-text.h`

**Look for:**

- `FONT_DIGITS[10][7]` — 5×7 pixel bitmaps for 0-9
- `FONT_LETTERS[26][7]` — 5×7 pixel bitmaps for A-Z
- `FONT_SPECIAL[]` — punctuation and symbols
- `draw_char()` — renders single character with scale
- `draw_text()` — renders string, advances cursor

**Pattern:** Each row is 5 bits packed into a byte. Bit 4 = leftmost pixel.

**Why self-contained?** No external font dependencies. Works on any platform.

---

## Scalable game patterns

Choose patterns based on your game's complexity. Start simple; add complexity only when needed.

### Fixed-size arrays with count (default)

**Reference:** `games/tetris/src/game.h` — `field[FIELD_WIDTH * FIELD_HEIGHT]`

```c
#define MAX_BULLETS 256
Bullet bullets[MAX_BULLETS];
int bullet_count;
```

**Use for:** Most games with < 100 entities. Tetris, Pong, Breakout.

### Entity pools with free list

For frequently spawned/despawned entities:

```c
typedef struct {
    Bullet bullets[MAX_BULLETS];
    int first_free;  /* index of first free slot, or -1 */
} BulletPool;
```

**Use for:** Shooters, particle systems.

### Spatial partitioning (grid-based)

For collision detection with many entities:

```c
#define GRID_CELL_SIZE 64
GridCell cells[GRID_WIDTH * GRID_HEIGHT];
```

**Use for:** Shmups, crowd simulations, physics with many bodies.

### Chunk-based tile storage

For large tile worlds:

```c
#define CHUNK_DIM 256
TileChunk *chunks;  /* sparse array or hash map */
```

**Use for:** RPGs, roguelikes, procedural worlds.

### Pattern selection guide

| Situation              | Pattern                   |
| ---------------------- | ------------------------- |
| < 100 entities         | Fixed array with count    |
| Frequent spawn/despawn | Free list pool            |
| Many collision checks  | Spatial grid              |
| Need undo/replay       | Double-buffered state     |
| > 1000 entities        | Parallel arrays (SoA)     |
| Large tile world       | Chunk-based storage       |
| Scrolling game         | Camera-relative rendering |
| World > 10,000 units   | Hierarchical coordinates  |

---

## State machine architecture

### When to use

Use when:

- Only **one mode of behavior** should run at a time
- Certain actions are invalid in certain modes
- Timers belong to specific phases
- Logic feels "scattered"

**Tetris phases:** Playing → Line Flash → Line Collapse → (Game Over or Playing)

**Reference:** `games/tetris/src/game.c` — look at how `game_over` and `completed_lines.flash_timer` create implicit phases. This should ideally be an explicit state machine.

### Recommended pattern

```c
typedef enum {
    PHASE_PLAYING,
    PHASE_LINE_FLASH,
    PHASE_LINE_COLLAPSE,
    PHASE_GAME_OVER,
    PHASE_COUNT
} GAME_PHASE;

typedef struct {
    GAME_PHASE phase;
    float phase_timer;
    /* ... other state ... */
} GameState;

void game_update(GameState *state, GameInput *input, float dt) {
    switch (state->phase) {
        case PHASE_PLAYING:
            update_playing(state, input, dt);
            break;
        case PHASE_LINE_FLASH:
            update_line_flash(state, dt);
            break;
        /* ... */
    }
}
```

### Transition rules

Never assign `state->phase` directly. Use a transition function:

```c
static void change_phase(GameState *state, GAME_PHASE next) {
#ifdef DEBUG
    validate_transition(state->phase, next);
#endif
    state->phase = next;
    state->phase_timer = 0.0f;
}
```

### Design principles

1. **States are mutually exclusive** — If two behaviors can run simultaneously, they need separate state machines
2. **A state owns its timers** — Reset on entry, update only inside that phase
3. **Don't encode state in flags** — Replace `bool flashing`, `bool locked` with explicit phases

---

## Lesson rules

Each lesson must follow this structure:

### 1. Title & Goal

```
# Lesson N — [What Gets Built]

**By the end of this lesson you will have:**
[Describe what the student sees running on screen]
```

### 2. Numbered build steps

- Each step = one small addition to the code
- Show the **exact code to write** — no "add something like..."
- Before each code block: one short paragraph explaining **why**
- After each major addition: remind the student to build and what they should see
- All new C concepts explained inline, with a JS analogy

### 3. Build & Run

```
## Build & Run
./build-dev.sh --backend=raylib -r
[What you should see]
```

### 4. Key Concepts

End with a bullet list of concepts introduced. Concrete and scannable.

### 5. Exercise

One small extension to try — proves understanding, not just copy-paste.

### 6. Pattern introduction lessons

When introducing a new pattern:

1. **Show the problem first** — demonstrate why the naive approach fails
2. **Introduce the pattern** — with a real-world analogy
3. **Implement incrementally** — smallest working version first
4. **Test edge cases** — boundary conditions, overflow
5. **Add debug visualization** — make the invisible visible

---

## Student profile

- Full-stack web dev: React, TypeScript, Node.js
- **C beginner** — treat every concept as new
- **Not strong at math** — worked examples with real numbers first
- Learns by doing — theory after experience

### Teaching patterns

| Situation           | How to handle                                                                                     |
| ------------------- | ------------------------------------------------------------------------------------------------- |
| New C concept       | Define it, show JS equivalent, then use it                                                        |
| Any formula/math    | Worked example with real numbers FIRST, then derive the formula                                   |
| Array indexing      | Always show: `index = row * width + col` with concrete example (row=2, col=3, width=4 → index=11) |
| Pointers            | "A pointer stores an address (like a street address), not a value"                                |
| Structs             | Compare to JS object: `struct { int x; int y; }` ≈ `{ x: number, y: number }`                     |
| `#include`          | Compare to `import` in JS                                                                         |
| `#define`           | Compare to `const` in JS (but simpler — just text substitution)                                   |
| `malloc`/`free`     | "Like `new` in JS, but no garbage collector — you must call `free()` yourself"                    |
| Stack memory        | "Like a local variable in JS — exists while the function runs, gone when it returns"              |
| World units         | "Like using `rem` in CSS — game logic uses meters/tiles, convert to pixels only when drawing"     |
| Y-up coordinates    | "Math textbooks use Y-up. We match that so formulas copy directly."                               |
| Hierarchical coords | "Like a street address: Country → City → Street → Building. Each level keeps numbers small."      |
| Camera-relative     | "Player stays centered. World scrolls around them. Like Google Maps."                             |

### Required patterns in every course

- **Backbuffer pipeline**: game renders to `uint32_t *`, platform uploads
- **Delta-time**: for all timing — no `sleep_ms`, no frame counters
- **`GameButtonState`**: for input — not booleans
- **Typed enums**: for every categorical value
- **Data-oriented design**: flat structs, no dynamic allocation in hot path
- **Separation**: `game_update()` modifies state, `game_render()` reads state and writes pixels

---

## Code style rules

### General

- No STL (`std::vector`, `std::string`, etc.) — use fixed-size C arrays
- No `new`/`delete` — use stack allocation where possible, `malloc`/`free` for backbuffer only
- `GameState` has zero dynamic allocation — sub-structs fine, heap pointers inside are not
- No heap allocation inside `game_update()` (the hot path)
- Use `clang` not `gcc`

### Platform/Game separation

- Platform functions never modify `GameState` — only `game_update()` does
- `game_render()` takes `Backbuffer *` and `GameState *` — writes pixels, reads state
- Platform layer should be thin — window creation, input polling, backbuffer upload

**Reference:** Compare `games/tetris/src/main_x11.c` (~400 lines) vs `games/tetris/src/game.c` (~600 lines). Platform is setup + loop; game is all logic.

### Naming conventions

| Type            | Convention             | Example                     |
| --------------- | ---------------------- | --------------------------- |
| Types/Structs   | `PascalCase`           | `GameState`, `Backbuffer`   |
| Enum types      | `SCREAMING_SNAKE_CASE` | `GAME_PHASE`, `MOVE_DIR`    |
| Enum values     | `SCREAMING_SNAKE_CASE` | `PHASE_PLAYING`, `DIR_LEFT` |
| Functions       | `snake_case`           | `game_update`, `draw_rect`  |
| Macros          | `SCREAMING_SNAKE_CASE` | `GAME_RGB`, `UPDATE_BUTTON` |
| Local variables | `snake_case`           | `delta_time`, `cell_x`      |
| Constants       | `SCREAMING_SNAKE_CASE` | `FIELD_WIDTH`, `COLOR_RED`  |

### Build commands

**Debug build:**

```bash
clang -Wall -Wextra -g -O0 -fsanitize=address,undefined -DDEBUG -o game src/*.c src/utils/*.c -lraylib -lm
```

**Release build:**

```bash
clang -Wall -Wextra -O2 -o game src/*.c src/utils/*.c -lraylib -lm
```

**Reference:** `games/tetris/build-dev.sh` for the full script with backend selection.

---

## Platform layer responsibilities

The platform provides **services**. The game contains **logic**.

### Platform layer handles:

- Window creation and management
- Input polling (keyboard, mouse, gamepad)
- Timing (monotonic clock)
- Backbuffer upload to GPU
- VSync / frame timing
- Audio output (if applicable)

### Game layer handles:

- All game state
- All update logic
- All rendering to backbuffer
- Coordinate systems and unit conversion
- Collision detection
- Entity management

### Consistency across courses

When building multiple courses:

1. Decide: should platform or game layer handle a new feature?
2. Be consistent across all courses
3. If a pattern should move layers, refactor previous courses
4. Update this document when new insights emerge

**Principle:** The game layer should compile (in theory) without any platform layer. The platform layer should swap out without changing game code.

---

## Reference implementation checklist

When creating a new game, verify against `games/tetris/`:

### File structure

- [ ] `build-dev.sh` with `--backend=` and `--run` flags
- [ ] `src/utils/backbuffer.h` — `Backbuffer` struct, color macros
- [ ] `src/utils/math.h` — `MIN`, `MAX` macros
- [ ] `src/utils/draw-shapes.c/h` — `draw_rect`, `draw_rect_blend`
- [ ] `src/utils/draw-text.c/h` — bitmap font, `draw_text`, `draw_char`
- [ ] `src/platform.h` — platform contract
- [ ] `src/game.h` — game types, enums, function declarations
- [ ] `src/game.c` — game logic, update, render
- [ ] `src/main_x11.c` — X11/OpenGL backend
- [ ] `src/main_raylib.c` — Raylib backend

### Patterns

- [ ] `Backbuffer` with `uint32_t *pixels`
- [ ] `GAME_RGB()` / `GAME_RGBA()` macros
- [ ] Named color constants
- [ ] `GameButtonState` with `half_transition_count` and `ended_down`
- [ ] `UPDATE_BUTTON()` macro
- [ ] `prepare_input_frame()` function
- [ ] `AutoRepeatInterval` for held-button actions (if needed)
- [ ] Delta-time passed to `game_update()`
- [ ] All game logic in `game_update()`, all drawing in `game_render()`
- [ ] Typed enums for all categorical values

### Platform backends

- [ ] X11: VSync detection with fallback to manual frame limiting
- [ ] X11: Monotonic clock via `clock_gettime(CLOCK_MONOTONIC)`
- [ ] X11: Key event handling with proper press/release detection
- [ ] Raylib: `SetTargetFPS(60)` or equivalent
- [ ] Raylib: `GetFrameTime()` for delta time

---

## Quick reference: When to use which pattern

| Your situation           | Use this pattern                                       |
| ------------------------ | ------------------------------------------------------ |
| Any game                 | Backbuffer, delta-time, `GameButtonState`, typed enums |
| Phase-based behavior     | State machine with explicit `GAME_PHASE` enum          |
| < 100 entities           | Fixed array with count                                 |
| Frequent spawn/despawn   | Free list pool                                         |
| Many collision checks    | Spatial grid                                           |
| Need undo/replay         | Double-buffered state                                  |
| > 1000 entities          | Parallel arrays (SoA)                                  |
| Large tile world         | Chunk-based storage                                    |
| Scrolling/large world    | Camera-relative rendering                              |
| World > 10,000 units     | Hierarchical coordinates                               |
| Single screen game       | Flat coordinates, no camera                            |
| Tile-based game (Tetris) | Y-down, tile units directly                            |
| Physics game             | Y-up, world units (meters)                             |

---

## Adding new patterns to this document

When you discover a new pattern while building a course:

1. **Implement it** in the game first
2. **Document it** here with:
   - When to use it
   - Reference to the implementation file
   - What to look for in the reference
   - Game-specific considerations
3. **Update the checklist** if it's a mandatory pattern
4. **Update the quick reference** table

**Goal:** This document + the Tetris reference = everything needed to build a new game course.
