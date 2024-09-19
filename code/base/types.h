#ifndef BASE_TYPES_H
#define BASE_TYPES_H

//~ @note: Base types

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

typedef void void_func(void);

//~ @note Tangible types

typedef struct Color {
    u8 r, g, b, a;
} Color;

// @todo: Maybe add functionality for defining per-project "color-pallettes"?
#define colors \
color(Black,   0,   0,   0)   \
color(White,   255, 255, 255) \
color(Red,     255, 0,   0)   \
color(Lime,    0,   255, 0)   \
color(Blue,    0,   0,   255) \
color(Yellow,  255, 255, 0)   \
color(Cyan,    0,   255, 255) \
color(Magenta, 255, 0,   255) \
color(Silver,  192, 192, 192) \
color(Gray,    128, 128, 128) \
color(Maroon,  128, 0,   0)   \
color(Olive,   128, 128, 0)   \
color(Green,   0,   128, 0)   \
color(Purple,  128, 0,   128) \
color(Teal,    0,   128, 128) \
color(Navy,    0,   0,   128) \

#define color(name, r, g, b) read_only Color glue(Color_,name) = {r,g,b};
colors
#undef color

//~ @note: Math types

#if COMPILER_CL
# define _USE_MATH_DEFINES
#else
# define  M_PI  3.1415926535897932384626433
#endif
#include <math.h>

typedef union Vec2 {
    struct { f32 x, y; };
    f32 e[2];
} Vec2;

typedef union Vec2i {
    struct { u32 x, y; };
    u32 e[2];
} Vec2i;

typedef union Vec3 {
    struct { f32 x, y, z; };
    f32 e[3];
} Vec3;

typedef union Vec3i {
    struct { u32 x, y, z; };
    u32 e[2];
} Vec3i;

//- @note: Constructors
core_function Vec2  v2(f32 x, f32 y);
core_function Vec2i v2i(u32 x, u32 y);
core_function Vec2i v2i_from_v2(Vec2 v);
core_function Vec2  dv3(Vec3 v); // demote v3

core_function Vec3  v3(f32 x, f32 y, f32 z);
core_function Vec3i v3i(u32 x, u32 y, u32 z);
core_function Vec3i v3i_from_v3(Vec3 v);
core_function Vec3  pv2(Vec2 v, f32 z); // promote v2

//- @note: Basic ops
core_function f32  v2len(Vec2 v);
core_function f32  v2dot(Vec2 a, Vec2 b);
core_function f32  v2cross(Vec2 a, Vec2 b, Vec2 c); // https://en.wikipedia.org/wiki/Cross_product#Computational_geometry
core_function Vec2 v2sub(Vec2 a, Vec2 b);
core_function Vec2 v2add(Vec2 a, Vec2 b);
core_function Vec2 v2muls(Vec2 v, f32 s);
core_function Vec2 v2norm(Vec2 v);

core_function f32  v3len(Vec3 v);
core_function f32  v3dot(Vec3 a, Vec3 b);
core_function Vec3 v3sub(Vec3 a, Vec3 b);
core_function Vec3 v3add(Vec3 a, Vec3 b);
core_function Vec3 v3muls(Vec3 v, f32 s);
core_function Vec3 v3norm(Vec3 v);
core_function Vec3 v3cross(Vec3 a, Vec3 b);

core_function f32 fmod_cycling(f32 x, f32 y); // Ripped this straight from Jai
core_function f32 lerp (f32 v0, f32 v1, f32 t);

// @todo: Fixed-point implementation

#endif // BASE_TYPES_H