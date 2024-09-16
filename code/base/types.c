core_function Vec2
v2 (f32 x, f32 y) {
    return comp_lit(Vec2, x, y);
}

core_function Vec2i
v2i (u32 x, u32 y) {
    return comp_lit(Vec2i, x, y);
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
    return comp_lit(Vec3, x, y, z);
}

core_function Vec3i 
v3i (u32 x, u32 y, u32 z) {
    return comp_lit(Vec3i, x, y, z);
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
    return sqrtf(sq(v.x) + sq(v.y));
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
v2cross (Vec2 a, Vec2 b, Vec2 c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

core_function f32  
v3len (Vec3 v) {
    return sqrtf(sq(v.x) + sq(v.y) + sq(v.z));
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

core_function f32 
fmod_cycling (f32 x, f32 y) {
    if (y == 0) {
        return INFINITY;
    }
    f32 remainder = x - (floorf(x/y) * y);
    
    return remainder;
}

core_function f32
lerp (f32 v0, f32 v1, f32 t) {
    return (1.f - t) * v0 + t * v1;
}