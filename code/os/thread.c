core_function Thread_Context*
os_get_thread_context (void) {
  local_persist threadvar Thread_Context current_thread_context;

  return &current_thread_context;
}

core_function void
os_set_thread_context (Thread_Context ctx) {
  Thread_Context *current_thread_context = os_get_thread_context();
  *current_thread_context = ctx;
}

core_function void
os_heat_sync (void) {
  os_barrier_wait(os_get_thread_context()->heat.barrier);
}

core_function void
os_heat_sync_u64 (u64 *value, u64 broadcaster_id) {
  Thread_Heat heat = os_get_thread_context()->heat;
  if (runner_id() == broadcaster_id) {
    memory_copy(heat.broadcast_buffer, value, sizeof(u64));
  }
  os_heat_sync();

  if (runner_id() != broadcaster_id) {
    memory_copy(value, heat.broadcast_buffer, sizeof(u64));
  }
  os_heat_sync();
}

core_function void
os_heat_begin_critical_section (void) {
  os_mutex_claim(os_get_thread_context()->heat.mutex);
}

core_function void
os_heat_end_critical_section (void) {
  os_mutex_release(os_get_thread_context()->heat.mutex);
}

core_function Rangei
os_heat_distribute(u64 num_items) {
  u64 work_per_thread = num_items / num_runners();
  u64 extra_work = num_items % num_runners();
  b32 thread_has_excess = (runner_id() < extra_work);
  u64 amount_excess = thread_has_excess ? runner_id() : extra_work;
  s32 start = runner_id() * work_per_thread + amount_excess;
  s32 end = start + work_per_thread + !!thread_has_excess;

  return comp_lit(Rangei, start, end);
}