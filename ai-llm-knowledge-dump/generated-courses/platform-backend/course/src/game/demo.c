/* ═══════════════════════════════════════════════════════════════════════════
 * game/demo.c — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 03 — demo_render() renders colored canvas + "PLATFORM READY" text.
 * LESSON 04 — demo_render() adds a rectangle demonstration.
 * LESSON 08 — demo_render() adds FPS counter via snprintf + draw_text.
 *
 * LESSON 14 — This file is REMOVED in the final template.  Game courses
 *             replace it with their own game/main.c containing
 *             game_init(), game_update(), game_render().
 *
 * Called from BOTH backends:
 *   // In main loop of platforms/raylib/main.c and platforms/x11/main.c:
 *   demo_render(&backbuffer, &input, fps);
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdio.h>   /* snprintf — LESSON 08 */

#include "../utils/backbuffer.h"
#include "../utils/draw-shapes.h"
#include "../utils/draw-text.h"
#include "../game/base.h"

/* demo_render — renders one demo frame into `bb`.
 *
 * LESSON 03 — Background fill.
 * LESSON 04 — Colored rectangles.
 * LESSON 06 — "PLATFORM READY" text label.
 * LESSON 07 — Visual feedback for button presses (play_tone input).
 * LESSON 08 — FPS counter via snprintf + draw_text.
 *
 * `fps` is passed in as an integer computed by the platform frame timing.  */
void demo_render(Backbuffer *bb, GameInput *input, int fps) {
  /* ── LESSON 03 — Fill background ─────────────────────────────────────── */
  draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BG);

  /* ── LESSON 04 — Draw sample rectangles ─────────────────────────────── */
  draw_rect(bb, 50,  50, 200, 100, COLOR_RED);
  draw_rect(bb, 300, 50, 200, 100, COLOR_GREEN);
  draw_rect(bb, 550, 50, 200, 100, COLOR_BLUE);

  /* Semi-transparent overlay to show alpha blend (LESSON 04). */
  draw_rect_blend(bb, 150, 30, 200, 140, GAME_RGBA(255, 255, 0, 120));

  /* ── LESSON 06 — Static label ────────────────────────────────────────── */
  draw_text(bb, 50, 200, 2, "PLATFORM READY", COLOR_WHITE);

  /* ── LESSON 07 — Input feedback ─────────────────────────────────────── */
  if (input && input->buttons.play_tone.ended_down) {
    draw_rect(bb, 50, 240, 300, 30, COLOR_YELLOW);
    draw_text(bb, 55, 247, 1, "SPACE: play tone", COLOR_BLACK);
  } else {
    draw_text(bb, 50, 247, 1, "SPACE: play tone", COLOR_GRAY);
  }

  /* ── LESSON 08 — FPS counter via snprintf ────────────────────────────── */
  char fps_str[32];
  snprintf(fps_str, sizeof(fps_str), "FPS: %d", fps);
  /* Draw top-right corner — advance x by (strlen) glyphs * GLYPH_W * scale */
  int scale = 1;
  int str_w = 0;
  for (const char *p = fps_str; *p; p++) str_w += GLYPH_W * scale;
  draw_text(bb, bb->width - str_w - 8, 8, scale, fps_str, COLOR_GREEN);
}
