# Lesson 05 — GameState with CurrentPiece Substruct; game_init()

## By the end of this lesson you will have:

A `GameState` struct organized into logical substructs — the current piece, completed lines, and timing — plus a `game_init()` function that initializes everything and configures input repeat intervals.

---

## From Flat Fields to Grouped Substructs

The original `GameState` was a flat struct:

```c
typedef struct {
    unsigned char field[FIELD_WIDTH * FIELD_HEIGHT];
    int current_piece;
    int current_rotation;
    int current_x;
    int current_y;
    int next_piece;
    int speed;
    int speed_count;
    int piece_count;
    int score;
    int lines[4];
    int line_count;
    int flash_timer;
    int game_over;
} GameState;
```

Twelve unrelated fields. When you look at `state->current_x` you have to remember it belongs to the current piece. When you see `state->flash_timer` you have to remember it relates to line clearing.

**The new approach:** group related fields into substructs. The struct tells a story.

---

## Step 1 — `GameActionRepeat`: a reusable timer primitive

Before the struct definitions, add:

```c
typedef struct {
    float timer;    /* accumulates delta time (seconds) */
    float interval; /* threshold — fire when timer >= interval */
} GameActionRepeat;
```

This is the building block for any timed action. We'll use it for:
- `tetromino_drop` — how often the piece falls one row
- `completed_lines.flash_timer` — how long completed lines flash white

The same primitive drives both, saving repetition.

---

## Step 2 — `CurrentPiece` substruct

```c
typedef struct {
    int              x;          /* starting column (left edge of 4×4 box) */
    int              y;          /* starting row    (top edge of 4×4 box)  */
    TETROMINO_BY_IDX index;      /* which tetromino (I, J, L...) */
    TETROMINO_BY_IDX next_index; /* upcoming piece — shown in preview */
    TETROMINO_R_DIR  rotation;   /* current rotation state */
} CurrentPiece;
```

All the "current piece" data is now in one place. Accessing `state->current_piece.x` reads clearly: "the x position of the current piece."

Note: `TETROMINO_BY_IDX` and `TETROMINO_R_DIR` are the typed enums from Lesson 04. Previously these were plain `int` fields.

---

## Step 3 — `GameState`

```c
typedef struct {
    unsigned char field[FIELD_WIDTH * FIELD_HEIGHT]; /* the play field */
    CurrentPiece  current_piece;

    int  score;
    int  level;
    int  pieces_count; /* total pieces locked — drives difficulty */
    bool game_over;

    struct {
        int              indexes[TETROMINO_LAYER_COUNT]; /* rows completed this lock */
        int              count;                          /* valid entries in indexes[] */
        GameActionRepeat flash_timer;                    /* countdown while flashing */
    } completed_lines;

    GameActionRepeat tetromino_drop; /* time-based falling speed */
} GameState;
```

**Changes from the original:**

| Old | New | Why |
|-----|-----|-----|
| `current_piece`, `current_x`, `current_y`, `current_rotation`, `next_piece` | `CurrentPiece current_piece` | grouped substruct |
| `speed`, `speed_count` | `GameActionRepeat tetromino_drop` | delta-time based |
| `piece_count` | `pieces_count` | clearer name |
| `lines[4]`, `line_count`, `flash_timer` | `completed_lines` substruct | grouped |
| _(missing)_ | `level` | explicit stored field |
| `int game_over` | `bool game_over` | `<stdbool.h>` |

**New C concept — anonymous inner struct:**  
`struct { ... } completed_lines;` declares an anonymous struct type and a field named `completed_lines` at the same time. You access it as `state->completed_lines.count`. This avoids polluting the global namespace with a `CompletedLines` type that's only used in one place.

---

## Step 4 — `game_init(GameState *, GameInput *)`

```c
void game_init(GameState *state, GameInput *input) {
    state->score        = 0;
    state->game_over    = false;
    state->pieces_count = 1;
    state->level        = 0;

    /* Completed lines substruct */
    state->completed_lines.count                   = 0;
    state->completed_lines.flash_timer.timer       = 0.0f;
    state->completed_lines.flash_timer.interval    = 0.4f; /* 400ms flash */
    memset(&state->completed_lines.indexes, 0,
           sizeof(int) * TETROMINO_LAYER_COUNT);

    /* Time-based dropping — 1 drop per 0.8 seconds initially */
    state->tetromino_drop.timer    = 0.0f;
    state->tetromino_drop.interval = 0.8f;

    /* Configure auto-repeat intervals for movement actions */
    input->move_left.repeat.interval  = 0.05f;  /* 50ms between auto-repeats */
    input->move_right.repeat.interval = 0.05f;
    input->move_down.repeat.interval  = 0.03f;  /* 30ms — slightly faster soft drop */

    /* Build boundary walls */
    for (int y = 0; y < FIELD_HEIGHT; y++) {
        for (int x = 0; x < FIELD_WIDTH; x++) {
            state->field[y * FIELD_WIDTH + x] =
                (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1)
                ? TETRIS_FIELD_WALL
                : TETRIS_FIELD_EMPTY;
        }
    }

    /* Spawn first piece at the horizontal center */
    srand((unsigned int)time(NULL));
    state->current_piece = (CurrentPiece){
        .x          = (FIELD_WIDTH / 2) - 2,
        .y          = 0,
        .index      = rand() % TETROMINOS_COUNT,
        .next_index = rand() % TETROMINOS_COUNT,
        .rotation   = TETROMINO_R_0,
    };
}
```

**Why `game_init` takes `GameInput *`:**  
The input repeat intervals (`move_left.repeat.interval`, etc.) need to be set once at startup. `game_init` is that startup moment — it's the natural place to configure them. Taking `GameInput *` avoids a separate "init input" function.

**New C concept — compound literal `(CurrentPiece){ ... }`:**  
A compound literal creates a temporary struct value inline. The `.x = ...` syntax initializes named fields. Any unspecified fields are zero-initialized. This is cleaner than setting each field individually.

**`TETRIS_FIELD_EMPTY` vs `0`:**  
We use the named constant, not the integer `0`. Even though they're the same value, using the constant makes the intent clear: this cell is empty, not "uninitialized."

---

## Step 5 — Add to `tetris.h`

```c
/* Declare game_init in tetris.h so platforms can call it */
void game_init(GameState *state, GameInput *input);
```

And include `<stdbool.h>` at the top of `tetris.h` for the `bool` type.

---

## Step 6 — Wire into `main()`

```c
GameInput game_input = {0};
GameState game_state = {0};
game_init(&game_state, &game_input);
```

The `{0}` initializes all fields to zero. Then `game_init` sets the non-zero values. The order matters: `{0}` first, then `game_init`.

**Why `{0}` first?** `game_init` only sets the fields it cares about. If `GameState` gains a new field in the future, `{0}` ensures it starts in a known (zero) state even before `game_init` is updated.

---

## Visual: GameState Memory Layout

```
GameState:
├── field[216]           ← the play field (12×18 bytes)
├── current_piece
│   ├── x, y             ← position
│   ├── index            ← which piece (TETROMINO_I_IDX...)
│   ├── next_index       ← upcoming piece
│   └── rotation         ← TETROMINO_R_0...R_270
├── score
├── level
├── pieces_count
├── game_over
├── completed_lines
│   ├── indexes[4]       ← which rows just completed
│   ├── count            ← how many
│   └── flash_timer
│       ├── timer        ← current countdown
│       └── interval     ← 0.4 seconds
└── tetromino_drop
    ├── timer            ← time since last drop
    └── interval         ← seconds between drops
```

---

## Key Concepts

- `CurrentPiece` substruct groups position + piece index + rotation
- `completed_lines` substruct groups line detection data
- `GameActionRepeat` is a reusable "timer that fires every N seconds" primitive
- `game_init` takes `GameInput *` to set repeat intervals at startup
- `(StructType){ .field = value }` compound literal — C99 designated initializer syntax
- `bool game_over` from `<stdbool.h>` — more expressive than `int`

---

## Exercise

`pieces_count` is initialized to `1`, not `0`. Why?

(Hint: Look at how difficulty scaling works — `if (state->pieces_count % 25 == 0) state->level++`. What would happen on the very first piece lock if `pieces_count` started at 0?)
