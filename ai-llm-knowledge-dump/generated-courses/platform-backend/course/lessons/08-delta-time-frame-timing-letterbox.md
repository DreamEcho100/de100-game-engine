# Lesson 08 — Delta Time, Frame Timing, and Letterbox

## Overview

| Item | Value |
|------|-------|
| **Backend** | Both (timing focus on X11; Raylib uses `GetFrameTime`/`GetFPS`) |
| **New concepts** | `FrameTiming` coarse-sleep + spin-wait, `FrameStats` (DEBUG), letterbox math, `snprintf` FPS counter |
| **Observable outcome** | Window resizable; canvas letterboxed on resize; FPS counter visible top-right |
| **Files modified** | `src/platforms/x11/main.c`, `src/platforms/raylib/main.c`, `src/game/demo.c` |

---

## What you'll build

1. **Letterbox scaling** — the canvas scales to fill the window without distortion.
2. **Frame timing** — X11 uses a two-phase sleep (coarse nanosleep + spin-wait) to hit the target frame rate precisely.
3. **FPS counter** — `snprintf` formats an integer into a string; `draw_text` renders it.

---

## Background

### The letterbox problem

When the window is 1200×600 and the canvas is 800×600, drawing the canvas at (0,0) with no scaling leaves a 400px black gap on the right.  "Letterbox" scales the canvas to fit, adding black bars only where the aspect ratios differ.

```
canvas: 800×600 (4:3)        window: 1200×600 (2:1)
                           ┌─────────────────────────┐
scale = min(1200/800,      │  black  ┌─────────┐  black │
            600/600) = 1.0 │         │ canvas  │        │
                           │         └─────────┘        │
                           └─────────────────────────────┘
```

Formula:
```c
float sx     = (float)win_w / (float)canvas_w;
float sy     = (float)win_h / (float)canvas_h;
float scale  = MIN(sx, sy);
int   off_x  = (int)((win_w  - canvas_w  * scale) / 2.0f);
int   off_y  = (int)((win_h  - canvas_h  * scale) / 2.0f);
```

### `XSizeHints` removal (L02 → L08)

In lesson 02 we added `XSizeHints` to lock the window size.  Now that we have letterbox math, **remove those five lines** from `init_window`.

### Frame timing (X11 only)

Raylib's `SetTargetFPS(60)` + `EndDrawing()` handles sleep internally.  X11 has no built-in frame limiter, so we implement the DE100 pattern:

```
┌── timing_begin() ────────────────────────────────────────────────┐
│   frame_start = clock_gettime(CLOCK_MONOTONIC)                  │
├── [game logic + render + GPU upload] ───────────────────────────┤
│                                                                  │
├── timing_mark_work_done() ───────────────────────────────────────┤
│   work_end = now; work_seconds = work_end - frame_start         │
├── timing_sleep_until_target() ───────────────────────────────────┤
│   Phase 1: nanosleep(1ms) in a loop until 3ms before deadline   │
│   Phase 2: spin-wait to exact deadline                          │
│   (spin eliminates scheduler jitter in the last 3ms)           │
├── timing_end() ──────────────────────────────────────────────────┤
│   frame_end = now; total = frame_end - frame_start              │
│   sleep = total - work                                          │
└──────────────────────────────────────────────────────────────────┘
```

**Phase 1** (coarse sleep) saves CPU.  **Phase 2** (spin) gives sub-millisecond accuracy.

`FrameStats` (`#ifdef DEBUG`) accumulates min/max/avg frame time and missed-frame count across the entire session, printed on shutdown.

### `snprintf` for the FPS counter

```c
char fps_str[32];
snprintf(fps_str, sizeof(fps_str), "FPS: %d", fps);
draw_text(bb, x, y, 1, fps_str, COLOR_GREEN);
```

`snprintf` formats an integer (`fps`) into a `char` buffer.  `sizeof(fps_str)` prevents buffer overflow.  This is the first time in the course we format a dynamic value into a string.

### FPS display — rolling average

A raw `(int)(1.0f / total_seconds)` truncates to integer every frame, causing the counter to jump ±2 fps constantly — visually distracting even when timing is stable.

The X11 backend computes a 60-frame rolling average instead:

```c
static float frame_time_accum = 0.0f;
static int   frame_time_count = 0;
static int   fps_display      = TARGET_FPS;

frame_time_accum += g_timing.total_seconds;
frame_time_count++;
if (frame_time_count >= 60) {
    fps_display      = (frame_time_accum > 0.0f)
                       ? (int)(60.0f / frame_time_accum + 0.5f)
                       : TARGET_FPS;
    frame_time_accum = 0.0f;
    frame_time_count = 0;
}
demo_render(&bb, curr_input, fps_display);
```

Raylib uses `GetFPS()` which already smooths the value internally.

---

## What to type

### X11 `main.c` — FrameTiming struct + helpers

```c
typedef struct {
    struct timespec frame_start, work_end, frame_end;
    float work_seconds, total_seconds, sleep_seconds;
} FrameTiming;

#ifdef DEBUG
typedef struct {
    unsigned int frame_count, missed_frames;
    float min_frame_ms, max_frame_ms, total_frame_ms;
} FrameStats;
static FrameStats g_stats = {0};
#endif

static FrameTiming g_timing = {0};
static const float TARGET_SECONDS = 1.0f / TARGET_FPS;

static inline void timing_begin(void) {
    clock_gettime(CLOCK_MONOTONIC, &g_timing.frame_start);
}
static inline void timing_mark_work_done(void) {
    clock_gettime(CLOCK_MONOTONIC, &g_timing.work_end);
    g_timing.work_seconds = timespec_diff_seconds(&g_timing.frame_start,
                                                    &g_timing.work_end);
}
static void timing_sleep_until_target(void) {
    float elapsed = g_timing.work_seconds;
    float threshold = TARGET_SECONDS - 0.003f;       /* 3ms spin budget */
    while (elapsed < threshold) {                     /* Phase 1: coarse */
        struct timespec t = { .tv_nsec = 1000000L };  /* 1ms */
        nanosleep(&t, NULL);
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed = timespec_diff_seconds(&g_timing.frame_start, &now);
    }
    while (elapsed < TARGET_SECONDS) {               /* Phase 2: spin */
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed = timespec_diff_seconds(&g_timing.frame_start, &now);
    }
}
```

### X11 `main.c` — letterbox in `display_backbuffer`

```c
// Remove the fixed glOrtho from init_window; recalculate each frame:
float sx = (float)g_x11.window_w / (float)bb->width;
float sy = (float)g_x11.window_h / (float)bb->height;
float scale = MIN(sx, sy);
int off_x = (int)((g_x11.window_w - bb->width  * scale) / 2.0f);
int off_y = (int)((g_x11.window_h - bb->height * scale) / 2.0f);
float x1 = off_x + bb->width * scale, y1 = off_y + bb->height * scale;

glViewport(0, 0, g_x11.window_w, g_x11.window_h);
glMatrixMode(GL_PROJECTION); glLoadIdentity();
glOrtho(0, g_x11.window_w, g_x11.window_h, 0, -1, 1);
glMatrixMode(GL_MODELVIEW); glLoadIdentity();
glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
// glTexSubImage2D uploads updated pixels into the already-allocated texture:
glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bb->width, bb->height,
                GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);
glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex2f(off_x, off_y);
    glTexCoord2f(1,0); glVertex2f(x1,    off_y);
    glTexCoord2f(1,1); glVertex2f(x1,    y1);
    glTexCoord2f(0,1); glVertex2f(off_x, y1);
glEnd();
glXSwapBuffers(g_x11.display, g_x11.window);
```

### Raylib `main.c` — letterbox with `DrawTexturePro`

```c
// Replace DrawTexture(texture, 0, 0, WHITE) with:
float sx = (float)GetScreenWidth()  / (float)bb.width;
float sy = (float)GetScreenHeight() / (float)bb.height;
float scale = MIN(sx, sy);
float dst_w = bb.width  * scale, dst_h = bb.height * scale;
float off_x = ((float)GetScreenWidth()  - dst_w) / 2.0f;
float off_y = ((float)GetScreenHeight() - dst_h) / 2.0f;

Rectangle src  = { 0, 0, (float)bb.width, (float)bb.height };
Rectangle dest = { off_x, off_y, dst_w, dst_h };
DrawTexturePro(texture, src, dest, (Vector2){0,0}, 0.0f, WHITE);
```

### `src/game/demo.c` — L08 FPS counter

```c
char fps_str[32];
snprintf(fps_str, sizeof(fps_str), "FPS: %d", fps);
int str_w = 0;
for (const char *p = fps_str; *p; p++) str_w += GLYPH_W;
draw_text(bb, bb->width - str_w - 8, 8, 1, fps_str, COLOR_GREEN);
```

---

## Explanation

| Concept | Detail |
|---------|--------|
| `CLOCK_MONOTONIC` | Wall-clock timer that never goes backward (unlike `CLOCK_REALTIME`).  Required for frame timing. |
| 3ms spin budget | Scheduler granularity on Linux is ~1ms.  We stop coarse-sleeping 3ms early and spin to hit the exact deadline. |
| `glViewport` per frame | Needed after window resize (ConfigureNotify) — otherwise the quad coords map to the old viewport. |
| `DrawTexturePro` | Raylib's textured quad with explicit source + destination rectangles; handles the letterbox in one call. |
| `snprintf` + `sizeof` | The `sizeof` argument prevents buffer overflow regardless of how large `fps` gets. |

---

## Build and run

```sh
./build-dev.sh --backend=x11 -r
```

Resize the window — the canvas stays centered with correct aspect ratio.  The FPS counter shows in the top-right corner.  Build with `-d` and quit to see frame stats.

---

## Quiz

1. Why does `MIN(sx, sy)` give the correct letterbox scale?  What would `MAX(sx, sy)` produce instead?
2. Why use `CLOCK_MONOTONIC` instead of `CLOCK_REALTIME` for frame timing?
3. The spin-wait consumes 100% of one CPU core for ~3ms per frame.  Is that acceptable?  What is the trade-off vs. a pure `nanosleep` approach?
