#ifndef DE100_PLATFORM__COMMON_FRAME_TIMING_FPS_H
#define DE100_PLATFORM__COMMON_FRAME_TIMING_FPS_H

#include "../../_common/base.h"
#include "../../_common/time.h"

typedef struct {
  PlatformTimeSpec frame_start;
  PlatformTimeSpec work_end;
  PlatformTimeSpec frame_end;
  real32 work_seconds;
  real32 total_seconds;
  real32 sleep_seconds;
#if DE100_INTERNAL
  uint64 start_cycles;
  uint64 end_cycles;
#endif
} FrameTiming;

void frame_timing_begin(FrameTiming *timing);

void frame_timing_mark_work_done(FrameTiming *timing);

void frame_timing_sleep_until_target(FrameTiming *timing,
                                     real32 target_seconds);

void frame_timing_end(FrameTiming *timing);

real32 frame_timing_get_ms(FrameTiming *timing);

real32 frame_timing_get_fps(FrameTiming *timing);

#if DE100_INTERNAL
real32 frame_timing_get_mcpf(FrameTiming *timing);
#endif

#endif // DE100_PLATFORM__COMMON_FRAME_TIMING_FPS_H