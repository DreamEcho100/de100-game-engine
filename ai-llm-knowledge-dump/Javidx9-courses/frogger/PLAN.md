# Frogger Course — Build Plan

## What the program does

A 2D Frogger clone. The player steers a frog across 10 scrolling lanes:
- Row 9 (bottom): safe starting pavement
- Rows 5-8: road — moving cars and buses kill the frog on contact
- Row 4: safe middle pavement strip
- Rows 1-3: river — the frog must ride floating logs; touching water is fatal
- Row 0: home row with wall tiles (deadly) and home slots (safe goal)

Arrow keys move the frog one tile per keypress. River logs carry the frog sideways.
Touching anything dangerous resets the frog to start.

Source: Javidx9 OneLoneCoder Frogger (C++, olcConsoleGameEngine).
Our port: **plain C**, Raylib + X11 backends, data-oriented design.

---

## Design principles applied (vs original C++ OOP)

| Original (C++ OOP)         | Our port (C, DOD)                             |
|----------------------------|-----------------------------------------------|
| Class with member variables | Flat `GameState` struct, no hidden state      |
| `vector<pair<float,wstring>>` lanes | Separate `lane_speeds[]` + `lane_patterns[][]` |
| `new olcSprite(...)` on heap | Fixed-size `SpriteBank` embedded in GameState |
| Render mixed into Update   | `frogger_tick()` = logic only, `platform_render()` = draw only |
| `bufDanger` heap-allocated  | `danger[128*80]` flat array inside GameState  |

DOD benefit: all lane speeds are contiguous in memory — when you loop over them
you stay in one cache line. With AOS (`Lane* lanes[]`) you'd skip over pattern
data (64 bytes) to get each speed.

---

## File structure

```
course/
├── PLAN.md                  ← this file
├── build_x11.sh             ← build with X11 backend
├── build_raylib.sh          ← build with Raylib backend
├── assets/                  ← .spr sprite files (copied from source)
│   ├── frog.spr, bus.spr, car1.spr, car2.spr
│   ├── log.spr, water.spr, pavement.spr, wall.spr, home.spr
├── src/
│   ├── platform.h           ← 6-function platform contract
│   ├── frogger.h            ← GameState, constants, declarations
│   ├── frogger.c            ← game logic (init, tick, run loop)
│   ├── main_x11.c           ← X11 platform implementation + main()
│   └── main_raylib.c        ← Raylib platform implementation + main()
└── lessons/
    ├── lesson-01.md  Open a window
    ├── lesson-02.md  Cell grid math
    ├── lesson-03.md  Binary .spr file format
    ├── lesson-04.md  Sprite rendering
    ├── lesson-05.md  Lane data and DOD layout
    ├── lesson-06.md  Time-based scrolling
    ├── lesson-07.md  Frog placement and keyboard input
    ├── lesson-08.md  Platform riding (river logs carry frog)
    ├── lesson-09.md  Danger buffer and collision detection
    └── lesson-10.md  Polish — bounds, homes, win state
```

---

## .spr binary format (decoded from file inspection)

```
int32_t  width;
int32_t  height;
int16_t  colors[width * height];  ← Windows console color attrs (FG = low nibble)
int16_t  glyphs[width * height];  ← Unicode char (0x2588 = full block, 0x0020 = space)
```

Rendering rule:
- glyph == 0x2588 → fill cell with PALETTE[color & 0x0F]  (foreground color)
- glyph == 0x0020 → transparent (skip)
- any other glyph → fill cell with PALETTE[color & 0x0F]

Sprite sizes:
- frog, water, pavement, wall, home: 8×8 cells
- car1, car2: 16×8 cells
- log: 24×8 cells
- bus: 32×8 cells

---

## Screen geometry

```
Virtual screen: 128 × 80 cells
Pixels per cell: CELL_PX = 8
Window size: 1024 × 640 pixels

One game tile = TILE_CELLS = 8 cells wide = 64 pixels wide
Lane width drawn: LANE_WIDTH = 18 tiles
Lane pattern length: 64 tiles (wraps)
```

---

## Lesson sequence

| # | What gets built               | What student sees                               |
|---|-------------------------------|-------------------------------------------------|
| 1 | Open a window + game loop     | Black 1024×640 window                           |
| 2 | Cell grid math + clear        | Dark-teal background fills window               |
| 3 | .spr binary loader            | Terminal prints sprite dimensions               |
| 4 | Sprite rendering              | Frog sprite drawn at center                     |
| 5 | Lane data (DOD layout)        | All 10 lanes drawn statically                   |
| 6 | Time-based scrolling          | Lanes scroll left/right smoothly                |
| 7 | Frog + keyboard input         | Frog moves one tile per arrow keypress          |
| 8 | Platform riding               | Frog carried sideways by river logs             |
| 9 | Danger buffer + collision     | Frog resets to start when it hits danger        |
|10 | Bounds + homes + win state    | Complete game with win message                  |
