function fix16
fix16_raw (s16 x) {
  fix16 result;
  result.val = x;

  return result;
}

function fix16
fix16_add (fix16 fa, fix16 fb) {
  s32 add = (s32)fa.val + fb.val;
  add = Clamp(add, INT16_MIN, INT16_MAX);

  return fix16_raw(add);
}

function fix16
fix16_sub (fix16 fa, fix16 fb) {
  s32 sub = (s32)fa.val - fb.val;
  sub = Clamp(sub, INT16_MIN, INT16_MAX);

  return fix16_raw(sub);
}

function fix16
fix16_mul (fix16 fa, fix16 fb) {
  s32 a = fa.val, b = fb.val;
  s32 mul = a * b;
  mul >>= FIX16_FRACTIONAL_BITS;
  mul = Clamp(mul, INT16_MIN, INT16_MAX);

  return fix16_raw(mul);
}

function fix16
fix16_div (fix16 fa, fix16 fb) {
  s32 a = fa.val, b = fb.val;
  s32 div = (a << FIX16_FRACTIONAL_BITS) / b;
  div = Clamp(div, INT16_MIN, INT16_MAX);

  return fix16_raw(div);
}

function fix16
fix16_abs (fix16 x) {
  if (x.val == INT16_MIN) return FIX16_MAX;
  return fix16_raw(x.val < 0 ? -x.val : x.val);
}

function fix16
fix16_sqrt (fix16 x) {
  return sqrtf(Fix16ToF32(x));
}

function fix16
fix16_floor (fix16 x) {
  return fix16_raw(x.val & (s16)~(FIX16_SCALING_FACTOR - 1));
}
