#ifndef OS_ENTRY_H
#define OS_ENTRY_H

/*
NOTE: Entry options
Define: OS_NO_ENTRY to avoid bootstrapping the entry point entirely
Define: OS_NUM_THREADS to control how many threads the bootstrap entry launches for execution
*/

typedef struct Entry_Params {
  Thread_Context tctx;
} Entry_Params;

s32 entry(void *params);

#endif // OS_ENTRY_H