#ifndef TETRIS_H
#define TETRIS_H

/* ============================================================
 * tetris.h — Core game types and constants
 *
 * This is the ONLY header both platform backends include.
 * NO OS-specific headers live here. Pure game data and logic API.
 *
 * Port of OneLoneCoder Tetris (javidx9) from C++/Windows to C/Linux.
 * ============================================================ */

#define FIELD_WIDTH  12
#define FIELD_HEIGHT 18
#define CELL_SIZE    30   /* pixels per cell in the rendered window */

/* ─── Field cell encoding ──────────────────────────────────────
 *
 *  0   = empty space
 *  1-7 = locked piece (stored as piece_index + 1)
 *  8   = completed line (flash state)
 *  9   = wall / boundary
 *
 *  Original display string: " ABCDEFG=#"
 *                             0123456789
 *
 * Why +1 for piece index?  So that 0 unambiguously means "empty".
 * ─────────────────────────────────────────────────────────── */

/* ─── Tetromino shape strings ──────────────────────────────────
 *
 *  Each tetromino is a 16-character string representing a 4x4 grid.
 *  'X' = solid cell, '.' = empty cell.
 *  Row-major layout: index = row*4 + col
 *
 *  Visual for I-piece "..X...X...X...X.":
 *
 *    col: 0 1 2 3
 *  row 0: . . X .   indices  0  1  2  3
 *  row 1: . . X .            4  5  6  7
 *  row 2: . . X .            8  9 10 11
 *  row 3: . . X .           12 13 14 15
 *
 * ─────────────────────────────────────────────────────────── */
extern const char *TETROMINOES[7];

/* ─── Input ────────────────────────────────────────────────────
 *
 *  Platform layer fills this struct each tick.
 *  Each field is 1 if the action fires this tick, else 0.
 *  'rotate' fires exactly ONCE per key press (no auto-repeat).
 * ─────────────────────────────────────────────────────────── */
typedef struct {
    int move_left;
    int move_right;
    int move_down;
    int rotate;   /* single-shot: fires once per physical key press */
    int restart;
    int quit;
} PlatformInput;

/* ─── Game State ───────────────────────────────────────────────
 *
 *  All mutable game data in a single struct.
 *  The platform layer reads this for rendering (read-only).
 *  tetris_tick() is the only function that writes to it.
 *
 *  Memory layout: ~230 bytes, fits in a few cache lines.
 * ─────────────────────────────────────────────────────────── */
typedef struct {
    unsigned char field[FIELD_WIDTH * FIELD_HEIGHT]; /* 216 bytes */

    int current_piece;      /* index 0-6 into TETROMINOES         */
    int current_rotation;   /* 0=0°, 1=90°, 2=180°, 3=270°       */
    int current_x;          /* column of piece's top-left corner  */
    int current_y;          /* row    of piece's top-left corner  */
    int next_piece;         /* preview panel: upcoming piece index */

    int speed;              /* ticks between forced drops (20→10) */
    int speed_count;        /* current tick accumulator           */
    int piece_count;        /* total pieces locked so far         */

    int score;

    int lines[4];           /* row indices of lines just completed */
    int line_count;         /* valid entries in lines[]            */
    int flash_timer;        /* ticks remaining in flash animation  */

    int game_over;
} GameState;

/* ─── Public API ───────────────────────────────────────────────*/

/* Initialize (or reset) a GameState. Call once before the first tick
 * and again on restart. Sets up boundary walls and spawns first piece. */
void tetris_init(GameState *state);

/* Advance game by one tick (called every 50ms by the platform layer).
 * Processes input and updates state. Does NOT render anything. */
void tetris_tick(GameState *state, const PlatformInput *input);

/* Rotate a (px, py) position in a 4x4 grid by r*90 degrees clockwise.
 * Returns the flat index into the 16-char tetromino string. */
int tetris_rotate(int px, int py, int r);

/* Returns 1 if the piece fits at (pos_x, pos_y) with given rotation.
 * Returns 0 if any solid cell overlaps a non-zero field cell. */
int tetris_does_piece_fit(const GameState *state, int piece, int rotation,
                          int pos_x, int pos_y);

#endif /* TETRIS_H */
