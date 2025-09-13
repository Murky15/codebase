core_function Vec2
v2 (f32 x, f32 y) {
  return comp_lit(Vec2, {x, y});
}

core_function Vec2i
v2i (u32 x, u32 y) {
  return comp_lit(Vec2i, {x, y});
}

core_function Vec2i
v2i_from_v2 (Vec2 v) {
  return v2i((u32)floor(v.x), (u32)floor(v.y));
}

core_function Vec2
dv3 (Vec3 v) {
  return v2(v.x, v.y);
}

core_function Vec3
v3 (f32 x, f32 y, f32 z) {
  return comp_lit(Vec3, {x, y, z});
}

core_function Vec3i
v3i (u32 x, u32 y, u32 z) {
  return comp_lit(Vec3i, {x, y, z});
}

core_function Vec3i
v3i_from_v3 (Vec3 v) {
  return v3i((u32)floor(v.x), (u32)floor(v.y), (u32)floor(v.z));
}

core_function Vec3
pv2 (Vec2 v, f32 z) {
  return v3(v.x, v.y, z);
}

core_function f32
v2len (Vec2 v) {
  return (f32)sqrt(sqr(v.x) + sqr(v.y));
}

core_function Vec2
v2sub (Vec2 a, Vec2 b) {
  return v2(a.x - b.x, a.y - b.y);
}

core_function Vec2
v2add (Vec2 a, Vec2 b) {
  return v2(a.x + b.x, a.y + b.y);
}

core_function Vec2
v2muls (Vec2 v, f32 s) {
  return v2(v.x * s, v.y * s);
}

core_function Vec2
v2norm (Vec2 v) {
  return v2muls(v, 1 / v2len(v));
}

core_function f32
v2dot (Vec2 a, Vec2 b) {
  return a.x * b.x + a.y * b.y;
}

core_function f32
v2cross (Vec2 a, Vec2 b) {
  return a.x * b.y - a.y * b.x;
}

core_function f32
v2cross3 (Vec2 a, Vec2 b, Vec2 c) {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

core_function f32
v3len (Vec3 v) {
  return (f32)sqrt(sqr(v.x) + sqr(v.y) + sqr(v.z));
}

core_function Vec3
v3sub (Vec3 a, Vec3 b) {
  return v3(a.x - b.x, a.y - b.y, a.z - b.z);
}

core_function Vec3
v3add (Vec3 a, Vec3 b) {
  return v3(a.x + b.x, a.y + b.y, a.z + b.z);
}

core_function Vec3
v3muls (Vec3 v, f32 s) {
  return v3(v.x * s, v.y * s, v.z * s);
}

core_function Vec3
v3norm (Vec3 v) {
  return v3muls(v, 1 / v3len(v));
}

core_function f32
v3dot (Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

core_function Vec3
v3cross (Vec3 a, Vec3 b) {
  Vec3 r;
  r.x = a.y * b.z - a.z * b.y;
  r.y = a.z * b.x - a.x * b.z;
  r.z = a.x * b.y - a.y * b.x;

  return r;
}

core_function Quat
qi (void) {
  return comp_lit(Quat, 0, 0, 0, 1);
}

core_function Quat
axis_angle (Vec3 v, f32 t) {
  v = v3norm(v);
  f32 st = sinf(t/2.f);
  f32 ct = cosf(t/2.f);

  return comp_lit(Quat, v.x*st, v.y*st, v.z*st, ct);
}

core_function Quat
qnorm (Quat q) {
  f32 len = sqrtf(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
  if (len == 0) { return comp_lit(Quat, 0,0,0,1); }
  f32 s = 1.f / len;
  f32 x = q.x * s;
  f32 y = q.y * s;
  f32 z = q.z * s;
  f32 w = q.w * s;

  return comp_lit(Quat, x,y,z,w);
}

core_function Quat
qmul (Quat a, Quat b) {
  f32 w = a.w*b.w - v3dot(a.xyz, b.xyz);
  Vec3 v0 = v3muls(b.xyz, a.w);
  Vec3 v1 = v3muls(a.xyz, b.w);
  Vec3 v2 = v3cross(a.xyz, b.xyz);
  Vec3 v = v3add(v3add(v0,v1),v2);

  return comp_lit(Quat, v.x,v.y,v.z,w);
}

core_function Mat4
m4i (void) {
  local_persist read_only Mat4 r = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
  };

  return r;
}

core_function Mat4
m4add (Mat4 a, Mat4 b) {
  Mat4 r;
  r.e[0]  = a.e[0]  + b.e[0];
  r.e[1]  = a.e[1]  + b.e[1];
  r.e[2]  = a.e[2]  + b.e[2];
  r.e[3]  = a.e[3]  + b.e[3];
  r.e[4]  = a.e[4]  + b.e[4];
  r.e[5]  = a.e[5]  + b.e[5];
  r.e[6]  = a.e[6]  + b.e[6];
  r.e[7]  = a.e[7]  + b.e[7];
  r.e[8]  = a.e[8]  + b.e[8];
  r.e[9]  = a.e[9]  + b.e[9];
  r.e[10] = a.e[10] + b.e[10];
  r.e[11] = a.e[11] + b.e[11];
  r.e[12] = a.e[12] + b.e[12];
  r.e[13] = a.e[13] + b.e[13];
  r.e[14] = a.e[14] + b.e[14];
  r.e[15] = a.e[15] + b.e[15];

  return r;
}

core_function Mat4
m4sub (Mat4 a, Mat4 b) {
  Mat4 r;
  r.e[0]  = a.e[0]  - b.e[0];
  r.e[1]  = a.e[1]  - b.e[1];
  r.e[2]  = a.e[2]  - b.e[2];
  r.e[3]  = a.e[3]  - b.e[3];
  r.e[4]  = a.e[4]  - b.e[4];
  r.e[5]  = a.e[5]  - b.e[5];
  r.e[6]  = a.e[6]  - b.e[6];
  r.e[7]  = a.e[7]  - b.e[7];
  r.e[8]  = a.e[8]  - b.e[8];
  r.e[9]  = a.e[9]  - b.e[9];
  r.e[10] = a.e[10] - b.e[10];
  r.e[11] = a.e[11] - b.e[11];
  r.e[12] = a.e[12] - b.e[12];
  r.e[13] = a.e[13] - b.e[13];
  r.e[14] = a.e[14] - b.e[14];
  r.e[15] = a.e[15] - b.e[15];

  return r;
}

core_function Mat4
m4mul (Mat4 a, Mat4 b) {
  Mat4 r;
  r.i[0][0] = a.i[0][0]*b.i[0][0] + a.i[0][1]*b.i[1][0] + a.i[0][2]*b.i[2][0] + a.i[0][3]*b.i[3][0];
  r.i[1][0] = a.i[1][0]*b.i[0][0] + a.i[1][1]*b.i[1][0] + a.i[1][2]*b.i[2][0] + a.i[1][3]*b.i[3][0];
  r.i[2][0] = a.i[2][0]*b.i[0][0] + a.i[2][1]*b.i[1][0] + a.i[2][2]*b.i[2][0] + a.i[2][3]*b.i[3][0];
  r.i[3][0] = a.i[3][0]*b.i[0][0] + a.i[3][1]*b.i[1][0] + a.i[3][2]*b.i[2][0] + a.i[3][3]*b.i[3][0];

  r.i[0][1] = a.i[0][0]*b.i[0][1] + a.i[0][1]*b.i[1][1] + a.i[0][2]*b.i[2][1] + a.i[0][3]*b.i[3][1];
  r.i[1][1] = a.i[1][0]*b.i[0][1] + a.i[1][1]*b.i[1][1] + a.i[1][2]*b.i[2][1] + a.i[1][3]*b.i[3][1];
  r.i[2][1] = a.i[2][0]*b.i[0][1] + a.i[2][1]*b.i[1][1] + a.i[2][2]*b.i[2][1] + a.i[2][3]*b.i[3][1];
  r.i[3][1] = a.i[3][0]*b.i[0][1] + a.i[3][1]*b.i[1][1] + a.i[3][2]*b.i[2][1] + a.i[3][3]*b.i[3][1];

  r.i[0][2] = a.i[0][0]*b.i[0][2] + a.i[0][1]*b.i[1][2] + a.i[0][2]*b.i[2][2] + a.i[0][3]*b.i[3][2];
  r.i[1][2] = a.i[1][0]*b.i[0][2] + a.i[1][1]*b.i[1][2] + a.i[1][2]*b.i[2][2] + a.i[1][3]*b.i[3][2];
  r.i[2][2] = a.i[2][0]*b.i[0][2] + a.i[2][1]*b.i[1][2] + a.i[2][2]*b.i[2][2] + a.i[2][3]*b.i[3][2];
  r.i[3][2] = a.i[3][0]*b.i[0][2] + a.i[3][1]*b.i[1][2] + a.i[3][2]*b.i[2][2] + a.i[3][3]*b.i[3][2];

  r.i[0][3] = a.i[0][0]*b.i[0][3] + a.i[0][1]*b.i[1][3] + a.i[0][2]*b.i[2][3] + a.i[0][3]*b.i[3][3];
  r.i[1][3] = a.i[1][0]*b.i[0][3] + a.i[1][1]*b.i[1][3] + a.i[1][2]*b.i[2][3] + a.i[1][3]*b.i[3][3];
  r.i[2][3] = a.i[2][0]*b.i[0][3] + a.i[2][1]*b.i[1][3] + a.i[2][2]*b.i[2][3] + a.i[2][3]*b.i[3][3];
  r.i[3][3] = a.i[3][0]*b.i[0][3] + a.i[3][1]*b.i[1][3] + a.i[3][2]*b.i[2][3] + a.i[3][3]*b.i[3][3];

  return r;
}

core_function Mat4
m4muls (Mat4 m, f32 s) {
  Mat4 r;
  r.e[0]  = m.e[0]  * s;
  r.e[1]  = m.e[1]  * s;
  r.e[2]  = m.e[2]  * s;
  r.e[3]  = m.e[3]  * s;
  r.e[4]  = m.e[4]  * s;
  r.e[5]  = m.e[5]  * s;
  r.e[6]  = m.e[6]  * s;
  r.e[7]  = m.e[7]  * s;
  r.e[8]  = m.e[8]  * s;
  r.e[9]  = m.e[9]  * s;
  r.e[10] = m.e[10] * s;
  r.e[11] = m.e[11] * s;
  r.e[12] = m.e[12] * s;
  r.e[13] = m.e[13] * s;
  r.e[14] = m.e[14] * s;
  r.e[15] = m.e[15] * s;

  return r;
}

core_function Vec4
m4mulv (Mat4 m, Vec4 v) {
  Vec4 r;
  r.e[0] = m.i[0][0]*v.e[0] + m.i[0][1]*v.e[1] + m.i[0][2]*v.e[2] + m.i[0][3]*v.e[3];
  r.e[1] = m.i[1][0]*v.e[0] + m.i[1][1]*v.e[1] + m.i[1][2]*v.e[2] + m.i[1][3]*v.e[3];
  r.e[2] = m.i[2][0]*v.e[0] + m.i[2][1]*v.e[1] + m.i[2][2]*v.e[2] + m.i[2][3]*v.e[3];
  r.e[3] = m.i[3][0]*v.e[0] + m.i[3][1]*v.e[1] + m.i[3][2]*v.e[2] + m.i[3][3]*v.e[3];

  return r;
}

core_function Mat4
m4scale (Vec3 s) {
  Mat4 r = m4i();
  r.i[0][0] = s.x;
  r.i[1][1] = s.y;
  r.i[2][2] = s.z;

  return r;
}

core_function Mat4
m4rotate (Quat r) {
  Mat4 result = m4i();
  r = qnorm(r);
  f32 s = 2.f;
  f32 bs = r.x*s,  cs = r.y*s,  ds = r.z*s;
  f32 ab = r.w*bs, ac = r.w*cs, ad = r.w*ds;
  f32 bb = r.x*bs, bc = r.x*cs, bd = r.x*ds;
  f32 cc = r.y*cs, cd = r.y*ds, dd = r.z*ds;

  result.i[0][0] = 1.f - cc - dd;
  result.i[0][1] = bc - ad;
  result.i[0][2] = bd + ac;
  result.i[1][0] = bc + ad;
  result.i[1][1] = 1.f - bb - dd;
  result.i[1][2] = cd - ab;
  result.i[2][0] = bd - ac;
  result.i[2][1] = cd + ab;
  result.i[2][2] = 1.f - bb - cc;

  return result;
}

core_function Mat4
m4translate (Vec3 t) {
  Mat4 r = m4i();
  r.i[0][3] = t.x;
  r.i[1][3] = t.y;
  r.i[2][3] = t.z;

  return r;
}

core_function Mat4
m4perspective(f32 fovy, f32 aspect, f32 znear, f32 zfar) {
  Mat4 r = {0};
  f32 h = 1.f / tanf(fovy*0.5f);
  f32 w = h / aspect;
  f32 frange = zfar / (zfar - znear);

  r.i[0][0] = w;
  r.i[1][1] = h;
  r.i[2][2] = frange;
  r.i[2][3] = -frange * znear;
  r.i[3][2] = 1.f;

  return r;
}

core_function Mat4
m4orthographic(f32 width, f32 height, f32 znear, f32 zfar) {
  Mat4 r = m4i();
  f32 w = 2.f/width;
  f32 h = 2.f/height;
  f32 a = 1.f/(zfar-znear);
  f32 b = -a * znear;

  r.i[0][0] = w;
  r.i[1][1] = h;
  r.i[2][2] = a;
  r.i[2][3] = b;

  return r;
}

core_function Mat4
m4lookat(Vec3 viewpoint, Vec3 focus, Vec3 reference_up) {
  Vec3 f = v3norm(v3sub(focus, viewpoint));
  Vec3 s = v3norm(v3cross(reference_up, f));
  Vec3 t = v3norm(v3cross(f, s));

  Mat4 V = m4i();
  V.r[0] = comp_lit(Vec4, s.x, s.y, s.z);
  V.r[1] = comp_lit(Vec4, t.x, t.y, t.z);
  V.r[2] = comp_lit(Vec4, f.x, f.y, f.z);
  Mat4 T = m4translate(v3(-viewpoint.x, -viewpoint.y, -viewpoint.z));
  Mat4 r = m4mul(V,T);

  return r;
}

core_function f64
fmod_cycling (f64 x, f64 y) {
  if (y == 0) {
    return INFINITY;
  }
  f64 remainder = x - ((f64)floor(x/y) * y);

  return remainder;
}

core_function f64
lerp (f64 v0, f64 v1, f64 t) {
  return (1.f - t) * v0 + t * v1;
}

core_function b32
almost_equal (f64 a, f64 b) {
  local_persist read_only f64 e = 0.0001f;
  return fabs(a - b) <= e;
}
