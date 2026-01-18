#ifndef BASE_TYPES_H
#define BASE_TYPES_H

//- @note: Base types

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

typedef void void_func(void);

read_only u8  u8_max  = 0xff;
read_only u16 u16_max = 0xffff;
read_only u32 u32_max = 0xffffffff;
read_only u64 u64_max = 0xffffffffffffffff;

read_only s8  s8_min  = 0x80;
read_only s8  s8_max  = 0x7f;
read_only s16 s16_min = 0x8000;
read_only s16 s16_max = 0x7fff;
read_only s32 s24_min = 0x800000;
read_only s32 s24_max = 0x7fffff;
read_only s32 s32_min = 0x80000000;
read_only s32 s32_max = 0x7fffffff;
read_only s64 s64_min = 0x8000000000000000;
read_only s64 s64_max = 0x7fffffffffffffff;

#ifndef BASE_TYPES_ESSENTIAL_ONLY
// @todo: Fixed-point implementation

#define make_array_type(T) typedef struct {T *array; u64 count;} glue(T,Array)

make_array_type(s8);
make_array_type(s16);
make_array_type(s32);
make_array_type(s64);

make_array_type(u8);
make_array_type(u16);
make_array_type(u32);
make_array_type(u64);

make_array_type(f32);
make_array_type(f64);

#include <math.h>

//- @note: Math types
#if !defined(M_PI)
# define M_PI  3.1415926535897932384626433
#endif
#define M_PI32 ((f32)M_PI)
#define RAD2DEG (180.f/M_PI32)
#define DEG2RAD (M_PI32/180.f)

typedef union Vec2 {
  struct { f32 x, y; };
  struct { f32 width, height; };
  struct { f32 first, last; };

  f32 e[2];
} Vec2;
typedef Vec2 Range, Pair;

typedef union Vec2i {
  struct { s32 x, y; };
  struct { s32 width, height; };
  struct { s32 first, last; };
  s32 e[2];
} Vec2i;
typedef Vec2i Rangei, Pairi;

// I finally found a feature I would add to C, cross-union initializers.
typedef union Vec3 {
  struct { f32 x, y, z; };
  struct { Vec2 xy; f32 z1; };
  struct { f32 x1; Vec2 yz; };

  f32 e[3];
} Vec3;

typedef union Vec3i {
  struct { s32 x, y, z; };
  struct { Vec2i xy; s32 z1; };
  struct { s32 x1; Vec2i yz; };
  s32 e[2];
} Vec3i;

typedef union Vec4 {
  struct { f32 x, y, z, w; };
  struct { Vec2 xy, zw; };
  struct { Vec3 xyz; f32 w1; };

  struct { Vec2 min, max; };
  struct { Vec2 xy1; f32 width, height; };

  f32 e[4];
} Vec4;
typedef Vec4 Rect, Quat;

typedef union Mat4 {
  struct {
    f32 _00, _01, _02, _03;
    f32 _10, _11, _12, _13;
    f32 _20, _21, _22, _23;
    f32 _30, _31, _32, _33;
  };
  Vec4 r[4];
  f32 i[4][4];
  f32 e[16];
} Mat4;

typedef struct Quad2D {
  Vec2 p0, p1, p2, p3;
} Quad2D;

typedef struct Quad3D {
  Vec3 p0, p1, p2, p3;
} Quad3D;

//- @note: Constructors
#define v2(...)  comp_lit(Vec2,  __VA_ARGS__)
#define v2i(...) comp_lit(Vec2i, __VA_ARGS__)
#define v3(...)  comp_lit(Vec3,  __VA_ARGS__)
#define v3i(...) comp_lit(Vec3i, __VA_ARGS__)
#define v4(...)  comp_lit(Vec4,  __VA_ARGS__)

// NOTE: More swizzles
#define xz(v) v2((v).x,(v).z)

// NOTE: Type punning
#if LANG_C
union {
  f64 d;
  u64 i;
} Double_u64_Pun;

#endif

//- @note: Vectors
// TODO: Can we inline these?
core_function b32  v2exact(Vec2 a, Vec2 b);
core_function b32  v2approx(Vec2 a, Vec2 b);
core_function f32  v2len(Vec2 v);
core_function f32  v2dot(Vec2 a, Vec2 b);
core_function f32  v2cross(Vec2 a, Vec2 b);
core_function f32  v2cross3(Vec2 a, Vec2 b, Vec2 c); // https://en.wikipedia.org/wiki/Cross_product#Computational_geometry
core_function f32  v2angle(Vec2 a, Vec2 b);
core_function f32  v2dist(Vec2 a, Vec2 b);
core_function Vec2 v2sub(Vec2 a, Vec2 b);
core_function Vec2 v2add(Vec2 a, Vec2 b);
core_function Vec2 v2mul(Vec2 a, Vec2 b);
core_function Vec2 v2div(Vec2 a, Vec2 b);
core_function Vec2 v2muls(Vec2 v, f32 s);
core_function Vec2 v2norm(Vec2 v);

core_function b32  v3exact(Vec3 a, Vec3 b);
core_function b32  v3approx(Vec3 a, Vec3 b);
core_function f32  v3len(Vec3 v);
core_function f32  v3dot(Vec3 a, Vec3 b);
core_function f32  v3dist(Vec3 a, Vec3 b);
core_function Vec3 v3sub(Vec3 a, Vec3 b);
core_function Vec3 v3add(Vec3 a, Vec3 b);
core_function Vec3 v3mul(Vec3 a, Vec3 b);
core_function Vec3 v3div(Vec3 a, Vec3 b);
core_function Vec3 v3muls(Vec3 v, f32 s);
core_function Vec3 v3norm(Vec3 v);
core_function Vec3 v3cross(Vec3 a, Vec3 b);

//- @note: Quaternions
core_function Quat qi(void);
core_function Quat axis_angle(Vec3 v, f32 t);
core_function Quat qneg(Quat q);
core_function Quat qnorm(Quat q);
core_function Quat qmul(Quat a, Quat b);
core_function Quat qinv(Quat q);
core_function Quat qpow(Quat q, f32 e);
core_function Quat slerp(Quat a, Quat b, f32 t);

//- @note: Matricies
core_function Mat4 m4i(void);
core_function Mat4 m4add(Mat4 a, Mat4 b);
core_function Mat4 m4sub(Mat4 a, Mat4 b);
core_function Mat4 m4mul(Mat4 a, Mat4 b);
core_function Mat4 m4muls(Mat4 m, f32 s);
core_function Vec4 m4mulv(Mat4 m, Vec4 v);
core_function Mat4 m4transpose(Mat4 m);
core_function Mat4 m4invert(Mat4 m);

core_function Mat4 m4scale(Vec3 s);
core_function Mat4 m4rotate(Quat r);
core_function Mat4 m4rotate_around(Quat r, Vec3 p);
core_function Mat4 m4translate(Vec3 t);

// These are all left handed
core_function Mat4 m4perspective(f32 fovy, f32 aspect, f32 znear, f32 zfar);
core_function Mat4 m4orthographic(f32 width, f32 height, f32 znear, f32 zfar);
core_function Mat4 m4lookat(Vec3 viewpoint, Vec3 focus, Vec3 reference_up);

//- @note: Float helpers
core_function f64 fmod_cycling(f64 x, f64 y);
core_function f64 lerp(f64 v0, f64 v1, f64 t);
#define v2lerp(a,b,t) v2(lerp((a).x,(b).x,(t)),lerp((a).y,(b).y,(t)))
#define v3lerp(a,b,t) v3(lerp((a).x,(b).x,(t)),lerp((a).y,(b).y,(t)),lerp((a).z,(b).z,(t)))
#define v4lerp(a,b,t) v4(lerp((a).x,(b).x,(t)),lerp((a).y,(b).y,(t)),lerp((a).z,(b).z,(t)),lerp((a).w,(b).w,(t)))
core_function f64 norm(f64 x, f64 min, f64 max);
core_function f64 cnorm(f64 x, f64 min, f64 max); // Clamped norm
core_function b32 almost_equal(f64 a, f64 b);

#undef make_array_type
#endif // BASE_TYPES_ESSENTIAL_ONLY
#endif // BASE_TYPES_H
