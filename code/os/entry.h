#ifndef OS_ENTRY_H
#define OS_ENTRY_H

/*
NOTE: Entry options
Define: OS_NO_ENTRY to avoid bootstrapping the entry point entirely
Define: OS_NUM_THREADS to control how many threads the bootstrap entry launches for execution
*/

typedef struct Thread_Init_Package {
  Thread_Context tctx;
} Thread_Init_Package;

s32 os_bootstrap_thread(void *params);

void os_entry(void);

#endif // OS_ENTRY_H