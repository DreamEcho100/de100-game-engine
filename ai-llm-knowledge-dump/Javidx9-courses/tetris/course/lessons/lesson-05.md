# Lesson 05 — Splitting Into Files: tetris.h, tetris.c, platform.h

---

## By the end of this lesson you will have:

The project split into **four files**:
- `src/tetris.h` — game constants and declarations
- `src/tetris.c` — game logic (rotation, piece data)
- `src/main_x11.c` — X11 window + rendering (smaller now)
- `src/main_raylib.c` — Raylib window + rendering (smaller now)

Everything compiles and works exactly as it did after Lesson 04.
Pressing Z still rotates the piece. Left/right arrows still move it.

---

## The Problem

Right now every line of code lives in `main_x11.c` — or duplicated in
`main_raylib.c`. That was fine for a small program. But think about what's
coming:

- Gravity (the piece falls automatically)
- Collision detection (can it move there?)
- Locking pieces to the field
- Clearing completed lines
- Score tracking
- Game over detection

If all of that lives in `main()`, both files grow huge and stay in sync by hand.
Change the collision logic in `main_x11.c`? You have to remember to change it
in `main_raylib.c` too. That's how bugs are born.

**The fix:** Pull the game logic into its own file. X11 and Raylib both
*use* the game logic — neither file *owns* it.

---

## The Plan

We'll create these files:

```
src/
  tetris.h        ← constants + declarations (shared by everyone)
  tetris.c        ← game logic (no X11, no Raylib)
  platform.h      ← platform function declarations (the "contract")
  main_x11.c      ← X11 window + implements the platform contract
  main_raylib.c   ← Raylib window + implements the platform contract
```

Think of it like a React app:
- `tetris.h` is like a TypeScript `.d.ts` file — types and signatures
- `tetris.c` is the "model" — pure logic, no UI
- `main_x11.c` / `main_raylib.c` are the "views" — rendering only
- `platform.h` is the interface the views must implement

---

## Step 1 — Create `src/tetris.h`

Create a new file `src/tetris.h` with this content:

```c
#ifndef TETRIS_H
#define TETRIS_H

/* ── Constants ─────────────────────────────────────── */

#define FIELD_WIDTH  12
#define FIELD_HEIGHT 18
#define CELL_SIZE    32

/* ── Piece data ─────────────────────────────────────── */

/* 7 tetrominoes, each a 4×4 grid of '.' (empty) and 'X' (solid).
   Defined in tetris.c — 'extern' tells the compiler
   "trust me, it exists somewhere else." */
extern const char *TETROMINOES[7];

/* ── Function declarations ──────────────────────────── */

/* Returns the index into a 4×4 piece string for rotation r.
   px = column (0–3), py = row (0–3), r = rotation counter. */
int tetris_rotate(int px, int py, int r);

#endif /* TETRIS_H */
```

### What is `#ifndef / #define / #endif`?

This is called a **header guard**. Here's the problem it solves:

Suppose two different `.c` files both `#include "tetris.h"`. When the compiler
processes them, it sees the contents of `tetris.h` twice. If `tetris.h` defined
a variable, that variable would be defined twice — a compile error.

The guard works like this:

```
First time tetris.h is included:
  → TETRIS_H is not defined yet
  → compiler processes the file
  → defines TETRIS_H

Second time tetris.h is included:
  → TETRIS_H is already defined
  → compiler skips everything until #endif
  → no duplicate definitions
```

In JavaScript/TypeScript you never think about this because `import` handles
it automatically. In C, you do it yourself with these three lines.

### What is `extern`?

`extern const char *TETROMINOES[7]` means:

> "An array called TETROMINOES exists somewhere in this project.
>  It has 7 elements, each is a `const char *` (a string).
>  Don't define it here — just declare that it exists."

The actual array will live in `tetris.c`. The `extern` keyword is like a
forward declaration in TypeScript: `declare const TETROMINOES: string[]`.

### What is a function declaration?

```c
int tetris_rotate(int px, int py, int r);
```

This is a **declaration** (notice the semicolon at the end — no body).
It tells the compiler: "a function with this name and these argument types
exists. When you see a call to it, you know what types to expect."

The actual *implementation* (the body with the switch statement) lives in
`tetris.c`. This is exactly like declaring a function type in a `.d.ts` file
and implementing it in a `.ts` file.

---

## Step 2 — Create `src/tetris.c`

Create a new file `src/tetris.c`:

```c
#include "tetris.h"

/* ── Piece definitions ──────────────────────────────── */

const char *TETROMINOES[7] = {
    /* I */ "..X...X...X...X.",
    /* O */ "..XX..XX........",
    /* T */ "..X..XX...X.....",
    /* S */ "..X..X...X...X..",
    /* Z */ ".X...XX...X.....",
    /* J */ "..X...X..XX.....",
    /* L */ ".X...X...XX....."
};

/* ── Rotation formula ───────────────────────────────── */

int tetris_rotate(int px, int py, int r)
{
    switch (r % 4) {
        case 0: return py * 4 + px;
        case 1: return 12 + py - (px * 4);
        case 2: return 15 - (py * 4) - px;
        case 3: return 3 - py + (px * 4);
    }
    return 0;
}
```

This file has **no X11 code** and **no Raylib code**. It's pure game data
and pure math. You could compile this file on any platform.

---

## Step 3 — Create `src/platform.h`

Create a new file `src/platform.h`:

```c
#ifndef PLATFORM_H
#define PLATFORM_H

/* ── Input state ─────────────────────────────────────── */

/* All the inputs the game cares about.
   platform_get_input() fills this in each frame. */
typedef struct {
    int move_left;   /* 1 if left arrow pressed this frame */
    int move_right;  /* 1 if right arrow pressed this frame */
    int move_down;   /* 1 if down arrow pressed this frame */
    int rotate;      /* 1 if Z pressed this frame */
    int quit;        /* 1 if window closed or Escape pressed */
} PlatformInput;

/* ── Platform functions ──────────────────────────────── */
/* These are declared here and implemented in main_x11.c / main_raylib.c */

void platform_init(const char *title, int width, int height);
PlatformInput platform_get_input(void);
void platform_render(int piece, int rotation, int piece_col, int piece_row);
void platform_sleep_ms(int ms);
int  platform_should_quit(void);
void platform_shutdown(void);

#endif /* PLATFORM_H */
```

### What is a `struct`?

A `struct` is like an object in JavaScript. Compare:

```js
// JavaScript
const input = { move_left: 0, move_right: 0, rotate: 0, quit: 0 };
```

```c
// C
typedef struct {
    int move_left;
    int move_right;
    int rotate;
    int quit;
} PlatformInput;

PlatformInput input;
input.move_left = 0;
```

The `typedef` lets you write `PlatformInput input` instead of
`struct PlatformInput input`. It's a type alias, exactly like
`type PlatformInput = { ... }` in TypeScript.

### Why declare functions in a header?

`platform_init`, `platform_render`, etc. are declared in `platform.h` but
**not implemented here**. Each platform file (`main_x11.c`, `main_raylib.c`)
will implement them.

This is the "contract" pattern. The header says:
> "Every platform must provide these 6 functions with exactly these signatures."

The compiler checks that the implementations match the declarations. If you
add a function to `platform.h` but forget to implement it in `main_x11.c`,
the linker will tell you: `undefined reference to 'platform_render'`.

---

## Step 4 — Update `src/main_x11.c`

Make these changes to `main_x11.c`:

### 4a. Remove what's now in tetris.c

Delete the `TETROMINOES` array definition from `main_x11.c`.
Delete the `tetris_rotate()` function body from `main_x11.c`.

### 4b. Add the includes at the top

```c
#include "tetris.h"
#include "platform.h"
```

Put these after any system includes (`<X11/Xlib.h>` etc.).

### 4c. Rename your functions to the platform names

Your draw function should now be called `platform_render`:

```c
void platform_render(int piece, int rotation, int piece_col, int piece_row)
{
    /* your existing drawing code — unchanged */
}
```

Your input reading should happen inside `platform_get_input`:

```c
PlatformInput platform_get_input(void)
{
    PlatformInput input = {0};   /* zero out all fields */

    while (XPending(display)) {
        XEvent event;
        XNextEvent(display, &event);

        if (event.type == KeyPress) {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_Left)        input.move_left  = 1;
            if (key == XK_Right)       input.move_right = 1;
            if (key == XK_Down)        input.move_down  = 1;
            if (key == XK_z || key == XK_Z) input.rotate = 1;
            if (key == XK_Escape)      input.quit       = 1;
        }
        if (event.type == ClientMessage) {
            input.quit = 1;
        }
    }
    return input;
}
```

> **Note:** `PlatformInput input = {0};` is a C idiom that sets every field
> to zero. It's equivalent to `const input = { move_left: 0, ... }` in JS
> where you list every field. The `= {0}` shortcut works for any struct.

### 4d. Update `main()`

Your `main()` loop now reads inputs through `platform_get_input` and applies them:

```c
int main(void)
{
    platform_init("Tetris", FIELD_WIDTH * CELL_SIZE, FIELD_HEIGHT * CELL_SIZE);

    int current_piece = 0;
    int rotation      = 0;
    int piece_col     = 5;
    int piece_row     = 0;

    while (!platform_should_quit()) {
        PlatformInput input = platform_get_input();

        if (input.quit)        break;
        if (input.rotate)      rotation++;
        if (input.move_left)   piece_col--;
        if (input.move_right)  piece_col++;

        platform_render(current_piece, rotation, piece_col, piece_row);
        platform_sleep_ms(16);   /* ~60 fps */
    }

    platform_shutdown();
    return 0;
}
```

---

## Step 5 — Update `src/main_raylib.c`

Apply the same changes to `main_raylib.c`:

- Remove `TETROMINOES` and `tetris_rotate` (now in tetris.c)
- Add `#include "tetris.h"` and `#include "platform.h"`
- Rename your draw function to `platform_render`
- Wrap Raylib's `IsKeyPressed` calls inside `platform_get_input`
- Update `main()` to match the structure above

---

## Step 6 — Update the Build Scripts

The build scripts need to compile `tetris.c` alongside the platform file.

**`build_x11.sh`:**

```bash
#!/bin/bash
gcc -Wall -o tetris_x11 src/main_x11.c src/tetris.c -lX11
```

**`build_raylib.sh`:**

```bash
#!/bin/bash
gcc -Wall -o tetris_raylib src/main_raylib.c src/tetris.c -lraylib -lm
```

The key change: **add `src/tetris.c`** to the gcc command.

When you compile, gcc compiles each `.c` file into an object file, then links
them together. `main_x11.c` says "there's a function called `tetris_rotate`" —
the linker finds it in `tetris.c`'s object file and connects them.

---

## Step 7 — Build & Run

```bash
./build_x11.sh && ./tetris_x11
```

or

```bash
./build_raylib.sh && ./tetris_raylib
```

**What to verify:**

1. It compiles without errors.
2. The piece appears on screen.
3. Pressing Z still rotates the piece through all four orientations.
4. Left/right arrows still move the piece.

The behavior is **identical** to Lesson 04. We only reorganized the code.

**If you get a compile error like:**

```
undefined reference to `tetris_rotate'
```

You forgot to add `src/tetris.c` to the build script.

**If you get:**

```
error: conflicting types for 'tetris_rotate'
```

You left the old `tetris_rotate` definition in `main_x11.c`. Remove it.

---

## Mental Model

> **Splitting files is separating concerns** — the same principle as separating
> components, hooks, and types in a React/TypeScript project.
>
> - `tetris.c` is the **model** — it knows what the pieces look like and how
>   rotation math works. It knows nothing about screens or windows.
> - `main_x11.c` / `main_raylib.c` are the **view** — they know how to draw
>   on screen. They call into the model to get the data they need.
> - `tetris.h` is the **type declarations** — like a `.d.ts` file, it tells
>   everyone what exists and what the function signatures are.
> - `platform.h` is the **interface** — a contract that both platform files
>   must fulfill.
>
> When you add gravity in Lesson 07, you'll add it to `tetris.c` once.
> Both X11 and Raylib get it for free.

---

## Common Confusions

**"Why do I need `.h` files at all? Can't I just include `.c` files?"**

Technically you can `#include "tetris.c"` — but then the compiler processes
that entire file every time it's included, and if two files include it, you
get duplicate definitions. Header files with guards let multiple files share
*declarations* (no code) without creating duplicates.

**"When I add `#include "tetris.h"` to main_x11.c, does it include tetris.c too?"**

No. `#include "tetris.h"` only pulls in the *declarations* from `tetris.h`.
The *implementations* in `tetris.c` are compiled separately and linked in by gcc.
The build script controls which `.c` files are compiled — the `#include` directives
do not automatically pull in `.c` files.

---

## Exercise

Move `CELL_SIZE` into `tetris.h` (it's already there from Step 1 — verify
it was removed from wherever it previously lived in `main_x11.c` and
`main_raylib.c`).

Then confirm:
1. Both platform files compile without errors.
2. The `CELL_SIZE` macro is only defined once (in `tetris.h`).
3. The cell drawing in both files still uses the correct size.

In the next lesson we'll add a `GameState` struct — the data structure that
holds the entire state of the game — and build the field as an actual array.
