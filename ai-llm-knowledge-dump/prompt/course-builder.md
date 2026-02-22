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
- Split into: shared game logic (`tetris.c` / equivalent) + platform backends (`main_x11.c`, `main_raylib.c`)
- Connected via a `platform.h` contract (6 functions: init, get_input, render, sleep_ms, should_quit, shutdown)
- Buildable with `gcc` using the provided build scripts
- Heavily commented — every non-obvious line gets a comment explaining the _why_

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
gcc ...
./program_name
[What you should see]
```

### 4. Mental Model

```
## Mental Model
[1-3 sentences. One key insight. One JS analogy if it helps.]
```

### 5. Exercise

```
## Exercise
[One small extension to try — something that proves understanding, not just copy-paste]
```

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

- redesign data layout
- flatten state
- remove heap usage
- isolate update vs render
- Follow Data-oriented Design principles as much as possible, and explain the benefits of doing so in simple terms. not an OOP way but a more procedural way, and explain the benefits of doing so in simple terms.

---

## Code style rules

- No STL (`std::vector`, `std::string`, etc.) — use fixed-size C arrays
- No `new`/`delete` — use stack allocation where possible, `malloc`/`free` when heap is needed
- `GameState` is a flat struct — all game data in one place, no dynamic allocation inside it
- Platform functions never modify `GameState` — only `tetris_tick()` does
- `platform_render()` takes `const GameState *state` — read-only
- No heap allocation inside `tetris_tick()` (the hot path)
- Build scripts use: `gcc -Wall -Wextra -g -o output src/file1.c src/file2.c -lX11` (or `-lraylib -lm`)
