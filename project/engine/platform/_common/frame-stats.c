#include "frame-stats.h"

void frame_stats_init(FrameStats *stats) {
  stats->frame_count = 0;
  stats->missed_frames = 0;
  stats->min_frame_time_ms = 0.0f;
  stats->max_frame_time_ms = 0.0f;
  stats->total_frame_time_ms = 0.0f;
}

void frame_stats_record(FrameStats *stats, real32 frame_time_ms,
                        real32 target_seconds_per_frame) {
  stats->frame_count++;

  if (frame_time_ms < stats->min_frame_time_ms || stats->frame_count == 1) {
    stats->min_frame_time_ms = frame_time_ms;
  }
  if (frame_time_ms > stats->max_frame_time_ms) {
    stats->max_frame_time_ms = frame_time_ms;
  }

  stats->total_frame_time_ms += frame_time_ms;

  if ((frame_time_ms / 1000.0f) > (target_seconds_per_frame + 0.002f)) {
    stats->missed_frames++;
  }
}

void frame_stats_print(FrameStats *stats) {
  printf("\n═══════════════════════════════════════════════════════════\n");
  printf("📊 FRAME TIME STATISTICS\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("Total frames:   %u\n", stats->frame_count);
  printf("Missed frames:  %u (%.2f%%)\n", stats->missed_frames,
         (real32)stats->missed_frames / stats->frame_count * 100.0f);
  printf("Min frame time: %.2fms\n", stats->min_frame_time_ms);
  printf("Max frame time: %.2fms\n", stats->max_frame_time_ms);
  printf("Avg frame time: %.2fms\n",
         stats->total_frame_time_ms / stats->frame_count);
  printf("═══════════════════════════════════════════════════════════\n");
}