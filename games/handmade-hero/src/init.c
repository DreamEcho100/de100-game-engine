// IWYU pragma: keep // clangd: unused-include-ignore // NOLINTNEXTLINE(clang-diagnostic-unused-include)
#include "./inputs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../../../engine/game/game-loader.h"
#include "main.h"

#if DE100_INTERNAL
#include "../../../engine/_common/debug-file-io.h"
#endif

GAME_INIT(game_init) {
  (void)thread_context;
  (void)inputs;
  (void)buffer;
  // Add at the end of the file to verify
  DEV_ASSERT_MSG(sizeof(_GameButtonsCounter) == sizeof(GameButtonState) * 12,
                 "Button struct size mismatch");

  HandMadeHeroGameState *game =
      (HandMadeHeroGameState *)memory->permanent_storage;

  if (!memory->is_initialized) { // false (skip!)
                                 // Never runs again!
                                 // First-time initialization
#if DE100_INTERNAL
    printf("[GAME] First-time init\n");
#endif

#if DE100_INTERNAL
    char *Filename = __FILE__;

    De100DebugDe100FileReadResult file =
        de100_debug_platform_read_entire_file(Filename);
    if (file.memory.base) {
      de100_debug_platform_write_entire_file("out/test.out", file.size,
                                             file.memory.base);
      de100_debug_platform_free_de100_file_memory(&file.memory);
      printf("Wrote test.out\n");
    }
#endif
    printf("[GAME INIT] game_update_and_render from build id %d\n",
           1); // change this number each rebuild

    // Initialize audio state
    game->audio.tone.frequency = 256;
    game->audio.tone.phase = 0.0f;
    game->audio.tone.volume = 1.0f;
    game->audio.tone.pan_position = 0.0f;
    game->audio.tone.is_playing = true;
    game->audio.master_volume = 1.0f;

    game->speed = 128;

    game->world.tiles_per_map_x_count = TILES_PER_MAP_X_COUNT;
    game->world.tiles_per_map_y_count = TILES_PER_MAP_Y_COUNT;

    game->world.origin_x = -30;
    game->world.origin_y = 0;
    game->world.tilemap_width_px = 60;
    game->world.tilemap_height_px = 60;
    game->world.tilemaps_count_x = 2;
    game->world.tilemaps_count_y = 2;

    // TODO: consider arenas
    local_persist_var uint32
        tiles_Y0_X0[TILES_PER_MAP_Y_COUNT][TILES_PER_MAP_X_COUNT] = {
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
            {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
            {1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
            {1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1},
            {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
            {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
            {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
        };

    local_persist_var uint32
        tiles_Y1_X0[TILES_PER_MAP_Y_COUNT][TILES_PER_MAP_X_COUNT] = {
            {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        };

    local_persist_var uint32
        tiles_Y0_X1[TILES_PER_MAP_Y_COUNT][TILES_PER_MAP_X_COUNT] = {
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
        };

    local_persist_var uint32
        tiles_Y1_X1[TILES_PER_MAP_Y_COUNT][TILES_PER_MAP_X_COUNT] = {
            {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        };

    local_persist_var Tilemap tilemaps[TILE_MAPS_Y_COUNT][TILE_MAPS_X_COUNT];

    tilemaps[0][0].tiles = (uint32 *)tiles_Y0_X0;
    tilemaps[0][1].tiles = (uint32 *)tiles_Y0_X1;
    tilemaps[1][0].tiles = (uint32 *)tiles_Y1_X0;
    tilemaps[1][1].tiles = (uint32 *)tiles_Y1_X1;

    game->world.tilemaps = (Tilemap *)tilemaps;

    game->player.x = 150;
    game->player.y = 150;
    game->player.color_r = 0.0f;
    game->player.color_g = 1.0f;
    game->player.color_b = 1.0f;
    game->player.color_a = 1.0f;
    game->player.width = 0.75f * game->world.tilemap_width_px;
    game->player.height = game->world.tilemap_height_px;

    memory->is_initialized = true;
#if DE100_INTERNAL
    printf("[GAME] Init complete\n");
#endif
    return;
  }
}