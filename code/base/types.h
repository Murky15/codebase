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

#include <math.h>

// So for vectors, we can use SIMD intrinsics but between mangling the data into
// the format we want and actually performing the operation it might just be good enough
// to implement this in scalar, and rely on auto-vectorization or put in sse for bigger things
// idk, I'll profile this later and see whats best. For now I'm gonna avoid the premature
// optimization. 

typedef union Vec2 {
  struct { f32 x, y; };
  f32 v[2];
} Vec2;

typedef union Vec3 {
  struct { f32 x, y, z; };
  struct { f32 r, g, b; };
  f32 v[3];
} Vec3;

typedef union Vec4 {
  struct { f32 x, y, z, w; };
  struct { f32 r, g, b, a; };
  struct { f32 x1, y1, x2, y2; };
  f32 v[4];
} Vec4, Rect;

// @todo: How should I handle matricies?

// @note: Constructors

core_function Vec2 v2(f32 x, f32 y);
core_function Vec3 v3(f32 x, f32 y, f32 z);
core_function Vec4 v4(f32 x, f32 y, f32 z, f32 w);

// @note: Arithmetic

core_function f32  v2_mag(Vec2 v);
core_function f32  v3_mag(Vec3 v);
core_function f32  v4_mag(Vec4 v);
core_function Vec2 v2_add(Vec2 a, Vec2 b);
core_function Vec3 v3_add(Vec3 a, Vec3 b);
core_function Vec4 v4_add(Vec4 a, Vec4 b);
core_function Vec2 v2_sub(Vec2 a, Vec2 b);
core_function Vec3 v3_sub(Vec3 a, Vec3 b);
core_function Vec4 v4_sub(Vec4 a, Vec4 b);
core_function Vec2 v2_muls(Vec2 v, f32 s);
core_function Vec3 v3_muls(Vec3 v, f32 s);
core_function Vec4 v4_muls(Vec4 v, f32 s);
core_function Vec2 v2_norm(Vec2 v);
core_function Vec3 v3_norm(Vec3 v);
core_function Vec4 v4_norm(Vec4 v);
core_function f32  v2_dot(Vec2 a, Vec2 b);
core_function f32  v3_dot(Vec3 a, Vec3 b);
core_function f32  v4_dot(Vec4 a, Vec4 b);
core_function Vec3 v3_cross(Vec3 a, Vec3 b);

#endif // BASE_TYPES_H