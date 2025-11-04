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
  EnterSynchronizationBarrier(ptr_from_int(barrier.opaque_data), 0);
}

core_function void
os_barrier_invalidate (Thread_Barrier barrier) {
  DeleteSynchronizationBarrier(ptr_from_int(barrier.opaque_data));
}