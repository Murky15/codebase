#ifndef OS_THREAD_H
#define OS_THREAD_H

typedef struct Thread_Barrier {
  u64 opaque_data;
} Thread_Barrier;

typedef struct Thread_Mutex {
  u64 opaque_data;
} Thread_Mutex;

typedef struct Thread_Heat {
  u64 runner_id;
  u64 num_runners;
  Thread_Barrier barrier;
  Thread_Mutex mutex;
  u64 *broadcast_buffer;
} Thread_Heat;

typedef struct Thread_Context {
  Thread_Heat heat;
} Thread_Context;

/*
TODO: I want to implement platform-independent threading here
But like the good programmer I am I will wait to see what kind of patterns emerge
from my game code before making the API.

-[X] Mutexes / Critical sections
-[X] Barriers
-[ ] Interlocked variable access
*/

// NOTE: General Threading

core_function Thread_Barrier os_barrier_create(Arena *arena, u64 num_threads);
core_function void os_barrier_wait(Thread_Barrier barrier);
core_function void os_barrier_invalidate(Thread_Barrier barrier);

core_function Thread_Mutex os_mutex_create(Arena *arena);
core_function void os_mutex_claim(Thread_Mutex mutex);
core_function void os_mutex_release(Thread_Mutex mutex);
core_function void os_mutex_invalidate(Thread_Mutex mutex);

core_function void os_set_thread_context(Thread_Context ctx);
core_function Thread_Context* os_get_thread_context(void);
#define runner_id() (os_get_thread_context()->heat.runner_id)
#define num_runners() (os_get_thread_context()->heat.num_runners)

// NOTE: Timing

function u64 os_get_perf_frequency(void);
function u64 os_query_clock(void);
function f64 os_clock_seconds(void);
function f64 os_get_elapsed_ms(u64 t1, u64 t2);
function u64 os_ms_to_tick_interval(f64 ms);

// NOTE: Heat Functions

core_function void os_heat_sync(void);
core_function void os_heat_sync_u64(u64 *value, u64 broadcaster_id);
#define os_heat_sync_ptr(p,i) os_heat_sync_u64((u64*)&(p),(i))
core_function void os_heat_begin_critical_section(void);
core_function void os_heat_end_critical_section(void);
core_function Rangei os_heat_distribute(u64 num_items);

#endif // OS_THREAD_H