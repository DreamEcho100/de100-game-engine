# DE100 Engine - Code Style Guide

> Consistent, C-idiomatic, Casey-inspired.

---

## Namespace Strategy

The engine uses `De100` / `de100_` prefix to avoid collisions when integrated into games.

| Code Location    | Types             | Functions               | Enums                   |
| ---------------- | ----------------- | ----------------------- | ----------------------- |
| `engine/*`       | `De100PascalCase` | `de100_module_action()` | `DE100_SCREAMING_SNAKE` |
| `yourgame/src/*` | `PascalCase`      | `snake_case()`          | `SCREAMING_SNAKE`       |

---

## Quick Reference

| Element          | Style             | Example                                     |
| ---------------- | ----------------- | ------------------------------------------- |
| Engine types     | `De100PascalCase` | `De100MemoryBlock`, `De100GameInput`        |
| Engine enums     | `DE100_SCREAMING` | `DE100_MEMORY_OK`, `DE100_FILE_ERROR`       |
| Engine functions | `de100_snake`     | `de100_memory_alloc()`, `de100_file_copy()` |
| Game types       | `PascalCase`      | `PlayerState`, `EnemyConfig`                |
| Game enums       | `SCREAMING_SNAKE` | `PLAYER_IDLE`, `ENEMY_ATTACKING`            |
| Game functions   | `snake_case`      | `update_player()`, `spawn_enemy()`          |
| Variables        | `snake_case`      | `frame_counter`, `is_valid`                 |
| Globals          | `g_snake_case`    | `g_frame_counter`, `g_is_running`           |
| Macros/Constants | `DE100_SCREAMING` | `DE100_KILOBYTES()`, `DE100_INTERNAL`       |
| Files            | `kebab-case`      | `debug-file-io.c`, `game-loader.h`          |

---

## Engine Types

```c
// engine/_common/memory.h
typedef struct {
    void *base;
    size_t size;
    bool is_valid;
} De100MemoryBlock;

typedef enum {
    DE100_MEMORY_OK = 0,
    DE100_MEMORY_ERR_OUT_OF_MEMORY,
    DE100_MEMORY_ERR_INVALID_SIZE,
} De100MemoryError;

typedef enum {
    DE100_MEMORY_FLAG_NONE    = 0,
    DE100_MEMORY_FLAG_READ    = 1 << 0,
    DE100_MEMORY_FLAG_WRITE   = 1 << 1,
    DE100_MEMORY_FLAG_EXECUTE = 1 << 2,
} De100MemoryFlags;
```

---

## Engine Functions

```c
// Pattern: de100_module_action()
De100MemoryBlock de100_memory_alloc(void *base, size_t size, De100MemoryFlags flags);
De100MemoryError de100_memory_free(De100MemoryBlock *block);
const char *de100_memory_error_str(De100MemoryError error);

De100FileResult de100_file_copy(const char *src, const char *dst);
De100PathResult de100_path_join(const char *dir, const char *file);

real64 de100_time_get_wall_clock(void);
void de100_time_sleep_ms(uint32 ms);
```

---

## Game Types (Your Game - No Prefix)

```c
// handmadehero/src/main.h
typedef struct {
    real32 frequency;
    real32 volume;
} SoundSource;

typedef struct {
    SoundSource tone;
    int offset_x;
    int offset_y;
} GameState;
```

---

## Game Functions (Your Game - No Prefix)

```c
// handmadehero/src/main.c
void update_player(GameState *state, De100GameInput *input);
void render_gradient(De100GameBackBuffer *buffer, int offset_x, int offset_y);
```

---

## Variables

```c
// Local variables
int32 frame_counter = 0;
real32 delta_time = 0.016f;
bool is_running = true;

// Globals (g_ prefix)
file_scoped_global_var bool g_is_running = true;
file_scoped_global_var int32 g_frame_counter = 0;

// Struct members
typedef struct {
    int32 sample_rate;
    int32 buffer_size;
    bool is_initialized;
} AudioConfig;
```

---

## Macros & Constants

```c
// Engine macros - DE100_ prefix
#define DE100_KILOBYTES(n) ((n) * 1024LL)
#define DE100_MEGABYTES(n) (DE100_KILOBYTES(n) * 1024LL)
#define DE100_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define DE100_INTERNAL 1
#define DE100_SLOW 1

// Game macros - no prefix
#define MAX_ENEMIES 100
#define PLAYER_SPEED 5.0f
```

---

## File Structure

```
project/
├── engine/
│   ├── _common/
│   │   ├── base.h
│   │   ├── memory.h          ← De100MemoryBlock
│   │   ├── memory.c          ← de100_memory_alloc()
│   │   ├── debug-file-io.h
│   │   └── debug-file-io.c
│   ├── game/
│   │   ├── game-loader.h     ← De100GameCode
│   │   └── input.h           ← De100GameInput
│   └── platform/
│       └── x11/
│           ├── backend.c
│           └── audio.c
└── handmadehero/
    └── src/
        ├── main.c            ← GameState (no prefix)
        └── startup.c
```

---

## Header Guards

```c
// Engine headers
#ifndef DE100_COMMON_MEMORY_H
#define DE100_COMMON_MEMORY_H
// ...
#endif // DE100_COMMON_MEMORY_H

// Game headers
#ifndef HANDMADE_HERO_MAIN_H
#define HANDMADE_HERO_MAIN_H
// ...
#endif // HANDMADE_HERO_MAIN_H
```

---

## Comments

```c
// Single-line for brief notes

/*
 * Multi-line for longer explanations
 * that span multiple lines.
 */

// ═══════════════════════════════════════════════════════════
// SECTION HEADERS (for major divisions)
// ═══════════════════════════════════════════════════════════
```

---

## Standard Macros (from `base.h`)

```c
file_scoped_fn          // static function
file_scoped_global_var  // static global
local_persist_var       // static local

DE100_ASSERT(expr)      // Always active
DE100_DEV_ASSERT(expr)  // Only in DE100_SLOW builds
```

---

## Do's and Don'ts

```c
// ✅ DO - Engine code
typedef struct { ... } De100GameState;
De100MemoryBlock de100_memory_alloc(...);
#define DE100_MAX_CONTROLLERS 4

// ✅ DO - Game code
typedef struct { ... } PlayerState;
void update_player(PlayerState *state);
#define MAX_ENEMIES 100

// ❌ DON'T
typedef struct { ... } memoryBlock;     // Wrong: camelCase type
void De100MemoryAlloc(...);             // Wrong: PascalCase function
int32 frameCounter = 0;                 // Wrong: camelCase variable
```

---

## Summary

```
ENGINE (reusable):
  Types:      De100PascalCase
  Functions:  de100_module_action
  Enums:      DE100_SCREAMING_SNAKE
  Macros:     DE100_SCREAMING_SNAKE

GAME (your code):
  Types:      PascalCase
  Functions:  snake_case
  Enums:      SCREAMING_SNAKE
  Macros:     SCREAMING_SNAKE

SHARED:
  Variables:  snake_case
  Globals:    g_snake_case
  Files:      kebab-case
```
