/* =============================================================================
   asteroids.c — Game Logic + Rendering

   All game update and rendering logic lives here.
   No X11, no Raylib, no OS calls — only math.h and stdlib.h.

   Key new techniques vs tetris/snake/frogger:
     - Bresenham's line algorithm (draw_line) — integer pixel-exact line drawing
     - 2D rotation matrix (draw_wireframe) — rotate model vertices each frame
     - Entity pools with compact_pool — remove dead objects in O(1) by swap-with-last
     - Circle collision (is_point_inside_circle) — squared distance, no sqrt
     - Toroidal wrapping — per-pixel in draw_pixel_w so lines cross edges seamlessly
   ============================================================================= */

#define _POSIX_C_SOURCE 200809L

#include "asteroids.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/* ─── Bitmap font (column-major, 5×7) ────────────────────────────────────────
   96 printable ASCII chars starting at 0x20 (space).
   Each char = 5 bytes (one per column). Each byte: bit 0 = top row, bit 6 = bottom.
   Usage: font_glyphs[ch - 32][col] & (1 << row)                              */
static const uint8_t font_glyphs[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' */ {0x00,0x00,0x5F,0x00,0x00}, /* '!' */
    {0x00,0x07,0x00,0x07,0x00}, /* '"' */ {0x14,0x7F,0x14,0x7F,0x14}, /* '#' */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* '$' */ {0x23,0x13,0x08,0x64,0x62}, /* '%' */
    {0x36,0x49,0x55,0x22,0x50}, /* '&' */ {0x00,0x05,0x03,0x00,0x00}, /* ''' */
    {0x00,0x1C,0x22,0x41,0x00}, /* '(' */ {0x00,0x41,0x22,0x1C,0x00}, /* ')' */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* '*' */ {0x08,0x08,0x3E,0x08,0x08}, /* '+' */
    {0x00,0x50,0x30,0x00,0x00}, /* ',' */ {0x08,0x08,0x08,0x08,0x08}, /* '-' */
    {0x00,0x30,0x30,0x00,0x00}, /* '.' */ {0x20,0x10,0x08,0x04,0x02}, /* '/' */
    {0x3E,0x51,0x49,0x45,0x3E}, /* '0' */ {0x00,0x42,0x7F,0x40,0x00}, /* '1' */
    {0x42,0x61,0x51,0x49,0x46}, /* '2' */ {0x21,0x41,0x45,0x4B,0x31}, /* '3' */
    {0x18,0x14,0x12,0x7F,0x10}, /* '4' */ {0x27,0x45,0x45,0x45,0x39}, /* '5' */
    {0x3C,0x4A,0x49,0x49,0x30}, /* '6' */ {0x01,0x71,0x09,0x05,0x03}, /* '7' */
    {0x36,0x49,0x49,0x49,0x36}, /* '8' */ {0x06,0x49,0x49,0x29,0x1E}, /* '9' */
    {0x00,0x36,0x36,0x00,0x00}, /* ':' */ {0x00,0x56,0x36,0x00,0x00}, /* ';' */
    {0x00,0x08,0x14,0x22,0x41}, /* '<' */ {0x14,0x14,0x14,0x14,0x14}, /* '=' */
    {0x41,0x22,0x14,0x08,0x00}, /* '>' */ {0x02,0x01,0x51,0x09,0x06}, /* '?' */
    {0x32,0x49,0x79,0x41,0x3E}, /* '@' */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 'A' */ {0x7F,0x49,0x49,0x49,0x36}, /* 'B' */
    {0x3E,0x41,0x41,0x41,0x22}, /* 'C' */ {0x7F,0x41,0x41,0x22,0x1C}, /* 'D' */
    {0x7F,0x49,0x49,0x49,0x41}, /* 'E' */ {0x7F,0x09,0x09,0x09,0x01}, /* 'F' */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 'G' */ {0x7F,0x08,0x08,0x08,0x7F}, /* 'H' */
    {0x00,0x41,0x7F,0x41,0x00}, /* 'I' */ {0x20,0x40,0x41,0x3F,0x01}, /* 'J' */
    {0x7F,0x08,0x14,0x22,0x41}, /* 'K' */ {0x7F,0x40,0x40,0x40,0x40}, /* 'L' */
    {0x7F,0x02,0x04,0x02,0x7F}, /* 'M' */ {0x7F,0x04,0x08,0x10,0x7F}, /* 'N' */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 'O' */ {0x7F,0x09,0x09,0x09,0x06}, /* 'P' */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' */ {0x7F,0x09,0x19,0x29,0x46}, /* 'R' */
    {0x46,0x49,0x49,0x49,0x31}, /* 'S' */ {0x01,0x01,0x7F,0x01,0x01}, /* 'T' */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 'U' */ {0x1F,0x20,0x40,0x20,0x1F}, /* 'V' */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 'W' */ {0x63,0x14,0x08,0x14,0x63}, /* 'X' */
    {0x07,0x08,0x70,0x08,0x07}, /* 'Y' */ {0x61,0x51,0x49,0x45,0x43}, /* 'Z' */
    {0x00,0x7F,0x41,0x41,0x00}, /* '[' */ {0x02,0x04,0x08,0x10,0x20}, /* '\' */
    {0x00,0x41,0x41,0x7F,0x00}, /* ']' */ {0x04,0x02,0x01,0x02,0x04}, /* '^' */
    {0x40,0x40,0x40,0x40,0x40}, /* '_' */ {0x00,0x01,0x02,0x04,0x00}, /* '`' */
    {0x20,0x54,0x54,0x54,0x78}, /* 'a' */ {0x7F,0x48,0x44,0x44,0x38}, /* 'b' */
    {0x38,0x44,0x44,0x44,0x20}, /* 'c' */ {0x38,0x44,0x44,0x48,0x7F}, /* 'd' */
    {0x38,0x54,0x54,0x54,0x18}, /* 'e' */ {0x08,0x7E,0x09,0x01,0x02}, /* 'f' */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 'g' */ {0x7F,0x08,0x04,0x04,0x78}, /* 'h' */
    {0x00,0x44,0x7D,0x40,0x00}, /* 'i' */ {0x20,0x40,0x44,0x3D,0x00}, /* 'j' */
    {0x7F,0x10,0x28,0x44,0x00}, /* 'k' */ {0x00,0x41,0x7F,0x40,0x00}, /* 'l' */
    {0x7C,0x04,0x18,0x04,0x78}, /* 'm' */ {0x7C,0x08,0x04,0x04,0x78}, /* 'n' */
    {0x38,0x44,0x44,0x44,0x38}, /* 'o' */ {0x7C,0x14,0x14,0x14,0x08}, /* 'p' */
    {0x08,0x14,0x14,0x18,0x7C}, /* 'q' */ {0x7C,0x08,0x04,0x04,0x08}, /* 'r' */
    {0x48,0x54,0x54,0x54,0x20}, /* 's' */ {0x04,0x3F,0x44,0x40,0x20}, /* 't' */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 'u' */ {0x1C,0x20,0x40,0x20,0x1C}, /* 'v' */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 'w' */ {0x44,0x28,0x10,0x28,0x44}, /* 'x' */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 'y' */ {0x44,0x64,0x54,0x4C,0x44}, /* 'z' */
    {0x00,0x08,0x36,0x41,0x00}, /* '{' */ {0x00,0x00,0x7F,0x00,0x00}, /* '|' */
    {0x00,0x41,0x36,0x08,0x00}, /* '}' */ {0x08,0x08,0x2A,0x1C,0x08}, /* '~' */
    {0x08,0x1C,0x2A,0x08,0x08}, /* DEL */
};

/* ─── Drawing Primitives ──────────────────────────────────────────────────── */

/* draw_pixel_w: write one pixel at (x,y) with toroidal wrapping.
 * Coordinates outside [0,width) or [0,height) wrap to the other side.
 * This is called per-pixel from draw_line, so wireframe objects crossing the
 * screen edge render seamlessly — the same trick as olcConsoleGameEngine's
 * overridden Draw() method. */
static void draw_pixel_w(AsteroidsBackbuffer *bb, int x, int y, uint32_t color) {
    x = ((x % bb->width)  + bb->width)  % bb->width;
    y = ((y % bb->height) + bb->height) % bb->height;
    bb->pixels[y * bb->width + x] = color;
}

/* draw_line: Bresenham's line algorithm — draws a straight line of pixels.
 *
 * Bresenham avoids floating-point by tracking an integer error term.
 * At each step we advance along the major axis; when accumulated error
 * exceeds half a pixel, we step along the minor axis too.
 *
 * Coordinates can be out-of-bounds — draw_pixel_w wraps them toroidally.
 * This means a line from x=790 to x=810 (in a 800-wide screen) draws
 * pixels 790..799 on the right edge AND 0..10 on the left edge. */
static void draw_line(AsteroidsBackbuffer *bb,
                      int x0, int y0, int x1, int y1,
                      uint32_t color)
{
    int dx  =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy  = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy; /* accumulated error term */

    for (;;) {
        draw_pixel_w(bb, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* draw_rect: filled rectangle — used to clear the screen each frame. */
static void draw_rect(AsteroidsBackbuffer *bb,
                      int x, int y, int w, int h,
                      uint32_t color)
{
    int x1 = x + w, y1 = y + h;
    if (x  <  0)         x  = 0;
    if (y  <  0)         y  = 0;
    if (x1 > bb->width)  x1 = bb->width;
    if (y1 > bb->height) y1 = bb->height;
    for (int py = y; py < y1; py++)
        for (int px = x; px < x1; px++)
            bb->pixels[py * bb->width + px] = color;
}

/* draw_rect_blend: alpha-blended rectangle for overlays.
 * out = src * alpha/255 + dst * (255-alpha)/255                              */
static void draw_rect_blend(AsteroidsBackbuffer *bb,
                             int x, int y, int w, int h,
                             uint32_t color)
{
    uint8_t sa = (uint8_t)((color >> 24) & 0xFF); /* source alpha from A byte */
    uint8_t sr = (uint8_t)( color        & 0xFF);
    uint8_t sg = (uint8_t)((color >>  8) & 0xFF);
    uint8_t sb = (uint8_t)((color >> 16) & 0xFF);
    int x1 = x + w, y1 = y + h;
    if (x  <  0)         x  = 0;
    if (y  <  0)         y  = 0;
    if (x1 > bb->width)  x1 = bb->width;
    if (y1 > bb->height) y1 = bb->height;
    for (int py = y; py < y1; py++) {
        for (int px = x; px < x1; px++) {
            uint32_t dst = bb->pixels[py * bb->width + px];
            uint8_t dr = (uint8_t)( dst        & 0xFF);
            uint8_t dg = (uint8_t)((dst >>  8) & 0xFF);
            uint8_t db = (uint8_t)((dst >> 16) & 0xFF);
            uint8_t or_ = (uint8_t)((sr * sa + dr * (255 - sa)) / 255);
            uint8_t og  = (uint8_t)((sg * sa + dg * (255 - sa)) / 255);
            uint8_t ob  = (uint8_t)((sb * sa + db * (255 - sa)) / 255);
            bb->pixels[py * bb->width + px] = ASTEROIDS_RGB(or_, og, ob);
        }
    }
}

/* draw_char: render one ASCII character at (x,y) at given scale and color. */
static void draw_char(AsteroidsBackbuffer *bb, int x, int y,
                      char c, int scale, uint32_t color)
{
    if (c < 0x20 || c > 0x7F) return;
    const uint8_t *glyph = font_glyphs[(unsigned char)c - 0x20];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if (!(glyph[col] & (1 << row))) continue;
            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    int px = x + col * scale + sx;
                    int py = y + row * scale + sy;
                    if (px >= 0 && px < bb->width &&
                        py >= 0 && py < bb->height)
                        bb->pixels[py * bb->width + px] = color;
                }
            }
        }
    }
}

/* draw_text: render a null-terminated string. Advance x by (5+1)*scale per char. */
static void draw_text(AsteroidsBackbuffer *bb, int x, int y,
                      const char *s, int scale, uint32_t color)
{
    for (; *s; s++, x += (5 + 1) * scale)
        draw_char(bb, x, y, *s, scale, color);
}

/* ─── Wireframe Rendering ─────────────────────────────────────────────────── */

/* draw_wireframe: rotate + scale + translate a polygon model, then draw it.
 *
 * The 2D rotation matrix:
 *   x' = x*cos(r) - y*sin(r)
 *   y' = x*sin(r) + y*cos(r)
 *
 * This rotates a point (x,y) counterclockwise by r radians around the origin.
 * We compute cos/sin once before the loop (expensive trig only 2 calls/draw).
 * Then scale by s and translate by (ox, oy) to place the model in the world.  */
static void draw_wireframe(AsteroidsBackbuffer *bb,
                           const Vec2 *model, int n_verts,
                           float ox, float oy,       /* world position */
                           float angle, float scale, /* orientation + size */
                           uint32_t color)
{
    /* Transformed vertices: max 64 (ASTEROID_VERTS=20 is well below this) */
    Vec2 t[64];
    float c = cosf(angle), s = sinf(angle);

    for (int i = 0; i < n_verts; i++) {
        /* Step 1: rotate */
        float rx = model[i].x * c - model[i].y * s;
        float ry = model[i].x * s + model[i].y * c;
        /* Step 2: scale */
        rx *= scale;
        ry *= scale;
        /* Step 3: translate to world position */
        t[i].x = rx + ox;
        t[i].y = ry + oy;
    }

    /* Draw closed polygon: connect each vertex to the next, and last to first */
    for (int i = 0; i < n_verts; i++) {
        int j = (i + 1) % n_verts;
        draw_line(bb,
                  (int)t[i].x, (int)t[i].y,
                  (int)t[j].x, (int)t[j].y,
                  color);
    }
}

/* ─── Math Utilities ──────────────────────────────────────────────────────── */

/* wrap_coordinates: toroidal (wraparound) space.
 * Objects that exit the right side reappear on the left, etc.
 * This keeps all objects in bounds without any boundary collision logic.    */
static void wrap_coordinates(float *x, float *y) {
    if (*x <  0.0f)           *x += (float)SCREEN_W;
    if (*x >= (float)SCREEN_W) *x -= (float)SCREEN_W;
    if (*y <  0.0f)           *y += (float)SCREEN_H;
    if (*y >= (float)SCREEN_H) *y -= (float)SCREEN_H;
}

/* is_point_inside_circle: returns 1 if (px,py) is within radius r of (cx,cy).
 *
 * The naive formula: sqrt((px-cx)^2 + (py-cy)^2) < r
 * Optimisation: square both sides to eliminate sqrt (sqrt is slow).
 *   (px-cx)^2 + (py-cy)^2 < r^2
 *
 * Example: cx=100, cy=100, r=32, px=120, py=110
 *   dx=20, dy=10 → dx²+dy² = 400+100 = 500 < 32²=1024 → inside ✓  */
static int is_point_inside_circle(float cx, float cy, float r,
                                  float px, float py)
{
    float dx = px - cx, dy = py - cy;
    return (dx * dx + dy * dy) < (r * r);
}

/* ─── Entity Pool Management ──────────────────────────────────────────────── */

/* compact_pool: remove all inactive objects from an array in O(n).
 *
 * Strategy: scan forward; when we find an inactive slot, overwrite it with
 * the last active entry and decrement count. This avoids shifting the array.
 *
 * Before: [ A | A | X | A | A ]  count=5, X=inactive
 * Step 1: slot[2] is inactive → copy slot[4] here, count=4
 * After:  [ A | A | A | A | . ]  count=4  (slot[4] is now unused)
 *
 * This is O(n) worst case, O(1) amortized per removal. No memory allocation. */
static void compact_pool(SpaceObject *pool, int *count) {
    int i = 0;
    while (i < *count) {
        if (!pool[i].active) {
            pool[i] = pool[*count - 1]; /* overwrite with last */
            (*count)--;
        } else {
            i++;
        }
    }
}

/* ─── Pool Helpers ────────────────────────────────────────────────────────── */

static void add_asteroid(GameState *state,
                         float x, float y,
                         float dx, float dy,
                         int size)
{
    if (state->asteroid_count >= MAX_ASTEROIDS) return;
    SpaceObject *a = &state->asteroids[state->asteroid_count++];
    a->x      = x;
    a->y      = y;
    a->dx     = dx;
    a->dy     = dy;
    a->angle  = 0.0f;
    a->timer  = 0.0f;
    a->size   = size;
    a->active = 1;
}

static void add_bullet(GameState *state, float x, float y,
                       float angle)
{
    if (state->bullet_count >= MAX_BULLETS) return;
    SpaceObject *b = &state->bullets[state->bullet_count++];
    b->x      = x;
    b->y      = y;
    b->dx     = BULLET_SPEED * sinf(angle);
    b->dy     = -BULLET_SPEED * cosf(angle); /* -cos: angle 0 = up (y-down screen) */
    b->angle  = angle;
    b->timer  = BULLET_LIFE;
    b->size   = 0;
    b->active = 1;
}

/* ─── Game Lifecycle ──────────────────────────────────────────────────────── */

/* reset_game: restart from a clean state — keeps the wireframe models intact.
 * Called from asteroids_init (first run) and by asteroids_update when the
 * death timer expires.                                                        */
static void reset_game(GameState *state) {
    /* Player: center screen, pointing up, no velocity */
    state->player.x      = SCREEN_W / 2.0f;
    state->player.y      = SCREEN_H / 2.0f;
    state->player.dx     = 0.0f;
    state->player.dy     = 0.0f;
    state->player.angle  = 0.0f;
    state->player.timer  = 0.0f;
    state->player.size   = 0;
    state->player.active = 1;

    /* Clear pools */
    state->asteroid_count = 0;
    state->bullet_count   = 0;

    /* Two large asteroids at corners, moving toward the center area */
    add_asteroid(state, 100.0f, 100.0f,  40.0f, -30.0f, LARGE_SIZE);
    add_asteroid(state, 500.0f, 100.0f, -25.0f,  15.0f, LARGE_SIZE);

    state->score     = 0;
    state->phase     = PHASE_PLAYING;
    state->dead_timer = 0.0f;
}

/* asteroids_init: build models + full init. Call once from main(). */
void asteroids_init(GameState *state) {
    /* Seed RNG once at startup. srand is called here (via main's call to
     * asteroids_init), NOT in reset_game — resetting never re-seeds. */
    srand((unsigned int)time(NULL));

    /* Ship model: isoceles triangle pointing up (y-down screen coords).
     * Vertex 0: tip (nose, pointing toward negative y = up on screen)
     * Vertex 1: bottom-left
     * Vertex 2: bottom-right
     * Scaled by SHIP_SCALE=10 when drawn → triangle ~50px tall.             */
    state->ship_model[0] = (Vec2){  0.0f, -5.0f };
    state->ship_model[1] = (Vec2){ -2.5f, +2.5f };
    state->ship_model[2] = (Vec2){ +2.5f, +2.5f };

    /* Asteroid model: jagged circle — 20 vertices evenly spaced around a
     * unit circle (radius ~1), each displaced outward by random noise.
     * noise ∈ [0.8, 1.2] → vertices jitter ±20% from perfect circle.
     * Scaled by size (16–64 pixels) when drawn.                             */
    for (int i = 0; i < ASTEROID_VERTS; i++) {
        float noise = (float)rand() / (float)RAND_MAX * 0.4f + 0.8f;
        float t     = ((float)i / (float)ASTEROID_VERTS) * 2.0f * 3.14159265f;
        state->asteroid_model[i].x = noise * sinf(t);
        state->asteroid_model[i].y = noise * cosf(t);
    }

    reset_game(state);
}

/* prepare_input_frame: reset half_transition_count before polling events.
 * Must be called at the start of every frame BEFORE platform_get_input.
 * Does NOT reset ended_down — the "held" state persists across frames.       */
void prepare_input_frame(GameInput *input) {
    input->left.half_transition_count  = 0;
    input->right.half_transition_count = 0;
    input->up.half_transition_count    = 0;
    input->fire.half_transition_count  = 0;
    input->quit                        = 0;
}

/* ─── asteroids_update ────────────────────────────────────────────────────── */
void asteroids_update(GameState *state, const GameInput *input, float dt) {
    int i, j;

    /* ── Dead phase: drain timer, then reset ──────────────────────────────── */
    if (state->phase == PHASE_DEAD) {
        state->dead_timer -= dt;
        if (state->dead_timer <= 0.0f) {
            reset_game(state);
        }
        return; /* freeze all game logic during death animation */
    }

    /* ── Rotate ship ──────────────────────────────────────────────────────── */
    if (input->left.ended_down)
        state->player.angle -= ROTATE_SPEED * dt;
    if (input->right.ended_down)
        state->player.angle += ROTATE_SPEED * dt;

    /* ── Thrust: acceleration → velocity (Euler integration) ─────────────── */
    if (input->up.ended_down) {
        /* sin/cos of heading angle gives thrust direction vector.
         * +sin = rightward component, -cos = upward component (y-down screen).
         * Multiply by accel * dt to get velocity change this frame.          */
        state->player.dx += sinf(state->player.angle) * THRUST_ACCEL * dt;
        state->player.dy += -cosf(state->player.angle) * THRUST_ACCEL * dt;
    }

    /* ── Velocity → position (Euler integration) ─────────────────────────── */
    state->player.x += state->player.dx * dt;
    state->player.y += state->player.dy * dt;
    wrap_coordinates(&state->player.x, &state->player.y);

    /* ── Fire bullet (just-pressed) ───────────────────────────────────────── */
    /* "Just pressed" = ended_down AND at least one transition this frame.
     * This fires once per press; holding space does NOT rapid-fire.          */
    if (input->fire.ended_down && input->fire.half_transition_count > 0) {
        /* Spawn bullet from the ship's nose (tip of model, in world space) */
        float nose_x = state->player.x + sinf(state->player.angle) * SHIP_SCALE * 5.0f;
        float nose_y = state->player.y - cosf(state->player.angle) * SHIP_SCALE * 5.0f;
        add_bullet(state, nose_x, nose_y, state->player.angle);
    }

    /* ── Update bullets ───────────────────────────────────────────────────── */
    for (i = 0; i < state->bullet_count; i++) {
        SpaceObject *b = &state->bullets[i];
        b->x     += b->dx * dt;
        b->y     += b->dy * dt;
        b->timer -= dt;
        wrap_coordinates(&b->x, &b->y);

        /* Expire bullet when its lifetime runs out */
        if (b->timer <= 0.0f)
            b->active = 0;
    }
    compact_pool(state->bullets, &state->bullet_count);

    /* ── Update asteroids (position + rotation) ───────────────────────────── */
    for (i = 0; i < state->asteroid_count; i++) {
        SpaceObject *a = &state->asteroids[i];
        a->x     += a->dx * dt;
        a->y     += a->dy * dt;
        a->angle += 0.5f * dt; /* slow visual rotation; doesn't affect collision */
        wrap_coordinates(&a->x, &a->y);
    }

    /* ── Bullet ↔ Asteroid collision ──────────────────────────────────────── */
    /* Iterate all active bullets against all active asteroids.
     * We cannot remove objects during iteration — doing so shifts indices.
     * Solution: mark inactive with active=0, compact AFTER both loops.      */
    for (i = 0; i < state->bullet_count; i++) {
        SpaceObject *b = &state->bullets[i];
        if (!b->active) continue;

        for (j = 0; j < state->asteroid_count; j++) {
            SpaceObject *a = &state->asteroids[j];
            if (!a->active) continue;

            if (is_point_inside_circle(a->x, a->y, (float)a->size, b->x, b->y)) {
                /* Hit! Remove bullet */
                b->active = 0;

                /* Split asteroid if large enough */
                if (a->size > SMALL_SIZE) {
                    int   new_size  = a->size / 2;
                    float new_speed = 60.0f;
                    float angle1 = ((float)rand() / (float)RAND_MAX) * 2.0f * 3.14159265f;
                    float angle2 = ((float)rand() / (float)RAND_MAX) * 2.0f * 3.14159265f;
                    add_asteroid(state,
                                 a->x, a->y,
                                 new_speed * sinf(angle1),
                                 new_speed * cosf(angle1),
                                 new_size);
                    add_asteroid(state,
                                 a->x, a->y,
                                 new_speed * sinf(angle2),
                                 new_speed * cosf(angle2),
                                 new_size);
                }

                /* Remove asteroid */
                a->active = 0;
                state->score += (LARGE_SIZE / a->size) * 100; /* bigger = fewer points */
                break; /* one bullet can only hit one asteroid */
            }
        }
    }
    compact_pool(state->bullets,   &state->bullet_count);
    compact_pool(state->asteroids, &state->asteroid_count);

    /* ── Player ↔ Asteroid collision ─────────────────────────────────────── */
    for (i = 0; i < state->asteroid_count; i++) {
        SpaceObject *a = &state->asteroids[i];
        if (is_point_inside_circle(a->x, a->y, (float)a->size,
                                   state->player.x, state->player.y)) {
            state->phase     = PHASE_DEAD;
            state->dead_timer = DEAD_ANIM_TIME;
            return;
        }
    }

    /* ── Level clear: all asteroids destroyed ────────────────────────────── */
    if (state->asteroid_count == 0) {
        state->score += 1000; /* bonus for clearing the wave */

        /* Spawn two large asteroids 90° left and right of the player's heading.
         * Placed 150px away so they don't spawn on top of the player.        */
        float pa   = state->player.angle;
        float px   = state->player.x;
        float py   = state->player.y;
        float left_x  = 150.0f * sinf(pa - 3.14159265f / 2.0f) + px;
        float left_y  = 150.0f * cosf(pa - 3.14159265f / 2.0f) + py;
        float right_x = 150.0f * sinf(pa + 3.14159265f / 2.0f) + px;
        float right_y = 150.0f * cosf(pa + 3.14159265f / 2.0f) + py;
        add_asteroid(state, left_x,  left_y,
                     50.0f * sinf(pa),  50.0f * cosf(pa),  LARGE_SIZE);
        add_asteroid(state, right_x, right_y,
                     50.0f * sinf(-pa), 50.0f * cosf(-pa), LARGE_SIZE);
    }
}

/* ─── asteroids_render ────────────────────────────────────────────────────── */
void asteroids_render(const GameState *state, AsteroidsBackbuffer *bb) {
    int i;
    char buf[32];

    /* 1. Clear to black */
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BLACK);

    /* 2. Draw asteroids (yellow wireframe circles) */
    for (i = 0; i < state->asteroid_count; i++) {
        const SpaceObject *a = &state->asteroids[i];
        if (!a->active) continue;
        draw_wireframe(bb,
                       state->asteroid_model, ASTEROID_VERTS,
                       a->x, a->y, a->angle, (float)a->size,
                       COLOR_YELLOW);
    }

    /* 3. Draw bullets (single white pixels) */
    for (i = 0; i < state->bullet_count; i++) {
        const SpaceObject *b = &state->bullets[i];
        if (!b->active) continue;
        draw_pixel_w(bb, (int)b->x, (int)b->y, COLOR_WHITE);
    }

    /* 4. Draw ship (only when not in death flash) */
    if (state->phase == PHASE_PLAYING) {
        draw_wireframe(bb,
                       state->ship_model, SHIP_VERTS,
                       state->player.x, state->player.y,
                       state->player.angle, SHIP_SCALE,
                       COLOR_WHITE);
    } else {
        /* Death flash: blink at 10Hz using dead_timer */
        int flash = (int)(state->dead_timer / 0.05f);
        if (flash % 2 == 0) {
            draw_wireframe(bb,
                           state->ship_model, SHIP_VERTS,
                           state->player.x, state->player.y,
                           state->player.angle, SHIP_SCALE,
                           COLOR_RED);
        }
    }

    /* 5. HUD: score */
    snprintf(buf, sizeof(buf), "SCORE: %d", state->score);
    draw_text(bb, 8, 8, buf, 2, COLOR_WHITE);

    /* 6. "GAME OVER" overlay during death animation */
    if (state->phase == PHASE_DEAD) {
        draw_rect_blend(bb, 0, 0, bb->width, bb->height,
                        ASTEROIDS_RGBA(160, 0, 0, 80));
        draw_text(bb,
                  bb->width / 2 - 42,
                  bb->height / 2 - 7,
                  "GAME OVER", 2, COLOR_WHITE);
    }
}
