#ifndef BASE_H
#define BASE_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


#ifndef ArraySize
#define ArraySize(Array) (sizeof(Array) / sizeof((Array)[0]))
#endif

#include <stdbool.h>
#include <stdint.h>

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
typedef uint32_t (*pixel_composer_fn)(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

#endif // BASE_H
