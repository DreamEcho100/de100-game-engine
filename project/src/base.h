#ifndef BASE_H
#define BASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

/**
 * Platform-agnostic pixel composer function
 *
 * Platform sets this once, game just calls it
 */
typedef uint32_t (*pixel_composer_fn)(uint8_t r, uint8_t g, uint8_t b,
                                      uint8_t a);

#endif // BASE_H
