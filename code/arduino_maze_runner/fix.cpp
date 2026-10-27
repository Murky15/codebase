// TODO: Saturate?
function fix16
fix16_add (fix16 fa, fix16 fb) {
  return {fa.val + fb.val};
}

function fix16
fix16_sub (fix16 fa, fix16 fb) {
  return {fa.val - fb.val};
}

function fix16
fix16_mul (fix16 fa, fix16 fb) {
  s16 a = fa.val, b = fb.val;
  s32 result = (s32)a * b;
  s32 pow  = 1 << (FIX16_FRACTIONAL_BITS-1);
  result += pow + (result >> 31);
  result >>= FIX16_FRACTIONAL_BITS;

  return {(s16)result};
}

function fix16
fix16_div (fix16 fa, fix16 fb) {
  s16 a = fa.val, b = fb.val;
  s32 sign = (a ^ b) >> 15;
  u32 ua = abs(a);
  u32 ub = abs(b);
  u32 uc = ((ua << FIX16_FRACTIONAL_BITS) + (ub >> 1)) / ub;
  s16 result = (uc + sign) ^ sign;

  return {result};
}