core_function u64
lcg_next (u64 *first_seed) {
    local_persist threadvar u64 _seed;
    _seed = first_seed != 0 ? *first_seed : _seed;
    return _seed = (25214903917 * _seed + 11) % LCG_MAX;
}