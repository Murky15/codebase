#ifndef FIX_H
#define FIX_H

template <typename T, int F>
struct fix {
  T val;

  static constexpr T FRACTIONAL_BITS = F;
  static constexpr T SCALING_FACTOR  = 1 << FRACTIONAL_BITS;

  static constexpr fix from_raw (T x) { fix result; result.val = x; return result; }
  static constexpr fix max () { return from_raw(~T(1 << (sizeof(T)*8)-1)); }

  fix() = default;
  constexpr fix (int x) : val(x<<FRACTIONAL_BITS) {}
  constexpr fix (f32 x) : val(x*(f32)SCALING_FACTOR) {}

  int to_int () { return val >> FRACTIONAL_BITS;  }
  f32 to_f32 () { return val/(f32)SCALING_FACTOR; }

  inline bool operator==(fix rhs) { return this->val==rhs.val; }
  inline bool operator!=(fix rhs) { return this->val!=rhs.val; }
  inline bool operator< (fix rhs) { return this->val< rhs.val; }
  inline bool operator> (fix rhs) { return this->val> rhs.val; }
  inline bool operator<=(fix rhs) { return this->val<=rhs.val; }
  inline bool operator>=(fix rhs) { return this->val>=rhs.val; }
};

typedef fix<s16, 10> fix16;
function s32   fix16_add             (fix16 fa, fix16 fb);
function s32   fix16_sub             (fix16 fa, fix16 fb);
function s32   fix16_mul             (fix16 fa, fix16 fb);
function s32   fix16_div             (fix16 fa, fix16 fb);
function fix16 fix16_add_saturated   (fix16 fa, fix16 fb);
function fix16 fix16_sub_saturated   (fix16 fa, fix16 fb);
function fix16 fix16_mul_saturated   (fix16 fa, fix16 fb);
function fix16 fix16_div_saturated   (fix16 fa, fix16 fb);
function fix16 fix16_abs             (fix16 x);
function fix16 fix16_sqrt            (fix16 x);
function fix16 fix16_floor           (fix16 x);

function inline fix16 operator+(fix16 a, fix16 b) { return fix16_add_saturated(a,b); }
function inline fix16 operator-(fix16 a, fix16 b) { return fix16_sub_saturated(a,b); }
function inline fix16 operator*(fix16 a, fix16 b) { return fix16_mul_saturated(a,b); }
function inline fix16 operator/(fix16 a, fix16 b) { return fix16_div_saturated(a,b); }
function inline fix16& operator+=(fix16 &a, fix16 b) { a = a + b; return a; }
function inline fix16& operator-=(fix16 &a, fix16 b) { a = a - b; return a; }
function inline fix16& operator*=(fix16 &a, fix16 b) { a = a * b; return a; }
function inline fix16& operator/=(fix16 &a, fix16 b) { a = a / b; return a; }

struct Vec2_Fix16 {
  fix16 x, y;
};

function inline Vec2_Fix16
v2fx16 (fix16 x, fix16 y) {
  return {x,y};
}

function inline Vec2
v2fx16_to_v2 (Vec2_Fix16 v) {
  return V2(v.x.to_f32(), v.y.to_f32());
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
  return fix16_sqrt(v.x*v.x + v.y*v.y);
}

function inline Vec2_Fix16
v2fx16_norm (Vec2_Fix16 v) {
  return v2fx16_muls(v, 1/v2fx16_len(v));
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