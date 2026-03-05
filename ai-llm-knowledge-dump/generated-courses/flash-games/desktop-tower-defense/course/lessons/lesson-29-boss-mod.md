# Lesson 29 — Boss Mod (MOD_BOSS)

## Overview

`MOD_BOSS` makes boss encounters more frequent and dramatically more dangerous.
Boss waves appear every 5 waves instead of every 10.  Each boss spawns with a
5-second damage-immunity shield, moves 30 % faster, has 50 % more HP, and
splits into three child creeps when it first drops to half health.

---

## New fields added to the Creep struct

```c
/* MOD_BOSS fields */
float     shield_timer;    /* if > 0, immune to all damage (boss spawn shield) */
int       half_hp_spawned; /* 1 once the 50%-HP child-spawn has triggered */
```

These fields are added to every `Creep` regardless of active mod.  Because
`memset(c, 0, sizeof(*c))` is called in `spawn_creep`, they start at zero for
all non-boss creeps and never interfere with normal gameplay.

---

## Concept 1 — Enhanced boss spawning (wave frequency override)

### Standard boss wave schedule

In `g_waves[]` bosses appear at waves 10, 15, 20, 25, 30, and 40.  The pattern
is roughly every 10 waves.

### MOD_BOSS override: every 5 waves

We do not modify `g_waves[]` — that array is `const` and shared by all mods.
Instead we introduce a local override at three call sites that read wave data:
`check_wave_complete`, `start_wave`, and `update_wave`.

```c
static const WaveDef boss_mod_wave = { CREEP_BOSS, 1, 0.5f, 0 };
if (s->active_mod == MOD_BOSS && (s->current_wave % 5 == 0))
    wd = &boss_mod_wave;
```

A single `static const WaveDef` and a two-line conditional is all it takes.
The override wave has:
- `creep_type = CREEP_BOSS`
- `count = 1` (one boss per wave)
- `spawn_interval = 0.5f` (effectively instant, since count = 1)
- `base_hp_override = 0` — use the default HP scaling (`CREEP_DEFS[CREEP_BOSS].base_hp * HP_SCALE_PER_WAVE^(wave-1)`)

This means waves 5, 10, 15, 20, 25, 30, 35, 40 all spawn a boss.  Waves
10/15/20/30/40 are *also* boss waves in the default table — the override fires
for all multiples of 5, so it never accidentally replaces a non-boss wave that
happened to align with multiples of 5.

### The three patched call sites

| Function              | Why                                                    |
|-----------------------|--------------------------------------------------------|
| `start_wave()`        | Computes `remaining_wave_time` from `wd->count`        |
| `update_wave()`       | Spawns creeps using `wd->creep_type` and `wd->count`   |
| `check_wave_complete()` | Tests `wave_spawn_index < wd->count`                 |

All three use the same pattern:
```c
const WaveDef *wd = &g_waves[s->current_wave - 1];
static const WaveDef boss_mod_wave = { CREEP_BOSS, 1, 0.5f, 0 };
if (s->active_mod == MOD_BOSS && (s->current_wave % 5 == 0))
    wd = &boss_mod_wave;
```

---

## Concept 2 — Boss shield mechanic — immune phase

### Spawning the shielded boss

```c
/* MOD_BOSS: boss creeps spawn with a 5-second damage shield,
 * +30% speed, and +50% HP. */
if (type == CREEP_BOSS && s->active_mod == MOD_BOSS) {
    c->shield_timer = 5.0f;
    c->speed       *= 1.3f;
    c->hp          *= 1.5f;
    c->max_hp       = c->hp;
}
```

`shield_timer` starts at 5.0 seconds.  During this window all damage is
absorbed silently.  The multipliers are applied to the values already set by
`spawn_creep` (which initialises `c->speed = def->speed` and `c->hp` from the
wave's HP override), so they stack correctly with the wave-progression scaling.

### Ticking the shield down

```c
/* In the creep movement loop inside update_wave: */
if (s->active_mod == MOD_BOSS && c->shield_timer > 0.0f) {
    c->shield_timer -= dt;
    if (c->shield_timer < 0.0f) c->shield_timer = 0.0f;
}
```

`shield_timer` is decremented every frame that the boss is alive.  When it
reaches zero the boss becomes vulnerable.

### Blocking damage during the shield

```c
static void apply_damage(GameState *s, int creep_idx, int dmg, TowerType tt)
{
    /* ... */
    /* MOD_BOSS: boss is immune while its spawn shield is active */
    if (s->active_mod == MOD_BOSS && c->shield_timer > 0.0f) return;
    c->hp -= (float)effective_damage(dmg, c->type, tt);
    /* ... */
}
```

The early `return` means:
- Projectiles still hit and despawn (they consume their `active` flag).
- No floating "+X" particles spawn from the creep.
- The boss HP bar does not move.
- The player sees the shield visual and knows to wait.

This creates a meaningful **timing mechanic**: if the player positions towers
well, their opening volley fires during the 5-second window but deals zero
damage.  Players learn to route the boss through a "waiting area" before it
reaches the main kill zone.

### Rendering the shield

```c
/* MOD_BOSS: yellow shield outline when spawn immunity is active */
if (s->active_mod == MOD_BOSS && c->type == CREEP_BOSS &&
    c->shield_timer > 0.0f) {
    draw_circle_outline(bb, cx, cy, r + 4, GAME_RGB(0xFF, 0xFF, 0x00));
}
```

A yellow circle 4 pixels larger than the boss body signals the active shield.
The same yellow colour is used by the night glow in MOD_NIGHT (see lesson 28),
so players intuit "yellow outline = invulnerability" across contexts.

---

## Concept 3 — Mid-health spawn trigger

### The half-HP check in apply_damage

```c
/* MOD_BOSS: when boss HP first crosses 50%, spawn 3 child creeps */
if (s->active_mod == MOD_BOSS && c->type == CREEP_BOSS &&
    c->hp > 0.0f && c->hp <= c->max_hp * 0.5f && c->half_hp_spawned == 0)
{
    c->half_hp_spawned = 1;
    float bx   = c->x;   float by   = c->y;
    int   bcol = c->col;  int   brow = c->row;
    for (int k = 0; k < 3; k++) {
        int ci = spawn_creep(s, CREEP_SPAWN_CHILD, 0.0f);
        if (ci >= 0) {
            s->creeps[ci].x   = bx;
            s->creeps[ci].y   = by;
            s->creeps[ci].col = bcol;
            s->creeps[ci].row = brow;
        }
    }
}
```

Breaking this down:

| Expression                          | Meaning                                   |
|-------------------------------------|-------------------------------------------|
| `c->hp > 0.0f`                      | boss is still alive after this hit        |
| `c->hp <= c->max_hp * 0.5f`         | HP has crossed the 50 % threshold         |
| `c->half_hp_spawned == 0`           | trigger has not fired yet                 |
| `c->half_hp_spawned = 1`            | arm the latch so it never fires again     |

The children are spawned at the boss's *current* pixel position and grid cell.
They immediately start following the BFS gradient toward the exit.

### Why apply_damage, not kill_creep?

`kill_creep` fires when HP reaches exactly zero.  The half-HP trigger is a
*threshold* event that should fire while the boss is still alive.  Placing it
in `apply_damage` (after HP reduction, before the `kill_creep` call) gives us
access to the boss's position while it is still valid.

```c
c->hp -= (float)effective_damage(dmg, c->type, tt);
/* half-HP check here — c is still alive */
if (c->hp <= 0.0f) kill_creep(s, creep_idx);
```

### Pointer safety when spawning children

`spawn_creep` may extend `s->creep_count` and reuse an inactive slot from the
pool, but it never moves existing elements.  The `Creep *c` pointer remains
valid throughout the loop because `creeps[]` is a fixed-size array, not a
dynamic allocation.

### Gameplay impact

When the boss drops to half HP it suddenly becomes *three* CREEP_SPAWN_CHILD
units — small, fast, zero-kill-gold enemies that slip through tower gaps.  The
player must choose between finishing off the boss quickly or pivoting to handle
the children.

---

## Full implementation map

| File     | Location                   | Change                                           |
|----------|----------------------------|--------------------------------------------------|
| `game.h` | `Creep` struct             | `shield_timer`, `half_hp_spawned`               |
| `game.c` | `spawn_creep()`            | boss gets shield, ×1.3 speed, ×1.5 HP           |
| `game.c` | `apply_damage()`           | shield immunity; half-HP child spawn             |
| `game.c` | `check_wave_complete()`    | boss wave override for multiples of 5            |
| `game.c` | `start_wave()`             | boss wave override for multiples of 5            |
| `game.c` | `update_wave()`            | boss wave override; shield timer decrement       |
| `game.c` | `game_render()` creeps     | yellow `draw_circle_outline` when shielded       |
| `game.c` | `game_render()` HUD        | "Boss Mode" red label in panel                   |

---

## Design notes

### Why ×1.3 speed and ×1.5 HP?

The defaults in `CREEP_DEFS[CREEP_BOSS]` already give a tanky slow creep (40
px/s, 1000 HP).  In MOD_BOSS the intent is for bosses to arrive faster and
require longer to kill, prolonging the danger window.  The values were chosen
to be clearly perceptible without being trivially unfair.

### Why `static const WaveDef` inside the function?

A `static const` local is initialised once at program start and shares the same
lifetime as a global, but without polluting the translation unit's namespace.
Because the same `boss_mod_wave` pattern appears in three functions, each has
its own local copy — three identical small structs, negligible memory cost.

### Why not modify the boss's HP directly in g_waves?

`g_waves` is `const` and intentionally not modified by mod code.  More
importantly, the HP in `g_waves` is `base_hp_override` — a raw value that
bypasses wave scaling.  We want the MOD_BOSS boss to scale with wave number
(getting harder on wave 35 than on wave 5), so we leave `base_hp_override = 0`
and let the normal HP scaling formula in `update_wave` apply.

---

## Summary

| Takeaway                              | Implementation detail                           |
|---------------------------------------|-------------------------------------------------|
| Const wave table untouched            | Local `static const WaveDef` override at 3 sites|
| Shield via `shield_timer` field       | Decremented in `update_wave` creep loop         |
| Damage blocked by early return        | In `apply_damage` when `shield_timer > 0`       |
| Half-HP spawn via latch flag          | `half_hp_spawned` prevents double-trigger       |
| Children spawned at boss position     | `spawn_creep()` + manual position copy          |
| Yellow outline = shield visual        | `draw_circle_outline` at `r + 4`                |
