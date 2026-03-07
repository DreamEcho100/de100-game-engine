#ifndef DE100_GAME__COMMON_CONFIG_H
#define DE100_GAME__COMMON_CONFIG_H

#include "../../_common/base.h"

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM CONFIG
// ═══════════════════════════════════════════════════════════════════════════
//
// Authoritative platform capabilities and state.
// Read-only from game's perspective.
//
// Audio config has been intentionally removed from here: each backend owns
// its own private audio config (LinuxAudioConfig, etc.). The only shared
// audio contract is GameAudioOutputBuffer in game/audio.h.
//
// Casey's equivalent: Scattered across win32_* structs and globals
//
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  // Display
  u32 window_width;
  u32 window_height;

  // Capabilities
  u32 cpu_core_count;
  u64 page_size;
  u32 max_controllers;

  // Feature flags
  bool vsync_enabled;
  bool has_keyboard;
  bool has_mouse;
  bool has_gamepads;

  // Timing
  f32 seconds_per_frame;
} PlatformConfig;

#endif /* DE100_GAME__COMMON_CONFIG_H */
