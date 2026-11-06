#ifndef VALUES_PER_LINE
# define VALUES_PER_LINE 16
#endif

#ifndef TRIG_SCALING_FACTOR
# define TRIG_SCALING_FACTOR 1024
#endif

#ifndef TRIG_INDEX_BITS
# define TRIG_INDEX_BITS 6
#endif
StaticAssert(TRIG_INDEX_BITS <= 14, index_resolution_cap);
#define TRIG_INDEX_MAX (1 << TRIG_INDEX_BITS)
#define TRIG_FRAC_BITS (14 - TRIG_INDEX_BITS)

#if !defined(RESOLUTION_WIDTH) || !defined(RESOLUTION_HEIGHT)
# define RESOLUTION_WIDTH  (64*10)
# define RESOLUTION_HEIGHT (64*10)
#endif

#ifndef RAYCAST_FOV
# define RAYCAST_FOV (M_PI/2.0)
#endif

#ifndef RAYCAST_RESOLUTION
# define RAYCAST_RESOLUTION 32
#endif

#if OS_WINDOWS
# define PROGMEM
#endif