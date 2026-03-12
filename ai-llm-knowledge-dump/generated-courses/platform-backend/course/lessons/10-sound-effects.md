# Lesson 10 — Sound Effects

## Overview

| Item | Value |
|------|-------|
| **Backend** | Both |
| **New concepts** | `SoundDef` table, `game_play_sound_at` (free-slot + steal-oldest), `ToneGenerator` fields |
| **Observable outcome** | Three distinct tones (low/mid/high) triggered by Space; voices mix correctly |
| **Files modified** | `src/game/audio_demo.c`, `src/utils/audio.h` |

---

## What you'll build

A voice pool with a static sound-definition table.  Pressing Space triggers a sound effect from the pool.  When all 8 voices are busy, the oldest voice is stolen.

---

## Background

### JS analogy

```js
// Like a Web Audio node pool:
function playSoundAt(soundId) {
    const def = SOUND_DEFS[soundId];
    let target = voices.find(v => !v.active);  // free slot
    if (!target) target = voices.reduce(...);   // steal oldest
    Object.assign(target, { ...def, active: true, age: nextAge++ });
}
```

### `SoundDef` — static descriptor

```c
typedef struct {
    float frequency;
    float freq_slide;   // Hz/sec — 0 for pure tones
    float volume;       // 0.0 – 1.0
    float pan;          // -1.0 (left) to +1.0 (right)
    int   duration_ms;  // -1 = loop forever
} SoundDef;
```

`SOUND_DEFS[]` is a `static const` table indexed by `SoundID` enum.  It never changes at runtime.

### Free-slot + steal-oldest policy

```c
void game_play_sound_at(GameAudioState *audio, SoundID id) {
    int target = -1, oldest_age = -1;
    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (!audio->voices[i].active) { target = i; break; }
        if (oldest_age < 0 || audio->voices[i].age < oldest_age) {
            oldest_age = audio->voices[i].age;
            target     = i;
        }
    }
    // Initialize target voice from SOUND_DEFS[id]...
}
```

**Why steal oldest?**  The oldest voice has been playing the longest — it has contributed the most energy already.  Stealing it causes the least audible artifact compared to stealing the newest.

### `freq_slide` (pitch sweep)

`freq_slide` (Hz/second) lets you create laser-gun, coin-collect, and power-up effects without separate sound files.  It's applied by `game_audio_update` in the game loop:

```c
gen->frequency += gen->freq_slide * (dt_ms / 1000.0f);
```

---

## What to type

### `src/utils/audio.h` — `SoundID` enum (add)

```c
typedef enum {
    SOUND_TONE_LOW   = 0,  /* 220 Hz A3 */
    SOUND_TONE_MID   = 1,  /* 440 Hz A4 */
    SOUND_TONE_HIGH  = 2,  /* 880 Hz A5 */
    SOUND_COUNT
} SoundID;
```

### `src/game/audio_demo.c` — `SOUND_DEFS` table

```c
static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
    /* SOUND_TONE_LOW  */ { 220.0f, 0.0f, 0.5f,  0.0f, 300 },
    /* SOUND_TONE_MID  */ { 440.0f, 0.0f, 0.5f,  0.0f, 300 },
    /* SOUND_TONE_HIGH */ { 880.0f, 0.0f, 0.5f, -0.2f, 200 },
};
```

### `game_play_sound_at` (add to `audio_demo.c`)

```c
void game_play_sound_at(GameAudioState *audio, SoundID id) {
    if (id < 0 || id >= SOUND_COUNT) return;
    const SoundDef *def = &SOUND_DEFS[id];

    int target = -1, oldest_age = -1;
    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (!audio->voices[i].active) { target = i; break; }
        if (oldest_age < 0 || audio->voices[i].age < oldest_age) {
            oldest_age = audio->voices[i].age;
            target     = i;
        }
    }
    if (target < 0) return;

    int total = (def->duration_ms < 0)
        ? -1
        : (def->duration_ms * audio->samples_per_second / 1000);

    ToneGenerator *v   = &audio->voices[target];
    v->phase_acc         = 0.0f;
    v->frequency         = def->frequency;
    v->freq_slide        = def->freq_slide;
    v->volume            = def->volume;
    v->pan               = def->pan;
    v->samples_remaining = total;
    v->total_samples     = (total > 0) ? total : 0;  /* for decay envelope */
    v->fade_in_samples   = (total > 0) ? 88 : 0;     /* 2ms attack, anti-click */
    v->active            = 1;
    v->age               = audio->next_age++;
}
```

**Why `total_samples` and `fade_in_samples`?**

The two new fields enable click-free synthesis (see also Lesson 09):

| Field | Purpose |
|-------|---------|
| `total_samples` | Set once at trigger = `samples_remaining`.  Used in the synthesis loop to compute `env = remaining / total` — a decay envelope that continuously ramps amplitude from 1.0→0.0 over the entire note, reaching exactly 0 when the voice deactivates.  No abrupt cut-off. |
| `fade_in_samples` | First `N` samples ramp amplitude from 0→1.  At 44100 Hz, `88` samples ≈ 2 ms — long enough to prevent the start-of-note click (where the wave jumps from silence to full amplitude), short enough to not audibly blunt the attack. |

Together they form a simple attack-decay shape:

```
amplitude
1.0 ──────────────────────────\
    |        (decay slope)     \
    /  (attack ramp, 2ms)       \
0.0 ──────────────────────────── time
    ^                           ^
    note start             note end
```

### Main loop — cycle through sounds on Space press

```c
static int tone_index = 0;
if (button_just_pressed(curr_input->buttons.play_tone)) {
    game_play_sound_at(&audio_state, (SoundID)(tone_index % SOUND_COUNT));
    tone_index++;
}
```

---

## Explanation

| Concept | Detail |
|---------|--------|
| `SoundDef` table | Static const; zero runtime allocation; indexed by enum |
| `age` counter | Monotonically increasing integer assigned to each new voice; `steal oldest` = steal lowest age |
| `samples_remaining = ms * rate / 1000` | Integer frame count, decremented every synthesis step |
| `total_samples` | Set = `samples_remaining` on trigger; enables the decay envelope `env = remaining / total` |
| `fade_in_samples = 88` | 2ms attack ramp prevents start-of-note click; combined with decay gives clean attack-decay shape |
| `pan = -0.2` on SOUND_TONE_HIGH | Slightly left-panned to give the three tones spatial separation |
| `freq_slide` | Applied by game loop (not audio loop) — avoids per-sample multiplies in synthesis |

---

## Build and run

```sh
./build-dev.sh --backend=raylib -r
```

Press Space repeatedly to cycle through low/mid/high tones.  Press quickly to fill the voice pool and hear the steal behavior.

---

## Quiz

1. Why does `game_play_sound_at` use `samples_remaining = ms * AUDIO_SAMPLE_RATE / 1000` (integer) instead of a float timer?
2. If you press Space 9 times in quick succession (filling all 8 voices), which voice is stolen for the 9th sound?
3. What `freq_slide` value (Hz/sec) would make a tone slide from 440 Hz down to 220 Hz over exactly 0.5 seconds?
