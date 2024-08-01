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

// @todo: Fixed-point implementation

// @note: Math types
// @important: I would like to try and compile asm for vector ops instead of intrinsics

#include <math.h>

// Integers
typedef struct Vec2 {
  u64 x, y;
} Vec2;

typedef struct Vec3 {
  u64 x, y, z;
} Vec3;

typedef struct Vec4 {
  u64 x, y, z, w;
} Vec4;

// Floats
typedef struct Vec2f {
  f32 x, y;
} Vec2f;

typedef struct Vec3f {
  f32 x, y, z;
} Vec3f;

typedef struct Vec4f {
  f32 x, y, z, w;
} Vec4f;

#endif // BASE_TYPES_H