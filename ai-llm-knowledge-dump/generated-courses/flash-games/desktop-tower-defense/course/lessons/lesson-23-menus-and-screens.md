# Lesson 23 — Menus and Screens: Phase State Machine, Mod Selection, and Restart Flow

**Prerequisites:** Lessons 1–22 (complete game loop, audio, particles, waves)  
**What you will build:** A polished start menu with a PLAY button, a mod-selection screen
with five distinct game mods, and improved game-over / victory screens with restart
navigation — all wired together through the existing `GamePhase` finite-state machine.

---

## Table of Contents

1. [Concept 1 — Game Phase State Machine](#concept-1--game-phase-state-machine)
2. [Concept 2 — Mod Selection Screen UI](#concept-2--mod-selection-screen-ui)
3. [Concept 3 — Restart and Navigation Flow](#concept-3--restart-and-navigation-flow)
4. [JS Analogy — React State vs C FSM](#js-analogy--react-state-vs-c-fsm)
5. [Step-by-Step Implementation](#step-by-step-implementation)
6. [Verifiable Checkpoints](#verifiable-checkpoints)
7. [Exercises](#exercises)

---

## Concept 1 — Game Phase State Machine

### What is a Finite-State Machine?

A Finite-State Machine (FSM) is a model where a system can be in exactly **one state** at
any time. Transitions between states happen on well-defined events (button clicks, timers,
conditions). Desktop Tower Defense uses `GamePhase` as its FSM:

```
TITLE ──(PLAY click)──► MOD_SELECT ──(mod chosen)──► PLACING
                                                         │
                                          ◄──(wave ends)─┤
                                          │              │
                                     WAVE_CLEAR      (start wave)
                                          │              │
                                          └──────────────► WAVE
                                                          │
                                              ┌───────────┤
                                         (lives=0)   (all waves)
                                              │              │
                                         GAME_OVER      VICTORY
                                              │              │
                                          (click)        (click)
                                              └──────(TITLE)──┘
```

### Why Add a New Phase Cleanly?

In C, the `GamePhase` enum gives every phase a symbolic name that compilers can check. To
add `GAME_PHASE_MOD_SELECT`, you:

1. Insert it into the enum (order matters — place it logically, not arbitrarily).
2. Add a `case` in `game_update()`.
3. Add a render block in `game_render()`.
4. Optionally add a `case` in `change_phase()` for entry actions.

Because the switch statements use symbolic constants, the compiler will warn (`-Wall`) if
you forget to handle a new enum value in any exhaustive switch.

### The `change_phase()` Entry-Action Pattern

```c
static void change_phase(GameState *s, GamePhase p)
{
    s->phase       = p;
    s->phase_timer = 0.0f;

    switch (p) {
        case GAME_PHASE_MOD_SELECT:
            s->mod_hover_idx = -1;   /* clear hover on entry */
            break;
        case GAME_PHASE_WAVE_CLEAR:
            s->phase_timer = 3.0f;
            game_play_sound(&s->audio, SFX_WAVE_COMPLETE);
            break;
        /* … */
    }
}
```

This is the **entry-action** pattern: when entering a phase, perform any one-time setup
(reset timers, clear transient UI state, play sounds). Keeping this in `change_phase()`
rather than scattered across `game_update()` makes transitions easy to reason about.

### Phase Data Ownership

Each phase "owns" certain fields in `GameState`. A discipline worth following:

| Phase             | Owns / Modifies                                  |
|-------------------|--------------------------------------------------|
| `MOD_SELECT`      | `mod_hover_idx`, `active_mod`                    |
| `PLACING`         | `towers[]`, `player_gold`, `selected_tower_type` |
| `WAVE`            | `creeps[]`, `projectiles[]`, `player_lives`      |
| `WAVE_CLEAR`      | `phase_timer`, `current_wave`                    |
| `GAME_OVER`       | reads `current_wave` for display                 |
| `VICTORY`         | reads `current_wave` for display                 |

When you return to `GAME_PHASE_TITLE` via `game_init()`, everything is zero-filled and
restarted from scratch — no partial state bleeds over.

---

## Concept 2 — Mod Selection Screen UI

### Button Grid Layout

The mod-select screen lays out five vertically-stacked buttons inside the 600×400 grid
area. The key layout constants (defined in `game.c`) are:

```c
#define MOD_BTN_X    30    /* left margin inside grid */
#define MOD_BTN_W    540   /* button width            */
#define MOD_BTN_H    42    /* button height           */
#define MOD_BTN_GAP  8     /* vertical gap            */
#define MOD_BTN_Y0   80    /* y of the first button   */
```

Button `i` occupies the rectangle:

```
x = MOD_BTN_X
y = MOD_BTN_Y0 + i * (MOD_BTN_H + MOD_BTN_GAP)
w = MOD_BTN_W
h = MOD_BTN_H
```

This formula is used **identically** in both `game_update()` (for hit-testing clicks) and
`game_render()` (for drawing). Keeping the constants in one place ensures the two are
always in sync.

### Hover State

`mod_hover_idx` stores which button (0–4) the mouse is currently over, or `-1` if none.
It is updated **every frame** in `game_update()`:

```c
case GAME_PHASE_MOD_SELECT:
    s->mod_hover_idx = -1;                       /* assume no hover */
    for (int i = 0; i < MOD_COUNT; i++) {
        int by = MOD_BTN_Y0 + i * (MOD_BTN_H + MOD_BTN_GAP);
        if (s->mouse.x >= MOD_BTN_X &&
            s->mouse.x <  MOD_BTN_X + MOD_BTN_W &&
            s->mouse.y >= by &&
            s->mouse.y <  by + MOD_BTN_H)
        {
            s->mod_hover_idx = i;
            if (s->mouse.left_pressed) {
                s->active_mod = (GameMod)i;
                change_phase(s, GAME_PHASE_PLACING);
            }
        }
    }
    break;
```

`game_render()` then reads `s->mod_hover_idx` to choose the button background:

```c
uint32_t bg;
if (i == s->mod_hover_idx)
    bg = COLOR_BTN_HOVER;
else if ((GameMod)i == s->active_mod)
    bg = COLOR_BTN_SELECTED;
else
    bg = COLOR_BTN_NORMAL;
draw_rect(bb, MOD_BTN_X, by, MOD_BTN_W, MOD_BTN_H, bg);
```

### Separating Update and Render

A key discipline: **`game_render()` takes `const GameState *s`** — it cannot mutate state.
This means:

- All hover/click decisions happen in `game_update()`.
- `game_render()` is a pure function of the state snapshot; you could call it on a frozen
  state from 10 frames ago and it would produce the same image for that state.

The PLAY button on the title screen is a partial exception: its hover *visual* is computed
inline from `s->mouse.x/y` inside the `const` render function (which is a read, not a
write), but the actual transition only fires in `game_update()`.

### The `GameMod` Enum

```c
typedef enum {
    MOD_DEFAULT = 0,  /* standard BFS pathfinding          */
    MOD_TERRAIN,      /* water/mountain terrain modifiers  */
    MOD_WEATHER,      /* periodic weather events           */
    MOD_NIGHT,        /* reduced tower range               */
    MOD_BOSS,         /* boss-heavy waves                  */
    MOD_COUNT,
} GameMod;
```

`MOD_COUNT` is the sentinel value used for array sizes and loop bounds:

```c
static const char *mod_names[MOD_COUNT] = { "DEFAULT", "TERRAIN", … };
for (int i = 0; i < MOD_COUNT; i++) { … }
```

`active_mod` is stored in `GameState` and persists through the entire play session. Future
lessons can query it to branch gameplay logic:

```c
if (s->active_mod == MOD_NIGHT) {
    effective_range *= 0.6f;   /* night penalty */
}
```

---

## Concept 3 — Restart and Navigation Flow

### The `game_init()` Contract

`game_init()` is the single authoritative reset point:

```c
void game_init(GameState *s)
{
    memset(s, 0, sizeof(*s));   /* zero everything */
    /* … set starting values … */
    s->phase         = GAME_PHASE_TITLE;
    s->active_mod    = MOD_DEFAULT;
    s->mod_hover_idx = -1;
    game_audio_init(&s->audio);
}
```

Calling `game_init(s)` from `GAME_PHASE_GAME_OVER` or `GAME_PHASE_VICTORY` on a click
returns the player to the title screen — a clean restart. No partial creep or tower state
survives.

### Why Return to TITLE (Not PLACING)?

Returning to `GAME_PHASE_TITLE` (via `game_init()`) instead of `GAME_PHASE_PLACING`
respects the new navigation flow:

```
GAME_OVER / VICTORY ──(click)──► TITLE ──(PLAY)──► MOD_SELECT ──(mod)──► PLACING
```

This lets the player choose a different mod on each restart. If you want a "quick restart"
that bypasses the title, you could transition directly to `MOD_SELECT` instead:

```c
case GAME_PHASE_GAME_OVER:
case GAME_PHASE_VICTORY:
    if (s->mouse.left_pressed)
        change_phase(s, GAME_PHASE_MOD_SELECT);  /* skip title */
    break;
```

Both approaches are valid; the choice is UX-driven.

### Displaying Context on Game Over

The game-over screen now shows how far the player got:

```c
char wbuf[32];
snprintf(wbuf, sizeof(wbuf), "Wave reached: %d", s->current_wave);
draw_text(bb, cx - text_width(wbuf, 1) / 2, CANVAS_H / 2 + 4,
          wbuf, COLOR_WHITE, 1);
```

`s->current_wave` is not reset until `game_init()`, so it is readable in the GAME_OVER
render call. This feedback helps players understand their progress.

### Screen Layering

All phase overlays use `draw_rect_blend()` (alpha-blended rectangle) drawn **after** all
gameplay elements, including the side panel. This means:

```
grid → towers → creeps → projectiles → particles → panel → OVERLAY
```

The overlay always sits on top, which is correct for fullscreen menus. For game-over and
victory screens, the full canvas is dimmed:

```c
draw_rect_blend(bb, 0, 0, CANVAS_W, CANVAS_H, GAME_RGBA(0x44, 0x00, 0x00, 0xC0));
```

For the mod-select screen, only the grid area is dimmed (the panel remains visible for
context):

```c
draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H, GAME_RGBA(0x00, 0x00, 0x00, 0xD8));
```

---

## JS Analogy — React State vs C GamePhase FSM

If you are coming from JavaScript/React, this section maps the C concepts to patterns you
already know.

### React Component State

In React you might model game screens like this:

```jsx
const [screen, setScreen] = useState('title');
const [activeMod, setActiveMod] = useState('default');

function App() {
    if (screen === 'title')      return <TitleScreen onPlay={() => setScreen('modSelect')} />;
    if (screen === 'modSelect')  return <ModSelect   onPick={mod => { setActiveMod(mod); setScreen('playing'); }} />;
    if (screen === 'playing')    return <Game        mod={activeMod} onOver={() => setScreen('gameOver')} />;
    if (screen === 'gameOver')   return <GameOver    onRestart={() => setScreen('title')} />;
}
```

### C GamePhase FSM

The C equivalent is structurally identical:

| React concept       | C equivalent                          |
|---------------------|---------------------------------------|
| `useState('title')` | `s->phase = GAME_PHASE_TITLE`         |
| `setScreen('...')`  | `change_phase(s, GAME_PHASE_...)`     |
| Component re-render | `game_render()` called every frame    |
| `useEffect` on mount| entry actions in `change_phase()`     |
| Props down          | Fields in `GameState` read by render  |
| Events up           | Mouse events processed in `game_update()` |

Key difference: in React, state is immutable between renders and React schedules rerenders.
In C, `GameState` is a single mutable struct and `game_render()` is called every frame
regardless. The discipline of `const GameState *s` in `game_render()` enforces the same
read-only contract that React enforces for you automatically.

Another difference: React can unmount/mount components to manage cleanup. In C, `change_phase()`
performs the cleanup (reset `mod_hover_idx`, play a sound, reset a timer) imperatively.
This is more explicit but requires you to remember to add cleanup for each new phase.

### Event Handling

React:
```jsx
<button onClick={() => setActiveMod('boss')}>BOSS</button>
```

C (game_update, MOD_SELECT case):
```c
if (s->mouse.left_pressed &&
    mouse_in_rect(s->mouse.x, s->mouse.y, MOD_BTN_X, by, MOD_BTN_W, MOD_BTN_H)) {
    s->active_mod = MOD_BOSS;
    change_phase(s, GAME_PHASE_PLACING);
}
```

Both check for a user action (click) and update state. React wraps this in JSX event
handlers; C does it manually in the update loop.

---

## Step-by-Step Implementation

### Step 1 — Add `GameMod` to `game.h`

Open `src/game.h`. After `TargetMode`, add:

```c
typedef enum {
    MOD_DEFAULT = 0,
    MOD_TERRAIN,
    MOD_WEATHER,
    MOD_NIGHT,
    MOD_BOSS,
    MOD_COUNT,
} GameMod;
```

**Why here?** The enum must be declared before `GameState` uses it.

### Step 2 — Add `GAME_PHASE_MOD_SELECT` to `GamePhase`

In the same file, insert the new phase between `GAME_PHASE_TITLE` and `GAME_PHASE_PLACING`:

```c
typedef enum {
    GAME_PHASE_TITLE = 0,
    GAME_PHASE_MOD_SELECT,    /* ← new */
    GAME_PHASE_PLACING,
    GAME_PHASE_WAVE,
    GAME_PHASE_WAVE_CLEAR,
    GAME_PHASE_GAME_OVER,
    GAME_PHASE_VICTORY,
    GAME_PHASE_COUNT,
} GamePhase;
```

**Checkpoint:** `GAME_PHASE_COUNT` should now be `7` (was `6`).

### Step 3 — Extend `GameState`

Add the new fields inside the `GameState` struct (near the Input section):

```c
/* Mod selection */
GameMod    active_mod;       /* which mod is currently selected */
int        mod_hover_idx;    /* which mod button mouse is over (-1 = none) */
```

### Step 4 — Initialise in `game_init()`

At the end of `game_init()`, after `s->phase = GAME_PHASE_TITLE;`:

```c
s->active_mod    = MOD_DEFAULT;
s->mod_hover_idx = -1;
```

### Step 5 — Add layout constants to `game.c`

After the shop button `#define`s near the top of `game.c`:

```c
/* Mod-select screen layout */
#define MOD_BTN_X    30
#define MOD_BTN_W    540
#define MOD_BTN_H    42
#define MOD_BTN_GAP  8
#define MOD_BTN_Y0   80
```

### Step 6 — Handle `GAME_PHASE_MOD_SELECT` in `change_phase()`

Add a case before the `default`:

```c
case GAME_PHASE_MOD_SELECT:
    s->mod_hover_idx = -1;
    break;
```

### Step 7 — Update `GAME_PHASE_TITLE` in `game_update()`

Replace the old "any click" handler:

```c
case GAME_PHASE_TITLE:
    if (s->mouse.left_pressed) {
        int play_x = (CANVAS_W - 120) / 2;
        int play_y = 222;
        if (s->mouse.x >= play_x && s->mouse.x < play_x + 120 &&
            s->mouse.y >= play_y && s->mouse.y < play_y + 36)
            change_phase(s, GAME_PHASE_MOD_SELECT);
    }
    break;
```

### Step 8 — Add `GAME_PHASE_MOD_SELECT` case in `game_update()`

Directly after the `GAME_PHASE_TITLE` case:

```c
case GAME_PHASE_MOD_SELECT:
    s->mod_hover_idx = -1;
    for (int i = 0; i < MOD_COUNT; i++) {
        int by = MOD_BTN_Y0 + i * (MOD_BTN_H + MOD_BTN_GAP);
        if (s->mouse.x >= MOD_BTN_X && s->mouse.x < MOD_BTN_X + MOD_BTN_W &&
            s->mouse.y >= by          && s->mouse.y < by + MOD_BTN_H) {
            s->mod_hover_idx = i;
            if (s->mouse.left_pressed) {
                s->active_mod = (GameMod)i;
                change_phase(s, GAME_PHASE_PLACING);
            }
        }
    }
    break;
```

### Step 9 — Redesign `GAME_PHASE_TITLE` in `game_render()`

Replace the old title overlay with:

```c
if (s->phase == GAME_PHASE_TITLE) {
    draw_rect_blend(bb, 0, 0, CANVAS_W, CANVAS_H, GAME_RGBA(0x00, 0x00, 0x00, 0xD0));

    /* Title — 21 chars × 8px × scale 2 = 336px wide */
    draw_text(bb, (CANVAS_W - 21*8*2) / 2, 150,
              "DESKTOP TOWER DEFENSE", COLOR_WHITE, 2);

    /* Subtitle */
    draw_text(bb, (CANVAS_W - 37*8) / 2, 178,
              "A BFS pathfinding tower defense game", COLOR_GOLD_TEXT, 1);

    /* PLAY button */
    {
        int play_x = (CANVAS_W - 120) / 2;
        int play_y = 222;
        int play_hov = (s->mouse.x >= play_x && s->mouse.x < play_x + 120 &&
                        s->mouse.y >= play_y  && s->mouse.y < play_y + 36);
        uint32_t play_col = play_hov ? GAME_RGB(0x33, 0xAA, 0x33)
                                     : GAME_RGB(0x22, 0x77, 0x22);
        draw_rect(bb, play_x, play_y, 120, 36, play_col);
        draw_text(bb, play_x + (120 - 4*8) / 2, play_y + (36 - 8) / 2,
                  "PLAY", COLOR_WHITE, 1);
    }

    /* Instructions */
    draw_text(bb, (CANVAS_W - 55*8) / 2, 280,
              "Place towers to stop creeps | BFS pathfinding | 40 waves",
              GAME_RGB(0xAA, 0xAA, 0xAA), 1);
}
```

### Step 10 — Add `GAME_PHASE_MOD_SELECT` render block

Immediately after the TITLE block (before WAVE_CLEAR):

```c
} else if (s->phase == GAME_PHASE_MOD_SELECT) {
    draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                    GAME_RGBA(0x00, 0x00, 0x00, 0xD8));

    draw_text(bb, (GRID_PIXEL_W - 16*8*2) / 2, 20,
              "SELECT GAME MODE", COLOR_WHITE, 2);

    static const char *mod_names[MOD_COUNT] = {
        "DEFAULT", "TERRAIN", "WEATHER", "NIGHT", "BOSS"
    };
    static const char *mod_descs[MOD_COUNT] = {
        "Standard BFS pathfinding. Classic DTD.",
        "Water slows creeps. Mountains block towers.",
        "Periodic rain/wind events affect movement.",
        "Reduced tower range. Night-adapted towers bonus.",
        "Boss-heavy waves with special abilities."
    };
    static const uint32_t mod_colors[MOD_COUNT] = {
        GAME_RGB(0x66, 0x66, 0x66),
        GAME_RGB(0x22, 0x88, 0x44),
        GAME_RGB(0x44, 0x88, 0xCC),
        GAME_RGB(0x22, 0x22, 0x88),
        GAME_RGB(0x99, 0x11, 0x11),
    };

    for (int i = 0; i < MOD_COUNT; i++) {
        int by = MOD_BTN_Y0 + i * (MOD_BTN_H + MOD_BTN_GAP);
        uint32_t bg = (i == s->mod_hover_idx) ? COLOR_BTN_HOVER :
                      ((GameMod)i == s->active_mod) ? COLOR_BTN_SELECTED :
                      COLOR_BTN_NORMAL;
        draw_rect(bb, MOD_BTN_X, by, MOD_BTN_W, MOD_BTN_H, bg);
        draw_rect(bb, MOD_BTN_X + 4, by + 4, MOD_BTN_H - 8, MOD_BTN_H - 8, mod_colors[i]);
        draw_text(bb, MOD_BTN_X + MOD_BTN_H + 4, by + (MOD_BTN_H - 8) / 2 - 5,
                  mod_names[i], COLOR_WHITE, 1);
        draw_text(bb, MOD_BTN_X + MOD_BTN_H + 4, by + (MOD_BTN_H - 8) / 2 + 6,
                  mod_descs[i], GAME_RGB(0xCC, 0xCC, 0xCC), 1);
    }

    draw_text(bb, (GRID_PIXEL_W - 24*8) / 2, 360,
              "Click a mode to begin playing", GAME_RGB(0xAA, 0xAA, 0xAA), 1);
}
```

### Step 11 — Improve GAME_OVER and VICTORY overlays

Replace the existing blocks with richer versions that show context and clearer calls to
action (see the full implementation in `src/game.c`).

### Step 12 — Build and verify

```bash
cd course
./build-dev.sh --backend=raylib
```

Expected: `Build successful → ./build/game` with zero warnings and zero errors.

---

## Verifiable Checkpoints

After completing each step, use the corresponding check to confirm correctness.

| # | Check                                                                             | How to verify                                                    |
|---|-----------------------------------------------------------------------------------|------------------------------------------------------------------|
| 1 | `GameMod` enum exists with `MOD_COUNT = 5`                                       | `grep -c 'MOD_' src/game.h` → should print ≥ 6                  |
| 2 | `GAME_PHASE_MOD_SELECT` is between TITLE and PLACING                             | Read the enum; inserting in the middle shifts numeric values     |
| 3 | `GameState` has `active_mod` and `mod_hover_idx`                                 | `grep 'active_mod\|mod_hover_idx' src/game.h`                    |
| 4 | Build compiles with zero errors                                                   | `./build-dev.sh --backend=raylib` exits with code 0              |
| 5 | Title screen shows "DESKTOP TOWER DEFENSE" and a PLAY button                     | Run the game; title overlay visible                              |
| 6 | Clicking PLAY opens the mod-select screen with 5 buttons                         | Run the game; click PLAY; 5 buttons appear in the grid area      |
| 7 | Hovering a mod button highlights it differently from non-hovered buttons          | Run the game; move mouse over buttons                            |
| 8 | Clicking a mod button starts the game in PLACING phase                            | Click DEFAULT; grid becomes interactive                          |
| 9 | Losing all lives shows "GAME OVER", wave number, and "Click anywhere to try again"| Use a debug skip or let creeps through                          |
| 10| Clicking after GAME OVER returns to TITLE screen                                  | After game-over, click anywhere; title overlay appears           |
| 11| VICTORY screen shows "VICTORY!" and "Click anywhere to play again"               | Use a debug `current_wave = WAVE_COUNT + 1` shortcut to test    |

---

## Exercises

### Beginner

**B1. Back button on mod-select**  
Add a small "BACK" button at the bottom of the mod-select screen. When clicked, it returns
to `GAME_PHASE_TITLE` via `change_phase(s, GAME_PHASE_TITLE)`.

*Hint:* Draw the button below the five mod buttons. Hit-test it the same way in
`game_update()` under the `GAME_PHASE_MOD_SELECT` case.

**B2. Display active mod during play**  
In the side panel, show the currently active mod name so the player can see which mod they
chose. Add a line like `"Mod: DEFAULT"` under the gold counter.

*Hint:* Use a `static const char *mod_names[MOD_COUNT]` array (same as the render block)
and `s->active_mod` as the index.

**B3. High-score display**  
Add a `best_wave` field to `GameState` (NOT reset by `game_init()` — store it outside or
use a static variable). On the title screen, show `"Best: wave XX"` if `best_wave > 0`.

---

### Intermediate

**I1. Persist mod selection across restarts**  
After a game-over/victory restart, have the mod-select screen pre-highlight (but not
pre-select) the mod that was last played. The player still has to click to confirm.

*Hint:* `active_mod` survives until `game_init()` is called. You can read it before the
`memset` and restore it after.

**I2. Keyboard navigation on mod-select**  
Add keyboard support: `UP`/`DOWN` arrows move the highlight, `ENTER` confirms selection.
You will need to expose key events from the platform layer (see `platform.h`), or add a
`keys` field to `GameState` similar to `MouseState`.

**I3. Animated title screen**  
Make the title screen show creeps or particles moving across the grid in the background.
Spawn a few CREEP_NORMAL instances in `GAME_PHASE_TITLE` update and let them move as if
a wave is running (but with no damage and no lives cost). Clear them when leaving TITLE.

---

### Challenge

**C1. Implement the TERRAIN mod (gameplay)**  
When `s->active_mod == MOD_TERRAIN`:
- At game start (in `game_init()`, or in a new `apply_mod_init()` function), place a
  handful of CELL_WATER and CELL_MOUNTAIN cells randomly on the grid.
- In `creep_move_toward_exit()`, apply a 0.5× speed multiplier if the creep is on a
  CELL_WATER cell.
- In `can_place_tower()`, disallow placement on CELL_MOUNTAIN cells.

*Hint:* You will need to add `CELL_WATER` and `CELL_MOUNTAIN` to the `CellState` enum and
handle them in BFS (WATER is passable, MOUNTAIN is blocked).

**C2. Implement the NIGHT mod (gameplay)**  
When `s->active_mod == MOD_NIGHT`:
- Multiply every tower's effective range by `0.6f` during targeting.
- Apply a "night vision" bonus: towers of type `TOWER_SNAP` (or a chosen type) are
  exempt from the range penalty.
- Darken the grid render (multiply grid cell colors by 0.5) to sell the atmosphere.

**C3. Animated mod-select buttons**  
Add a `float mod_anim_timer` to `GameState`, increment it in `GAME_PHASE_MOD_SELECT`
update. Use it to draw a pulsing colored glow (slightly varying alpha) on the swatch of
the currently hovered button:

```c
float pulse = 0.5f + 0.5f * sinf(s->mod_anim_timer * 4.0f);
uint8_t glow_alpha = (uint8_t)(pulse * 120);
draw_rect_blend(bb, swatch_x, swatch_y, swatch_w, swatch_h,
                GAME_RGBA(0xFF, 0xFF, 0xFF, glow_alpha));
```

---

## Summary

In this lesson you:

1. Extended the `GamePhase` FSM with a new `GAME_PHASE_MOD_SELECT` phase, cleanly wired
   through `change_phase()`, `game_update()`, and `game_render()`.
2. Built a vertical button-grid UI driven by `mod_hover_idx` and consistent
   `MOD_BTN_*` layout constants shared between update and render.
3. Redesigned the title screen with a proper PLAY button and instructions text.
4. Improved game-over and victory overlays with contextual information and explicit restart
   calls to action.
5. Understood how the C FSM pattern maps to React component state and why separating
   update (mutation) from render (pure read) is the key discipline in both paradigms.

The `active_mod` field is intentionally left as a stub — it gives you, the learner, a
clear hook to add real gameplay branching in the challenge exercises above.

**Next up:** Lesson 24 — Tower Upgrades and Progression (adding upgrade tiers, cost
scaling, and visual feedback for upgraded towers).
