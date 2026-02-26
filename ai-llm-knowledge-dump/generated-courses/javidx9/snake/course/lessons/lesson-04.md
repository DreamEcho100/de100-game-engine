# Lesson 04 — Typed Enums: `SNAKE_DIR`

## By the end of this lesson you will have:

A `SNAKE_DIR` enum replacing the old `#define DIR_* 0/1/2/3` integer constants, with self-documenting function signatures and correct direction arithmetic using `% 4`.

---

## The Problem with `#define` Integers

The original code used:

```c
#define DIR_UP    0
#define DIR_RIGHT 1
#define DIR_DOWN  2
#define DIR_LEFT  3
```

And stored direction as `int direction`. This compiles fine but has problems:

```c
void move_snake(GameState *s, int direction);  /* Which ints are valid? */
move_snake(&s, 42);  /* Compiles! But 42 is not a direction. */
move_snake(&s, 1);   /* Is 1 RIGHT or DOWN? Hard to tell at the call site. */
```

The compiler can't help you catch mistakes because `int` accepts any integer.

---

## Step 1 — `typedef enum`

```c
typedef enum {
    SNAKE_DIR_UP    = 0,
    SNAKE_DIR_RIGHT = 1,
    SNAKE_DIR_DOWN  = 2,
    SNAKE_DIR_LEFT  = 3
} SNAKE_DIR;
```

**New C concept — `enum`:**
An enum defines a set of named integer constants. `SNAKE_DIR_UP` is exactly like `#define DIR_UP 0` but scoped to the type. The compiler knows `SNAKE_DIR` can only be one of the four values.

**New C concept — `typedef enum`:**
Without `typedef`, you'd have to write `enum SNAKE_DIR dir`. With `typedef`, you write `SNAKE_DIR dir`. The `typedef` makes the enum name usable as a standalone type name.

```js
// Closest JS equivalent: a const object
const SNAKE_DIR = Object.freeze({ UP: 0, RIGHT: 1, DOWN: 2, LEFT: 3 });
```

---

## Step 2 — Self-documenting function signatures

```c
/* Before — unclear what direction values are valid */
void snake_init(GameState *s, int direction);

/* After — obvious at the call site */
void snake_init(GameState *s, SNAKE_DIR direction);
```

When a caller sees `SNAKE_DIR`, they know to use `SNAKE_DIR_RIGHT`, not a raw integer. IDEs and code search can find all usages of the type.

---

## Step 3 — Direction arithmetic still works

Turning clockwise (right turn) = next value in 0→1→2→3→0 cycle:

```c
SNAKE_DIR turn_right = (SNAKE_DIR)((s->direction + 1) % 4);
```

Turning counter-clockwise (left turn) = previous value, wrapping with `+3` trick:

```c
SNAKE_DIR turn_left = (SNAKE_DIR)((s->direction + 3) % 4);
```

Why `+3` instead of `-1`? In C, `(-1) % 4` may return `-1` (implementation-defined). Adding 3 achieves the same result without negative numbers: `(0+3)%4=3=LEFT`, `(1+3)%4=0=UP`, `(2+3)%4=1=RIGHT`, `(3+3)%4=2=DOWN`.

**New C concept — `(SNAKE_DIR)` cast:**
The arithmetic `(direction + 1) % 4` returns an `int`. The cast `(SNAKE_DIR)(...)` converts it back to the enum type. This is an explicit acknowledgment that "yes, I know this int is a valid direction value."

---

## Step 4 — Direction deltas

The `DX` and `DY` arrays in `snake.c` are indexed by `SNAKE_DIR`:

```c
static const int DX[4] = {  0,  1,  0, -1 };  /* UP RIGHT DOWN LEFT */
static const int DY[4] = { -1,  0,  1,  0 };
```

Using the enum as an array index works because enum values are integers:

```c
int new_x = s->segments[s->head].x + DX[s->direction];
int new_y = s->segments[s->head].y + DY[s->direction];
```

`SNAKE_DIR_UP=0` → `DX[0]=0, DY[0]=-1` → y decreases (moving up on screen).
`SNAKE_DIR_RIGHT=1` → `DX[1]=1, DY[1]=0` → x increases (moving right).

---

## Step 5 — In `GameState`

```c
typedef struct {
    /* ... */
    SNAKE_DIR direction;       /* current moving direction */
    SNAKE_DIR next_direction;  /* queued by input, applied on next move */
    /* ... */
} GameState;
```

Both fields are `SNAKE_DIR` — the compiler would warn if you accidentally assigned a plain integer without a cast.

---

## Key Concepts

- `typedef enum { A=0, B=1, ... } NAME` — named integer set with type safety
- Self-documenting: `SNAKE_DIR` in a signature tells callers what values are valid
- `(dir + 1) % 4` — CW turn; `(dir + 3) % 4` — CCW turn (no negatives)
- `(SNAKE_DIR)(expr)` — cast int expression back to enum type
- Enum values are array indices — `DX[SNAKE_DIR_RIGHT]` works as expected

---

## Exercise

The game currently prevents reversing direction (you can't turn 180° — that would cause instant self-collision). This is handled by `next_direction` being set only on a turn press, never to `(dir + 2) % 4`. How would you add an explicit guard? Write the condition that checks if the new direction is the opposite of the current direction and rejects the turn.
