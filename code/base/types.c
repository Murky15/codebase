// @note: Constructors

core_function Vec2 
v2 (f32 x, f32 y) {
  return (Vec2){x,y};
}

core_function Vec3 
v3 (f32 x, f32 y, f32 z) {
  return (Vec3){x,y,z};
}

core_function Vec4 
v4 (f32 x, f32 y, f32 z, f32 w) {
  return (Vec4){x,y,z,w};

}

// @note: Arithmetic

core_function f32  
v2_mag (Vec2 v) {
  return sqrtf(square(v.x) + square(v.y));
}

core_function f32  
v3_mag (Vec3 v) {
  return sqrtf(square(v.x) + square(v.y) + square(v.z));
}

core_function f32  
v4_mag (Vec4 v) {
  return sqrtf(square(v.x) + square(v.y) + square(v.z) + square(v.w));
}

core_function Vec2 
v2_add (Vec2 a, Vec2 b) {
  return (Vec2){a.x + b.x, a.y + b.y};
}

core_function Vec3 
v3_add (Vec3 a, Vec3 b) {
  return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

core_function Vec4 
v4_add (Vec4 a, Vec4 b) {
  return (Vec4){a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

core_function Vec2 
v2_sub (Vec2 a, Vec2 b) {
  return (Vec2){a.x - b.x, a.y - b.y};
}

core_function Vec3 
v3_sub (Vec3 a, Vec3 b) {
  return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

core_function Vec4 
v4_sub (Vec4 a, Vec4 b) {
  return (Vec4){a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}

core_function Vec2 
v2_muls (Vec2 v, f32 s) {
  return (Vec2){v.x * s, v.y * s};
}

core_function Vec3 
v3_muls (Vec3 v, f32 s) {
  return (Vec3){v.x * s, v.y * s, v.z * s};
}

core_function Vec4 
v4_muls (Vec4 v, f32 s) {
  return (Vec4){v.x * s, v.y * s, v.z * s, v.w * s};
}

core_function Vec2 
v2_norm (Vec2 v) {
  f32 mag = v2_mag(v);
  return (Vec2){v.x / mag, v.y / mag};
}

core_function Vec3 
v3_norm (Vec3 v) {
  f32 mag = v3_mag(v);
  return (Vec3){v.x / mag, v.y / mag, v.z / mag};
}

core_function Vec4 
v4_norm (Vec4 v) {
  f32 mag = v4_mag(v);
  return (Vec4){v.x / mag, v.y / mag, v.z / mag, v.w / mag};
}

core_function f32  
v2_dot (Vec2 a, Vec2 b) {
  return a.x * b.x + a.y * b.y;
}

core_function f32  
v3_dot (Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

core_function f32  
v4_dot (Vec4 a, Vec4 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

core_function Vec3 
v3_cross (Vec3 a, Vec3 b) {
  Vec3 r;
  r.x = a.y * b.z - a.z * b.y;
  r.y = a.z * b.x - a.x * b.z;
  r.z = a.x * b.y - a.y * b.x;
  return r;
}