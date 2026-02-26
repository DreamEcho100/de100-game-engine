# Tetris Course Upgrade Plan

## Overview

This document describes the plan to upgrade the Tetris course lessons at
`ai-llm-knowledge-dump/Javidx9-courses/tetris/course/lessons/` to incorporate
the architectural improvements introduced in commit `c499c31ddf7dee442ffa0fcf481cc88ea592deec`
(implemented at `games/tetris/src/`).

The upgrade rewrites the lessons to teach a **more professional, portable, and
extensible architecture** while preserving the original pedagogical intent of
the course.

---

## Source Materials for the Upgrade

There are **three** input sources for this upgrade:

### 1. Original Course (12 lessons)

The baseline course as originally authored:

```
ai-llm-knowledge-dump/Javidx9-courses/tetris/course/lessons/
├── lesson-01.md  through  lesson-12.md
```

### 2. User-Authored v2 Lessons (2 lessons)

These two lessons were **NOT part of the original course** — they were written
by the user as intermediate upgrades between earlier commits and the final
refactor. They are high-quality source material that the new lesson series
must fully incorporate:

| File | Topic | Covers |
|------|-------|--------|
| `lesson-8-v2.md` | Delta Time Game Loop | Replacing tick-based speed with `drop_timer += delta_time`; `platform_get_time()`; non-blocking X11 input; `platform_sleep_ms()`; `tetris_update(state, input, delta_time)` |
| `lesson-9.5-v2.md` | Professional Input System | `GameButtonState` (half_transition_count, ended_down); `GameActionRepeat` (timer, interval); `GameActionWithRepeat`; `UPDATE_BUTTON` macro; independent auto-repeat per action; DAS/ARR tuning |

### 3. Final Implementation (commit c499c31)

The latest state at `games/tetris/src/`, which builds on the v2 lessons and
adds the full rendering refactor. The canonical source of truth.

---

## What Changed — Complete Feature Inventory

### CHANGED: Rendering Model — Platform-Independent Backbuffer

**Before (course):** Each platform backend (`main_x11.c`, `main_raylib.c`)
contained all rendering code using platform-specific drawing APIs.
`platform_render(const GameState *)` was the rendering contract.

**After (user's):** `tetris.c` owns rendering. It writes into a `TetrisBackbuffer`
(a plain `uint32_t *` pixel array). Platform backends just upload that array.
`platform_render` is no longer the active rendering path — the main loop
calls `tetris_render(&backbuffer, &state)` directly, then hands the backbuffer
to a platform-local display function.

```c
/* NEW: Platform-independent rendering target in tetris.h */
typedef struct {
  uint32_t *pixels;  /* RGBA pixel data (0xAARRGGBB) */
  int width, height;
  int pitch;         /* bytes per row (usually width * 4) */
} TetrisBackbuffer;
```

**Impact on lessons:** The concept of "platform renders the game" flips to
"game renders itself; platform displays the result."

---

### ADDED: Color System in tetris.h

**Before (course):** Colors were platform-specific types.
- X11: `unsigned long pixel` allocated per-color with `XAllocNamedColor`
- Raylib: `Color` struct `{R, G, B, A}` defined per backend

**After (user's):** Colors are `uint32_t` values (RGBA, `0xAARRGGBB`) defined
once in `tetris.h`, available to any backend:

```c
#define TETRIS_RGBA(r, g, b, a) \
  (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define TETRIS_RGB(r, g, b)  TETRIS_RGBA(r, g, b, 255)

#define COLOR_BLACK     TETRIS_RGB(0,   0,   0)
#define COLOR_WHITE     TETRIS_RGB(255, 255, 255)
#define COLOR_GRAY      TETRIS_RGB(128, 128, 128)
#define COLOR_DARK_GRAY TETRIS_RGB(64,  64,  64)
#define COLOR_CYAN      TETRIS_RGB(0,   255, 255)
#define COLOR_BLUE      TETRIS_RGB(0,   0,   255)
#define COLOR_ORANGE    TETRIS_RGB(255, 165, 0)
#define COLOR_YELLOW    TETRIS_RGB(255, 255, 0)
#define COLOR_GREEN     TETRIS_RGB(0,   255, 0)
#define COLOR_MAGENTA   TETRIS_RGB(255, 0,   255)
#define COLOR_RED       TETRIS_RGB(255, 0,   0)
```

---

### ADDED: Software Drawing Primitives in tetris.c

**Before (course):** `XFillRectangle`, `XDrawString`, `DrawRectangle`,
`DrawText` — all platform APIs, duplicated or adapted per backend.

**After (user's):** Drawing functions live entirely in `tetris.c`:

| Function | Signature | Purpose |
|----------|-----------|---------|
| `draw_rect` | `(TetrisBackbuffer *, x, y, w, h, color)` | Fill a rectangle with bounds clipping |
| `draw_rect_blend` | `(TetrisBackbuffer *, x, y, w, h, color)` | Alpha-blended rectangle |
| `draw_char` _(static)_ | `(TetrisBackbuffer *, x, y, char, color, scale)` | Draw one 5×7 bitmap glyph |
| `draw_cell` _(static)_ | `(TetrisBackbuffer *, col, row, color)` | Draw one grid cell (pixel coords from grid coords) |
| `draw_piece` _(static)_ | `(TetrisBackbuffer *, piece, col, row, color, rotation)` | Draw a full tetromino |
| `draw_text` | `(TetrisBackbuffer *, x, y, text, color, scale)` | Draw a string using bitmap font |

All functions are declared in `tetris.h` (public ones) or `static` in `tetris.c`
(internal helpers).

---

### ADDED: Full 5×7 Bitmap Font in tetris.c

**Before (course):** Font rendering delegated to X11 default font (`XDrawString`)
or Raylib's built-in font (`DrawText`). No font data in the game layer.

**After (user's):** Complete font dataset defined as static byte arrays in
`tetris.c`. Each character is 7 bytes; each byte encodes 5 pixels (bits 4–0):

```c
static const unsigned char FONT_DIGITS[10][7]   = { /* 0-9 */ };
static const unsigned char FONT_LETTERS[26][7]  = { /* A-Z (case-insensitive) */ };

typedef struct { char character; unsigned char bitmap[7]; } FontSpecialChar;
static const FontSpecialChar FONT_SPECIAL[] = {
  /* . , : ; ! ? - + = / \ ( ) [ ] < > _ * # @ % & ' " */
  /* Custom arrow mappings: ^ = ↑, v = ↓, { = ←, } = → */
  {0, {0}} /* terminator */
};
static const unsigned char *find_special_char(char c);
```

Bit extraction pattern: `bitmap[row] & (0x10 >> col)` — bit 4 is leftmost pixel.

---

### ADDED: New HUD Rendering in Game Layer (tetris_render)

**Before (course):** HUD (score, level, next piece, controls) drawn per backend
in `platform_render()`. Code duplicated or diverged between X11 and Raylib.

**After (user's):** `tetris_render(TetrisBackbuffer *, GameState *)` in `tetris.c`
draws the complete frame for all platforms:

1. Clear backbuffer to `COLOR_BLACK`
2. Draw the field (switch on `TETRIS_FIELD_CELL` enum value)
3. Draw the current falling piece (`draw_piece`)
4. Draw HUD sidebar:
   - "SCORE" + score value (in `COLOR_YELLOW`)
   - "LEVEL" + level value (in `COLOR_CYAN`)
   - "PIECES" + pieces_count
   - "NEXT" label + mini piece preview at `CELL_SIZE / 2`
   - Controls hint block (uses custom arrow char mappings `{`, `}`, `v`, `^`)
5. Game over overlay (only when `state->game_over`):
   - Semi-transparent dark rectangle via `draw_rect_blend`
   - Red border (4 thin rectangles)
   - "GAME OVER" in `COLOR_RED`
   - "R RESTART" and "Q QUIT" in `COLOR_WHITE`

---

### ADDED: Enhanced Game Over Overlay with Alpha Blending

**Before (course):** Game over state shown via text in the sidebar; no overlay.

**After (user's):** `draw_rect_blend` implements the compositing equation:

```c
out_r = (src_r * alpha + dst_r * (255 - alpha)) / 255;
/* repeated for G and B channels */
```

Applied with `TETRIS_RGBA(0, 0, 0, 200)` — a semi-transparent black box over
the field — creating a dim-and-overlay effect without any OS blending API.

---

### CHANGED: platform_get_input API — GameState Dependency Removed

**Before (course):**
```c
void platform_get_input(PlatformInput *input);
```

**After (user's):**
```c
void platform_get_input(GameInput *input);
```

The parameter type changed from `PlatformInput` (simple booleans) to `GameInput`
(full `GameButtonState` + `GameActionWithRepeat` system). The function no longer
takes a `GameState *` parameter — input handling is fully decoupled from game state.

---

### CHANGED: platform.h API Contract Simplified

**Before (course):** Six public functions:

```c
void platform_init(void);
void platform_get_input(PlatformInput *input);
void platform_render(const GameState *state);
void platform_sleep_ms(int ms);
int  platform_should_quit(void);
void platform_shutdown(void);
```

**After (user's):** Contract streamlined — `platform_sleep_ms` and
`platform_should_quit` are **removed** from the public interface (commented out;
their logic is now internal to each backend's `main()` loop). `platform_get_time`
is **added**. `platform_init` gains a signature:

```c
int  platform_init(const char *title, int width, int height);
void platform_get_input(GameInput *input);
void platform_render(GameState *state);   /* declared but superseded by tetris_render */
// void platform_sleep_ms(int ms);        /* removed from contract */
// int  platform_should_quit(void);       /* removed from contract */
double platform_get_time(void);           /* new */
void platform_shutdown(void);
```

Note: `platform_render` is still declared but is no longer called by the main
loop — the main loop now calls `tetris_render` + a backend-local
`platform_display_backbuffer` (X11) or inline texture upload (Raylib).

---

### CHANGED: Raylib Entrypoint — Backbuffer Texture Pipeline

**Before (course):** `main()` in `main_raylib.c` called `platform_render(state)`
which internally used `DrawRectangle`, `DrawText`, and Raylib's color types.

**After (user's):** `main()` manages input directly (no platform_get_input in
older sense) and runs a backbuffer pipeline:

```c
/* Allocate backbuffer once */
uint32_t *pixels = malloc(width * height * sizeof(uint32_t));
Image img = { .data = pixels, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, ... };
Texture2D texture = LoadTextureFromImage(img);

while (!WindowShouldClose()) {
  float delta_time = GetFrameTime();
  prepare_input_frame(&input);
  /* ... fill input ... */
  tetris_update(&state, &input, delta_time);
  tetris_render(&backbuffer, &state);   /* game draws itself */
  UpdateTexture(texture, backbuffer.pixels);  /* upload to GPU */
  BeginDrawing();
  DrawTexture(texture, 0, 0, WHITE);
  EndDrawing();
}
UnloadTexture(texture);
free(backbuffer.pixels);
```

**Removed from Raylib backend:** `PIECE_COLORS[]` array, `DrawRectangle`,
`DrawText`, `DrawRectangleRec`, all per-piece color logic.

---

### CHANGED: X11 Entrypoint — OpenGL/GLX Backbuffer Presentation

**Before (course):** `main_x11.c` used `XFillRectangle`, `XSetForeground`,
`XAllocNamedColor`, and `XDrawString` to render each cell directly via Xlib.
The `X11_State` struct held `GC gc`, `XEvent event`, and per-color
`unsigned long` fields.

**After (user's):** `main_x11.c` uses OpenGL via GLX to display the backbuffer
as a texture. The rendering path is:

```
game writes uint32_t pixels → glTexImage2D uploads to GPU
→ full-screen quad drawn with GL_TEXTURE_2D → glXSwapBuffers
```

Key new elements in `X11_State`:

```c
typedef struct {
  Display    *display;
  Window      window;
  GLXContext  gl_context;
  GLuint      texture_id;
  int         screen, width, height;
  bool        vsync_enabled;
} X11_State;
```

**Removed from X11 backend:**
- `GC gc` (Graphics Context)
- `XEvent event` (now local to input loop)
- Per-color `unsigned long` fields (`alloc_colors` struct)
- `XFillRectangle`, `XSetForeground`, `XDrawString` calls
- `XAllocNamedColor` / `XAllocColor` calls

---

### ADDED: X11 VSync Support via GLX Extensions

**Before (course):** Frame timing via `nanosleep` only — no VSync.

**After (user's):** VSync detection and setup at init time using two extension
paths with fallback:

```c
/* Attempt 1: GLX_EXT_swap_control */
PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = glXGetProcAddressARB(...);
if (glXSwapIntervalEXT) { glXSwapIntervalEXT(dpy, drawable, 1); }

/* Attempt 2: GLX_MESA_swap_control */
PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA = glXGetProcAddressARB(...);
if (glXSwapIntervalMESA) { glXSwapIntervalMESA(1); }
```

When VSync is active, `glXSwapBuffers` blocks until the next refresh — no
extra sleep needed. When VSync is unavailable, the original `nanosleep`-based
frame limiter is used as fallback. VSync state is logged to the terminal on startup.

---

### CHANGED: X11 Window Resize Handling

**After (user's):** `ConfigureNotify` events update the OpenGL viewport and
projection matrix when the window is resized:

```c
case ConfigureNotify:
  g_x11.width  = event.xconfigure.width;
  g_x11.height = event.xconfigure.height;
  glViewport(0, 0, g_x11.width, g_x11.height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, g_x11.width, g_x11.height, 0, -1, 1);
  break;
```

This keeps the backbuffer displayed correctly at any window size.

---

### CHANGED: Timing in X11 Main Loop

**Before (course):** `platform_get_time()` called from `main()`.

**After (user's):** Timing consolidated into static helper `get_time()` in
`main_x11.c`. On first call it records `g_start_time`; thereafter returns
`now - g_start_time`. The frame-limiter sleep is conditional — skipped when
VSync handles pacing.

Global variable naming improved: `g_is_game_running` → `static g_is_running`;
`g_fps` / `g_target_frame_time` → `static const double g_target_frame_time`.

---

### CHANGED: Build Script — New Linker Flags

**Before (course):** `build_x11.sh` links only `-lX11`.

**After (user's):** Links OpenGL and XKB libraries:

```bash
clang src/main_x11.c src/tetris.c -o build/tetris_x11 \
  -lX11 -lxkbcommon -lGL -lGLX
```

| Flag | Reason |
|------|--------|
| `-lGL` | OpenGL functions (`glGenTextures`, `glTexImage2D`, etc.) |
| `-lGLX` | GLX context creation and `glXSwapBuffers` |
| `-lxkbcommon` | `XKBlib.h` for correct keyboard symbol lookup |

Also: compiler changed from `gcc` to `clang`; `build/` output directory created
via `mkdir -p build`.

---

### CHANGED: GameState — Typed Enums and CurrentPiece Substruct

**Before (course):** Flat struct with plain `int` for piece index and rotation:

```c
int current_piece;      /* 0-6 */
int current_rotation;   /* 0-3 */
int current_x, current_y;
int next_piece;
int speed, speed_count;
int piece_count;
```

**After (user's):** Grouped into `CurrentPiece` substruct with typed enums:

```c
typedef enum { TETROMINO_I_IDX, TETROMINO_J_IDX, TETROMINO_L_IDX,
               TETROMINO_O_IDX, TETROMINO_S_IDX, TETROMINO_T_IDX,
               TETROMINO_Z_IDX } TETROMINO_BY_IDX;

typedef enum { TETROMINO_R_0, TETROMINO_R_90,
               TETROMINO_R_180, TETROMINO_R_270 } TETROMINO_R_DIR;

typedef struct {
  int x, y;
  TETROMINO_BY_IDX index;
  TETROMINO_BY_IDX next_index;   /* next piece preview */
  TETROMINO_R_DIR  rotation;
} CurrentPiece;
```

`speed`/`speed_count` replaced by `GameActionRepeat tetromino_drop` (delta-time
based). `level` added as an explicit stored field. Completed lines stored in a
substruct:

```c
struct {
  int indexes[TETROMINO_LAYER_COUNT];
  int count;
  GameActionRepeat flash_timer;
} completed_lines;
```

---

### CHANGED: Field Cell Encoding — Typed Enum

**Before (course):** Plain int constants and magic numbers for field values.

**After (user's):**

```c
typedef enum {
  TETRIS_FIELD_EMPTY,
  TETRIS_FIELD_I, TETRIS_FIELD_J, TETRIS_FIELD_L,
  TETRIS_FIELD_O, TETRIS_FIELD_S, TETRIS_FIELD_T, TETRIS_FIELD_Z,
  TETRIS_FIELD_WALL,
  TETRIS_FIELD_TMP_FLASH,  /* marks completed lines for flash animation */
} TETRIS_FIELD_CELL;
```

`TETRIS_FIELD_EMPTY = 0` ensures `memset` zero-fill correctly initializes the
field. `TETRIS_FIELD_I … TETRIS_FIELD_Z` = piece_index + 1 (so 0 = empty).

---

### CHANGED: Tetromino Data — Named Constants and New Ordering

**Before (course):** Anonymous inline strings, order: I/S/Z/T/J/L/O.

**After (user's):** Named individual `const char` variables plus explicit array.
Order changed to I/J/L/O/S/T/Z (matching `TETROMINO_BY_IDX` enum order):

```c
const char TETROMINO_I[TETROMINO_SIZE] = "..X...X...X...X.";
const char TETROMINO_J[TETROMINO_SIZE] = "..X...X..XX.....";
/* ... */
const char *TETROMINOES[TETROMINOS_COUNT] = {
  TETROMINO_I, TETROMINO_J, TETROMINO_L,
  TETROMINO_O, TETROMINO_S, TETROMINO_T, TETROMINO_Z,
};
```

New constants: `TETROMINOS_COUNT 7` and `SIDEBAR_WIDTH 200` now in `tetris.h`
(previously defined per-backend or missing).

---

### CHANGED: Function Names and Signatures

| Old (course) | New (user's) | Note |
|-------------|-------------|------|
| `tetris_init(state)` | `game_init(state, input)` | Also takes `GameInput *` to set repeat intervals |
| `tetris_tick(state, input)` | `tetris_update(state, input, delta_time)` | Delta time added |
| `tetris_rotate(px, py, r)` | `tetromino_pos_value(px, py, r)` | Renamed for clarity |
| `tetris_does_piece_fit(...)` | `tetromino_does_piece_fit(...)` | Renamed |
| _(new)_ | `prepare_input_frame(GameInput *)` | Resets transition counts each frame |
| _(new)_ | `handle_action_with_repeat(...)` _(static)_ | Auto-repeat logic in tetris.c |
| _(new)_ | `tetris_apply_input(state, input, delta_time)` _(static)_ | Input → state logic |

---

### CHANGED: Input System — GameButtonState + GameActionWithRepeat

_(Covered in detail in `lesson-9.5-v2.md`. This is the foundation from the
user's v2 work now fully integrated into the final implementation.)_

**Before (course):** `PlatformInput` with simple boolean flags.

**After (user's):** Layered input system:

```c
typedef struct {
  int half_transition_count;  /* state changes this frame */
  int ended_down;             /* final state: 1=pressed, 0=released */
} GameButtonState;

typedef struct {
  float timer, interval;
} GameActionRepeat;

typedef struct {
  GameButtonState button;
  GameActionRepeat repeat;
} GameActionWithRepeat;

typedef struct {
  GameActionWithRepeat move_left, move_right, move_down;
  struct {
    GameButtonState button;
    TETROMINO_ROTATE_X_VALUE value;  /* GO_LEFT / GO_RIGHT / NONE */
  } rotate_x;
  int quit, restart;
} GameInput;

#define UPDATE_BUTTON(button, is_down) do {       \
  if ((button).ended_down != (is_down)) {         \
    (button).half_transition_count++;             \
    (button).ended_down = (is_down);              \
  }                                               \
} while (0)
```

---

## File Inventory

### Reference Files (Read-Only — Source of Truth)

```
games/tetris/src/
├── tetris.h        ← All new types, enums, macros, public API
├── tetris.c        ← Bitmap font, drawing primitives, tetris_render, game logic
├── platform.h      ← Updated API contract
├── main_x11.c      ← GLX/OpenGL backend + new input system
├── main_raylib.c   ← Backbuffer texture pipeline + new input system
└── build_x11.sh    ← New link flags: -lX11 -lxkbcommon -lGL -lGLX

ai-llm-knowledge-dump/Javidx9-courses/tetris/course/lessons/
├── lesson-8-v2.md  ← User-authored; source for Lesson 07 (delta time)
└── lesson-9.5-v2.md ← User-authored; source for Lesson 11 (pro input)
```

### Course Source Files — Will Be Updated

```
ai-llm-knowledge-dump/Javidx9-courses/tetris/course/src/
├── tetris.h        ← Full rewrite
├── tetris.c        ← Full rewrite (adds bitmap font + drawing + tetris_render)
├── platform.h      ← Updated signatures, new functions, removals
├── main_x11.c      ← Full rewrite (GLX + OpenGL + new input)
└── main_raylib.c   ← Full rewrite (backbuffer texture pipeline + new input)

ai-llm-knowledge-dump/Javidx9-courses/tetris/course/
├── build_x11.sh    ← Updated link flags
└── build_raylib.sh ← Minor updates if needed
```

### Lessons — Backup (All Current Files → lessons/old/)

All 14 existing lesson files will be moved to `lessons/old/` **unchanged**:

```
ai-llm-knowledge-dump/Javidx9-courses/tetris/course/lessons/old/
├── lesson-01.md          (original course — open a window)
├── lesson-02.md          (original course — the field)
├── lesson-03.md          (original course — tetrominoes)
├── lesson-04.md          (original course — rotation)
├── lesson-05.md          (original course — collision)
├── lesson-06.md          (original course — piece locking)
├── lesson-07.md          (original course — line detection)
├── lesson-08.md          (original course — game loop, tick-based)
├── lesson-09.md          (original course — score + difficulty)
├── lesson-10.md          (original course — HUD + next piece)
├── lesson-11.md          (original course — game over)
├── lesson-12.md          (original course — restart + polish)
├── lesson-8-v2.md        (USER-AUTHORED — delta time upgrade)
└── lesson-9.5-v2.md      (USER-AUTHORED — professional input upgrade)
```

### Lessons — New Files to Create

```
ai-llm-knowledge-dump/Javidx9-courses/tetris/course/lessons/
├── lesson-01.md   Window + GLX/OpenGL context (X11) and Raylib window
├── lesson-02.md   TetrisBackbuffer: platform-independent pixel canvas
├── lesson-03.md   Color system: uint32_t, TETRIS_RGB macros, predefined colors
├── lesson-04.md   Typed enums: TETROMINO_BY_IDX, TETROMINO_R_DIR, TETRIS_FIELD_CELL
├── lesson-05.md   GameState with CurrentPiece substruct; game_init()
├── lesson-06.md   Collision: tetromino_pos_value, tetromino_does_piece_fit
├── lesson-07.md   Delta-time game loop + GameActionRepeat (from lesson-8-v2.md)
├── lesson-08.md   Drawing primitives: draw_rect, draw_cell, draw_piece
├── lesson-09.md   Bitmap font: 5×7 glyphs, draw_char, draw_text, special chars
├── lesson-10.md   tetris_render(): one render function for all platforms
├── lesson-11.md   Pro input system: GameButtonState, GameActionWithRepeat (from lesson-9.5-v2.md)
├── lesson-12.md   Full HUD: score, level, pieces count, next-piece preview
├── lesson-13.md   Game over overlay: draw_rect_blend, alpha compositing
└── lesson-14.md   Final integration, VSync, resize, build flags, polish
```

---

## New Lesson Descriptions

### Lesson 01 — Open a Window (Updated)

**What changes:** The X11 backend now requires an OpenGL/GLX context to
display the backbuffer. The lesson extends the original window-creation steps
with:

- `glXChooseVisual` — find a framebuffer config with double buffering
- `glXCreateContext` — create a GL rendering context
- `XCreateWindow` using the visual from `glXChooseVisual` (not `XCreateSimpleWindow`)
- `glXMakeCurrent` — bind the GL context to the window
- VSync detection: query `GLX_EXTENSIONS`, attempt `GLX_EXT_swap_control` then
  `GLX_MESA_swap_control`, log result to terminal
- `platform_shutdown` creates the texture: `glGenTextures(1, &g_x11.texture_id)`

Build flags change: `-lX11 -lxkbcommon -lGL -lGLX` (was `-lX11`).

Raylib backend: `InitWindow`, `SetTargetFPS(60)` — unchanged from original.

**Key concept:** Display pipeline — CPU writes pixels → GPU uploads texture →
GPU composites to screen → `glXSwapBuffers` synchronizes to the monitor refresh.

---

### Lesson 02 — TetrisBackbuffer: The Platform-Independent Canvas

**What's new:** Introduces the `TetrisBackbuffer` struct and the ownership model:

```c
typedef struct {
  uint32_t *pixels;
  int width, height, pitch;
} TetrisBackbuffer;
```

Students allocate it in `main()` with `malloc(width * height * sizeof(uint32_t))`.
They learn:
- The platform **owns** the buffer lifetime (`malloc`/`free` in `main`)
- `tetris.c` **writes** into it but never allocates or frees it
- `pitch = width * 4` — bytes per row (useful for image libraries that may pad rows)
- Why `uint32_t *` instead of `unsigned char *` — one write per pixel vs four

**Key concept:** Separating buffer ownership from buffer usage. The platform
creates the canvas; the game layer paints it.

---

### Lesson 03 — Color System: uint32_t, TETRIS_RGB, Predefined Colors

**What's new:** Teaches pixel format from first principles:

- `0xAARRGGBB` bit layout — why alpha is highest byte
- Bit-packing: `TETRIS_RGBA(r,g,b,a)` macro using `<<` and `|`
- `TETRIS_RGB(r,g,b)` — shorthand for fully opaque (alpha = 255)
- Predefined color constants (`COLOR_CYAN`, `COLOR_RED`, etc.)
- Why one `uint32_t` works on both X11/GL and Raylib (`PIXELFORMAT_UNCOMPRESSED_R8G8B8A8`)
- Why platform-specific color types (X11 `unsigned long`, Raylib `Color`) are
  replaced by a single shared type

**Key concept:** Bit operations for color packing/unpacking. The same technique
used in OpenGL, Vulkan, Metal, and every pixel format specification.

---

### Lesson 04 — Typed Enums: TETROMINO_BY_IDX, TETROMINO_R_DIR, TETRIS_FIELD_CELL

**What's new:** Replaces "magic numbers" with named enums throughout.

- `TETROMINO_BY_IDX`: I/J/L/O/S/T/Z matching the `TETROMINOES[]` array order
- `TETROMINO_R_DIR`: R_0 / R_90 / R_180 / R_270 — no more "rotation 0-3"
- `TETRIS_FIELD_CELL`: `EMPTY=0`, `I…Z` = piece_index+1, `WALL`, `TMP_FLASH`
- Why `EMPTY = 0` — matches `memset(field, 0, sizeof(field))`
- Named tetromino constants: `TETROMINO_I`, `TETROMINO_J`, etc.
- `TETROMINOS_COUNT` and `SIDEBAR_WIDTH` in `tetris.h` (not per-backend)

**Key concept:** Sentinel values and why `0 == empty` is a useful convention.
Enums as self-documenting code vs plain `int`.

---

### Lesson 05 — GameState with CurrentPiece Substruct; game_init()

**What changes:** Restructures `GameState` from flat fields to grouped substructs.

- `CurrentPiece` substruct: `x`, `y`, `index` (typed), `next_index`, `rotation` (typed)
- `completed_lines` substruct: `indexes[]`, `count`, `flash_timer` (GameActionRepeat)
- `tetromino_drop` as `GameActionRepeat` (replaces `speed`/`speed_count`)
- `level` as explicit stored field; `pieces_count` renamed from `piece_count`
- `game_init(GameState *, GameInput *)` — why `GameInput *` is needed
  (initializes `move_left.repeat.interval`, `move_down.repeat.interval`, etc.)

**Key concept:** Grouping related fields into substructs. The struct tells a
story about which data belongs together.

---

### Lesson 06 — Collision Detection

**What changes:** Minor rename only — same algorithm, new function names:

- `tetris_rotate` → `tetromino_pos_value(px, py, rotation)`
- `tetris_does_piece_fit` → `tetromino_does_piece_fit`

Teaches the rotation formula (`case 0-3` in a switch), 1D array indexing of a
2D grid, and why checking `!= '.'` (not `== 'X'`) works for boundary detection.

**Key concept:** Coordinate-space rotation via arithmetic, not trigonometry.

---

### Lesson 07 — Delta-Time Game Loop and GameActionRepeat

_(Based on user-authored `lesson-8-v2.md` — incorporate its content fully.)_

**What changes vs lesson-8-v2:** `drop_timer`/`drop_interval` are no longer
standalone `float` fields — they are wrapped in `GameActionRepeat`:

```c
typedef struct { float timer; float interval; } GameActionRepeat;
/* Used for: tetromino_drop, flash_timer */
```

`tetris_update(GameState *, GameInput *, float delta_time)` replaces `tetris_tick`.
`prepare_input_frame(GameInput *)` resets transition counts.

Teaches: delta time accumulation, "subtract not reset" precision trick,
`nanosleep` vs `glXSwapBuffers` frame pacing, conditional sleep when VSync active.

**Key concept:** `GameActionRepeat` as a reusable pacing primitive. One struct
drives both the fall timer and the line-clear flash timer.

---

### Lesson 08 — Drawing Primitives in tetris.c

**What's new:** Students implement platform-independent drawing in `tetris.c`:

1. `draw_rect` — write a colored rectangle into `backbuffer->pixels`, with full
   bounds clipping (`x0 = max(x, 0)`, `x1 = min(x+w, bb->width)`)
2. `draw_cell(bb, col, row, color)` — convert grid coordinates to pixel
   coordinates (`x = col * CELL_SIZE + 1`, etc.) and call `draw_rect`
3. `draw_piece(bb, piece, col, row, color, rotation)` — iterate the 4×4
   tetromino string, call `draw_cell` for each `'X'`

Emphasizes: why `+1` padding makes grid lines visible between cells; how
`tetromino_pos_value` maps grid position to tetromino string index.

**Key concept:** Grid-to-pixel coordinate mapping; pixel array direct writes;
clipping to prevent out-of-bounds memory access.

---

### Lesson 09 — Bitmap Font: 5×7 Glyphs, draw_char, draw_text

**What's new:** One of the deepest lessons. Students implement a font defined
entirely as data:

- `FONT_DIGITS[10][7]` and `FONT_LETTERS[26][7]` — reading a glyph table
- `FONT_SPECIAL[]` — lookup table with terminator `{0, {0}}`
- `find_special_char(char c)` — linear scan returning a `const unsigned char *`
- Bit extraction: `bitmap[row] & (0x10 >> col)` — why `0x10` is the leftmost bit
- `draw_char(bb, x, y, c, color, scale)` — maps char → bitmap → `draw_rect` calls
- `draw_text(bb, x, y, text, color, scale)` — walks string, advances cursor by `6 * scale`
- Custom arrow mappings: `{` = ←, `}` = →, `^` = ↑, `v` = ↓

Visualizes a glyph: `0x0E = 01110 = .###.` — connects hex to pixels.

**Key concept:** Font as a data table. Bitmapped rendering. The `0x10 >> col`
pattern for extracting individual bits.

---

### Lesson 10 — tetris_render(): One Function for All Platforms

**What's new:** The architectural centerpiece of the upgrade. Students implement
`tetris_render(TetrisBackbuffer *, GameState *)` step by step:

1. Clear backbuffer (loop over all pixels, assign `COLOR_BLACK`)
2. Draw the field (switch on `TETRIS_FIELD_CELL`, call `draw_cell`)
3. Draw the falling piece (call `draw_piece`)
4. Draw HUD labels using `draw_text` with `COLOR_WHITE`, values in accent colors
5. Draw the next-piece mini preview at `preview_cell_size = CELL_SIZE / 2`
6. Draw controls hint with custom arrow characters (`{`, `}`, `v`)

After this lesson, both `main_x11.c` and `main_raylib.c` remove all their
drawing code. `platform_render` is superseded by `tetris_render` + a one-line
display call per backend.

**Key concept:** Inversion of the rendering relationship — the game draws itself
to a portable pixel buffer; each platform needs only to display it.

---

### Lesson 11 — Professional Input System

_(Based on user-authored `lesson-9.5-v2.md` — incorporate its content fully.)_

**What changes vs lesson-9.5-v2:** The `GameActionWithValue` for rotation
is replaced by the concrete `rotate_x` struct with `TETROMINO_ROTATE_X_VALUE`
enum (GO_LEFT / GO_RIGHT / NONE). `handle_action_with_repeat` is a `static`
function in `tetris.c`.

Teaches:
- `GameButtonState`: `half_transition_count` + `ended_down` (Handmade Hero origin)
- Why simple booleans miss sub-frame events (20ms tap in a 33ms frame)
- `UPDATE_BUTTON` macro: detect transitions, update final state
- `GameActionWithRepeat`: first-press fires immediately, then fires every `interval` seconds
- `prepare_input_frame`: must be called once per frame to reset `half_transition_count`
- Configuring intervals in `game_init` (e.g., `move_left.repeat.interval = 0.1f`)
- DAS/ARR concept (optional advanced section)

**Key concept:** Sub-frame input detection. Each action has its own independent
timer — holding Left and Down simultaneously works without interference.

---

### Lesson 12 — Full HUD: Score, Level, Pieces, Next-Piece Preview

**What changes:** The HUD is now drawn by `tetris_render` using the bitmap font.
Students finalize the sidebar layout:

- Score: `draw_text(bb, sx, sy, "SCORE", COLOR_WHITE, 2)` + value in `COLOR_YELLOW`
- Level: label + value in `COLOR_CYAN`
- Pieces count: `state->pieces_count`
- `level` increments every 25 pieces (`pieces_count % 25 == 0`)
- Next-piece preview: half-size grid using `preview_cell_size = CELL_SIZE / 2`
- Controls hint: uses custom arrow chars from `FONT_SPECIAL`
  (`{` `}` for left/right, `v` for down)

**Key concept:** HUD layout arithmetic. Scaling a mini-grid preview.
Using a custom font character mapping for UI symbols.

---

### Lesson 13 — Game Over Overlay: Alpha Compositing

**What's new:** Students implement `draw_rect_blend` in `tetris.c`:

```c
void draw_rect_blend(TetrisBackbuffer *bb, int x, int y, int w, int h,
                     uint32_t color) {
  uint8_t alpha = (color >> 24) & 0xFF;
  /* for each pixel: out = (src * alpha + dst * (255 - alpha)) / 255 */
}
```

Applied in `tetris_render` when `state->game_over`:
1. `draw_rect_blend` with `TETRIS_RGBA(0, 0, 0, 200)` — dim the field
2. Four thin `draw_rect` calls for the red border
3. `draw_text` for "GAME OVER", "R RESTART", "Q QUIT"

**Key concept:** The compositing formula. Why `alpha == 255` skips blending
(fast path). Why `alpha == 0` returns early. How to extract the alpha channel:
`(color >> 24) & 0xFF`.

---

### Lesson 14 — Final Integration, Polish, and Where to Go Next

**What changes:** Updates original lesson 12's integration checklist for the
new feature set. Adds:

- Verifying GLX context and VSync on startup (terminal output: `✓ VSync enabled`)
- Window resize: confirm `glViewport` + `glOrtho` keep the image correct
- Bitmap font: visually confirm all HUD labels render
- Alpha overlay: confirm game over shows dimmed field + red border
- Build flags: `clang … -lX11 -lxkbcommon -lGL -lGLX`
- Valgrind: `backbuffer.pixels` `malloc`/`free` pair must balance;
  `glDeleteTextures` must be called in `platform_shutdown`

Updated "where to go next" extensions:

| Extension | New concept introduced |
|-----------|----------------------|
| Ghost piece | `tetromino_does_piece_fit` raycast downward |
| Hard drop | Space bar; same raycast as ghost piece |
| SDL3 backend | `src/main_sdl3.c` — same backbuffer pattern, third platform |
| WebAssembly | Compile `tetris.c` unchanged via Emscripten; `main_wasm.c` feeds `tetris_render` into a canvas pixel array |
| 7-bag randomizer | Fisher-Yates shuffle |
| Wall kicks | Retry `tetromino_does_piece_fit` at `x-1`, `x+1` before failing rotation |
| High score to disk | `fopen`, `fprintf`, `fscanf` |
| Sound effects | Raylib `PlaySound()` |

---

## Lesson Count Summary

| # | Old Title (backup) | New Title | Status |
|---|--------------------|-----------|--------|
| 01 | Open a Window | Window + GLX/OpenGL Context | Updated |
| 02 | The Field | TetrisBackbuffer | New |
| 03 | Tetrominoes | Color System | New |
| 04 | Rotation | Typed Enums | New |
| 05 | Collision | GameState + CurrentPiece + game_init | Restructured |
| 06 | Piece Locking | Collision Detection | Minor rename |
| 07 | Line Detection | Delta-Time Loop + GameActionRepeat | Replaces 08+8-v2 |
| 08 | Game Loop (tick) | Drawing Primitives in tetris.c | New |
| 09 | Score + Difficulty | Bitmap Font | New |
| 10 | HUD + Next Piece | tetris_render() | New |
| 11 | Game Over | Pro Input System | Replaces 9.5-v2 |
| 12 | Restart + Polish | Full HUD | Updated |
| 13 | _(new)_ | Game Over Overlay + Alpha Blend | New |
| 14 | _(was 12)_ | Final Integration | Updated |

**Total: 14 lessons** (all single-numbered, no v2 variants — those are now merged in)

---

## Implementation Steps

1. **Create `lessons/old/`** and move all 14 existing lesson files into it
   (12 originals + 2 user-authored v2 files). Preserve them unchanged.

2. **Rewrite `src/` files** to match `games/tetris/src/`:
   - `tetris.h` — new types, enums, macros, API
   - `tetris.c` — bitmap font, drawing primitives, `tetris_render`, game logic
   - `platform.h` — updated signatures
   - `main_x11.c` — GLX/OpenGL backend
   - `main_raylib.c` — backbuffer texture pipeline

3. **Update build scripts**:
   - `build_x11.sh` — add `-lGL -lGLX -lxkbcommon`, switch to `clang`

4. **Write lessons 01–14** referencing the updated `src/` files, drawing from:
   - The original 12 lessons for pedagogical structure and wording
   - `lesson-8-v2.md` (user-authored) for lesson 07 content
   - `lesson-9.5-v2.md` (user-authored) for lesson 11 content
   - `games/tetris/src/` as the canonical code reference

---

## Codebase Reference Map

| Concept | File | Location |
|---------|------|----------|
| TetrisBackbuffer | `games/tetris/src/tetris.h` | Lines 22–29 |
| TETRIS_RGB/RGBA macros | `games/tetris/src/tetris.h` | Lines 31–44 |
| Predefined colors | `games/tetris/src/tetris.h` | Lines 35–44 |
| TETROMINO_BY_IDX enum | `games/tetris/src/tetris.h` | Lines 55–65 |
| TETROMINO_R_DIR enum | `games/tetris/src/tetris.h` | Lines 67–72 |
| TETRIS_FIELD_CELL enum | `games/tetris/src/tetris.h` | Lines 73–82 |
| GameActionRepeat | `games/tetris/src/tetris.h` | Lines 84–93 |
| CurrentPiece substruct | `games/tetris/src/tetris.h` | Lines 94–102 |
| GameState (full) | `games/tetris/src/tetris.h` | Lines 103–127 |
| GameButtonState | `games/tetris/src/tetris.h` | Lines 128–155 |
| GameActionWithRepeat | `games/tetris/src/tetris.h` | Lines 156–168 |
| TETROMINO_ROTATE_X_VALUE | `games/tetris/src/tetris.h` | Lines 174–178 |
| GameInput | `games/tetris/src/tetris.h` | Lines 179–199 |
| UPDATE_BUTTON macro | `games/tetris/src/tetris.h` | Lines 200–210 |
| TETROMINOS_COUNT, SIDEBAR_WIDTH | `games/tetris/src/tetris.h` | Lines 9–11, 17 |
| FONT_DIGITS / FONT_LETTERS | `games/tetris/src/tetris.c` | Lines ~37–90 |
| FONT_SPECIAL + find_special_char | `games/tetris/src/tetris.c` | Lines ~93–145 |
| draw_rect | `games/tetris/src/tetris.c` | Lines ~155–172 |
| draw_rect_blend | `games/tetris/src/tetris.c` | Lines ~174–203 |
| draw_char (static) | `games/tetris/src/tetris.c` | Lines ~205–232 |
| get_tetromino_color (static) | `games/tetris/src/tetris.c` | Lines ~244–264 |
| draw_cell / draw_piece (static) | `games/tetris/src/tetris.c` | Lines ~266–285 |
| draw_text | `games/tetris/src/tetris.c` | Lines ~287–295 |
| tetris_render | `games/tetris/src/tetris.c` | Lines ~300–426 |
| game_init | `games/tetris/src/tetris.c` | Lines ~427–520 |
| handle_action_with_repeat (static) | `games/tetris/src/tetris.c` | Lines ~541–560 |
| tetris_apply_input (static) | `games/tetris/src/tetris.c` | Lines ~568–650 |
| tetris_update | `games/tetris/src/tetris.c` | Lines ~654–850 |
| GLX context + VSync setup | `games/tetris/src/main_x11.c` | Lines ~21–105 |
| ConfigureNotify / resize handler | `games/tetris/src/main_x11.c` | Lines ~289–298 |
| platform_display_backbuffer | `games/tetris/src/main_x11.c` | Lines ~310–342 |
| X11 backbuffer alloc/main loop | `games/tetris/src/main_x11.c` | Lines ~345–400 |
| platform_get_input (X11) | `games/tetris/src/main_x11.c` | Lines ~200–310 |
| Raylib backbuffer pipeline | `games/tetris/src/main_raylib.c` | Full file (~111 lines) |
| Delta-time teaching material | `lesson-8-v2.md` | Full file (~758 lines) |
| Pro input teaching material | `lesson-9.5-v2.md` | Full file |
