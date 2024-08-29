core_function f32 
fmod_cycling (f32 x, f32 y) {
    if (y == 0) {
        return INFINITY;
    }
    f32 remainder = x - (floorf(x/y) * y);
    
    return remainder;
}