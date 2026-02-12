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

#define TILE_MAPS_Y_COUNT 2
#define TILE_MAPS_X_COUNT 2

#define TILES_PER_MAP_Y_COUNT 9
#define TILES_PER_MAP_X_COUNT 17

// ═══════════════════════════════════════════════════════════════════════════
// COORDINATE SYSTEM OVERVIEW
// ═══════════════════════════════════════════════════════════════════════════
//
// We have THREE coordinate spaces:
//
// 1. SCREEN SPACE (pixels)
//    - What we draw to the backbuffer
//    - Origin at top-left of window
//    - Units: pixels
//
// 2. WORLD SPACE (tilemaps + tiles + sub-tile)
//    - Logical game world position
//    - Hierarchical: which tilemap → which tile → where in tile
//    - Units: tilemap index, tile index, pixels within tile
//
// 3. RAW WORLD SPACE (tilemaps + pixel offset)
//    - Intermediate format for movement calculations
//    - Pixel offset is relative to tilemap origin, NOT tile
//    - Units: tilemap index, pixels from tilemap origin
//
// ═══════════════════════════════════════════════════════════════════════════

// ───────────────────────────────────────────────────────────────────────────
// WorldCanonicalPosition (BETTER NAME: TilePosition or WorldTileCoord)
// ───────────────────────────────────────────────────────────────────────────
// This is the "fully resolved" position in the world.
// Think of it as a FULL ADDRESS: Country → City → Street → House Number
//
// tilemap_x/y  = Which tilemap (which "room" or "screen")
// tile_x/y     = Which tile within that tilemap (which "cell")
// tile_rel_*   = Pixel offset within that tile (where in the "cell")
//
// INVARIANTS:
//   - 0 <= tile_x < tiles_per_map_x_count
//   - 0 <= tile_y < tiles_per_map_y_count
//   - 0 <= tile_rel_offset_x < tilemap_width (tile width in pixels)
//   - 0 <= tile_rel_offset_y < tilemap_height (tile height in pixels)
// ───────────────────────────────────────────────────────────────────────────
typedef struct {
  // Which tilemap in the world grid (like which "screen" or "room")
  int32 tilemap_x; // Column index in world's tilemap grid
  int32 tilemap_y; // Row index in world's tilemap grid

  // Which tile within that tilemap (like which "cell" in the grid)
  int32 tile_x; // Column index within tilemap (0 to tiles_per_map_x - 1)
  int32 tile_y; // Row index within tilemap (0 to tiles_per_map_y - 1)

  // Pixel offset WITHIN the tile (sub-tile precision)
  // Range: [0, tilemap_width) and [0, tilemap_height)
  // NOTE: "tilemap_width" is actually TILE width in pixels (confusing name!)
  // TODO: These are still in pixels... :/
  real32 tile_rel_offset_x; // Pixels from left edge of tile
  real32 tile_rel_offset_y; // Pixels from top edge of tile
} WorldCanonicalPosition;

// ───────────────────────────────────────────────────────────────────────────
// WorldRawPosition (BETTER NAME: TilemapRelativePosition)
// ───────────────────────────────────────────────────────────────────────────
// This is an UNRESOLVED position - used during movement calculations.
// The offset can be OUTSIDE the current tilemap bounds!
//
// Example: Player at tile 16 moves right → offset_x might be 1020 pixels
//          But tilemap is only 1020 pixels wide (17 tiles × 60 pixels)
//          So this position needs to be "canonicalized" to tilemap[1], tile 0
//
// Think of it as: "I'm in tilemap X,Y and I'm THIS many pixels from its origin"
// The pixels might overflow into adjacent tilemaps - that's OK, we fix it
// later.
// ───────────────────────────────────────────────────────────────────────────
// TODO: Is this ever necessary?
typedef struct {
  // Which tilemap we THINK we're in (might change after canonicalization)
  int32 tilemap_x;
  int32 tilemap_y;

  // Pixel offset from the tilemap's origin (top-left corner)
  // WARNING: These can be NEGATIVE or LARGER than tilemap size!
  // That means we've crossed into an adjacent tilemap.
  real32 offset_x; // Pixels from tilemap origin (can overflow!)
  real32 offset_y; // Pixels from tilemap origin (can overflow!)
} TilemapRelativePosition;

// ───────────────────────────────────────────────────────────────────────────
// Tilemap
// ───────────────────────────────────────────────────────────────────────────
// A single "screen" or "room" in the world.
// Contains a 2D grid of tile IDs.
//
// Tile ID meanings (game-specific):
//   0 = Empty/walkable
//   1 = Wall/solid
// ───────────────────────────────────────────────────────────────────────────
typedef struct {
  // Flat array storing tile IDs in row-major order
  // Access: tiles[tile_y * tiles_per_map_x_count + tile_x]
  uint32 *tiles;
} Tilemap;

// ───────────────────────────────────────────────────────────────────────────
// World
// ───────────────────────────────────────────────────────────────────────────
// The entire game world - a grid of tilemaps.
//
// CONFUSING NAMING ALERT:
//   tilemap_width/height = Size of ONE TILE in pixels (should be tile_width!)
//   tiles_per_map_x/y    = How many tiles fit in one tilemap
//   tilemaps_count_x/y   = How many tilemaps in the world grid
// ───────────────────────────────────────────────────────────────────────────
typedef struct {
  // ─── Screen-space offset ───
  // Where tilemap[0,0]'s origin appears on screen (pixels)
  // Usually negative to center the view or provide margin
  real32 origin_x;
  real32 origin_y;

  // ─── Tile dimensions ───
  // These are the size of ONE TILE in pixels, NOT the tilemap!
  real32 tilemap_width_px;
  real32 tilemap_height_px;

  // ─── Tilemap grid dimensions ───
  // How many tiles in each tilemap
  int32 tiles_per_map_x_count; // e.g., 17 tiles wide
  int32 tiles_per_map_y_count; // e.g., 9 tiles tall

  // ─── World grid dimensions ───
  // How many tilemaps in the world
  int32 tilemaps_count_x; // e.g., 2 tilemaps horizontally
  int32 tilemaps_count_y; // e.g., 2 tilemaps vertically

  // ─── Tilemap storage ───
  // Flat array of tilemaps in row-major order
  // Access: tilemaps[tilemap_y * tilemaps_count_x + tilemap_x]
  Tilemap *tilemaps;
} World;

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

  int32 tilemap_x;
  int32 tilemap_y;
} Player;
typedef struct {
  // Audio state (embedded struct, not pointer)
  GameAudioState audio;
  // Future: More game-specific state
  // PlayerState player;
  // WorldState world;

  // // Audio state (matches Casey's game_state)
  // int32 tone_hz;
  // real32 t_sine; // Phase accumulator

  Tilemap *tilemaps;
  World world;
  Tilemap *active_tilemap;
  Player player;
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
    size_t tiles_size = sizeof(uint32) * TILES_PER_MAP_Y_COUNT *
TILES_PER_MAP_X_COUNT; game_state->tile_state.maps = (uint32 *)push_size(memory,
game_state, tiles_size);

    // Now copy is safe
    uint32 temp_map_tiles[] = {
        1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
        // ... rest of tiles ...
    };

    game_state->tile_state.y_count = TILES_PER_MAP_Y_COUNT;
    game_state->tile_state.x_count = TILES_PER_MAP_X_COUNT;
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