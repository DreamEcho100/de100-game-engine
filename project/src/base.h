#ifndef BASE_H
#define BASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════
// 🎯 FRAME RATE CONFIGURATION (Day 18)
// ═══════════════════════════════════════════════════════════════

#ifndef HANDMADE_TARGET_FPS
  #define HANDMADE_TARGET_FPS 60  // Default to 60 FPS
#endif

// Convenience macros for common targets
#define FPS_30  30
#define FPS_60  60
#define FPS_120 120
#define FPS_144 144
#define FPS_UNLIMITED 0  // No frame limiting (for benchmarking)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef ArraySize
#define ArraySize(Array) (sizeof(Array) / sizeof((Array)[0]))
#endif

#if HANDMADE_SLOW
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__) ||       \
    defined(__CYGWIN__) || defined(__BORLANDC__)
#include <intrin.h>
#define DebugTrap() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define DebugTrap() __builtin_trap()
#else
#define DebugTrap()                                                            \
  {                                                                            \
    *(volatile int *)0 = 0;                                                    \
  }
#endif
#define Assert(expression)                                                     \
  if (!(expression)) {                                                         \
    DebugTrap();                                                               \
  }
#else
#define Assert(expression)
#endif

#define KILOBYTES(value) ((value) * 1024LL)
#define MEGABYTES(value) (KILOBYTES(value) * 1024LL)
#define GIGABYTES(value) (MEGABYTES(value) * 1024LL)
#define TERABYTES(value) (GIGABYTES(value) * 1024LL)

#define file_scoped_fn static
#define local_persist_var static
#define file_scoped_global_var static
#define real32 float
#define real64 double

typedef int32_t bool32;

#endif // BASE_H
