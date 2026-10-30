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
  s16 a = fa.val, b = fb.val;
  s32 mul = (s32)a * b;
  s32 pow  = 1 << (FIX16_FRACTIONAL_BITS-1);
  mul += pow + (mul >> 31);
  mul >>= FIX16_FRACTIONAL_BITS;
  mul = Clamp(mul, INT16_MIN, INT16_MAX);

  return fix16_raw(mul);
}

function fix16
fix16_div (fix16 fa, fix16 fb) {
  s16 a = fa.val, b = fb.val;
  s32 sign = (a ^ b) >> 15;
  u32 ua = abs(a);
  u32 ub = abs(b);
  u32 uc = ((ua << FIX16_FRACTIONAL_BITS) + (ub >> 1)) / ub;
  s32 div = (uc + sign) ^ sign;
  div = Clamp(div, INT16_MIN, INT16_MAX);

  return fix16_raw(div);
}

function fix16
fix16_abs (fix16 x) {
  return fix16_raw(x.val == INT16_MIN ? INT16_MAX : s16(abs(x.val)));
}

function fix16
fix16_sqrt (fix16 x) {
  return sqrtf(Fix16ToF32(x));
}

function fix16
fix16_floor (fix16 x) {
  s16 int_bits = 16 - FIX16_FRACTIONAL_BITS;
  s16 mask = (1 << int_bits)-1;

  return fix16_raw(x.val & (mask << FIX16_FRACTIONAL_BITS));
}
