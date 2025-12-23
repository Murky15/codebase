#include <process.h>

s32
os_bootstrap_thread (void *params) {
  Thread_Init_Package *init = (Thread_Init_Package*)params;
  os_set_thread_context(init->tctx);
  os_entry();

  return 0;
}

int
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  /*
  TODO:
  -[ ] Tune arenas
  -[X] Create threads
  -[ ] Process command line
  */

  SYSTEM_INFO sys_info;
  GetSystemInfo(&sys_info);
  u64 page_size = sys_info.dwPageSize;
#ifndef OS_NUM_THREADS
  u64 num_threads = sys_info.dwNumberOfProcessors;
#else
  u64 num_threads = OS_NUM_THREADS;
#endif

  timeBeginPeriod(1);

  Temp_Arena scratch;
  ldefer(scratch = get_scratch(0,0),release_scratch(scratch)) {
    HANDLE *threads = arena_pushn(scratch.arena, HANDLE, num_threads);
    Thread_Init_Package *params = arena_pushn(scratch.arena, Thread_Init_Package, num_threads);
    Thread_Barrier default_barrier = os_barrier_create(scratch.arena, num_threads);
    Thread_Mutex default_mutex = os_mutex_create(scratch.arena);
    u64 *broadcast_buffer = arena_pushn(scratch.arena, u64, 1);

    for (u64 i = 0; i < num_threads; ++i) {
      Thread_Init_Package *p = &params[i];
      Thread_Heat heat = comp_lit(Thread_Heat, i, num_threads, default_barrier, default_mutex, broadcast_buffer);
      p->tctx.heat = heat;
      threads[i] = (HANDLE)_beginthread((_beginthread_proc_type)os_bootstrap_thread, 0, p);
    }

    for (u64 i = 0; i < num_threads; ++i) {
      WaitForSingleObject(threads[i], INFINITE);
    }
  }

  return 0;
}
