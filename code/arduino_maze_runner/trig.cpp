#include "meta.h"
#include "generated/trig_lookup.cpp"

function fix16
fix16_sin (u16 angle) {
  u16 q    = angle >> 14;
  u16 pos  = angle & 0x3FFF;
  if (q & 1) pos = 0x4000 - pos;

  u16 idx  = pos >> TRIG_FRAC_BITS;
  u16 frac = pos & ((1 << TRIG_FRAC_BITS)-1);

  s32 a = sin_lookup[idx], b = sin_lookup[idx+1];
  s32 val = a + (((b - a) * (s32)frac) >> TRIG_FRAC_BITS);
  if (q & 2) val = -val;

  return fix16_raw((s16)val);
}

function fix16
fix16_cos (u16 angle) {
  return fix16_sin(angle + 0x4000);
}

function fix16
fix16_tan (u16 angle) {
  fix16 y = fix16_sin(angle);
  fix16 x = fix16_cos(angle);
  if (x == 0) return fix16_raw(y >= 0 ? INT16_MAX : INT16_MIN);

  return fix16_div(y,x);
}