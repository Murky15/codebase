#ifndef FIX_H
#define FIX_H

/*
TODO:
- [X] Remove constructors and add literals
- [X] Make designated fix32 type with quick operations
- [X] Consolidate unsaturated & saturated fix16 functions
- [X] Remove remaining f32 stragglers
*/

// NOTE: Must be a signed integer
template <typename T, int F>
struct fix {
  T val;

  static constexpr T FRACTIONAL_BITS = F;
  static constexpr T SCALING_FACTOR  = 1 << FRACTIONAL_BITS;

  static constexpr fix max () { return from_raw(~T(1LL << (sizeof(T)*8)-1)); }
  static constexpr fix min () { return from_raw(~T(0)); }

  static constexpr fix from_raw (T x)   { return fix{x}; }
  static constexpr fix from_int (int x) { return from_raw(x<<FRACTIONAL_BITS); }
  static constexpr fix from_f32 (f32 x) { return from_raw(x*(f32)SCALING_FACTOR); }
  int to_int () { return val>>FRACTIONAL_BITS;  }
  f32 to_f32 () { return val/(f32)SCALING_FACTOR; }

  inline bool operator==(fix rhs) { return this->val==rhs.val; }
  inline bool operator!=(fix rhs) { return this->val!=rhs.val; }
  inline bool operator< (fix rhs) { return this->val< rhs.val; }
  inline bool operator> (fix rhs) { return this->val> rhs.val; }
  inline bool operator<=(fix rhs) { return this->val<=rhs.val; }
  inline bool operator>=(fix rhs) { return this->val>=rhs.val; }
};

typedef fix<s16, 10> fix16;
function fix16 fix16_add_saturated   (fix16 fa, fix16 fb);
function fix16 fix16_sub_saturated   (fix16 fa, fix16 fb);
function fix16 fix16_mul_saturated   (fix16 fa, fix16 fb);
function fix16 fix16_div_saturated   (fix16 fa, fix16 fb);
function fix16 fix16_abs             (fix16 x);
function fix16 fix16_sqrt            (fix16 x);
function fix16 fix16_floor           (fix16 x);

constexpr fix16 operator"" _fx16(long double x) { return fix16::from_f32(x); };
function inline fix16 operator+(fix16 a, fix16 b) { return fix16_add_saturated(a,b); }
function inline fix16 operator-(fix16 a, fix16 b) { return fix16_sub_saturated(a,b); }
function inline fix16 operator*(fix16 a, fix16 b) { return fix16_mul_saturated(a,b); }
function inline fix16 operator/(fix16 a, fix16 b) { return fix16_div_saturated(a,b); }
function inline fix16& operator+=(fix16 &a, fix16 b) { a = a + b; return a; }
function inline fix16& operator-=(fix16 &a, fix16 b) { a = a - b; return a; }
function inline fix16& operator*=(fix16 &a, fix16 b) { a = a * b; return a; }
function inline fix16& operator/=(fix16 &a, fix16 b) { a = a / b; return a; }

typedef fix<s32, fix16::FRACTIONAL_BITS> fix32;
constexpr fix32 operator"" _fx32(long double x) { return fix32::from_f32(x); };
function fix32 fix32_add      (fix32 fa, fix32 fb);
function fix32 fix32_sub      (fix32 fa, fix32 fb);
function fix32 fix32_mul_lazy (fix32 fa, fix32 fb);
function fix32 fix32_div_lazy (fix32 fa, fix32 fb);
function fix32 fix32_abs      (fix32 x);

struct Vec2_Fix16 {
  fix16 x, y;
};

union Rect_Fix16 {
  struct { fix16 x, y, w, h;  };
  struct { Vec2_Fix16 xy, wh; };
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
  return v2fx16_muls(v, 1.0_fx16/v2fx16_len(v));
}

function inline fix16
v2fx16_dist (Vec2_Fix16 a, Vec2_Fix16 b) {
  return v2fx16_len(v2fx16_sub(a,b));
}

function inline fix16
v2fx16_dot (Vec2_Fix16 a, Vec2_Fix16 b) {
  return a.x*b.x + a.y*b.y;
}

function bool rect_circle_intersect (Rect_Fix16 rect, Vec2_Fix16 c, fix16 r, fix16 *overlap = nullptr);

#endif // FIX_H