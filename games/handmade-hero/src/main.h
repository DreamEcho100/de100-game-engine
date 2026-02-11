#ifndef DE100_HERO_GAME_H
#define DE100_HERO_GAME_H
#include "./inputs.h"

#include "../../../engine/_common/base.h"
#include "../../../engine/_common/memory.h"
#include "../../../engine/game/audio.h"
#include "../../../engine/game/backbuffer.h"
#include "../../../engine/game/base.h"
#include "../../../engine/game/game-loader.h"
#include "../../../engine/game/inputs.h"
#include "../../../engine/game/memory.h"

GameControllerInput *GetController(GameInput *Input,
                                   unsigned int ControllerIndex);

#define TILE_MAP_ROW_COUNT 9
#define TILE_MAP_COLUMN_COUNT 17
typedef struct {
  // uint32 map[TILE_MAP_ROW_COUNT][TILE_MAP_COLUMN_COUNT];
  uint32 *tiles;
  uint32 row_count;
  uint32 column_count;
  real32 origin_x;
  real32 origin_y;
  real32 width;
  real32 height;
} TileMapState;

// typedef struct {
//     int32 row;
//     int32 col;
// } TileCoord;

typedef struct {
  // TODO(casey): Beginner's sparseness
  int32 tile_map_count_x;
  int32 tile_map_count_y;

  TileMapState *tile_maps;
} WorldState;

typedef struct {
  int32 x;
  int32 y;
  real32 width;
  real32 height;
  real32 t_jump;
  real32 color_r;
  real32 color_g;
  real32 color_b;
  real32 color_a;
} PlayerState;
typedef struct {
  // Audio state (embedded struct, not pointer)
  GameAudioState audio;
  // Future: More game-specific state
  // PlayerState player;
  // WorldState world;

  // // Audio state (matches Casey's game_state)
  // int32 tone_hz;
  // real32 t_sine; // Phase accumulator

  // TileMapState tile_maps;
  WorldState world;
  TileMapState *active_tile_map;
  PlayerState player_state;
  // Your other state
  int32 speed;
} HandMadeHeroGameState;

#endif // DE100_HERO_GAME_H

// TODO: use arena allocator for tile map instead of static array
// Arena example
/*
You need a **memory arena** - track how much of `permanent_storage` you've used.

**Add a `used` field to your game state or memory struct:**

```c:games/handmade-hero/src/main.h
typedef struct {
  // ... existing fields ...
  GameAudioState audio;
  TileState tile_state;
  PlayerState player_state;
  int32 speed;

  // Arena allocator - tracks where free memory starts
  size_t permanent_storage_used;
} HandMadeHeroGameState;
```

**Create a simple push allocator:**

```c:games/handmade-hero/src/init.c
// Simple arena push - returns pointer to allocated memory
void *push_size(GameMemory *memory, HandMadeHeroGameState *game_state, size_t
size) {
    // Align to 8 bytes
    size_t aligned_size = (size + 7) & ~7;

    void *result = (uint8 *)memory->permanent_storage +
game_state->permanent_storage_used; game_state->permanent_storage_used +=
aligned_size;

    // Safety check
    DEV_ASSERT_MSG(game_state->permanent_storage_used <=
memory->permanent_storage_size, "Out of permanent storage!");

    return result;
}
```

**Use it in init:**

```c:games/handmade-hero/src/init.c
if (!memory->is_initialized) {
    // game_state is at the START of permanent_storage
    // Mark that space as used
    game_state->permanent_storage_used = sizeof(HandMadeHeroGameState);

    // ... audio init, speed, etc ...

    // Allocate tiles from arena
    size_t tiles_size = sizeof(uint32) * TILE_MAP_ROW_COUNT *
TILE_MAP_COLUMN_COUNT; game_state->tile_state.maps = (uint32 *)push_size(memory,
game_state, tiles_size);

    // Now copy is safe
    uint32 temp_map_tiles[] = {
        1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
        // ... rest of tiles ...
    };

    game_state->tile_state.row_count = TILE_MAP_ROW_COUNT;
    game_state->tile_state.column_count = TILE_MAP_COLUMN_COUNT;
    de100_mem_copy(game_state->tile_state.maps, temp_map_tiles,
sizeof(temp_map_tiles));

    // Future allocations just call push_size() again
    // game_state->something_else = push_size(memory, game_state, some_size);

    memory->is_initialized = true;
}
```

**Memory layout:**

```
permanent_storage:
┌─────────────────────────────┐ 0
│ HandMadeHeroGameState       │
│   - audio                   │
│   - tile_state.maps ────────┼──┐ (pointer)
│   - player_state            │  │
│   - permanent_storage_used  │  │
├─────────────────────────────┤  │ sizeof(HandMadeHeroGameState)
│ tiles[9*17]                 │◄─┘
├─────────────────────────────┤
│ (future allocations...)     │
├─────────────────────────────┤
│ FREE SPACE                  │
└─────────────────────────────┘ permanent_storage_size
```

This is exactly how Casey does it in Handmade Hero. No malloc, no free, just
push forward.
*/