# Lesson 01 — Open a Window

**By the end of this lesson you will have:**
Two working programs (`snake_x11` and `snake_raylib`) — each opens a black window titled "Snake" and stays open until you press Q or close it.

---

## 1. Create the Directory Structure

Run these exact commands from your project root:

```bash
mkdir -p src
touch src/main_x11.c
touch src/main_raylib.c
touch build_x11.sh
touch build_raylib.sh
chmod +x build_x11.sh build_raylib.sh
```

You should now have:
```
project/
├── build_x11.sh
├── build_raylib.sh
└── src/
    ├── main_x11.c
    └── main_raylib.c
```

---

## 2. Create `build_x11.sh`

Open `build_x11.sh` and write:

```bash
#!/bin/bash
set -e
gcc src/main_x11.c -o snake_x11 -lX11
echo "Build OK -> ./snake_x11"
```

**What each part means:**

| Part | What it does |
|------|-------------|
| `#!/bin/bash` | Tells the OS: run this with bash |
| `set -e` | Stop immediately if any command fails (like `strict mode` in Node) |
| `gcc src/main_x11.c` | Compile this C source file |
| `-o snake_x11` | Name the output binary `snake_x11` |
| `-lX11` | Link the X11 library (like `npm install` but for system libs) |

> **C vs JS:** In JavaScript, `node myfile.js` runs your file directly. In C, you first **compile** — translate source into a binary — then run the binary. `gcc` is the compiler.

---

## 3. Build `src/main_x11.c` — Step by Step

### Step 1 — A main() that does nothing

Open `src/main_x11.c` and write:

```c
int main(void) {
    return 0;
}
```

Run `./build_x11.sh` and then `./snake_x11`. Nothing happens — that's expected. It compiles and exits immediately. **This proves your compiler is working.**

> **C vs JS:** Every C program starts in `main()`, just like `index.js` is the entry point of a Node app. `return 0` means "exited successfully" — like `process.exit(0)`.

---

### Step 2 — Add the Includes

Add these three lines at the very top of the file, above `main()`:

```c
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
```

**Why each one:**

| Include | What it gives you |
|---------|-------------------|
| `<X11/Xlib.h>` | All X11 window functions: `XOpenDisplay`, `XCreateSimpleWindow`, etc. |
| `<X11/keysym.h>` | Named key constants like `XK_Escape`, `XK_q` |
| `<stdio.h>` | `fprintf`, `printf` — like `console.log` |

> **`#include` is like `import` in JS** — it pastes the contents of a header file at that spot so the compiler knows what functions exist.

The file now looks like:

```c
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>

int main(void) {
    return 0;
}
```

---

### Step 3 — Open the X11 Display

A "display" in X11 is the connection to the windowing system. Add this inside `main()`, replacing the lone `return 0`:

```c
int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Error: cannot open X display\n");
        return 1;
    }

    return 0;
}
```

**What's happening:**

- `Display *display` — declares a **pointer** variable named `display`.

> **What's a pointer?** In JS you never deal with memory addresses directly. In C, a pointer is a variable that stores the *address* of something in memory — not the value itself. Think of it like a WebSocket connection handle: `const ws = new WebSocket(url)`. You don't hold the whole WebSocket object — you hold a reference to it. `Display *` is that kind of handle.

- `XOpenDisplay(NULL)` — connects to the X server. `NULL` means "use the `$DISPLAY` environment variable" (usually `:0`, which is your screen).
- `if (!display)` — if the connection failed (returned `NULL` / zero), print an error and quit. `fprintf(stderr, ...)` writes to the error stream, like `console.error()`.

---

### Step 4 — Get the Screen Number

Add this line right after the `if (!display)` block:

```c
    int screen = DefaultScreen(display);
```

This returns which monitor to use. On most systems with a single monitor it returns `0`. You'll pass this number to other X11 functions.

---

### Step 5 — Create the Window

Now create the window. Add this after the `screen` line:

```c
    Window window = XCreateSimpleWindow(
        display,                          /* connection to X server         */
        RootWindow(display, screen),      /* parent window (the desktop)    */
        0, 0,                             /* x, y position on screen        */
        840, 322,                         /* width, height in pixels         */
        1,                                /* border width in pixels          */
        BlackPixel(display, screen),      /* border color                   */
        BlackPixel(display, screen)       /* background color               */
    );
```

**Parameter diagram — each argument labeled:**

```
XCreateSimpleWindow(
    display,                   ← the X server connection
    RootWindow(display,screen) ← parent: the full desktop
    0, 0,                      ← top-left corner position on screen
    840,                       ← window width  (we'll derive this from constants later)
    322,                       ← window height (we'll derive this from constants later)
    1,                         ← border: 1 pixel wide
    BlackPixel(...),           ← border color: black
    BlackPixel(...)            ← background fill: black
);
```

> `Window` (capital W) is a type defined by X11 — it's just an `unsigned long` ID number under the hood, like a DOM element's internal ID.

---

### Step 6 — Subscribe to Key Events

Add this line after creating the window:

```c
    XSelectInput(display, window, KeyPressMask);
```

**Why this matters:** X11 only sends you the events you ask for. If you skip this line, pressing keys does **nothing** — the events are swallowed. It's like building a server route in Express but forgetting to call `app.listen()` — the route exists but nobody can reach it.

`KeyPressMask` is a bitmask constant meaning "I want key-press events". You can combine masks with `|` to get multiple event types.

---

### Step 7 — Set the Window Title

```c
    XStoreName(display, window, "Snake");
```

This sets the title bar text. Without it the title is blank or shows the program name.

---

### Step 8 — Show the Window and Flush

```c
    XMapWindow(display, window);
    XFlush(display);
```

- `XMapWindow` — makes the window **visible**. Windows start hidden in X11. This is like calling `.style.display = 'block'` after creating a hidden DOM element.
- `XFlush` — sends all pending X11 commands to the server right now. X11 buffers commands internally for performance. `XFlush` is like `response.flush()` in a Node HTTP server — forces the buffer out immediately.

---

### Step 9 — The Event Loop

Now add the main event loop. This keeps the program alive and responds to key presses:

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

**What each piece does:**

| Code | Meaning |
|------|---------|
| `XEvent event` | A union struct that holds any kind of X11 event |
| `XNextEvent(display, &event)` | **Blocks** until the next event arrives, then writes it into `event`. Like `await ws.receive()`. |
| `event.type == KeyPress` | Check what kind of event it is |
| `XLookupKeysym(...)` | Convert the raw key code to a named symbol (`XK_q`, `XK_Escape`, etc.) |
| `break` | Exit the `while(1)` loop |

> **`&event`** — the `&` means "address of". We're passing the *address* of the `event` variable so `XNextEvent` can write into it. In JS you'd just return a value: `const event = await getEvent()`. In C, we pass *where to put* the result.

---

### Step 10 — Cleanup and Final Full File

Add cleanup after the loop:

```c
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
```

**Complete `src/main_x11.c`:**

```c
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>

int main(void) {
    /* Open connection to X server */
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Error: cannot open X display\n");
        return 1;
    }

    int screen = DefaultScreen(display);

    /* Create the window */
    Window window = XCreateSimpleWindow(
        display,
        RootWindow(display, screen),
        0, 0,
        840, 322,
        1,
        BlackPixel(display, screen),
        BlackPixel(display, screen)
    );

    /* Subscribe to key-press events */
    XSelectInput(display, window, KeyPressMask);

    /* Set title bar text */
    XStoreName(display, window, "Snake");

    /* Make window visible and flush commands */
    XMapWindow(display, window);
    XFlush(display);

    /* Event loop */
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

    /* Cleanup */
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
```

---

## 4. Create `build_raylib.sh` and `src/main_raylib.c`

### `build_raylib.sh`

```bash
#!/bin/bash
set -e
gcc src/main_raylib.c -o snake_raylib -lraylib -lm -lpthread -ldl
echo "Build OK -> ./snake_raylib"
```

The extra flags (`-lm`, `-lpthread`, `-ldl`) are libraries Raylib depends on internally.

### `src/main_raylib.c`

```c
#include "raylib.h"

int main(void) {
    InitWindow(840, 322, "Snake");
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

**`WindowShouldClose()`** returns true when you click the X button or press Escape — it's the built-in quit condition. No manual event loop needed.

### X11 vs Raylib — Side-by-Side Comparison

| Task | X11 | Raylib |
|------|-----|--------|
| Open window | `XOpenDisplay` + `XCreateSimpleWindow` + `XMapWindow` | `InitWindow(w, h, title)` |
| Set title | `XStoreName(display, window, "Snake")` | Third argument to `InitWindow` |
| Show window | `XMapWindow` + `XFlush` | Automatic |
| Main loop condition | `while(1)` + manual break | `while (!WindowShouldClose())` |
| Begin frame | *(no concept — draw any time)* | `BeginDrawing()` |
| Clear screen | Draw a filled black rectangle | `ClearBackground(BLACK)` |
| End / present frame | `XFlush(display)` | `EndDrawing()` |
| Cleanup | `XDestroyWindow` + `XCloseDisplay` | `CloseWindow()` |

**Bottom line:** Raylib wraps all the X11 boilerplate into ~10 lines. X11 is verbose but you see every moving part. In this course we build both so you understand what Raylib is hiding.

---

## Build & Run

**X11:**
```bash
./build_x11.sh
./snake_x11
```

**Raylib:**
```bash
./build_raylib.sh
./snake_raylib
```

**What to see:** A black window appears titled "Snake". It stays open. Press Q or Escape to close it (X11 version). Click the X button or press Escape for Raylib.

If `./build_x11.sh` fails with `X11/Xlib.h: No such file`, install it:
```bash
sudo apt install libx11-dev   # Ubuntu/Debian
```

If `./build_raylib.sh` fails, install Raylib:
```bash
sudo apt install libraylib-dev   # or build from source
```

---

## Mental Model

The X11 event loop is like a Node.js event loop — it blocks waiting for events, processes them one at a time, and loops forever. `XNextEvent` is `await nextEvent()`. The window is not an object you hold — it's just an integer ID the X server manages; you send it commands through your `display` connection handle.

---

## Exercise

**Change the background color from black to dark navy blue in both backends.**

**In X11:** Allocate a named color and use it as the background. Add this code before `XCreateSimpleWindow`, then pass `navy.pixel` as the last argument instead of `BlackPixel(display, screen)`:

```c
Colormap cmap = DefaultColormap(display, screen);
XColor navy, exact;
XAllocNamedColor(display, cmap, "navy", &navy, &exact);
```

Then in `XCreateSimpleWindow`, change the last argument:
```c
/* before: */ BlackPixel(display, screen)
/* after:  */ navy.pixel
```

**In Raylib:** Change `ClearBackground(BLACK)` to:
```c
ClearBackground((Color){0, 0, 128, 255});
```

The `Color` struct in Raylib is `{red, green, blue, alpha}` — each value 0–255. Dark navy is R=0, G=0, B=128, A=255 (fully opaque).

Run both and confirm the background is dark blue instead of black.
