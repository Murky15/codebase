#ifndef TRIG_H
#define TRIG_H

#define ANGLE_U16TOF32 ((2*M_PI)/(UINT16_MAX+1))
#define ANGLE_F32TOU16 ((UINT16_MAX+1)/(2*M_PI))

function fix16 fix16_sin (u16 angle);
function fix16 fix16_cos (u16 angle);
function fix16 fix16_tan (u16 angle);

#endif // TRIG_H
