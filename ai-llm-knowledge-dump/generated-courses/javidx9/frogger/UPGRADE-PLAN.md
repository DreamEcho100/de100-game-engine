# Frogger Course — Upgrade Plan

## Goal

Upgrade the frogger course from its current architecture (6-function platform contract, X11 Pixmap double-buffering, plain `InputState` ints, `gcc`) to the standardised architecture used by the upgraded tetris and snake courses: **backbuffer pipeline, 4-function platform contract, `GameButtonState` input, `clang`, OpenGL/GLX on X11, Texture2D pipeline on Raylib**.

---

## Architecture Comparison

| Aspect | Current (old) | Target (new) |
|--------|--------------|--------------|
| Rendering | `platform_render` in platform files | `frogger_render` in `frogger.c` → writes to `FroggerBackbuffer` |
| Double-buffer (X11) | `XPixmap` + `XCopyArea` | `uint32_t *pixels` → `glTexImage2D` |
| Double-buffer (Raylib) | `BeginDrawing/DrawRectangle` | `Texture2D` + `UpdateTexture` |
| Platform contract | 6 functions (init, get_input, render, sleep_ms, should_quit, shutdown) | 4 functions (init, get_time, get_input + display_backbuffer static/inline) |
| Game loop location | `frogger_run()` in `frogger.c` | `main()` in each platform file |
| Input type | `InputState { up_released, down_released, ... }` ints | `GameInput` with `GameButtonState` + `UPDATE_BUTTON` |
| Auto-repeat (X11) | Peek-ahead `XPeekEvent` detection | `XkbSetDetectableAutoRepeat` |
| Build compiler | `gcc` | `clang` |
| Build flags (X11) | `-lX11` | `-lX11 -lxkbcommon -lGL -lGLX` |
| HUD text | `XDrawString` / Raylib `DrawText` | Bitmap font in `frogger.c` |
| Rendering duplication | `lane_speeds`, `lane_patterns`, `local_tile_to_sprite` duplicated in both platform files | All rendering in `frogger.c`, platform files are pure infrastructure |

---

## 3-Layer Architecture

```
┌─────────────────────────────────────────────────────┐
│  Game Layer (frogger.c + frogger.h)                 │
│  frogger_init / frogger_tick / frogger_render        │
│  Reads: GameInput   Writes: FroggerBackbuffer pixels │
└───────────────┬─────────────────────────────────────┘
                │ platform_get_time()
                │ platform_get_input()
                │ platform_display_backbuffer() [static/inline]
┌───────────────▼─────────────────────────────────────┐
│  Platform Layer (main_x11.c or main_raylib.c)        │
│  Contains: platform_init, platform_get_time,         │
│            platform_get_input, delta-time main loop  │
│            static platform_display_backbuffer        │
└─────────────────────────────────────────────────────┘
```

---

## Per-File Changes

### `src/frogger.h`

| What | Old | New |
|------|-----|-----|
| Backbuffer type | none | `FroggerBackbuffer { uint32_t *pixels; int width, height; }` |
| Color macros | none | `FROGGER_RGB(r,g,b)` → `0xFF000000 \| r<<16 \| g<<8 \| b` |
| Input type | `InputState { int up_released; ... }` | `GameInput` with `GameButtonState { int half_transition_count; int ended_down; }` |
| `UPDATE_BUTTON` | none | `#define UPDATE_BUTTON(btn, is_down) do { if ((btn).ended_down != (is_down)) { (btn).half_transition_count++; } (btn).ended_down = (is_down); } while(0)` |
| `frogger_run` | declared | removed |
| `frogger_render` | none (was in platform) | declared: `void frogger_render(const GameState *s, FroggerBackbuffer *bb)` |

**`GameInput` for frogger:**
```c
typedef struct { int half_transition_count; int ended_down; } GameButtonState;

typedef struct {
    GameButtonState move_up;
    GameButtonState move_down;
    GameButtonState move_left;
    GameButtonState move_right;
    int quit;
} GameInput;
```

**`FroggerBackbuffer`:**
```c
typedef struct {
    uint32_t *pixels;
    int       width;
    int       height;
} FroggerBackbuffer;

#define FROGGER_RGB(r,g,b) \
    ((uint32_t)0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
```

**Input semantics — "just released" → "just pressed":**

Old code fires on key *release* (`up_released`). New code uses `GameButtonState` "just pressed" (`ended_down && htc > 0`). Both prevent holding from causing continuous movement. "Just pressed" is consistent with the upgraded tetris/snake architecture.

**`prepare_input_frame`:**
```c
static void prepare_input_frame(GameInput *input) {
    input->move_up.half_transition_count    = 0;
    input->move_down.half_transition_count  = 0;
    input->move_left.half_transition_count  = 0;
    input->move_right.half_transition_count = 0;
    input->quit = 0;
}
```

---

### `src/frogger.c`

| What | Old | New |
|------|-----|-----|
| `frogger_run` | present (game loop, platform calls) | removed |
| `frogger_render` | none | new — writes sprite pixels to `FroggerBackbuffer` |
| Input type | `const InputState *input` | `const GameInput *input` |
| Movement trigger | `input->up_released` | `input->move_up.ended_down && input->move_up.half_transition_count > 0` |
| `prepare_input_frame` | none | added (static) |
| Lane data duplication | none (`frogger.c` had it; platform files had their own copies) | All rendering uses `frogger.c`'s lane data — no duplication |
| HUD text | `XDrawString` / Raylib DrawText in platform | Bitmap font in `frogger_render` |

**`draw_sprite_partial` moves to `frogger.c`** and writes to `FroggerBackbuffer` instead of X11/Raylib:

```c
/* Old (X11): */
XSetForeground(g_display, g_gc, pixel);
XFillRectangle(g_display, g_backbuffer, g_gc,
    dest_px_x + sx * CELL_PX,
    dest_px_y + sy * CELL_PX,
    CELL_PX, CELL_PX);

/* New: */
for (int py = 0; py < CELL_PX; py++)
    for (int px = 0; px < CELL_PX; px++) {
        int bx = dest_px_x + sx * CELL_PX + px;
        int by = dest_px_y + sy * CELL_PX + py;
        if (bx >= 0 && bx < bb->width && by >= 0 && by < bb->height)
            bb->pixels[by * bb->width + bx] = FROGGER_RGB(r, g, b);
    }
```

**`frogger_render` structure:**
```
1. Clear backbuffer to black
2. Draw all lanes (using lane_scroll + draw_sprite_partial into bb)
3. Draw frog (or flashing frog on death)
4. Draw bitmap font HUD (score, DEAD! overlay, YOU WIN! overlay)
```

---

### `src/platform.h`

Remove: `platform_render`, `platform_sleep_ms`, `platform_should_quit`, `platform_shutdown`.

Add: `platform_get_time` → `double platform_get_time(void)`.

Note: `platform_display_backbuffer(FroggerBackbuffer *bb)` is `static` in `main_x11.c`; done inline in Raylib `main()`. Not declared in `platform.h`.

**New 4-function contract:**
```c
void   platform_init(int width, int height, const char *title);
double platform_get_time(void);
void   platform_get_input(GameInput *input);
/* platform_display_backbuffer: static in main_x11.c, inline in main_raylib.c */
```

---

### `src/main_x11.c`

| What | Old | New |
|------|-----|-----|
| Double-buffer | `Pixmap g_backbuffer` + `XCopyArea` | `glTexImage2D` → fullscreen quad |
| Window creation | `XCreateSimpleWindow` | `XCreateWindow` (required for GLX visual) |
| Auto-repeat | Peek-ahead `XPeekEvent` | `XkbSetDetectableAutoRepeat` |
| VSync | none | `GLX_EXT_swap_control` |
| Input type | `InputState *` | `GameInput *` |
| Game loop | `frogger_run()` call in `main` | Full delta-time loop in `main` |
| `platform_render` | draws via X11 | removed |
| `platform_sleep_ms` | `nanosleep` | removed |
| `platform_should_quit` | returns `g_quit` | removed (loop checks `g_is_running`) |
| `platform_shutdown` | frees GC, closes display | removed (cleanup inline before loop exit) |
| `platform_get_time` | none | `clock_gettime(CLOCK_MONOTONIC)` |

**OpenGL fullscreen quad** (same as tetris/snake):
```c
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
             GL_BGRA, GL_UNSIGNED_BYTE, backbuffer.pixels);
/* draw fullscreen quad */
```

Pixel format: `0xAARRGGBB` stored as bytes in memory → `GL_BGRA` upload matches.

**Delta-time loop in `main_x11.c`:**
```c
double prev_time = platform_get_time();
while (g_is_running) {
    double curr_time  = platform_get_time();
    float  delta_time = (float)(curr_time - prev_time);
    prev_time = curr_time;
    if (delta_time > 0.1f) delta_time = 0.1f;

    prepare_input_frame(&input);
    platform_get_input(&input);
    if (input.quit) break;

    frogger_tick(&state, &input, delta_time);
    frogger_render(&state, &backbuffer);
    platform_display_backbuffer(&backbuffer);
}
```

---

### `src/main_raylib.c`

| What | Old | New |
|------|-----|-----|
| Rendering | `BeginDrawing` / `DrawRectangle` per sprite cell | `Texture2D` + `UpdateTexture` → `DrawTexture` |
| `platform_render` | draws via Raylib | removed |
| `platform_sleep_ms` | no-op | removed |
| `platform_should_quit` | `g_quit || WindowShouldClose()` | removed |
| `platform_shutdown` | `CloseWindow()` | removed |
| `platform_get_time` | none | `GetTime()` |
| Game loop | `frogger_run()` call in `main` | Delta-time loop in `main` |

**Texture2D pipeline:**
```c
Texture2D tex = LoadTextureFromImage(
    (Image){ .data=bb.pixels, .width=w, .height=h,
             .mipmaps=1, .format=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 });
/* each frame: */
UpdateTexture(tex, bb.pixels);
BeginDrawing();
DrawTexture(tex, 0, 0, WHITE);
EndDrawing();
```

**Pixel format note:** `FROGGER_RGB` packs as `0xFFRRGGBB`. Raylib `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` reads bytes in order R,G,B,A. `0xFFRRGGBB` in memory (little-endian) is `[BB, GG, RR, FF]` → wrong. Use `FROGGER_RGBA` with bytes in the order Raylib expects, or use `PIXELFORMAT_UNCOMPRESSED_B8G8R8A8`.

Simplest fix: use the same `0xAARRGGBB` format as tetris/snake and pass `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` with byte-swapped R/B in the pixel write macro — or use `GL_BGRA` on X11 and `PIXELFORMAT_UNCOMPRESSED_B8G8R8A8` on Raylib. Follow exactly the same convention as snake/tetris.

---

### Build Scripts

**`build_x11.sh`:**
```sh
clang -Wall -Wextra -Wpedantic -std=c99 \
    -DASSETS_DIR=\"assets\" \
    -o build/frogger_x11 \
    src/frogger.c src/main_x11.c \
    -lX11 -lxkbcommon -lGL -lGLX
```

**`build_raylib.sh`:**
```sh
clang -Wall -Wextra -Wpedantic -std=c99 \
    -DASSETS_DIR=\"assets\" \
    -o build/frogger_raylib \
    src/frogger.c src/main_raylib.c \
    -lraylib -lm
```

---

## Things to Keep (Not Change)

- `lane_scroll()` — pixel-accurate scroll math is already correct and well-commented
- `SpriteBank` + `sprite_load()` — `.spr` binary format loader is correct
- `danger[]` buffer + collision detection — already correct (center-cell check)
- `CONSOLE_PALETTE` — correct Windows console color table
- `lane_speeds[]` and `lane_patterns[][]` — DOD layout, correct data
- `frogger_init` — no changes needed (keep `memset`, sprite loading, frog start pos)
- `frogger_tick` — update input parameter type from `InputState*` to `GameInput*`, update movement checks; keep all other logic

---

## Lesson Plan (12 Lessons)

| # | Title | Key concepts |
|---|-------|-------------|
| 01 | Window + GLX/OpenGL Context | `XCreateWindow` vs `XCreateSimpleWindow`, GLX context creation, VSync |
| 02 | FroggerBackbuffer: Platform-Independent Canvas | `uint32_t *pixels`, `malloc`/`free`, `glTexImage2D` vs `UpdateTexture` |
| 03 | Color System: `FROGGER_RGB` + `CONSOLE_PALETTE` | Pixel format `0xAARRGGBB`, palette lookup, transparent cells |
| 04 | SpriteBank: `.spr` Binary Format + Pool Loader | `fread`, `int32_t`/`int16_t`, pool offsets, sprite IDs |
| 05 | `GameInput`: `GameButtonState` + `UPDATE_BUTTON` | `htc`, `ended_down`, "just pressed", `XkbSetDetectableAutoRepeat` |
| 06 | Data-Oriented Design: Lane Data | DOD vs AOS, `lane_speeds[]` vs `struct Lane`, cache lines |
| 07 | `lane_scroll`: Pixel-Accurate Scrolling | Positive modulo, pixel vs cell offset, sub-pixel smooth rendering |
| 08 | `frogger_render`: Sprites into the Backbuffer | `draw_sprite_partial` writing to `bb->pixels`, rendering order |
| 09 | Delta-Time Game Loop + `platform_get_time` | `clock_gettime(CLOCK_MONOTONIC)`, `GetTime()`, dt cap |
| 10 | Platform Riding: River Logs Carry the Frog | `frog_x -= lane_speeds[fy] * dt` for river rows |
| 11 | Danger Buffer + Collision Detection | `danger[]` rebuild, center-cell check, why not 4-corner |
| 12 | Death Flash, Win State + Bitmap Font HUD | `dead_timer`, flash formula, bitmap font, `draw_rect_blend` overlay |
