function fix16
fix16_add_saturated (fix16 fa, fix16 fb) {
  s32 add = (s32)fa.val + fb.val;
  return fix16::from_raw(Clamp(add, INT16_MIN, INT16_MAX));
}

function fix16
fix16_sub_saturated (fix16 fa, fix16 fb) {
  s32 sub = (s32)fa.val - fb.val;
  return fix16::from_raw(Clamp(sub, INT16_MIN, INT16_MAX));
}

function fix16
fix16_mul_saturated (fix16 fa, fix16 fb) {
  s32 mul = ((s32)fa.val * fb.val) + (1 << fix16::FRACTIONAL_BITS-1);
  mul >>= fix16::FRACTIONAL_BITS;

  return fix16::from_raw(Clamp(mul, INT16_MIN, INT16_MAX));
}

function fix16
fix16_div_saturated (fix16 fa, fix16 fb) {
  s32 a = fa.val, b = fb.val;
  s32 div = ((a << fix16::FRACTIONAL_BITS) + (b >> 1)) / b;

  return fix16::from_raw(Clamp(div, INT16_MIN, INT16_MAX));
}

function fix16
fix16_abs (fix16 x) {
  if (x.val == INT16_MIN) return fix16::max();
  return fix16::from_raw(x.val < 0 ? -x.val : x.val);
}

function fix16
fix16_sqrt (fix16 x) {
  return fix16::from_f32(sqrtf(x.to_f32()));
}

function fix16
fix16_floor (fix16 x) {
  return fix16::from_raw(x.val & (s16)~(fix16::SCALING_FACTOR - 1));
}


function fix32
fix32_add (fix32 fa, fix32 fb) {
  return fix32::from_raw(fa.val + fb.val);
}

function fix32
fix32_sub (fix32 fa, fix32 fb) {
  return fix32::from_raw(fa.val - fb.val);
}

function fix32
fix32_mul_lazy (fix32 fa, fix32 fb) {
  s32 mul = ((s32)fa.val * fb.val) + (1 << fix32::FRACTIONAL_BITS-1);
  mul >>= fix32::FRACTIONAL_BITS;

  return fix32::from_raw(mul);
}

function fix32
fix32_div_lazy (fix32 fa, fix32 fb) {
  s32 a = fa.val, b = fb.val;
  s32 div = ((a << fix32::FRACTIONAL_BITS) + (b >> 1)) / b;

  return fix32::from_raw(div);
}

function fix32
fix32_abs (fix32 x) {
  if (x.val == INT32_MIN) return fix32::max();
  return fix32::from_raw(x.val < 0 ? -x.val : x.val);
}