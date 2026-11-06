#ifndef TRIG_H
#define TRIG_H

#define ANGLE_U16TOF32 ((2.f*M_PI)/(65536.f))
#define ANGLE_F32TOU16 ((65536.f)/(2.f*M_PI))

function fix16 fix16_sin (u16 angle);
function fix16 fix16_cos (u16 angle);

#endif // TRIG_H
