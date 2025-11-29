#include <immintrin.h>

#if ARCH_X64
core_function u64
rand_next (void) {
  u64 result = 0;
  _rdrand64_step(&result);

  return result;
}
#else
# error "No RNG implemented for this architecture!"
#endif