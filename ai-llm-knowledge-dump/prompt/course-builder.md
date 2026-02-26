# Course Builder Prompt

Use this file with the Copilot CLI agent to convert a source file or project into a build-along course.

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

### Step 2 — Build the course files

Create the following inside the output directory:

```
course/
├── build_x11.sh          (if X11 backend applies)
├── build_raylib.sh       (if Raylib backend applies)
├── src/                  (complete, buildable source code)
│   ├── ...               (all headers and implementation files)
└── lessons/
    ├── lesson-01.md
    ├── lesson-02.md
    └── ...
```

---

## Source file rules

The source code in `course/src/` must be:

- Written in **C** (not C++), even if the original is C++
- Split into three layers:
  - **Game logic**: `game.c` / `tetris.c` — all update, render, and draw calls; zero platform API
  - **Shared header**: `game.h` / `tetris.h` — types, structs, enums, public function declarations
  - **Platform backends**: `main_x11.c`, `main_raylib.c` — one file per platform
- Connected via a **`platform.h` contract** with exactly 4 functions:
  ```c
  void   platform_init(const char *title, int width, int height);
  double platform_get_time(void);   /* monotonic clock, seconds */
  void   platform_get_input(GameInput *input);
  void   platform_display_backbuffer(const GameBackbuffer *backbuffer);
  ```
  No `sleep_ms`, no `should_quit`, no `platform_render`. Platforms only move pixels.
- Uses a **backbuffer pipeline**: game writes ARGB pixels into a `uint32_t *` array; platform uploads that array to the GPU each frame
- Buildable with `clang` using the provided build scripts
- Heavily commented — every non-obvious line gets a comment explaining the _why_

### Backbuffer architecture (mandatory pattern)

```
┌──────────────────────────────────────────┐
│  main_x11.c / main_raylib.c              │
│  platform_init → loop:                   │
│    platform_get_input                    │
│    game_update(state, input, dt)         │
│    game_render(state, backbuffer)        │
│    platform_display_backbuffer(bb)  ←── uploads pixels to GPU
└──────────────────────────────────────────┘
                  ↓ GameBackbuffer *
┌──────────────────────────────────────────┐
│  game.c                                  │
│  game_render writes ARGB pixels          │
│  draw_rect / draw_rect_blend             │
│  draw_text / draw_char (bitmap font)     │
└──────────────────────────────────────────┘
```

**Why?** Game logic is 100% platform-independent. Adding a new backend (SDL3, WebAssembly, Win32) only requires a new `main_*.c` — no changes to `game.c` or `game.h`.

### Color system

```c
/* 0xAARRGGBB — alpha in high byte, blue in low byte */
#define GAME_RGB(r, g, b)    GAME_RGBA(r, g, b, 0xFF)
#define GAME_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) << 8)  | (uint32_t)(b))
```

Pre-define named color constants (`COLOR_WHITE`, `COLOR_RED`, etc.) in `game.h`.

### Delta-time game loop (mandatory pattern)

```c
double prev_time = platform_get_time();
while (!quit) {
    double curr_time  = platform_get_time();
    float  delta_time = (float)(curr_time - prev_time);
    prev_time = curr_time;
    if (delta_time > 0.1f) delta_time = 0.1f;  /* cap for debugger pauses */

    prepare_input_frame(&input);
    platform_get_input(&input);
    game_update(&state, &input, delta_time);
    game_render(&state, &backbuffer);
    platform_display_backbuffer(&backbuffer);
}
```

VSync in `platform_display_backbuffer` drives frame timing — no `sleep_ms` needed.

### Input system (mandatory pattern)

Replace simple booleans with `GameButtonState` (origin: Casey Muratori's Handmade Hero):

```c
typedef struct {
    int half_transition_count; /* state changes this frame */
    int ended_down;            /* current state */
} GameButtonState;

#define UPDATE_BUTTON(button, is_down)          \
    do {                                         \
        if ((button).ended_down != (is_down)) {  \
            (button).half_transition_count++;    \
            (button).ended_down = (is_down);     \
        }                                        \
    } while (0)
```

Wrap repeating actions in `GameActionWithRepeat`:

```c
typedef struct { float timer; float interval; } GameActionRepeat;
typedef struct { GameButtonState button; GameActionRepeat repeat; } GameActionWithRepeat;
```

`prepare_input_frame` resets only `half_transition_count` — not `ended_down` or `repeat.timer`.

### Typed enums

Use `typedef enum { ... } ENUM_NAME;` for every categorical value in the game. Name with `SCREAMING_SNAKE_CASE`. This makes function signatures self-documenting and prevents passing wrong integer values:

```c
typedef enum { DIR_NONE, DIR_LEFT, DIR_RIGHT } MOVE_DIR;
void piece_move(GameState *state, MOVE_DIR dir);  /* not: void piece_move(..., int dir) */
```

---

## Lesson rules

Each lesson must follow this exact structure:

### 1. Title & Goal

```
# Lesson N — [What Gets Built]

**By the end of this lesson you will have:**
[Describe what the student sees running on screen — like describing a screenshot]
```

### 2. Numbered build steps

- Each step = one small addition to the code
- Show the **exact code to write** — no "add something like..."
- Before each code block: one short paragraph explaining **why** we're doing this
- After each major addition: remind the student to build and what they should see
- All new C concepts explained inline the first time they appear, with a JS analogy

### 3. Build & Run

```
## Build & Run
clang main_x11.c game.c -o game -lX11 -lxkbcommon -lGL -lGLX
./game
[What you should see]
```

### 4. Key Concepts

End each lesson with a bullet list (or table) of concepts introduced:

```
## Key Concepts
- `GameButtonState`: half_transition_count + ended_down — captures sub-frame events
- `UPDATE_BUTTON(btn, is_down)` — call on key press/release events
- ...
```

Do **not** use a "Mental Model" section. "Key Concepts" is more concrete and scannable.

### 5. Exercise

```
## Exercise
[One small extension to try — something that proves understanding, not just copy-paste]
```

### 6. State Machine Architecture (Mandatory for Phase-Based Logic)

Many game systems are **phase-driven**, not condition-driven.

If you find yourself writing logic like:

- `if (timer > 0)`
- `if (lines_to_clear > 0)`
- `if (piece_locked)`
- `if (collision_detected && drop_interval_elapsed)`

You are likely modeling **implicit state**.

Implicit state becomes scattered logic.

Scattered logic becomes confusion.

Instead, we formalize behavior using a **Finite State Machine (FSM)**.

---

#### What a State Machine Is (In Low-Level C)

In this course, a state machine is:

- A `typedef enum`
- A single `phase` field inside `GameState`
- A `switch` inside `game_update`
- A centralized transition function
- Debug-only transition validation

Nothing more.

No classes.  
No virtual functions.  
No function pointer tables.  
No Redux reducers.  
No polymorphism.

Just data + switch.

---

#### When to Use a State Machine

Use a state machine when:

- Only **one mode of behavior** should run at a time
- Certain actions are invalid in certain modes
- Timers belong to specific phases
- Logic feels “scattered”

Examples in games:

- Tetris: Falling → Locking → Flash → Collapse → Spawn
- Snake: Playing → GameOver
- Menu: Title → Playing → Paused

---

#### Required Pattern

##### 1. Define a typed enum

```c
typedef enum {
    PHASE_FALLING,
    PHASE_LOCKING,
    PHASE_LINE_FLASH,
    PHASE_LINE_COLLAPSE,
    PHASE_SPAWN,

    PHASE_COUNT
} GAME_PHASE;
```

Typed enums are mandatory — never use raw integers.

---

##### 2. Store the phase inside GameState

```c
typedef struct {
    GAME_PHASE phase;
    float phase_timer;  /* Used only when relevant to the current phase */
} GameState;
```

The state machine lives in data — not in code structure.

---

##### 3. Dispatch using a switch inside game_update

```c
void game_update(GameState *state, GameInput *input, float dt)
{
    switch (state->phase)
    {
        case PHASE_FALLING:
            update_falling(state, input, dt);
            break;

        case PHASE_LOCKING:
            update_locking(state, dt);
            break;

        case PHASE_LINE_FLASH:
            update_line_flash(state, dt);
            break;

        case PHASE_LINE_COLLAPSE:
            update_line_collapse(state);
            break;

        case PHASE_SPAWN:
            update_spawn(state);
            break;
    }
}
```

This has effectively zero runtime cost.

One enum load.
One branch.

Compared to rendering and GPU work, this cost is noise.

---

#### Transition Rules (Mandatory)

Never assign `state->phase` directly.

All transitions must go through:

```c
static void change_phase(GameState *state, GAME_PHASE next)
{
#ifdef DEBUG
    validate_transition(state->phase, next);
#endif

    state->phase = next;
    state->phase_timer = 0.0f;
}
```

---

#### Debug-Time Transition Validation

Illegal transitions must crash in debug builds.

```c
static void validate_transition(GAME_PHASE from, GAME_PHASE to)
{
    switch (from)
    {
        case PHASE_FALLING:
            Assert(to == PHASE_LOCKING);
            break;

        case PHASE_LOCKING:
            Assert(to == PHASE_LINE_FLASH || to == PHASE_SPAWN);
            break;

        case PHASE_LINE_FLASH:
            Assert(to == PHASE_LINE_COLLAPSE);
            break;

        case PHASE_LINE_COLLAPSE:
            Assert(to == PHASE_SPAWN);
            break;

        case PHASE_SPAWN:
            Assert(to == PHASE_FALLING);
            break;

        default:
            Assert(0);
    }
}
```

In release builds:

- `Assert` compiles out
- There is zero cost

---

#### Important Design Principles

##### 1. States Are Mutually Exclusive

If two behaviors can run at the same time, they do NOT belong in the same state machine.

Example:

- Movement state
- Animation state

Those should be separate state machines.

---

##### 2. A State Owns Its Timers

If a timer only matters in one phase, it must:

- Reset on entry
- Update only inside that phase

Never let a timer leak across unrelated phases.

---

##### 3. Do Not Encode State in Flags

Avoid:

- `bool piece_locked`
- `bool flashing`
- `int lines_to_clear`

If these control flow, they are hidden state.

Replace them with explicit phases.

---

#### Performance Notes

This pattern:

- Is branch-predictable
- Is cache-friendly
- Uses no heap allocation
- Has no virtual dispatch
- Adds no measurable overhead

The cognitive clarity gain far outweighs the negligible branch cost.

If a state machine is ever your bottleneck, your architecture is already incorrect elsewhere.

---

#### Teaching Requirement

When generating a course:

- If the source contains scattered condition-driven logic,
- Or phase-based behavior without explicit state,

You must refactor it into an explicit FSM using this pattern.

Explain:

1. Why implicit state causes confusion
2. Why explicit phases compress mental load
3. How debug-time validation prevents illegal transitions
4. Why this has zero runtime cost in release builds

---

#### Student Framing (Important)

The student comes from React/Redux.

Clarify the difference:

| Web UI State        | Game Simulation State      |
| ------------------- | -------------------------- |
| Event-driven        | Frame-driven               |
| Often async         | Deterministic              |
| Reducer-based       | Phase-based                |
| Many states coexist | One active phase at a time |

Avoid enterprise terminology.

Keep it procedural and mechanical.

---

#### Summary Rule

A state machine is not a design pattern.

It is a control-flow compression tool.

If logic feels scattered,
formalize the phases.

---

## Student profile (from llm.txt)

- Full-stack web dev: React, TypeScript, Node.js — this is their mental model
- **C beginner** — never written C before, treat every concept as new
- **Not strong at math** — all formulas derived visually with examples first
- Learns by doing — theory after experience, not before
- Prefers: short sentences, concrete examples with real numbers, ASCII diagrams, JS analogies

### Required teaching patterns

| Situation        | How to handle                                                                                             |
| ---------------- | --------------------------------------------------------------------------------------------------------- |
| New C concept    | Define it, then show a JS equivalent, then use it                                                         |
| Any formula/math | Show a worked example with real numbers FIRST, derive the pattern second                                  |
| Array indexing   | Always show: "index = row \* width + col" with a concrete example (e.g. row=2, col=3, width=4 → index=11) |
| Pointers         | "A pointer is like a variable that stores an address (like a street address), not a value"                |
| Structs          | Compare to a JS object: `struct { int x; int y; }` ≈ `{ x: number, y: number }`                           |
| #include         | Compare to `import` in JS                                                                                 |
| #define          | Compare to `const` in JS (but simpler — just text substitution)                                           |
| malloc/free      | "Like `new` in JS, but no garbage collector — you must call `free()` yourself"                            |
| Memory on stack  | "Like a local variable in JS — exists while the function runs, gone when it returns"                      |

Also, make sure if possible to:

- Use a **backbuffer pipeline**: game renders to a `uint32_t *` pixel array; platform uploads it (see "Backbuffer architecture" above)
- Use **delta-time** for all timing — no `sleep_ms`, no frame counters
- Use **`GameButtonState`** for input — not booleans
- Use **typed enums** (`typedef enum`) for every categorical value
- Follow **Data-oriented Design** principles: flat structs, no dynamic allocation in the hot path, update and render separated. Explain the benefits in simple terms — not as OOP alternatives, but as "here's why a flat array beats a linked list."
- Isolate **update** (`game_update`) from **render** (`game_render`) — render is read-only on state

---

## Code style rules

- No STL (`std::vector`, `std::string`, etc.) — use fixed-size C arrays
- No `new`/`delete` — use stack allocation where possible, `malloc`/`free` for the backbuffer pixel array only
- `GameState` is a struct with zero dynamic allocation — sub-structs are fine, heap pointers inside are not
- Platform functions never modify `GameState` — only `game_update()` does
- `game_render()` takes `GameState *state` and `GameBackbuffer *backbuffer` — writes pixels, reads state
- No heap allocation inside `game_update()` (the hot path)
- Build scripts use: `clang -Wall -Wextra -O2 -o output src/main_x11.c src/game.c -lX11 -lxkbcommon -lGL -lGLX`
- Debug builds add: `-g -O0 -fsanitize=address,undefined`
- Use `clang` not `gcc`

## Important

The platform layer shouldn't have the game logic in general, as much as possible on any of the game implementationa and perimitives, while providing it with setup, servicing-functions _(if needed)_, and systems of the engine, like input, rendering, and timing. The game layer should be able to be compiled and run without any platform layer at all, and the platform layer should be able to be swapped out without changing any of the game code. The platform layer should be as thin as possible, ideally just a few hundred lines of code that handles window creation, input polling, and backbuffer uploading.

This mean that every thing/game we build the course could have new things that we will consider either that the platform layer should handle, or the game layer, and we will try to be consistent with that across all the courses, and if we find something that we think should be handled by the platform layer, we will refactor the previous courses to move that responsibility to the platform layer, and if we find something that should be handled by the game layer, we will refactor the previous courses to move that responsibility to the game layer.

And if we found a new idea/thought/perspective that we think could be useful for the courses, we will add it to the "course-builder.md" prompt, and we will also keep track/updating of these ideas in the "course-builder.md" document _when needed_.
