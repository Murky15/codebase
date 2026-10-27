#ifndef FIX_H
#define FIX_H

#define FIX16_FRACTIONAL_BITS 8
#define FIX16_SCALING_FACTOR (1 << FIX16_FRACTIONAL_BITS)

struct fix16 {
  s16 val;
};

#define Fix16FromInt(x) (fix16{s16((x)<<FIX16_FRACTIONAL_BITS)})
#define Fix16FromF32(x) (fix16{s16((x)*(f32)FIX16_SCALING_FACTOR)})
#define Fix16ToInt(x) ((int)((x)>>FIX16_FRACTIONAL_BITS))
#define Fix16ToF32(x) ((f32)((x).val)/(f32)FIX16_SCALING_FACTOR)
function fix16 fix16_add(fix16 fa, fix16 fb);
function fix16 fix16_sub(fix16 fa, fix16 fb);
function fix16 fix16_mul(fix16 fa, fix16 fb);
function fix16 fix16_div(fix16 fa, fix16 fb);

function inline fix16
operator+(fix16 a, fix16 b) {
  return fix16_add(a,b);
}

function inline fix16
operator-(fix16 a, fix16 b) {
  return fix16_sub(a,b);
}

function inline fix16
operator*(fix16 a, fix16 b) {
  return fix16_mul(a,b);
}

function inline fix16
operator/(fix16 a, fix16 b) {
  return fix16_div(a,b);
}

union Vec2_Fix16 {
  struct { fix16 x, y; };
  struct { fix16 min, max; };

  fix16 e[2];
};

function inline Vec2_Fix16
v2fx16 (fix16 x, fix16 y) {
  return {x,y};
}

function inline Vec2_Fix16
v2fx16 (s16 x, s16 y) {
  return {Fix16FromInt(x),Fix16FromInt(y)};
}

function inline Vec2_Fix16
v2fx16 (f32 x, f32 y) {
  return {Fix16FromF32(x),Fix16FromF32(y)};
}

function inline Vec2_Fix16
v2fx16_add (Vec2_Fix16 a, Vec2_Fix16 b) {
  return v2fx16(a.x+b.x, a.y+b.y);
}

function inline Vec2_Fix16
v2fx16_sub (Vec2_Fix16 a, Vec2_Fix16 b) {
  return v2fx16(a.x-b.x,a.y-b.y);
}

function inline Vec2_Fix16
v2fx16_mul (Vec2_Fix16 a, Vec2_Fix16 b) {
  return v2fx16(a.x*b.x,a.y*b.y);
}

function inline Vec2_Fix16
v2fx16_div (Vec2_Fix16 a, Vec2_Fix16 b) {
  return v2fx16(a.x/b.x,a.y/b.y);
}

function inline Vec2_Fix16
v2fx16_muls (Vec2_Fix16 v, fix16 s) {
  return v2fx16(v.x*s,v.y*s);
}

function inline fix16
v2fx16_len (Vec2_Fix16 v) {
  return Fix16FromF32(sqrt(Fix16ToF32(v.x*v.x + v.y*v.y)));
}

function inline Vec2_Fix16
v2fx16_norm (Vec2_Fix16 v) {
  return v2fx16_muls(v, Fix16FromInt(1)/v2fx16_len(v));
}

function inline fix16
v2fx16_dist (Vec2_Fix16 a, Vec2_Fix16 b) {
  return v2fx16_len(v2fx16_sub(a,b));
}

function inline fix16
v2fx16_dot (Vec2_Fix16 a, Vec2_Fix16 b) {
  return a.x*b.x + a.y*b.y;
}

#endif // FIX_H