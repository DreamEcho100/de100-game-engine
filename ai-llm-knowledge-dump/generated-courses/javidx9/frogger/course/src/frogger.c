/* =============================================================================
   frogger.c — Game Logic + Rendering
   =============================================================================

   All game logic AND rendering live here — no X11, no Raylib, no OS calls.
   Reads from GameInput, writes to GameState (tick) or FroggerBackbuffer (render).

   DOD NOTE: Lane data is split into two separate arrays:
     lane_speeds[]      — just the floats, 10 × 4 = 40 bytes (one cache line)
     lane_patterns[][]  — just the char grids, touched only during rendering
   ============================================================================= */

#define _POSIX_C_SOURCE 200809L

#include "frogger.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Bitmap font ─────────────────────────────────────────────────────────────
   5 columns × 7 rows per glyph, column-major, 96 printable ASCII chars.
   Index = character - 32 (space = index 0).
   Each byte is one column; bit N = row N (bit 0 = top row).            */
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

/* ─── Lane data (DOD layout) ──────────────────────────────────────────────────
   lane_speeds[]      — only the floats; 40 bytes, one cache line.
   lane_patterns[][]  — only the chars; touched only during rendering.

   Lane map (top = row 0):
     0  home row  (wall + homes, stationary)
     1  river     (logs + water, moves LEFT  at 3)
     2  river     (logs + water, moves RIGHT at 3)
     3  river     (logs + water, moves RIGHT at 2)
     4  pavement  (safe middle strip, stationary)
     5  road      (bus,  moves LEFT  at 3)
     6  road      (car2, moves RIGHT at 3)
     7  road      (car1, moves LEFT  at 4)
     8  road      (car2, moves RIGHT at 2)
     9  pavement  (safe start row, stationary)                          */

static const float lane_speeds[NUM_LANES] = {
     0.0f,  /* 0 home row    */
    -3.0f,  /* 1 river left  */
     3.0f,  /* 2 river right */
     2.0f,  /* 3 river right */
     0.0f,  /* 4 pavement    */
    -3.0f,  /* 5 road left   */
     3.0f,  /* 6 road right  */
    -4.0f,  /* 7 road left   */
     2.0f,  /* 8 road right  */
     0.0f,  /* 9 pavement    */
};

/* Each row is exactly LANE_PATTERN_LEN (64) characters.
   Tile characters:
     w = wall (deadly)         h = home (safe goal)
     , = water (deadly)        j/l/k = log start/mid/end (safe)
     p = pavement (safe)
     . = road (safe ground)    a/s/d/f = bus segments (deadly)
     z/x = car1 back/front     t/y = car2 back/front  (all deadly)  */
static const char lane_patterns[NUM_LANES][LANE_PATTERN_LEN + 1] = {
    "wwwhhwwwhhwwwhhwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww",
    ",,,jllk,,jllllk,,,,,,,jllk,,,,,jk,,,jlllk,,,,jllllk,,,,jlllk,,,,",
    ",,,,jllk,,,,,jllk,,,,jllk,,,,,,,,,jllk,,,,,jk,,,,,,jllllk,,,,,,,",
    ",,jlk,,,,,jlk,,,,,jk,,,,,jlk,,,jlk,,,,jk,,,,jllk,,,,jk,,,,,,jk,,",
    "pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp",
    "....asdf.......asdf....asdf..........asdf........asdf....asdf....",
    ".....ty..ty....ty....ty.....ty........ty..ty.ty......ty.......ty.",
    "..zx.....zx.........zx..zx........zx...zx...zx....zx...zx...zx..",
    "..ty.....ty.......ty.....ty......ty..ty.ty.......ty....ty........",
    "pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp",
};

/* Returns 1 if the tile character is safe to stand on, 0 if deadly. */
static int tile_is_safe(char c) {
    return (c == '.' || c == 'j' || c == 'l' || c == 'k' ||
            c == 'p' || c == 'h');
}

static int tile_to_sprite(char c, int *src_x) {
    *src_x = 0;
    switch (c) {
        case 'w': return SPR_WALL;
        case 'h': return SPR_HOME;
        case ',': return SPR_WATER;
        case 'p': return SPR_PAVEMENT;
        case 'j': *src_x =  0; return SPR_LOG;
        case 'l': *src_x =  8; return SPR_LOG;
        case 'k': *src_x = 16; return SPR_LOG;
        case 'z': *src_x =  0; return SPR_CAR1;
        case 'x': *src_x =  8; return SPR_CAR1;
        case 't': *src_x =  0; return SPR_CAR2;
        case 'y': *src_x =  8; return SPR_CAR2;
        case 'a': *src_x =  0; return SPR_BUS;
        case 's': *src_x =  8; return SPR_BUS;
        case 'd': *src_x = 16; return SPR_BUS;
        case 'f': *src_x = 24; return SPR_BUS;
        default:  return -1;
    }
}

/* ─── Sprite loader ───────────────────────────────────────────────────────────
   .spr binary layout:
     int32 width; int32 height;
     int16 colors[w*h];   (FG in low nibble, BG in high nibble)
     int16 glyphs[w*h];   (0x2588 = solid block, 0x0020 = transparent)   */
static int sprite_load(SpriteBank *bank, int spr_id, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "FATAL: cannot open sprite '%s'\n", path);
        return 0;
    }
    int32_t w, h;
    fread(&w, sizeof(int32_t), 1, f);
    fread(&h, sizeof(int32_t), 1, f);
    int offset = bank->offsets[spr_id];
    int count  = w * h;
    if (offset + count > SPR_POOL_CELLS) {
        fprintf(stderr, "FATAL: sprite pool overflow for '%s'\n", path);
        fclose(f);
        return 0;
    }
    fread(bank->colors + offset, sizeof(int16_t), count, f);
    fread(bank->glyphs + offset, sizeof(int16_t), count, f);
    bank->widths [spr_id] = (int)w;
    bank->heights[spr_id] = (int)h;
    fclose(f);
    return 1;
}

/* ─── Drawing primitives ──────────────────────────────────────────────────────*/

static void draw_rect(FroggerBackbuffer *bb, int x, int y, int w, int h,
                      uint32_t color) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w > bb->width)  ? bb->width  : x + w;
    int y1 = (y + h > bb->height) ? bb->height : y + h;
    int px, py;
    for (py = y0; py < y1; py++)
        for (px = x0; px < x1; px++)
            bb->pixels[py * bb->width + px] = color;
}

static void draw_rect_blend(FroggerBackbuffer *bb, int x, int y, int w, int h,
                             uint32_t color) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w > bb->width)  ? bb->width  : x + w;
    int y1 = (y + h > bb->height) ? bb->height : y + h;
    uint8_t sa = (uint8_t)((color >> 24) & 0xFF);
    uint8_t sr = (uint8_t)((color >> 16) & 0xFF);
    uint8_t sg = (uint8_t)((color >>  8) & 0xFF);
    uint8_t sb = (uint8_t)( color        & 0xFF);
    int px, py;
    for (py = y0; py < y1; py++) {
        for (px = x0; px < x1; px++) {
            uint32_t dst = bb->pixels[py * bb->width + px];
            uint8_t dr = (uint8_t)((dst >> 16) & 0xFF);
            uint8_t dg = (uint8_t)((dst >>  8) & 0xFF);
            uint8_t db = (uint8_t)( dst         & 0xFF);
            uint8_t or_ = (uint8_t)((sr * sa + dr * (255 - sa)) / 255);
            uint8_t og  = (uint8_t)((sg * sa + dg * (255 - sa)) / 255);
            uint8_t ob  = (uint8_t)((sb * sa + db * (255 - sa)) / 255);
            bb->pixels[py * bb->width + px] = FROGGER_RGB(or_, og, ob);
        }
    }
}

static void draw_char(FroggerBackbuffer *bb, int x, int y, char c,
                      int scale, uint32_t color) {
    if (c < 32 || c > 127) return;
    const uint8_t *glyph = font_glyphs[(int)c - 32];
    int col, row;
    for (col = 0; col < 5; col++)
        for (row = 0; row < 7; row++)
            if (glyph[col] & (1 << row))
                draw_rect(bb, x + col * scale, y + row * scale,
                          scale, scale, color);
}

static void draw_text(FroggerBackbuffer *bb, int x, int y, const char *text,
                      int scale, uint32_t color) {
    while (*text) {
        draw_char(bb, x, y, *text, scale, color);
        x += (5 + 1) * scale;
        text++;
    }
}

/* Draw a sub-region of a sprite into the backbuffer.
   src_x, src_y, src_w, src_h: source region in sprite cells.
   dest_px_x, dest_px_y: destination in pixels (backbuffer coords).     */
static void draw_sprite_partial(
    FroggerBackbuffer *bb,
    const SpriteBank *bank, int spr_id,
    int src_x, int src_y, int src_w, int src_h,
    int dest_px_x, int dest_px_y)
{
    int sheet_w = bank->widths[spr_id];
    int offset  = bank->offsets[spr_id];
    int sy, sx, py, px;

    for (sy = 0; sy < src_h; sy++) {
        for (sx = 0; sx < src_w; sx++) {
            int idx    = offset + (src_y + sy) * sheet_w + (src_x + sx);
            int16_t c  = bank->colors[idx];
            int16_t gl = bank->glyphs[idx];
            if (gl == 0x0020) continue;  /* transparent */

            int ci = c & 0x0F;
            uint32_t color = FROGGER_RGB(
                CONSOLE_PALETTE[ci][0],
                CONSOLE_PALETTE[ci][1],
                CONSOLE_PALETTE[ci][2]);

            /* Fill CELL_PX × CELL_PX pixel block */
            for (py = 0; py < CELL_PX; py++) {
                for (px = 0; px < CELL_PX; px++) {
                    int bx = dest_px_x + sx * CELL_PX + px;
                    int by = dest_px_y + sy * CELL_PX + py;
                    if (bx >= 0 && bx < bb->width &&
                        by >= 0 && by < bb->height)
                        bb->pixels[by * bb->width + bx] = color;
                }
            }
        }
    }
}

/* ─── prepare_input_frame ─────────────────────────────────────────────────────*/
void prepare_input_frame(GameInput *input) {
    input->move_up.half_transition_count    = 0;
    input->move_down.half_transition_count  = 0;
    input->move_left.half_transition_count  = 0;
    input->move_right.half_transition_count = 0;
    input->quit = 0;
}

/* ─── frogger_init ────────────────────────────────────────────────────────────*/
void frogger_init(GameState *state, const char *assets_dir) {
    memset(state, 0, sizeof(GameState));
    state->frog_x = 8.0f;
    state->frog_y = 9.0f;

    static const int spr_cells[NUM_SPRITES] = {
        /* frog */ 8*8,  /* water */ 8*8,  /* pavement */ 8*8,
        /* wall */ 8*8,  /* home  */ 8*8,  /* log  */ 24*8,
        /* car1 */ 16*8, /* car2  */ 16*8, /* bus  */ 32*8,
    };
    int i, offset = 0;
    for (i = 0; i < NUM_SPRITES; i++) {
        state->sprites.offsets[i] = offset;
        offset += spr_cells[i];
    }

    char path[256];
    static const struct { int id; const char *filename; } spr_files[NUM_SPRITES] = {
        {SPR_FROG,     "frog.spr"},
        {SPR_WATER,    "water.spr"},
        {SPR_PAVEMENT, "pavement.spr"},
        {SPR_WALL,     "wall.spr"},
        {SPR_HOME,     "home.spr"},
        {SPR_LOG,      "log.spr"},
        {SPR_CAR1,     "car1.spr"},
        {SPR_CAR2,     "car2.spr"},
        {SPR_BUS,      "bus.spr"},
    };
    for (i = 0; i < NUM_SPRITES; i++) {
        snprintf(path, sizeof(path), "%s/%s", assets_dir, spr_files[i].filename);
        if (!sprite_load(&state->sprites, spr_files[i].id, path)) {
            fprintf(stderr, "Failed to load: %s\n", path);
            exit(1);
        }
    }
}

/* ─── frogger_tick ────────────────────────────────────────────────────────────
   Pure game logic — no drawing, no platform calls.

   Steps:
     1. Cap dt; accumulate time
     2. Handle death flash timer
     3. Input: move frog one tile per just-pressed button
     4. River: push frog sideways on log rows
     5. Rebuild danger[] buffer
     6. Collision-check frog center cell
     7. Win detection                                                      */
void frogger_tick(GameState *state, const GameInput *input, float dt) {
    if (dt > 0.1f) dt = 0.1f;
    state->time += dt;

    /* Death flash */
    if (state->dead) {
        state->dead_timer -= dt;
        if (state->dead_timer <= 0.0f) {
            state->frog_x  = 8.0f;
            state->frog_y  = 9.0f;
            state->dead    = 0;
        }
        return;
    }

    /* Input: one hop per just-pressed ("ended_down AND transition this frame") */
    if (input->move_up.ended_down    && input->move_up.half_transition_count    > 0)
        state->frog_y -= 1.0f;
    if (input->move_down.ended_down  && input->move_down.half_transition_count  > 0)
        state->frog_y += 1.0f;
    if (input->move_left.ended_down  && input->move_left.half_transition_count  > 0)
        state->frog_x -= 1.0f;
    if (input->move_right.ended_down && input->move_right.half_transition_count > 0)
        state->frog_x += 1.0f;

    /* River: push frog sideways on log rows (0–3) */
    int fy_int = (int)state->frog_y;
    if (fy_int >= 0 && fy_int <= 3)
        state->frog_x -= lane_speeds[fy_int] * dt;

    /* Rebuild danger buffer */
    memset(state->danger, 1, SCREEN_CELLS_W * SCREEN_CELLS_H);

    int y, i, dy, cx, cy;
    for (y = 0; y < NUM_LANES; y++) {
        int tile_start, px_offset;
        lane_scroll(state->time, lane_speeds[y], &tile_start, &px_offset);
        int cell_offset = px_offset / CELL_PX;

        for (i = 0; i < LANE_WIDTH; i++) {
            char c    = lane_patterns[y][(tile_start + i) % LANE_PATTERN_LEN];
            int  safe = tile_is_safe(c);
            int  cx_start = (-1 + i) * TILE_CELLS - cell_offset;
            for (dy = 0; dy < TILE_CELLS; dy++) {
                int row = y * TILE_CELLS + dy;
                for (cx = cx_start; cx < cx_start + TILE_CELLS; cx++) {
                    if (cx >= 0 && cx < SCREEN_CELLS_W)
                        state->danger[row * SCREEN_CELLS_W + cx] = (uint8_t)(!safe);
                }
            }
        }
    }

    /* Collision: center-cell check (immune to log-edge cell flickering) */
    int frog_px_x = (int)(state->frog_x * (float)TILE_PX);
    int frog_px_y = (int)(state->frog_y * (float)TILE_PX);

    if (frog_px_x < 0 || frog_px_y < 0 ||
        frog_px_x + TILE_PX > SCREEN_PX_W ||
        frog_px_y + TILE_PX > SCREEN_PX_H) {
        state->dead       = 1;
        state->dead_timer = 0.4f;
        return;
    }

    cx = (frog_px_x + TILE_PX / 2) / CELL_PX;
    cy = (frog_px_y + TILE_PX / 2) / CELL_PX;

    if (state->danger[cy * SCREEN_CELLS_W + cx]) {
        state->dead       = 1;
        state->dead_timer = 0.4f;
        return;
    }

    /* Win detection */
    if (fy_int == 0) {
        int pattern_pos = (int)(state->frog_x + 0.5f) % LANE_PATTERN_LEN;
        if (pattern_pos < 0) pattern_pos += LANE_PATTERN_LEN;
        char c = lane_patterns[0][pattern_pos % LANE_PATTERN_LEN];
        if (c == 'h') {
            state->homes_reached++;
            state->frog_x = 8.0f;
            state->frog_y = 9.0f;
        }
    }
}

/* ─── frogger_render ──────────────────────────────────────────────────────────
   Writes the complete frame into bb->pixels.
   Rendering order (painter's algorithm — back to front):
     1. Clear to black
     2. Draw all lanes (background + sprites)
     3. Draw frog (with flash on death)
     4. HUD text (homes count, DEAD! overlay, YOU WIN! overlay)            */
void frogger_render(const GameState *state, FroggerBackbuffer *bb) {
    int y, i;
    char buf[64];

    /* 1. Clear */
    draw_rect(bb, 0, 0, bb->width, bb->height, FROGGER_RGB(0, 0, 0));

    /* 2. Draw lanes */
    for (y = 0; y < NUM_LANES; y++) {
        int tile_start, px_offset;
        lane_scroll(state->time, lane_speeds[y], &tile_start, &px_offset);
        int dest_py = y * TILE_PX;

        for (i = 0; i < LANE_WIDTH; i++) {
            char c  = lane_patterns[y][(tile_start + i) % LANE_PATTERN_LEN];
            int src_x = 0;
            int spr   = tile_to_sprite(c, &src_x);
            int dest_px = (-1 + i) * TILE_PX - px_offset;

            if (spr >= 0) {
                draw_sprite_partial(bb, &state->sprites, spr,
                    src_x, 0, TILE_CELLS, TILE_CELLS,
                    dest_px, dest_py);
            }
            /* '.' (road) stays black — already cleared */
        }
    }

    /* 3. Draw frog (with death flash) */
    int show_frog = 1;
    if (state->dead) {
        int flash = (int)(state->dead_timer / 0.05f);
        show_frog = (flash % 2 == 0);
    }
    if (show_frog) {
        int frog_px = (int)(state->frog_x * (float)TILE_PX);
        int frog_py = (int)(state->frog_y * (float)TILE_PX);
        draw_sprite_partial(bb, &state->sprites, SPR_FROG,
            0, 0,
            state->sprites.widths [SPR_FROG],
            state->sprites.heights[SPR_FROG],
            frog_px, frog_py);
    }

    /* 4. HUD */
    snprintf(buf, sizeof(buf), "HOMES: %d", state->homes_reached);
    draw_text(bb, 8, 8, buf, 2, FROGGER_RGB(255, 255, 255));

    if (state->dead) {
        draw_rect_blend(bb, 0, 0, bb->width, bb->height,
                        FROGGER_RGBA(0, 0, 0, 160));
        draw_text(bb, bb->width / 2 - 2 * 12, bb->height / 2 - 14,
                  "DEAD!", 2, FROGGER_RGB(255, 80, 80));
    }

    if (state->homes_reached >= 3) {
        draw_rect_blend(bb, 0, 0, bb->width, bb->height,
                        FROGGER_RGBA(0, 0, 0, 160));
        const char *win = "YOU WIN!";
        int cw = (5 + 1) * 3;
        int tx = bb->width / 2 - (int)(strlen(win)) * cw / 2;
        draw_text(bb, tx, bb->height / 2 - 14, win, 3,
                  FROGGER_RGB(255, 255, 0));
    }
}
