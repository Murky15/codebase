#ifndef VALUES_PER_LINE
# define VALUES_PER_LINE 16
#endif

// NOTE: FIX16_SCALING_FACTOR
#ifndef TRIG_SCALING_FACTOR
# define TRIG_SCALING_FACTOR 1024
#endif

// NOTE: Q8.8:  8
// NOTE: Q6.10: 6
#ifndef TRIG_INDEX_BITS
# define TRIG_INDEX_BITS 6
#endif
StaticAssert(TRIG_INDEX_BITS <= 14, index_resolution_cap);
#define TRIG_INDEX_MAX (1 << TRIG_INDEX_BITS)
#define TRIG_FRAC_BITS (14 - TRIG_INDEX_BITS)

/*
#if !defined(RESOLUTION_WIDTH) || !defined(RESOLUTION_HEIGHT)
# error "Output resolution must be defined!"
#endif
*/

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