#ifndef BASE_TYPES_H
#define BASE_TYPES_H

// @note: Base types

#if LANG_C
# define true 1
# define false 0
#endif

#include <stdint.h>
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float    f32;
typedef double   f64;
typedef s8  b8;
typedef s16 b16;
typedef s32 b32;
typedef s64 b64;

typedef void VoidFunc(void);

// @note: Math types

#include <math.h>
#define HANDMADE_MATH_NO_SSE 1
#include "third_party/HandmadeMath.h"

typedef HMM_Vec2 Vec2;
typedef HMM_Vec3 Vec3;
typedef HMM_Vec4 Vec4;
typedef HMM_Mat2 Mat2;
typedef HMM_Mat3 Mat3;
typedef HMM_Mat4 Mat4;
typedef HMM_Quat Quat;
typedef Vec4 Color;
typedef Vec4 Rect;

core_function f32 fmod_cycling(f32 x, f32 y);

// @todo: Fixed-point implementation

#endif // BASE_TYPES_H