/* src/utils/math.h  —  Desktop Tower Defense | Math Utilities */
#ifndef DTD_MATH_H
#define DTD_MATH_H

#define MIN(a, b)        ((a) < (b) ? (a) : (b))
#define MAX(a, b)        ((a) > (b) ? (a) : (b))
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
#define ABS(a)           ((a) < 0 ? -(a) : (a))

#endif /* DTD_MATH_H */
