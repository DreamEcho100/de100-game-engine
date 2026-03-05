# Lesson 28 — Night Mod (MOD_NIGHT)

## Overview

`MOD_NIGHT` turns the game into a night-time scenario.  By default every tower
suffers a range penalty (×0.75), but two tower types gain *night-vision*
bonuses that make them more effective in the dark.  A dark overlay and yellow
glow effects reinforce the visual theme.

---

## Concept 1 — Global state modifiers (range reduction)

A **global modifier** is a change that applies uniformly to every instance of a
system at the moment of creation.  Rather than patching live towers each frame,
we apply the night adjustment *once* when each tower is placed.

This is different from a per-frame multiplier (like the weather speed factor)
because the range change must persist across frames, be saved in the sell
value calculation, and remain accurate even if the mod were toggled mid-game.

### apply_night_bonus

```c
/* Apply night-mode bonuses/penalties to a freshly-placed tower. */
static void apply_night_bonus(GameState *s, Tower *t)
{
    if (s->active_mod != MOD_NIGHT) return;
    if (t->type == TOWER_DART) {
        t->range   *= 1.2f;  /* dart: sniper bonus at night */
    } else if (t->type == TOWER_PELLET) {
        t->fire_rate *= 1.5f; /* pellet: machine-gun bonus at night */
    } else {
        t->range   *= 0.75f; /* all other towers: reduced range at night */
    }
}
```

This function is called once per tower after placement:

```c
/* In handle_placement_input(), after creating the tower struct: */
apply_night_bonus(s, t);
```

Because `active_mod` is checked at the top of the function, calling it in
`MOD_DEFAULT` is a no-op with zero cost — the guard returns immediately.

### Why modify range at placement time?

- `Tower::range` is the *only* field used by `in_range()` and by the
  hover-preview circle.  Modifying it once at placement means every downstream
  consumer (targeting, preview, HUD) automatically sees the corrected value.
- No per-frame branch is needed; the stored range is already the night range.

---

## Concept 2 — Per-tower-type bonuses in night mode

Night mode is not simply "nerf everything" — it creates a **tower meta** where
certain types become dominant.

| Tower type    | Night adjustment              | Rationale                         |
|---------------|-------------------------------|-----------------------------------|
| `TOWER_DART`  | range × 1.2                   | Sniper — sees farther in the dark |
| `TOWER_PELLET`| fire_rate × 1.5               | Machine gun — rapid-fire at night |
| All others    | range × 0.75                  | Impaired without night vision     |

The asymmetry forces the player to rethink their build.  In the default mode
`TOWER_SNAP` (high single-shot damage) and `TOWER_BASH` (AoE stun) are
all-rounders.  At night their reduced range makes them less effective,
pushing the player toward Dart snipers and Pellet spam.

### Fire rate modification (TOWER_PELLET)

```c
} else if (t->type == TOWER_PELLET) {
    t->fire_rate *= 1.5f;
```

`fire_rate` is stored in shots-per-second.  Multiplying by 1.5 means the
pellet tower fires 50 % more shots per second.  Because the cooldown is
`1.0 / fire_rate`, a higher `fire_rate` means a shorter cooldown — the tower
fires more frequently.

This is a different axis than range: the pellet compensates for its short range
(unaffected by the ×0.75 penalty) with increased throughput.

---

## Concept 3 — Visual overlays and glow effects

### The night overlay

```c
/* Night overlay (MOD_NIGHT): dark blue semi-transparent wash */
if (s->active_mod == MOD_NIGHT) {
    draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                    GAME_RGBA(0x00, 0x00, 0x20, 0x80));
}
```

`GAME_RGBA(0x00, 0x00, 0x20, 0x80)` is a very dark navy blue with 50 % alpha
(0x80 / 255 ≈ 0.50).  This darkens the grid while preserving enough colour
information that the player can still read terrain.

Draw order matters:

```
1. Grid tiles           (light grey checker)
2. Terrain overlay      (MOD_TERRAIN, if active — separate mod)
3. Night overlay        (darkens everything below)
4. Hover placement preview
5. Towers               (drawn on top of the dark overlay)
6. Creeps
...
```

The night overlay is drawn *before* towers and creeps so those entities appear
"lit" against the dark background, as if each tower emits its own faint light.

### Tower glow effect

```c
/* MOD_NIGHT: yellow glow outline one pixel outside the tower radius */
if (s->active_mod == MOD_NIGHT) {
    draw_circle_outline(bb, t->cx, t->cy,
                        CELL_SIZE / 2 + 1, GAME_RGB(0xFF, 0xFF, 0x00));
}
```

A one-pixel circle in bright yellow is drawn around each tower after its body
and barrel.  The radius `CELL_SIZE / 2 + 1 = 11` is one pixel outside the
tower body, creating a halo effect.

`draw_circle_outline` uses Bresenham's midpoint algorithm, so the glow is a
thin single-pixel ring rather than a filled disc.  This is subtle but
immediately communicates "night vision active" to the player.

### Boss shield glow (MOD_BOSS cross-reference)

The same `draw_circle_outline` technique is reused in `MOD_BOSS` for the boss
shield:

```c
if (s->active_mod == MOD_BOSS && c->type == CREEP_BOSS &&
    c->shield_timer > 0.0f) {
    draw_circle_outline(bb, cx, cy, r + 4, GAME_RGB(0xFF, 0xFF, 0x00));
}
```

Reusing the same visual language (yellow outline = special state) across mods
is good UX — the player builds intuition faster.

---

## Full implementation walkthrough

### Step 1 — No new fields in GameState

MOD_NIGHT does not require any new persistent state.  The night adjustments are
baked into the Tower struct fields at placement time.  `active_mod` is the only
runtime signal needed.

### Step 2 — apply_night_bonus (called once at tower placement)

```c
static void apply_night_bonus(GameState *s, Tower *t)
{
    if (s->active_mod != MOD_NIGHT) return;
    if (t->type == TOWER_DART)
        t->range     *= 1.2f;
    else if (t->type == TOWER_PELLET)
        t->fire_rate *= 1.5f;
    else
        t->range     *= 0.75f;
}
```

### Step 3 — Call it in handle_placement_input

```c
/* After building the Tower struct and calling bfs_fill_distance(): */
apply_night_bonus(s, t);
game_play_sound(&s->audio, SFX_TOWER_PLACE);
```

### Step 4 — Night overlay in game_render (draw order matters)

```c
/* ---- Grid tiles ---- */
for (int r = 0; r < GRID_ROWS; r++) {
    /* ... */
}

/* Night overlay before hover preview and tower rendering */
if (s->active_mod == MOD_NIGHT) {
    draw_rect_blend(bb, 0, 0, GRID_PIXEL_W, GRID_PIXEL_H,
                    GAME_RGBA(0x00, 0x00, 0x20, 0x80));
}

/* ---- Hover placement preview ---- */
/* ---- Towers ---- */
```

### Step 5 — Per-tower glow in the tower rendering loop

```c
for (int i = 0; i < s->tower_count; i++) {
    const Tower *t = &s->towers[i];
    if (!t->active) continue;
    /* ... existing body + barrel rendering ... */

    /* MOD_NIGHT: yellow glow outline */
    if (s->active_mod == MOD_NIGHT) {
        draw_circle_outline(bb, t->cx, t->cy,
                            CELL_SIZE / 2 + 1, GAME_RGB(0xFF, 0xFF, 0x00));
    }
}
```

### Step 6 — HUD indicator

```c
} else if (s->active_mod == MOD_NIGHT) {
    draw_text(bb, PANEL_X + 4, mi_y, "Night Mode",
              GAME_RGB(0x88, 0x88, 0xFF), 1);
}
```

A dim blue-purple label appears just above the Start Wave button.

---

## Gameplay implications

| Effect                          | Strategic impact                             |
|---------------------------------|----------------------------------------------|
| Most towers: range × 0.75       | Maze needs to be tighter; more towers needed |
| TOWER_DART: range × 1.2         | Dart towers cover more ground than usual     |
| TOWER_PELLET: fire_rate × 1.5   | Pellets become reliable early-wave DPS       |
| Dark overlay                    | Visual difficulty — harder to read creep HP  |

The optimal Night Mode build is radically different from the default:
instead of building Snap/Bash, players should front-load Pellet towers for
the fire-rate bonus and invest in a few Dart towers as late-game snipers.

---

## Summary

| Takeaway                              | Implementation detail                      |
|---------------------------------------|--------------------------------------------|
| One-time modifier at placement        | `apply_night_bonus(s, t)` after build      |
| Three tower classes                   | Dart+, Pellet+, everything else −          |
| Range stored in tower struct          | downstream systems need no changes         |
| Night overlay is additive/blended     | drawn before towers, after grid            |
| Yellow glow via draw_circle_outline   | one pixel wider than tower body            |
| No new GameState fields needed        | `active_mod` is sufficient                 |
