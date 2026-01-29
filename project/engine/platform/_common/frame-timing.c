
#include "frame-timing.h"

#if DE100_INTERNAL
#include <x86intrin.h> // For __rdtsc() CPU cycle counter
#endif

void frame_timing_begin(FrameTiming *timing) {
  platform_get_timespec(&timing->frame_start);
#if DE100_INTERNAL
  timing->start_cycles = __rdtsc();
#endif
}

void frame_timing_mark_work_done(FrameTiming *timing) {
  platform_get_timespec(&timing->work_end);
  timing->work_seconds = (real32)platform_timespec_diff_seconds(
      &timing->frame_start, &timing->work_end);
}

void frame_timing_sleep_until_target(FrameTiming *timing,
                                     real32 target_seconds) {
  real32 seconds_elapsed = timing->work_seconds;

  if (seconds_elapsed < target_seconds) {
    real32 sleep_threshold = target_seconds - 0.003f;

    // Phase 1: Coarse sleep
    while (seconds_elapsed < sleep_threshold) {
      platform_sleep_ms(1);

      PlatformTimeSpec current;
      platform_get_timespec(&current);
      seconds_elapsed = (real32)platform_timespec_diff_seconds(
          &timing->frame_start, &current);
    }

    // Phase 2: Spin-wait
    while (seconds_elapsed < target_seconds) {
      PlatformTimeSpec current;
      platform_get_timespec(&current);
      seconds_elapsed = (real32)platform_timespec_diff_seconds(
          &timing->frame_start, &current);
    }
  }
}

void frame_timing_end(FrameTiming *timing) {
  platform_get_timespec(&timing->frame_end);
#if DE100_INTERNAL
  timing->end_cycles = __rdtsc();
#endif

  timing->total_seconds = (real32)platform_timespec_diff_seconds(
      &timing->frame_start, &timing->frame_end);
  timing->sleep_seconds = timing->total_seconds - timing->work_seconds;
}

real32 frame_timing_get_ms(FrameTiming *timing) {
  return timing->total_seconds * 1000.0f;
}

real32 frame_timing_get_fps(FrameTiming *timing) {
  return 1.0f / timing->total_seconds;
}

#if DE100_INTERNAL
real32 frame_timing_get_mcpf(FrameTiming *timing) {
  return (timing->end_cycles - timing->start_cycles) / 1000000.0f;
}
#endif