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
