#ifndef BASE_RANDOM_H
#define BASE_RANDOM_H

// @todo: Hardware rng & better prngs

#define LCG_MAX (1ULL << 48) 

core_function u64 lcg_next(u64 *first_seed);

#endif //BASE_RANDOM_H
