#include "main.h"
#include "../../../engine/_common/base.h"
#include "../../../engine/game/backbuffer.h"
#include "../../../engine/game/inputs.h"
#include "../../../engine/platforms/_common/hooks/utils.h"
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if 0
de100_file_scoped_fn inline real32 apply_deadzone(real32 value) {
  if (fabsf(value) < CONTROLLER_DEADZONE) {
    return 0.0f;
  }
  return value;
}
#endif

de100_file_scoped_fn inline int32 round_real32_to_int32(real32 num) {
  int32 result = (int32)(num + 0.5f);
  // TODO: Intrinsic????
  return (result);
}
de100_file_scoped_fn inline uint32 round_real32_to_uint32(real32 num) {
  uint32 result = (uint32)(num + 0.5f);
  // TODO: Intrinsic????
  return (result);
}

de100_file_scoped_fn inline int32 floor_real32_to_int32(real32 num) {
  int32 result = (int32)floorf(num);
  return (result);
}

de100_file_scoped_fn void draw_rect(GameBackBuffer *backbuffer,
                                    real32 real_min_x, real32 real_min_y,
                                    real32 real_max_x, real32 real_max_y,
                                    real32 r, real32 g, real32 b, real32 a) {
  (void)a;
  int32 min_x = real_min_x < 0 ? 0 : round_real32_to_int32(real_min_x);
  int32 min_y = real_min_y < 0 ? 0 : round_real32_to_int32(real_min_y);
  int32 max_x = real_max_x > backbuffer->width
                    ? backbuffer->width
                    : round_real32_to_int32(real_max_x);
  int32 max_y = real_max_y > backbuffer->height
                    ? backbuffer->height
                    : round_real32_to_int32(real_max_y);

  uint32 color =
      // AARRGGBB format - works for BOTH OpenGL X11 AND Raylib!
      (round_real32_to_uint32(a * 255) << 24 |
       round_real32_to_uint32(r * 255) << 16 |
       round_real32_to_uint32(g * 255) << 8 |
       round_real32_to_uint32(b * 255) << 0);

  uint8 *xy_mem_pos =
      // Base address to start from
      (uint8 *)backbuffer->memory.base +
      // Where it should be on x/row
      backbuffer->bytes_per_pixel * min_x +
      // Where it should be on y/column
      backbuffer->pitch * min_y;

  for (int32 y = min_y; y < max_y; ++y) {
    uint32 *pixel_mem_pos = (uint32 *)xy_mem_pos;
    for (int32 x = min_x; x < max_x; ++x) {
      *pixel_mem_pos++ = color;
    }
    xy_mem_pos += backbuffer->pitch;
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// TILE ACCESS FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

// ───────────────────────────────────────────────────────────────────────────
// get_tile_value_unchecked
// ───────────────────────────────────────────────────────────────────────────
// Returns the tile ID at (tile_x, tile_y) within the given tilemap.
//
// PRECONDITIONS (enforced by DEV_ASSERT in debug builds):
//   - tilemap != NULL
//   - 0 <= tile_x < world->tiles_per_map_x_count
//   - 0 <= tile_y < world->tiles_per_map_y_count
//
// RETURNS:
//   Tile ID (0 = empty, 1 = wall, etc.)
//
// WHY "UNCHECKED"?
//   This function assumes valid inputs for SPEED (hot path).
//   Use check_if_tilemap_point_empty() for safe access.
// ───────────────────────────────────────────────────────────────────────────
de100_file_scoped_fn inline uint32 get_tile_value_unchecked(World *world,
                                                            Tilemap *tilemap,
                                                            int32 tile_x,
                                                            int32 tile_y) {
  DEV_ASSERT_MSG(tilemap, "Tile map is null!");
  DEV_ASSERT_MSG((tile_x >= 0 && tile_x < world->tiles_per_map_x_count),
                 "Tile x out of bounds! tile_x: %d, tiles_per_map_x_count: %d",
                 tile_x, world->tiles_per_map_x_count);
  DEV_ASSERT_MSG((tile_y >= 0 && tile_y < world->tiles_per_map_y_count),
                 "Tile y out of bounds! tile_y: %d, tiles_per_map_y_count: %d",
                 tile_y, world->tiles_per_map_y_count);

  // Row-major indexing: row * width + column
  // ┌───┬───┬───┬───┐
  // │ 0 │ 1 │ 2 │ 3 │  row 0: indices 0-3
  // ├───┼───┼───┼───┤
  // │ 4 │ 5 │ 6 │ 7 │  row 1: indices 4-7
  // ├───┼───┼───┼───┤
  // │ 8 │ 9 │10 │11 │  row 2: indices 8-11
  // └───┴───┴───┴───┘
  // tile[1,2] = tiles[2 * 4 + 1] = tiles[9]
  return tilemap->tiles[tile_y * world->tiles_per_map_x_count + tile_x];
}

// ───────────────────────────────────────────────────────────────────────────
// check_if_tilemap_point_empty
// ───────────────────────────────────────────────────────────────────────────
// SAFELY checks if a tile is empty (walkable).
//
// Unlike get_tile_value_unchecked, this handles:
//   - NULL tilemap (returns false = not empty = blocked)
//   - Out-of-bounds coordinates (returns false = blocked)
//
// RETURNS:
//   true  = tile exists and is empty (tile_id == 0)
//   false = tile is solid, out of bounds, or tilemap is NULL
// ───────────────────────────────────────────────────────────────────────────
de100_file_scoped_fn inline bool32
check_if_tilemap_point_empty(World *world, Tilemap *tilemap, int32 tile_x,
                             int32 tile_y) {
  bool32 is_empty = false;

  // Guard: NULL tilemap means we're outside the world → blocked
  if (tilemap) {
    // Guard: Out-of-bounds tile coordinates → blocked
    if ((tile_x >= 0) && (tile_x < world->tiles_per_map_x_count) &&
        (tile_y >= 0) && (tile_y < world->tiles_per_map_y_count)) {
      uint32 tilemap_value =
          get_tile_value_unchecked(world, tilemap, tile_x, tile_y);

      // Convention: 0 = empty/walkable, non-zero = solid
      is_empty = (tilemap_value == 0);
    }
  }

  return is_empty;
}

// ───────────────────────────────────────────────────────────────────────────
// get_tilemap
// ───────────────────────────────────────────────────────────────────────────
// Returns a pointer to the tilemap at (tilemap_x, tilemap_y) in the world.
//
// RETURNS:
//   Pointer to tilemap if coordinates are valid
//   NULL if coordinates are out of bounds (outside world)
//
// USE CASE:
//   Tilemap *tm = get_tilemap(world, player.tilemap_x, player.tilemap_y);
//   if (tm) { /* player is in a valid tilemap */ }
// ───────────────────────────────────────────────────────────────────────────
de100_file_scoped_fn inline Tilemap *get_tilemap(World *world, int32 tilemap_x,
                                                 int32 tilemap_y) {
  Tilemap *tilemap = NULL; // NULL = invalid/outside world

  // Bounds check: is this tilemap coordinate within the world?
  if (tilemap_x >= 0 && tilemap_x < world->tilemaps_count_x && tilemap_y >= 0 &&
      tilemap_y < world->tilemaps_count_y) {
    // Row-major indexing into tilemap array
    tilemap = &world->tilemaps[tilemap_y * world->tilemaps_count_x + tilemap_x];
  }

  return tilemap;
}

// ═══════════════════════════════════════════════════════════════════════════
// COORDINATE CANONICALIZATION
// ═══════════════════════════════════════════════════════════════════════════

// ───────────────────────────────────────────────────────────────────────────
// get_canonical_pos
// ───────────────────────────────────────────────────────────────────────────
// Converts a "raw" position (tilemap + pixel offset) into a "canonical"
// position (tilemap + tile + sub-tile offset).
//
// This is the CORE function for handling tilemap transitions!
//
// WHAT IT DOES:
//   1. Subtracts world origin to get tilemap-relative coordinates
//   2. Divides by tile size to get tile indices
//   3. Computes remainder for sub-tile offset
//   4. Handles overflow/underflow into adjacent tilemaps
//
// EXAMPLE:
//   Raw: tilemap(0,0), offset(1050, 300)
//   Tilemap is 1020px wide (17 tiles × 60px)
//   → offset_x overflows by 30px
//   → Canonical: tilemap(1,0), tile(0,5), sub-offset(30, 0)
//
// ───────────────────────────────────────────────────────────────────────────
de100_file_scoped_fn inline WorldCanonicalPosition
get_canonical_pos(World *world, TilemapRelativePosition pos) {
  WorldCanonicalPosition result = {0};

  // Start with the same tilemap (might change if we overflow)
  result.tilemap_x = pos.tilemap_x;
  result.tilemap_y = pos.tilemap_y;

  // ─── Step 1: Convert to tilemap-relative coordinates ───
  // Subtract world origin to get position relative to tilemap[0,0]'s top-left
  //
  // Example: origin_x = -30 (tilemap starts 30px left of screen)
  //          offset_x = 150 (player is 150px from tilemap origin on screen)
  //          tilemap_offset_x = 150 - (-30) = 180px into the tilemap
  real32 tilemap_offset_x = pos.offset_x - world->origin_x;
  real32 tilemap_offset_y = pos.offset_y - world->origin_y;

  // ─── Step 2: Calculate tile indices ───
  // Divide by tile size (tilemap_width_px is actually TILE width in pixels)
  // floor() ensures negative values round toward negative infinity
  //
  // Example: tilemap_offset_x = 180, tile_width = 60
  //          tile_x = floor(180 / 60) = floor(3.0) = 3
  //
  // Example: tilemap_offset_x = -30, tile_width = 60
  //          tile_x = floor(-30 / 60) = floor(-0.5) = -1 (overflow left!)
  result.tile_x =
      floor_real32_to_int32(tilemap_offset_x / world->tilemap_width_px);
  result.tile_y =
      floor_real32_to_int32(tilemap_offset_y / world->tilemap_height_px);

  // ─── Step 3: Calculate sub-tile offset ───
  // This is the pixel position WITHIN the tile (0 to tile_size-1)
  //
  // Example: tilemap_offset_x = 180, tile_x = 3, tile_width = 60
  //          tile_rel_offset_x = 180 - (3 * 60) = 180 - 180 = 0
  //
  // Example: tilemap_offset_x = 195, tile_x = 3, tile_width = 60
  //          tile_rel_offset_x = 195 - (3 * 60) = 195 - 180 = 15
  result.tile_rel_offset_x =
      tilemap_offset_x - (result.tile_x * world->tilemap_width_px);
  result.tile_rel_offset_y =
      tilemap_offset_y - (result.tile_y * world->tilemap_height_px);

  // Sanity checks (should always pass if math is correct)
  DEV_ASSERT_MSG(result.tile_rel_offset_x >= 0, "tile_rel_offset_x < 0: %f",
                 result.tile_rel_offset_x);
  DEV_ASSERT_MSG(result.tile_rel_offset_y >= 0, "tile_rel_offset_y < 0: %f",
                 result.tile_rel_offset_y);
  DEV_ASSERT_MSG(result.tile_rel_offset_x < world->tilemap_width_px,
                 "tile_rel_offset_x >= tile_width: %f",
                 result.tile_rel_offset_x);
  DEV_ASSERT_MSG(result.tile_rel_offset_y < world->tilemap_height_px,
                 "tile_rel_offset_y >= tile_height: %f",
                 result.tile_rel_offset_y);

  // ─── Step 4: Handle tilemap transitions (overflow/underflow) ───
  //
  // If tile_x < 0, we've moved LEFT into the previous tilemap
  // If tile_x >= tiles_per_map_x, we've moved RIGHT into the next tilemap

  if (result.tile_x < 0) {
    // ┌─────────────────┬─────────────────┐
    // │  tilemap[0,0]   │  tilemap[1,0]   │
    // │                 │                 │
    // │  ●←─ Player was │                 │
    // │      at tile -1 │                 │
    // │                 │                 │
    // │  Wraps to tile  │                 │
    // │  (n-1) of       │                 │
    // │  tilemap[-1,0]  │                 │
    // └─────────────────┴─────────────────┘
    //
    // tile_x = -1 → should become tile (tiles_per_map - 1) in tilemap to the
    // left tile_x = -2 → should become tile (tiles_per_map - 2) in tilemap to
    // the left
    //
    // Formula: new_tile_x = tile_x + tiles_per_map
    // But Casey adds -1 here... let me trace through:
    //   tile_x = -1, tiles_per_map = 17
    //   result = -1 + 17 - 1 = 15  ← This seems wrong? Should be 16?
    //
    // TODO(DreamEcho100): Verify this logic with Casey's Day 30 video
    result.tile_x = result.tile_x + world->tiles_per_map_x_count - 1;
    --result.tilemap_x;
  }

  if (result.tile_y < 0) {
    // Same logic for vertical (moving UP into tilemap above)
    result.tile_y = result.tile_y + world->tiles_per_map_y_count;
    --result.tilemap_y;
  }

  if (result.tile_x >= world->tiles_per_map_x_count) {
    // ┌─────────────────┬─────────────────┐
    // │  tilemap[0,0]   │  tilemap[1,0]   │
    // │                 │                 │
    // │  Player was at  │  ●←─ Wraps to   │
    // │  tile 17        │      tile 0     │
    // │  (out of bounds)│                 │
    // └─────────────────┴─────────────────┘
    //
    // tile_x = 17, tiles_per_map = 17
    // result = 17 - 17 + 1 = 1  ← Should be 0?
    //
    // TODO(DreamEcho100): Verify this logic with Casey's Day 30 video
    result.tile_x = result.tile_x - world->tiles_per_map_x_count + 1;
    ++result.tilemap_x;
  }

  if (result.tile_y >= world->tiles_per_map_y_count) {
    // Same logic for vertical (moving DOWN into tilemap below)
    result.tile_y = result.tile_y - world->tiles_per_map_y_count;
    ++result.tilemap_y;
  }

  return result;
}

// ───────────────────────────────────────────────────────────────────────────
// is_world_point_empty
// ───────────────────────────────────────────────────────────────────────────
// The main collision check function!
//
// Given a raw position (which might overflow tilemap bounds), this:
//   1. Canonicalizes it (resolves to correct tilemap + tile)
//   2. Checks if that tile is empty
//
// RETURNS:
//   true  = position is walkable
//   false = position is blocked (wall, out of world, etc.)
// ───────────────────────────────────────────────────────────────────────────
de100_file_scoped_fn inline bool32
is_world_point_empty(World *world, TilemapRelativePosition pos) {
  bool32 is_empty = false;

  // Step 1: Resolve raw position to canonical (handles tilemap transitions)
  WorldCanonicalPosition canonical_pos = get_canonical_pos(world, pos);

  // Step 2: Get the tilemap at the canonical position
  // (might be NULL if we've left the world entirely)
  Tilemap *current_tilemap =
      get_tilemap(world, canonical_pos.tilemap_x, canonical_pos.tilemap_y);

  // Step 3: Check if the tile at that position is empty
  is_empty = check_if_tilemap_point_empty(
      world, current_tilemap, canonical_pos.tile_x, canonical_pos.tile_y);

  return is_empty;
}

// Handle game controls
void handle_controls(GameControllerInput *inputs, HandMadeHeroGameState *game,
                     real32 frame_time) {
#if 0
  if (inputs->action_down.ended_down && game->player.t_jump <= 0) {
    game->player.t_jump = 4.0; // Start jump
  }

  // Simple jump arc
  int jump_offset = 0;
  if (game->player.t_jump > 0) {
    jump_offset =
        (int)(5.0f * sinf(0.5f * M_PI * game->player.t_jump));
    game->player.y += jump_offset;
  }
  game->player.t_jump -= 0.033f; // Tick down jump timer
#endif

  if (inputs->is_analog) {
// NOTE: Use analog movement tuning
#if 0

    real32 stick_avg_x = apply_deadzone(inputs->stick_avg_x);
    real32 stick_avg_y = apply_deadzone(inputs->stick_avg_y);

    // Horizontal stick controls blue offset

    game->audio.tone.frequency = 256 + (int)(128.0f * stick_avg_y);
    
    // In GameUpdateAndRender:
    game->player.x += (int)(4.0f * stick_avg_x);
    game->player.y += (int)(4.0f * stick_avg_y);
#endif
  } else {
    // ═══════════════════════════════════════════════════════════════════════════
    // PLAYER MOVEMENT & COLLISION
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // Movement flow:
    //   1. Calculate desired new position based on input
    //   2. Check if new position collides with walls
    //   3. If no collision, update player position
    //   4. Canonicalize to handle tilemap transitions
    //
    // Collision points checked:
    //   ┌─────────────┐
    //   │   Player    │
    //   │             │
    //   │             │
    //   ●─────●─────● ← Bottom edge (left, center, right)
    //   └─────────────┘
    //
    //   We check 3 points because the player is wider than one tile.
    //   If ANY point hits a wall, movement is blocked.
    // ═══════════════════════════════════════════════════════════════════════════

    // Inside handle_controls(), digital movement section:

    // ─── Calculate movement delta ───
    real32 d_player_x = 0.0f; // Horizontal velocity direction (-1, 0, or 1)
    real32 d_player_y = 0.0f; // Vertical velocity direction (-1, 0, or 1)

    if (inputs->move_up.ended_down) {
      d_player_y = -1.0f; // Screen Y increases downward, so UP is negative
    }
    if (inputs->move_down.ended_down) {
      d_player_y = 1.0f; // DOWN is positive Y
    }
    if (inputs->move_left.ended_down) {
      d_player_x = -1.0f; // LEFT is negative X
    }
    if (inputs->move_right.ended_down) {
      d_player_x = 1.0f; // RIGHT is positive X
    }

    // Scale by speed (pixels per second)
    d_player_x *= game->speed; // e.g., 128 pixels/second
    d_player_y *= game->speed;

    // ─── Calculate proposed new position ───
    // This is a RAW position - the offset might overflow tilemap bounds!
    //
    // Position format:
    //   offset_x/y = screen-space pixel position (relative to world origin)
    //   tilemap_x/y = which tilemap we THINK we're in
    //
    // After movement, offset might be negative or > tilemap size
    // That's OK - canonicalization will fix it
    //
    // NOTE: frame_time is seconds since last frame (e.g., 0.0166 for 60 FPS)
    //       velocity * time = distance moved this frame
    TilemapRelativePosition player_bottom_center_pos = {
        .offset_x = game->player.x + (d_player_x * frame_time),
        .offset_y = game->player.y + (d_player_y * frame_time),
        .tilemap_x = game->player.tilemap_x,
        .tilemap_y = game->player.tilemap_y,
    };

    // ─── Calculate collision check points ───
    // Player origin is at BOTTOM CENTER of sprite
    //
    //        ┌─────────────────┐
    //        │                 │
    //        │     Player      │
    //        │     Sprite      │
    //        │                 │
    //        └────────●────────┘
    //                 ↑
    //           Origin (x, y)
    //           Bottom center
    //
    // We need to check the full width of the player's feet:
    //
    //   LEFT           CENTER         RIGHT
    //     ●──────────────●──────────────●
    //     ↑              ↑              ↑
    //   x - width/2      x          x + width/2

    // Left foot position
    TilemapRelativePosition player_bottom_left_pos = player_bottom_center_pos;
    player_bottom_left_pos.offset_x -= game->player.width * 0.5f;

    // Right foot position
    TilemapRelativePosition player_bottom_right_pos = player_bottom_center_pos;
    player_bottom_right_pos.offset_x += game->player.width * 0.5f;

    // ─── Collision detection ───
    // ALL THREE points must be in empty tiles for movement to succeed
    //
    // Why check 3 points?
    //   - Player width (45px) might span 2 tiles (each 60px wide)
    //   - Checking only center would let player clip into walls
    //
    //   ┌───────┬───────┐
    //   │ WALL  │ EMPTY │
    //   │   ●───┼───●   │  ← If we only check center (●), player clips into
    //   wall! │  LEFT │ CENTER│ └───────┴───────┘
    //
    if (
        // Check center point (player's origin)
        is_world_point_empty(&game->world, player_bottom_center_pos) &&
        // Check left edge of player
        is_world_point_empty(&game->world, player_bottom_left_pos) &&
        // Check right edge of player
        is_world_point_empty(&game->world, player_bottom_right_pos)) {
      // ─── Movement allowed! Update player position ───

      // First, canonicalize the raw position
      // This resolves tilemap transitions (e.g., walking off right edge)
      WorldCanonicalPosition canonical_pos =
          get_canonical_pos(&game->world, player_bottom_center_pos);

      // Update which tilemap the player is in
      // (might have changed if we crossed a boundary)
      game->player.tilemap_x = canonical_pos.tilemap_x;
      game->player.tilemap_y = canonical_pos.tilemap_y;

      // ─── Convert canonical position back to screen coordinates ───
      //
      // Screen position =
      //   world_origin                    (where tilemap[0,0] starts on screen)
      //   + tile_index * tile_size        (offset to the tile we're in)
      //   + sub_tile_offset               (position within that tile)
      //
      // VISUAL:
      //   Screen
      //   ┌──────────────────────────────────────────────┐
      //   │                                              │
      //   │  origin_x                                    │
      //   │  ↓                                           │
      //   │  ├──tile 0──┼──tile 1──┼──tile 2──┼──tile 3──│
      //   │             │          │    ●     │          │
      //   │             │          │    ↑     │          │
      //   │             │          │ player   │          │
      //   │             │          │          │          │
      //   └──────────────────────────────────────────────┘
      //
      //   player.x = origin_x + (tile_x * tile_width) + tile_rel_offset_x
      //            = -30 + (2 * 60) + 15
      //            = -30 + 120 + 15
      //            = 105 pixels from left edge of screen
      //
      game->player.x = canonical_pos.tile_rel_offset_x +
                       (game->world.tilemap_width_px * canonical_pos.tile_x) +
                       game->world.origin_x;

      // BUG WARNING: Casey uses tilemap_width_px for Y calculation too!
      // This should probably be tilemap_height_px for correctness.
      // Works because tiles are square (60x60), but would break for non-square
      // tiles.
      game->player.y =
          canonical_pos.tile_rel_offset_y +
          (game->world.tilemap_width_px *
           canonical_pos.tile_y) + // ← Should be tilemap_height_px?
          game->world.origin_y;
    }
    // If collision detected, player position is NOT updated (blocked by wall)
  }

#if 0
  // Clamp tone
  if (game->audio.tone.frequency < 20)
    game->audio.tone.frequency = 20;
  if (game->audio.tone.frequency > 2000)
    game->audio.tone.frequency = 2000;

  // // Update wave_period when tone_hz changes (prevent audio clicking)
  // audio_output->wave_period =
  //     audio_output->samples_per_second / game->audio.tone.frequency;
#endif
}

GAME_UPDATE_AND_RENDER(game_update_and_render) {
  (void)thread_context;

  HandMadeHeroGameState *game =
      (HandMadeHeroGameState *)memory->permanent_storage;

  // Find active controller
  GameControllerInput *active_controller = NULL;

  // Priority 1: Check for active joystick inputs (controllers 1-4)
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {

    if (i == KEYBOARD_CONTROLLER_INDEX)
      continue;

    GameControllerInput *c = &inputs->controllers[i];
    if (c->is_connected) {
      active_controller = c;
      break;
    }
  }

  // Fallback: Always use keyboard (controller[0]) if nothing else
  if (!active_controller) {
    active_controller = &inputs->controllers[KEYBOARD_CONTROLLER_INDEX];
  }

#if DE100_INTERNAL
  // Show stats every 60 frames
  if (FRAME_LOG_EVERY_FIVE_SECONDS_CHECK) {
    printf("Debug Counter %d: active_controller=%p\n", g_frame_counter,
           (void *)active_controller);
    if (active_controller) {
      printf("  is_analog=%d stick_avg_x=%.2f stick_avg_y=%.2f\n",
             active_controller->is_analog, active_controller->stick_avg_x,
             active_controller->stick_avg_y);
      printf("  up=%d down=%d left=%d right=%d\n",
             active_controller->move_up.ended_down,
             active_controller->move_down.ended_down,
             active_controller->move_left.ended_down,
             active_controller->move_right.ended_down);
      printf("Debug Counter %d: is_analog=%d stick_avg_x=%.2f stick_avg_y=%.2f "
             "up=%d down=%d left=%d right=%d\n",
             g_frame_counter, active_controller->is_analog,
             active_controller->stick_avg_x, active_controller->stick_avg_y,
             active_controller->move_up.ended_down,
             active_controller->move_down.ended_down,
             active_controller->move_left.ended_down,
             active_controller->move_right.ended_down);
    }
  }
#endif

  real32 frame_time = de100_get_frame_time();

  handle_controls(active_controller, game, frame_time);

  draw_rect(buffer, 0.0f, 0.0f, (real32)buffer->width, (real32)buffer->height,
            1.0f, 0.0f, 1.0f, 1.0f);

  // ═══════════════════════════════════════════════════════════════════════════
  // TILEMAP RENDERING
  // ═══════════════════════════════════════════════════════════════════════════
  //
  // This renders the CURRENT tilemap (the one the player is in).
  // Each tile is drawn as a colored rectangle:
  //   - Gray (0.5) = Empty/walkable (tile_id == 0)
  //   - White (1.0) = Wall/solid (tile_id == 1)
  //
  // ═══════════════════════════════════════════════════════════════════════════

  // Get the tilemap the player is currently in
  Tilemap *current_tilemap =
      get_tilemap(&game->world, game->player.tilemap_x, game->player.tilemap_y);
  ASSERT_MSG(current_tilemap, "Current tile map is null!");

  // ─── Render each tile in the current tilemap ───
  //
  // Tile grid layout (example 17x9):
  //
  //   col:  0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
  //       ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
  // row 0 │ W │ W │ W │ W │ W │ W │ W │ W │ W │ W │ W │ W │ W │ W │ W │ W │ W │
  //       ├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
  // row 1 │ W │   │   │   │   │ W │   │   │   │   │   │   │   │ W │   │   │ W │
  //       ├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
  // row 2 │ W │   │   │   │   │   │   │   │ W │   │   │   │   │   │ W │   │ W │
  //       ├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
  //  ...  │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │
  //       └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
  //
  // W = Wall (tile_id 1, drawn white)
  // (empty) = Walkable (tile_id 0, drawn gray)
  //
  for (int32 row = 0; row < game->world.tiles_per_map_y_count; ++row) {
    for (int32 col = 0; col < game->world.tiles_per_map_x_count; ++col) {

      // Get tile ID at this position
      uint32 tile_id =
          get_tile_value_unchecked(&game->world, current_tilemap, col, row);

      // Choose color based on tile type
      // 1 (wall) = white (1.0), 0 (empty) = dark gray (0.5)
      real32 gray = tile_id == 1 ? 1.0f : 0.5f;

      // ─── Calculate screen-space rectangle for this tile ───
      //
      // Screen position calculation:
      //
      //   min_x = origin_x + (col * tile_width)
      //   min_y = origin_y + (row * tile_height)
      //
      // Example for tile at col=2, row=1:
      //   origin_x = -30, tile_width = 60
      //   min_x = -30 + (2 * 60) = -30 + 120 = 90
      //   min_y = 0 + (1 * 60) = 60
      //
      //   Screen:
      //   ┌────────────────────────────────────────┐
      //   │  -30   0    30   60   90   120  150    │
      //   │   │    │    │    │    │    │    │      │
      //   │   ├────┼────┼────┼────┼────┼────┤ row 0│
      //   │   │    │    │    │    │    │    │      │
      //   │   ├────┼────┼────┼────┼────┼────┤ row 1│
      //   │   │    │    │    │████│    │    │      │
      //   │   │    │    │    │████│← tile(2,1)     │
      //   │   ├────┼────┼────┼────┼────┼────┤      │
      //   └────────────────────────────────────────┘
      //
      real32 min_x =
          game->world.origin_x + (real32)col * game->world.tilemap_width_px;
      real32 min_y =
          game->world.origin_y + (real32)row * game->world.tilemap_height_px;
      real32 max_x = min_x + game->world.tilemap_width_px;
      real32 max_y = min_y + game->world.tilemap_height_px;

      // Draw the tile as a filled rectangle
      // Note: draw_rect handles screen clipping (negative coords, overflow)
      draw_rect(buffer, min_x, min_y, max_x, max_y, gray, gray, gray, 1.0f);
    }
  }

  real32 player_left = game->player.x - game->player.width * 0.5;
  real32 player_top = game->player.y - game->player.height * 0.5;
  draw_rect(buffer, player_left, player_top, player_left + game->player.width,
            player_top + game->player.height, game->player.color_r,
            game->player.color_g, game->player.color_b, game->player.color_a);

  // Render indicators for pressed mouse buttons
  for (int button_index = 0; button_index < 5; ++button_index) {
    if (inputs->mouse_buttons[button_index].ended_down) {
      draw_rect(buffer,
                // (10 + 20 * button_index), 0
                inputs->mouse_x, inputs->mouse_y, inputs->mouse_x + 10,
                inputs->mouse_y + 10, 0.0f, 1.0f, 1.0f, 1.0f);
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// GAME GET SOUND SAMPLES
// ═══════════════════════════════════════════════════════════════════════════
// This is the ONLY place audio samples are generated!
// Platform tells us how many samples to generate, we fill the buffer.
//
// KEY REQUIREMENTS:
// 1. Phase must be CONTINUOUS across calls (stored in game)
// 2. Volume changes should be gradual (future: implement ramping)
// 3. Frequency changes should be gradual (future: implement smoothing)
// ═══════════════════════════════════════════════════════════════════════════

GAME_GET_AUDIO_SAMPLES(game_get_audio_samples) {
  HandMadeHeroGameState *game =
      (HandMadeHeroGameState *)memory->permanent_storage;

  if (!memory->is_initialized) {
    int16 *sample_out = (int16 *)audio_buffer->samples;
    for (int i = 0; i < audio_buffer->sample_count; ++i) {
      *sample_out++ = 0;
      *sample_out++ = 0;
    }
    return;
  }

  // ═══════════════════════════════════════════════════════════════
  // DEBUG: Log audio state periodically
  // ═══════════════════════════════════════════════════════════════
#if DE100_INTERNAL
  if (FRAME_LOG_EVERY_THREE_SECONDS_CHECK) { // Every ~3 seconds at 30Hz
    SoundSource *tone = &game->audio.tone;
    printf("[AUDIO DEBUG] is_playing=%d, freq=%.1f, vol=%.2f, phase=%.2f, "
           "samples=%d\n",
           tone->is_playing, tone->frequency, tone->volume, tone->phase,
           audio_buffer->sample_count);
  }
#endif

  // Get audio state
  SoundSource *tone = &game->audio.tone;
  real32 master_volume = game->audio.master_volume;

  // Safety check: if not initialized, output silence
  if (!tone->is_playing || tone->frequency <= 0.0f) {
    int16 *sample_out = (int16 *)audio_buffer->samples;
    for (int sample_index = 0; sample_index < audio_buffer->sample_count;
         ++sample_index) {
      *sample_out++ = 0; // Left
      *sample_out++ = 0; // Right
    }
    return;
  }

  // Calculate wave period
  real32 wave_period =
      (real32)audio_buffer->samples_per_second / tone->frequency;

  // Clamp volume
  if (master_volume < 0.0f)
    master_volume = 0.0f;
  if (master_volume > 1.0f)
    master_volume = 1.0f;
  if (tone->volume < 0.0f)
    tone->volume = 0.0f;
  if (tone->volume > 1.0f)
    tone->volume = 1.0f;

  // Volume in 16-bit range
  int16 sample_volume = (int16)(tone->volume * master_volume * 16000.0f);

  // Calculate pan volumes
  real32 left_volume =
      (tone->pan_position <= 0.0f) ? 1.0f : (1.0f - tone->pan_position);
  real32 right_volume =
      (tone->pan_position >= 0.0f) ? 1.0f : (1.0f + tone->pan_position);

  int16 *sample_out = (int16 *)audio_buffer->samples;

  if (FRAME_LOG_EVERY_ONE_SECONDS_CHECK) {
    printf("[AUDIO GEN] sample_count=%d, volume=%.1f, freq=%.1f, phase=%.3f\n",
           audio_buffer->sample_count, game->audio.tone.volume,
           game->audio.tone.frequency, game->audio.tone.phase);
  }

  for (int sample_index = 0; sample_index < audio_buffer->sample_count;
       ++sample_index) {
#if 0
    real32 sine_value = sinf(tone->phase);
    int16 sample_value = (int16)(sine_value * sample_volume);
#endif
    int16 sample_value = 0 * sample_volume;

    // Apply panning
    int16 left_sample = (int16)(sample_value * left_volume);
    int16 right_sample = (int16)(sample_value * right_volume);

    *sample_out++ = left_sample;
    *sample_out++ = right_sample;

    // Advance phase
    tone->phase += 2.0f * M_PI / wave_period;

    // Wrap phase to prevent floating point precision issues
    if (tone->phase > 2.0f * M_PI) {
      tone->phase -= 2.0f * M_PI;
    }
  }
}

/*
void render_weird_gradient(GameBackBuffer *backbuffer,
                           HandMadeHeroGameState *game) {
  uint8 *row = (uint8 *)backbuffer->memory.base;

  // The following is correct for X11
  for (int y = 0; y < backbuffer->height; ++y) {
    int offset_x = game->gradient.offset_x;
    uint32 *pixel = (uint32 *)row;
    int offset_y = game->gradient.offset_y;

    for (int x = 0; x < backbuffer->width; ++x) {
      uint8 blue = (uint8)(x + offset_x);
      uint8 green = (uint8)(y + offset_y);

      // RGBA format - works for BOTH OpenGL X11 AND Raylib!
      *pixel++ = (0xFF000000u |  // Alpha = 255
                  (blue << 16) | // Blue
                  (green << 8) | // Green
                  (0));
    }
    row += backbuffer->pitch;
  }
}
*/