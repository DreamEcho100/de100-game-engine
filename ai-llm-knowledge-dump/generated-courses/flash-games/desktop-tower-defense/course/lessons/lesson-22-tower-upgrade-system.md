# Lesson 22: Tower Upgrade System

## What we're building

A two-tier upgrade system for every tower type.  After placing a tower the
player can spend gold to level it up — level 1 (LV1) and level 2 (LV2) each
multiply damage output (or, for Frost, expand firing range).  A new
**Upgrade** button appears in the info panel below the Sell button whenever a
tower is selected.  When the tower is at max level the button grays out and
reads "MAX".  Successful upgrades play a distinct rising-tone sound and spawn
a floating "UP!" text particle.

## What you'll learn

1. **Upgrade data in `TowerDef`** — storing per-type upgrade costs and
   damage multipliers directly in the static data table so no global state
   is needed.
2. **Upgrade logic and economy** — a single `upgrade_tower()` function that
   validates the purchase, mutates the `Tower` struct, recalculates sell
   value, plays audio, and spawns a particle.
3. **Upgrade UI button** — rendering a context-sensitive panel button and
   wiring up click detection with the existing input handler.

## Prerequisites

- Lessons 01–21 complete
- `TowerDef`, `Tower`, `SfxId`, `spawn_particle()`, `game_play_sound()`, and
  `SELL_RATIO` all exist as taught previously
- The side-panel UI (Lesson 19) is working: `SEL_INFO_Y`, `MODE_BTN_Y`,
  `SELL_BTN_Y`, etc.

---

## Concept 1: Upgrade Data in `TowerDef`

### The problem

We need each tower type to know:
- How much an upgrade costs at each level (`upgrade_cost[0]` = lv0→lv1,
  `upgrade_cost[1]` = lv1→lv2).
- What damage multiplier the player gets after each upgrade
  (`upgrade_damage_mult[0]` = multiplier at lv1, `[1]` = at lv2).

The cleanest place for this data is right inside the existing `TowerDef`
struct, next to `cost`, `damage`, and `range_cells`.  No separate table, no
parallel array — everything for a tower type lives in one row.

### JS analogy

In JavaScript you might write:

```js
const TOWER_DEFS = {
  pellet: {
    name: "Pellet", cost: 5, damage: 5,
    upgradeCost:   [25, 50],
    upgradeDmgMult: [1.5, 2.25],
  },
  // ...
};
```

In C we do the same with a `struct` field:

```c
typedef struct {
    const char *name;
    int         cost;
    int         damage;
    float       range_cells;
    float       fire_rate;
    uint32_t    color;
    int         is_aoe;
    float       splash_radius;
    int         upgrade_cost[2];          /* [0]=lv0→1, [1]=lv1→2 */
    float       upgrade_damage_mult[2];   /* multiplier at lv1, lv2 */
} TowerDef;
```

Fixed-size arrays inside a struct are laid out contiguously in memory — no
heap allocation, no pointer, zero overhead.

### Step 1a: Add the fields to `TowerDef` in `src/game.h`

Open `src/game.h` and find the `TowerDef` typedef.  After `splash_radius`
add the two new fields:

```c
/* One entry in the static tower definition table */
typedef struct {
    const char *name;
    int         cost;
    int         damage;
    float       range_cells;
    float       fire_rate;
    uint32_t    color;
    int         is_aoe;
    float       splash_radius;
    int         upgrade_cost[2];          /* [0]=lv0→1, [1]=lv1→2 */
    float       upgrade_damage_mult[2];   /* multiplier at lv1, lv2 */
} TowerDef;
```

**Verify**: the compiler should accept this without any changes to the rest of
the codebase (the new fields default to zero in any existing static
initializers that omit them).

### Step 1b: Add `upgrade_level` to the `Tower` struct

Each live tower on the grid needs to remember its current upgrade tier (0, 1,
or 2).  Add `int upgrade_level;` to the `Tower` struct in `src/game.h`:

```c
typedef struct {
    int        col, row;
    int        cx,  cy;
    TowerType  type;
    float      range;
    int        damage;
    float      fire_rate;
    float      cooldown_timer;
    float      angle;
    int        target_id;
    TargetMode target_mode;
    int        active;
    int        sell_value;
    float      place_flash;
    int        upgrade_level;   /* 0 = base, 1 = upgraded once, 2 = max */
} Tower;
```

Because `game_init()` calls `memset(s, 0, ...)`, every new tower starts with
`upgrade_level == 0` for free.

### Step 1c: Populate the data table in `src/game.c`

Find `TOWER_DEFS[TOWER_COUNT]` in `src/game.c` and add the new fields to each
row.  The comment column headers help keep columns aligned:

```c
const TowerDef TOWER_DEFS[TOWER_COUNT] = {
    /*                  name      cost  dmg  range  rate        color        aoe  splash  up_cost        up_dmg_mult */
    [TOWER_NONE]   = { "",          0,   0,  0.0f,  0.0f, 0,               0,   0.0f, {  0,   0}, {1.00f, 1.00f} },
    [TOWER_PELLET] = { "Pellet",    5,   5,  3.0f,  2.5f, COLOR_PELLET,    0,   0.0f, { 25,  50}, {1.50f, 2.25f} },
    [TOWER_SQUIRT] = { "Squirt",   15,  10,  3.5f,  1.5f, COLOR_SQUIRT,    0,   0.0f, { 35,  70}, {1.50f, 2.25f} },
    [TOWER_DART]   = { "Dart",     40,  35,  4.0f,  1.0f, COLOR_DART,      0,   0.0f, { 45,  90}, {1.50f, 2.25f} },
    [TOWER_SNAP]   = { "Snap",     75, 125,  4.0f,  0.4f, COLOR_SNAP,      0,   0.0f, { 80, 160}, {1.50f, 2.25f} },
    [TOWER_SWARM]  = { "Swarm",   125,  15,  4.0f,  0.7f, COLOR_SWARM,     1,  30.0f, { 65, 130}, {1.50f, 2.25f} },
    [TOWER_FROST]  = { "Frost",    60,   0,  3.5f,  1.0f, COLOR_FROST,     1,   0.0f, { 50, 100}, {1.00f, 1.00f} },
    [TOWER_BASH]   = { "Bash",     90,  40,  3.0f,  0.5f, COLOR_BASH,      1,  40.0f, { 55, 110}, {1.50f, 2.25f} },
};
```

**Why `{1.00f, 1.00f}` for Frost?**  Frost deals zero damage — its upgrade
benefit is extended range, handled separately in `upgrade_tower()`.  The
multipliers are `1.0` to avoid accidentally inflating a zero-damage value.

**Quick check**: build now — zero new warnings expected.

---

## Concept 2: Upgrade Logic and Economy

### Design decisions

| Question | Answer |
|---|---|
| Max level | 2 (gives three tiers: base / LV1 / LV2) |
| What improves? | Damage × multiplier; Frost gets range × 1.3 per level instead |
| Sell value | Recalculates to cover 70% of (base cost + all upgrade costs paid so far) |
| Insufficient gold | Set `shop_error_timer` so the shop button flashes red — same UX as placement |

### Step 2a: Add `SFX_TOWER_UPGRADE` to the audio enum

In `src/game.h`, add the new sound ID just before `SFX_COUNT`:

```c
typedef enum {
    SFX_TOWER_FIRE_PELLET = 0,
    /* … existing entries … */
    SFX_WAVE_START,
    SFX_INTEREST_EARN,
    SFX_EARLY_SEND,
    SFX_TOWER_UPGRADE,   /* ← new */
    SFX_COUNT,
} SfxId;
```

### Step 2b: Register the sound definition in `src/audio.c`

Add one row to `SOUND_DEFS` — a rising tone from 880 Hz to 1200 Hz over
100 ms:

```c
static const SoundDef SOUND_DEFS[SFX_COUNT] = {
    /* existing rows … */
    /* SFX_WAVE_START         */ { 350.0f,  550.0f, 120.0f,  0.4f  },
    /* SFX_INTEREST_EARN      */ { 660.0f,  880.0f,  80.0f,  0.25f },
    /* SFX_EARLY_SEND         */ { 500.0f,  700.0f, 160.0f,  0.45f },
    /* SFX_TOWER_UPGRADE      */ { 880.0f, 1200.0f, 100.0f,  0.35f },  /* ← new */
};
```

Because `SOUND_DEFS` is indexed by `SfxId`, the position of the new row must
match the enum value.  The array length is `SFX_COUNT` which the compiler
checks automatically.

**Verify**: `clang -Wall` will error if `SFX_COUNT` changed but the array
didn't (or vice versa).

### Step 2c: Add UI layout constant for the Upgrade button

In `src/game.c`, the panel buttons use named Y-position constants.  Add the
upgrade button just below `SELL_BTN_H`:

```c
#define SELL_BTN_Y    (MODE_BTN_Y + 24)
#define SELL_BTN_H    20
#define UPGRADE_BTN_Y (SELL_BTN_Y + SELL_BTN_H + 4)  /* ← new */
#define UPGRADE_BTN_H 20                              /* ← new */
#define START_BTN_Y   (CANVAS_H - 42)
```

The 4-pixel gap gives a visual separation between "destructive" Sell and
"constructive" Upgrade.

### Step 2d: Write the `upgrade_tower()` function

Add this static function to `src/game.c`.  Place it above
`handle_placement_input()` so that function can call it:

```c
/* =========================================================================
 * TOWER UPGRADE
 * ========================================================================= */

static void upgrade_tower(GameState *s, int idx)
{
    if (idx < 0 || idx >= s->tower_count) return;
    Tower *t = &s->towers[idx];
    if (!t->active) return;
    if (t->upgrade_level >= 2) return;   /* already max */

    int cost = TOWER_DEFS[t->type].upgrade_cost[t->upgrade_level];
    if (s->player_gold < cost) { s->shop_error_timer = 0.5f; return; }

    s->player_gold -= cost;
    t->upgrade_level++;

    /* Apply damage multiplier (indexed by new level - 1) */
    float mult = TOWER_DEFS[t->type].upgrade_damage_mult[t->upgrade_level - 1];
    t->damage = (int)((float)TOWER_DEFS[t->type].damage * mult);

    /* Special: Frost upgrades extend range instead of boosting damage */
    if (t->type == TOWER_FROST) {
        t->range = TOWER_DEFS[t->type].range_cells * CELL_SIZE
                   * (1.0f + 0.3f * (float)t->upgrade_level);
    }

    /* Recalculate sell value: cover SELL_RATIO of total spent (base + upgrades) */
    int total_upgrade_cost = 0;
    for (int i = 0; i < t->upgrade_level; i++)
        total_upgrade_cost += TOWER_DEFS[t->type].upgrade_cost[i];
    t->sell_value = (int)((float)(TOWER_DEFS[t->type].cost + total_upgrade_cost) * SELL_RATIO);

    game_play_sound(&s->audio, SFX_TOWER_UPGRADE);
    spawn_particle(s, (float)t->cx, (float)t->cy - CELL_SIZE,
                   0.0f, -30.0f, 1.5f, COLOR_WHITE, 1, "UP!");
}
```

#### Walk-through

```
upgrade_level before call: 0
upgrade_level after call:  1
cost looked up:  upgrade_cost[0]   (lv0→lv1 cost)
mult looked up:  upgrade_damage_mult[0]  (lv1 multiplier)
```

On a second call:

```
upgrade_level before call: 1
upgrade_level after call:  2
cost looked up:  upgrade_cost[1]   (lv1→lv2 cost)
mult looked up:  upgrade_damage_mult[1]  (lv2 multiplier)
```

The index `[t->upgrade_level - 1]` after incrementing means:
- New level 1 → index 0
- New level 2 → index 1

Always correct; no off-by-one.

#### Sell value recalculation

```c
int total_upgrade_cost = 0;
for (int i = 0; i < t->upgrade_level; i++)
    total_upgrade_cost += TOWER_DEFS[t->type].upgrade_cost[i];
t->sell_value = (int)((float)(TOWER_DEFS[t->type].cost + total_upgrade_cost) * SELL_RATIO);
```

Example for a Pellet tower upgraded twice:
- Base cost: 5
- First upgrade: 25
- Second upgrade: 50
- Total: 80
- Sell value: (int)(80 × 0.70) = 56

The player never profits from buy/sell cycling — the SELL_RATIO discount
applies to upgrades too.

---

## Concept 3: Upgrade UI Button

### Step 3a: Wire up click detection in `handle_placement_input()`

After the existing `SELL_BTN_Y` block (which fires on sell), add:

```c
if (m->y >= SELL_BTN_Y && m->y < SELL_BTN_Y + SELL_BTN_H) {
    /* … existing sell logic … */
    return;
}
if (m->y >= UPGRADE_BTN_Y && m->y < UPGRADE_BTN_Y + UPGRADE_BTN_H) {
    upgrade_tower(s, s->selected_tower_idx);
    return;
}
```

Both checks are inside the `selected_tower_idx >= 0` guard that already
exists for the Sell and Mode buttons — no extra null check needed.

### Step 3b: Render the upgrade button in `game_render()`

Inside the `selected_tower_idx >= 0` block in the panel renderer, after the
Sell button draw calls, add:

```c
/* Upgrade button */
if (t->upgrade_level < 2) {
    int up_cost = TOWER_DEFS[t->type].upgrade_cost[t->upgrade_level];
    uint32_t up_col = (s->player_gold >= up_cost)
                      ? GAME_RGB(0x22, 0x55, 0x22)   /* affordable: green tint */
                      : GAME_RGB(0x30, 0x20, 0x20);  /* unaffordable: red tint */
    draw_rect(bb, SHOP_BTN_X, UPGRADE_BTN_Y, SHOP_BTN_W, UPGRADE_BTN_H, up_col);
    char ubuf[24];
    snprintf(ubuf, sizeof(ubuf), "Upgrade $%d", up_cost);
    draw_text(bb, SHOP_BTN_X + 4, UPGRADE_BTN_Y + (UPGRADE_BTN_H - 8) / 2,
              ubuf, COLOR_WHITE, 1);
} else {
    /* MAX level — grayed out, non-interactive */
    draw_rect(bb, SHOP_BTN_X, UPGRADE_BTN_Y, SHOP_BTN_W, UPGRADE_BTN_H,
              GAME_RGB(0x33, 0x33, 0x33));
    draw_text(bb, SHOP_BTN_X + 4, UPGRADE_BTN_Y + (UPGRADE_BTN_H - 8) / 2,
              "MAX", GAME_RGB(0x88, 0x88, 0x88), 1);
}
```

### Step 3c: Show level indicator next to tower name

The tower name in the info panel (`SEL_INFO_Y` row) currently shows only the
type name.  Append a level tag so the player always knows the current tier:

```c
draw_text(bb, PANEL_X + 4, SEL_INFO_Y,
          TOWER_DEFS[t->type].name, TOWER_DEFS[t->type].color, 1);

/* Level indicator */
{
    char lvbuf[16];
    if      (t->upgrade_level == 1) snprintf(lvbuf, sizeof(lvbuf), " LV1");
    else if (t->upgrade_level == 2) snprintf(lvbuf, sizeof(lvbuf), " LV2");
    else                            lvbuf[0] = '\0';

    if (lvbuf[0] != '\0')
        draw_text(bb, PANEL_X + 4 + 40, SEL_INFO_Y, lvbuf, COLOR_GOLD_TEXT, 1);
}
```

The offset `+ 40` puts the label to the right of the tower name (which is
roughly 5–6 characters × ~7px wide at scale 1).

---

## Putting it all together

Run the build to confirm zero errors:

```bash
./build-dev.sh --backend=raylib
```

Expected output:

```
Build successful → ./build/game
```

Then run the game:

```bash
./build/game
```

**Manual test steps:**

1. Place any tower (e.g., Pellet for $5).
2. Click the placed tower — the info panel shows the name, a targeting-mode
   button, a Sell button, and a new **Upgrade $25** button.
3. Click **Upgrade $25** (assuming you have ≥ 25 gold).  
   - The button should update to show **Upgrade $50**.
   - The panel label now reads "Pellet LV1".
   - A rising "UP!" tone plays and a floating "UP!" text appears.
4. Click **Upgrade $50**.  
   - Button changes to gray "MAX".
   - Label reads "Pellet LV2".
5. Sell the upgraded tower.  
   - Gold refunded = (int)((5 + 25 + 50) × 0.70) = 56.  Verify in the HUD.
6. Place a Frost tower and upgrade it twice — note range expands visually
   (the selection ring grows) rather than damage increasing.
7. Start a wave, try to upgrade a tower with less gold than required — the
   shop buttons briefly flash red (same error state as unaffordable placement).

---

## Checklist: "done" criteria

- [ ] `TowerDef` has `upgrade_cost[2]` and `upgrade_damage_mult[2]`.
- [ ] `Tower` has `int upgrade_level`.
- [ ] `TOWER_DEFS` table is populated for all 8 tower types.
- [ ] `SFX_TOWER_UPGRADE` is in the `SfxId` enum.
- [ ] `SOUND_DEFS` has the rising-tone entry for `SFX_TOWER_UPGRADE`.
- [ ] `upgrade_tower()` correctly validates cost, mutates damage/range, and
      recalculates sell value.
- [ ] Clicking the Upgrade button in the panel calls `upgrade_tower()`.
- [ ] Upgrade button renders green (affordable), red (not affordable), or
      gray "MAX" depending on state.
- [ ] Tower name in panel shows " LV1" / " LV2" tag after upgrading.
- [ ] Build: zero errors, zero warnings with `-Wall -Wextra`.

---

## Exercises

### Beginner

**B1 — Show upgrade multiplier in tooltip**  
Below the Upgrade button label, add a second line showing the resulting
damage multiplier as a percentage:

```
Upgrade $25
→ +50% dmg
```

Hint: use `upgrade_damage_mult[t->upgrade_level]` and render a second
`draw_text` call at `UPGRADE_BTN_Y + 10`.

**B2 — Color the level badge**  
Change the "LV1" tag color to `GAME_RGB(0xCC, 0xCC, 0x00)` (yellow) and
"LV2" to `GAME_RGB(0xFF, 0x88, 0x00)` (orange) to give each tier a distinct
look.

### Intermediate

**I1 — Fire-rate upgrade variant**  
Add a third field to `TowerDef`: `float upgrade_firerate_mult[2]`.  For
Squirt tower set `{1.25f, 1.5f}` so upgrades also speed up firing.  Apply
the multiplier in `upgrade_tower()` similarly to damage.

**I2 — Animate the upgrade**  
Instead of a single "UP!" particle, spawn a ring of 8 colored particles
exploding outward from the tower centre (re-use `spawn_explosion()`).  Also
briefly set `t->place_flash = 0.4f` so the tower body flashes white.

**I3 — Sell value preview**  
In the info panel, below the current sell value, show what the sell value
would become after the next upgrade:

```
Sell $21
(after UP: $38)
```

### Challenge

**C1 — Three-tier system**  
Extend to level 3 by changing `upgrade_cost[2]` and `upgrade_damage_mult[2]`
to size-3 arrays and raising `MAX_UPGRADE_LEVEL` to 3.  Update the button
label to "LV3" and disable further upgrades.  Ensure the sell value loop
covers all three tiers.

**C2 — Per-tower unique upgrade effect**  
Make Snap tower's upgrade also reduce its fire cooldown (faster snap), and
make Swarm tower's upgrade increase `splash_radius` by 10 px per level
instead of damage.  Add a branch in `upgrade_tower()` keyed on `t->type`.

**C3 — Upgrade undo / refund**  
Add a "Downgrade" button that only appears at LV1 or LV2, refunds 50% of the
upgrade cost, and reverts `damage` and `range` to the previous level's
values.  This requires storing the original base damage somewhere — consider
reading it fresh from `TOWER_DEFS[t->type].damage` and deriving prior levels
by applying multipliers in reverse.

---

## What's next

**Lesson 23** will add a persistent **high-score / save system**: writing the
player's best wave and gold-earned to a small binary file using `fwrite` /
`fread`.  You'll see how to handle file I/O without allocating heap memory and
how the `#ifdef _WIN32` pattern makes the same code compile on Linux, macOS,
and Windows.
