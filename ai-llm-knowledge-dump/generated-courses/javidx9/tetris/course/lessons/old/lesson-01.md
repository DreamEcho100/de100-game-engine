# Lesson 01 — Open a Window

## By the end of this lesson you will have:

Two working programs — `tetris_x11` and `tetris_raylib` — each opens a **black window** about 360×540 pixels, titled **"Tetris"**, and stays open until you press **Q** or close the window.

Picture this: a plain black rectangle on your screen. No drawing yet. Just a window you can move, resize, and close. That's the goal. Everything in the rest of the course builds on top of this.

---

## Step 0 — Create the directory structure

Open a terminal in the project root and run:

```bash
mkdir -p src
touch src/main_x11.c
touch src/main_raylib.c
touch build_x11.sh
touch build_raylib.sh
chmod +x build_x11.sh build_raylib.sh
```

What each folder/file is for:
- `src/` — all C source files live here
- `build_x11.sh` — the script you run to compile the X11 version
- `build_raylib.sh` — same, for the Raylib version

---

## Part A — The X11 version

X11 is the low-level windowing system built into Linux. When you open any window on Linux, X11 (or Wayland, which speaks X11 too) is what creates it. We're going to talk to it directly — no GUI framework, no wrapper. This is the real thing.

---

### Step 1 — Create `build_x11.sh`

Open `build_x11.sh` and write this:

```bash
#!/bin/bash
set -e
gcc src/main_x11.c -o tetris_x11 -lX11
echo "Build OK -> ./tetris_x11"
```

**What each part means:**

| Part | Meaning |
|------|---------|
| `#!/bin/bash` | "This file is a bash script" — always the first line |
| `set -e` | Stop immediately if any command fails (like `||` throw in JS) |
| `gcc` | The C compiler. It reads your `.c` file and produces a binary |
| `src/main_x11.c` | The source file to compile |
| `-o tetris_x11` | `-o` means "output". Name the compiled binary `tetris_x11` |
| `-lX11` | Link against the X11 library. `l` = "library". Like `import` but at compile time |

---

### Step 2 — Write an empty `main()` and compile it

Open `src/main_x11.c` and write:

```c
int main(void) {
    return 0;
}
```

**New C concept — `main()`:**  
Every C program starts at `main()`. It's like the entry point in Node — the first thing that runs. `int` means it returns an integer. `return 0` means "exited successfully" (0 = OK, anything else = error code). This is a Unix convention.

Now build it:

```bash
./build_x11.sh
```

You should see:
```
Build OK -> ./tetris_x11
```

Run it:
```bash
./tetris_x11
```

Nothing happens. No output. The program starts, returns 0, and exits. That's correct. We haven't told it to do anything yet.

---

### Step 3 — Add the X11 includes

Replace your file with:

```c
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>

int main(void) {
    return 0;
}
```

**New C concept — `#include`:**  
`#include` is like `import` in JavaScript. It pulls in declarations from another file so your code can use them. `<X11/Xlib.h>` gives you all the X11 functions (`XOpenDisplay`, `XCreateSimpleWindow`, etc.). `<stdlib.h>` gives `exit()`. `<stdio.h>` gives `printf()`.

Build again:
```bash
./build_x11.sh
```

Still builds, still does nothing. But now the X11 functions are available to call.

---

### Step 4 — Connect to the display

Replace `main()` with:

```c
int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    return 0;
}
```

**New C concept — pointers (`Display *`):**  
The `*` means "pointer". A pointer is a variable that holds a memory address — think of it like a reference or a handle in JavaScript. When you do `const db = connectToDatabase()` in Node and get back an object you pass around, that's conceptually the same thing. `Display *display` is a handle to the X server connection.

**What `XOpenDisplay(NULL)` does:**  
Connects to the X server. `NULL` means "use the `$DISPLAY` environment variable" (which is normally `:0` or `:1` on Linux — the screen you're looking at). If it fails (e.g., no display), it returns `NULL`.

**Why check for `NULL`?**  
In C, functions often return `NULL` to signal failure. There are no exceptions. You must check manually. This is a habit you will develop quickly.

Build and run. Still does nothing visible. The connection is opened and then immediately closed when `main` returns.

---

### Step 5 — Get the screen number

Add one line inside `main()`, after the display check:

```c
int screen = DefaultScreen(display);
```

**What is a screen?**  
X11 was designed for multi-monitor setups from the 1980s. A "screen" is one monitor. `DefaultScreen(display)` returns the index of the primary screen (usually `0`). You need this number to create windows and set colors later.

Your full `main()` now:

```c
int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    int screen = DefaultScreen(display);

    return 0;
}
```

---

### Step 6 — Create the window

Add these lines after `screen`:

```c
    Window window = XCreateSimpleWindow(
        display,                          /* connection to X server      */
        RootWindow(display, screen),      /* parent window (the desktop) */
        100, 100,                         /* x, y position on screen     */
        360, 540,                         /* width, height in pixels     */
        1,                                /* border thickness (pixels)   */
        BlackPixel(display, screen),      /* border color                */
        BlackPixel(display, screen)       /* background color            */
    );
```

**Labeled diagram — what each parameter does:**

```
XCreateSimpleWindow(
  display,               ← the connection handle from XOpenDisplay
  RootWindow(...),       ← "attach to the desktop" (the parent window)
  100,                   ← open 100px from the LEFT edge of the screen
  100,                   ← open 100px from the TOP edge of the screen
  360,                   ← make it 360px WIDE  (12 columns × 30px each)
  540,                   ← make it 540px TALL  (18 rows  × 30px each)
  1,                     ← draw a 1px border around it
  BlackPixel(...),       ← border is black
  BlackPixel(...)        ← background fill is black
);
```

**Why 360×540?** We'll have 12 columns and 18 rows, each 30 pixels. 12×30=360, 18×30=540. Everything fits perfectly.

`Window` is just a type alias for an integer ID — X11 uses integer IDs to identify windows internally.

---

### Step 7 — Subscribe to keyboard events

Add this line:

```c
    XSelectInput(display, window, KeyPressMask);
```

**What this does:**  
X11 only sends you events you explicitly ask for. If you skip this line, pressing a key does absolutely nothing — the event never arrives at your program. Think of it like subscribing to a webhook: you have to register first.

`KeyPressMask` is a flag that says "send me key press events". You can OR together multiple flags (e.g., `KeyPressMask | ExposureMask`) to subscribe to multiple event types.

---

### Step 8 — Set the window title

```c
    XStoreName(display, window, "Tetris");
```

This sets the text that appears in the title bar of the window. Simple.

---

### Step 9 — Make the window visible and flush

```c
    XMapWindow(display, window);
    XFlush(display);
```

**Why two calls?**
- `XMapWindow` tells X11 "show this window" — but X11 buffers commands internally
- `XFlush` forces all buffered commands to actually be sent to the X server right now

Think of X11's buffer like `console.log` buffering in Node. `XFlush` is the equivalent of flushing stdout.

Build and run now:
```bash
./build_x11.sh
./tetris_x11
```

You should see a black window flash open and immediately disappear. That's because `main()` hits `return 0` and exits. We need the event loop next.

---

### Step 10 — The event loop

This is the core of every X11 program. Replace the final `return 0;` with:

```c
    XEvent event;
    while (1) {
        XNextEvent(display, &event);

        if (event.type == KeyPress) {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_q || key == XK_Q || key == XK_Escape) {
                break;
            }
        }
    }
```

**New C concept — `while (1)`:**  
`while (1)` is an infinite loop — it runs forever until something `break`s out of it. In JavaScript you'd write `while (true)`. In C, `1` is true (any non-zero value is truthy in C).

**New C concept — `XEvent event` and `&event`:**  
`XEvent event` declares a variable named `event` of type `XEvent`. The `&` before `event` means "give me the address of this variable" — a pointer to it. `XNextEvent` needs a pointer so it can write the event data into your variable. Think of it like passing an object by reference.

**How the loop works:**
1. `XNextEvent` blocks (waits) until an event arrives, then fills `event` with it
2. We check `event.type` — is it a key press?
3. `XLookupKeysym` converts the raw key code into a named symbol like `XK_q`
4. If it's Q, q, or Escape, `break` exits the loop

---

### Step 11 — Cleanup

After the loop (before the final return), add:

```c
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
```

**Why clean up?**  
The OS will reclaim memory when the process exits anyway, but explicitly closing resources is good practice — especially connections (display) that might have server-side state.

---

### Complete `src/main_x11.c`

Here is the complete file as it should look now:

```c
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>

int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    int screen = DefaultScreen(display);

    Window window = XCreateSimpleWindow(
        display,
        RootWindow(display, screen),
        100, 100,
        360, 540,
        1,
        BlackPixel(display, screen),
        BlackPixel(display, screen)
    );

    XSelectInput(display, window, KeyPressMask);
    XStoreName(display, window, "Tetris");
    XMapWindow(display, window);
    XFlush(display);

    XEvent event;
    while (1) {
        XNextEvent(display, &event);

        if (event.type == KeyPress) {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_q || key == XK_Q || key == XK_Escape) {
                break;
            }
        }
    }

    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
```

### Build and Run

```bash
./build_x11.sh
./tetris_x11
```

**What you should see:**  
A black window, roughly 360×540 pixels, titled "Tetris" appears on screen. It stays open. You can drag it, minimize it. Press Q or Escape — the window closes and you're back at the terminal.

If it doesn't compile, make sure you have X11 dev headers:
```bash
sudo apt install libx11-dev
```

---

## Part B — The Raylib version

Raylib is a much friendlier library. It wraps all the event loop and window management into simple function calls. We'll use it as an alternative backend — same game, less boilerplate.

---

### Step 1 — Create `build_raylib.sh`

```bash
#!/bin/bash
set -e
gcc src/main_raylib.c -o tetris_raylib -lraylib -lm -lpthread -ldl
echo "Build OK -> ./tetris_raylib"
```

The extra flags (`-lm -lpthread -ldl`) are libraries Raylib depends on internally: math, threads, and dynamic loading.

Install Raylib if you haven't:
```bash
sudo apt install libraylib-dev
# or on some systems:
sudo apt install raylib
```

---

### Step 2 — Write `src/main_raylib.c` step by step

**Step 2a — includes and main skeleton:**

```c
#include "raylib.h"

int main(void) {
    return 0;
}
```

`raylib.h` gives you all the Raylib functions. Note: no angle brackets, just quotes — this is a local/system header depending on how Raylib is installed. On most Linux distros with `libraylib-dev` it's a system header, so `#include <raylib.h>` also works.

**Step 2b — Initialize the window:**

```c
#include "raylib.h"

int main(void) {
    InitWindow(360, 540, "Tetris");
    SetTargetFPS(60);

    return 0;
}
```

`InitWindow(width, height, title)` — creates the window. `SetTargetFPS(60)` — cap the loop to 60 frames per second. Raylib handles all the OS-level windowing internally.

**Step 2c — The game loop:**

```c
#include "raylib.h"

int main(void) {
    InitWindow(360, 540, "Tetris");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(BLACK);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

**How this maps to X11:**

| Raylib | X11 equivalent | What it does |
|--------|---------------|--------------|
| `InitWindow(360, 540, "Tetris")` | `XCreateSimpleWindow` + `XStoreName` + `XMapWindow` | Create and show the window |
| `WindowShouldClose()` | `event.type == KeyPress && key == XK_q` | Returns true when user closes or presses Escape |
| `BeginDrawing()` | *(implicit)* | Start a new frame |
| `ClearBackground(BLACK)` | `XFillRectangle` with black over entire window | Paint the background |
| `EndDrawing()` | `XFlush` | Send everything to the screen |
| `CloseWindow()` | `XDestroyWindow` + `XCloseDisplay` | Clean up |

Raylib handles keyboard, mouse, resize events, and timing internally. You never write `XNextEvent`.

---

### Build and Run

```bash
./build_raylib.sh
./tetris_raylib
```

**What you should see:** Same black window, titled "Tetris". Press Escape or click the X button to close.

---

## Mental Model

**X11:** You manually connect to the display server, create a window, subscribe to events, and run your own infinite loop — pulling events one at a time and responding to them. You control everything.

**Raylib:** The library runs the event loop internally. You just write what happens each frame between `BeginDrawing()` and `EndDrawing()`. It's like the difference between writing a raw `http.createServer` in Node vs using Express — same result, very different amount of glue code.

Both approaches give you the same window. We maintain both because X11 teaches you what's actually happening underneath, and Raylib teaches you the pattern used by most game frameworks.

---

## Exercise

Change the window background color from black to dark blue in both backends.

**X11 hint:** `BlackPixel(display, screen)` is the last argument to `XCreateSimpleWindow`. You can replace it with a custom color using `XAllocNamedColor`. For now, try just changing it to `WhitePixel(display, screen)` to confirm you understand which argument is the background — then we'll do proper colors in Lesson 02.

**Raylib hint:** `ClearBackground(BLACK)` — replace `BLACK` with `DARKBLUE`. Raylib defines color constants like `BLACK`, `WHITE`, `DARKBLUE`, `RED` etc. Try a few.
