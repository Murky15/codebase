#ifndef BASE_TYPES_H
#define BASE_TYPES_H

// @note: Base types

#if LANG_C
# define true 1
# define false 0
#endif

#include <stdint.h>
typedef int8_t   s8,  b8;
typedef int16_t  s16, b16;
typedef int32_t  s32, b32;
typedef int64_t  s64, b64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float    f32;
typedef double   f64;

typedef void VoidFunc(void);

// @note: Math types

#include <math.h>
#define HANDMADE_MATH_NO_SSE 1
// #include "third_party/Murky_HandmadeMath.h"

// @todo: Fixed-point implementation

#endif // BASE_TYPES_H