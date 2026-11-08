function s32
fix16_add (fix16 fa, fix16 fb) {
  return (s32)fa.val + fb.val;
}

function s32
fix16_sub (fix16 fa, fix16 fb) {
  return (s32)fa.val - fb.val;
}

function s32
fix16_mul (fix16 fa, fix16 fb) {
  s32 a = fa.val, b = fb.val;
  s32 mul = a * b;
  mul += (1 << fix16::FRACTIONAL_BITS-1);
  mul >>= fix16::FRACTIONAL_BITS;

  return mul;
}

function s32
fix16_div (fix16 fa, fix16 fb) {
  s32 a = fa.val, b = fb.val;
  s32 div = ((a << fix16::FRACTIONAL_BITS) + (b >> 1)) / b;

  return div;
}

function fix16
fix16_add_saturated (fix16 fa, fix16 fb) {
  return fix16::from_raw(Clamp(fix16_add(fa,fb), INT16_MIN, INT16_MAX));
}

function fix16
fix16_sub_saturated (fix16 fa, fix16 fb) {
  return fix16::from_raw(Clamp(fix16_sub(fa,fb), INT16_MIN, INT16_MAX));
}

function fix16
fix16_mul_saturated (fix16 fa, fix16 fb) {
  return fix16::from_raw(Clamp(fix16_mul(fa,fb), INT16_MIN, INT16_MAX));
}

function fix16
fix16_div_saturated (fix16 fa, fix16 fb) {
  return fix16::from_raw(Clamp(fix16_div(fa,fb), INT16_MIN, INT16_MAX));
}

function fix16
fix16_abs (fix16 x) {
  if (x.val == INT16_MIN) return fix16::max();
  return fix16::from_raw(x.val < 0 ? -x.val : x.val);
}

function fix16
fix16_sqrt (fix16 x) {
  return sqrtf(x.to_f32());
}

function fix16
fix16_floor (fix16 x) {
  return fix16::from_raw(x.val & (s16)~(fix16::SCALING_FACTOR - 1));
}