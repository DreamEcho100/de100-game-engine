#include "../../_common/hooks/utils.h"
#include "../../../game/base.h"
#include <raylib.h>

void de100_set_target_fps(u32 fps) {
  SetTargetFPS((int)fps);
  g_fps = fps;
}

f32 de100_get_frame_time(void) { return GetFrameTime(); }

f64 de100_get_time(void) { return GetTime(); }

u32 de100_get_fps(void) { return (u32)GetFPS(); }
