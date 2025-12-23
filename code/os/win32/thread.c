core_function Thread_Barrier
os_barrier_create (Arena *arena, u64 num_threads) {
  Thread_Barrier result;
  SYNCHRONIZATION_BARRIER *barrier = arena_pushn(arena, SYNCHRONIZATION_BARRIER, 1);
  InitializeSynchronizationBarrier(barrier, num_threads, -1);
  result.opaque_data = int_from_ptr(barrier);

  return result;
}

core_function void
os_barrier_wait (Thread_Barrier barrier) {
  EnterSynchronizationBarrier((LPSYNCHRONIZATION_BARRIER)ptr_from_int(barrier.opaque_data), 0);
}

core_function void
os_barrier_invalidate (Thread_Barrier barrier) {
  DeleteSynchronizationBarrier((LPSYNCHRONIZATION_BARRIER)ptr_from_int(barrier.opaque_data));
}

core_function Thread_Mutex
os_mutex_create (Arena *arena) {
  Thread_Mutex result;
  CRITICAL_SECTION *mutex = arena_pushn(arena, CRITICAL_SECTION, 1);
  InitializeCriticalSectionAndSpinCount(mutex, -1);
  result.opaque_data = int_from_ptr(mutex);

  return result;
}

core_function void
os_mutex_claim (Thread_Mutex mutex) {
  EnterCriticalSection((LPCRITICAL_SECTION)ptr_from_int(mutex.opaque_data));
}

core_function void
os_mutex_release (Thread_Mutex mutex) {
  LeaveCriticalSection((LPCRITICAL_SECTION)ptr_from_int(mutex.opaque_data));
}

core_function void
os_mutex_invalidate (Thread_Mutex mutex) {
  DeleteCriticalSection((LPCRITICAL_SECTION)ptr_from_int(mutex.opaque_data));
}

core_function u64
os_get_perf_frequency (void) {
  local_persist threadvar LARGE_INTEGER freq;
  if (freq.QuadPart == 0)
    QueryPerformanceFrequency(&freq);

  return freq.QuadPart;
}

core_function u64
os_query_clock (void) {
  LARGE_INTEGER tick;
  QueryPerformanceCounter(&tick);

  return tick.QuadPart;
}

core_function f64
os_get_elapsed_ms (u64 t1, u64 t2) {
  u64 freq = os_get_perf_frequency();
  u64 elapsed_ms = (t2 - t1) * 1000;

  return (f32)elapsed_ms / freq;
}

core_function f64
os_clock_seconds (void) {
  u64 freq = os_get_perf_frequency();
  u64 current_time = os_query_clock();

  return (f64)current_time / freq;
}

core_function u64
os_ms_to_tick_interval (f64 ms) {
  u64 freq = os_get_perf_frequency();
  u64 ticks = ms * freq;
  ticks /= 1000;

  return ticks;
}