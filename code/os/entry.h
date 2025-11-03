#ifndef OS_ENTRY_H
#define OS_ENTRY_H

/*
NOTE: Entry options

We should be able to control the number of threads created

*/

typedef struct Thread_Heat {
  u64 runner_id;
  u64 num_runners;
} Thread_Heat;

typedef struct Thread_Context {
  Thread_Heat heat;
} Thread_Context;

typedef struct Entry_Params {
  int cmd_line;
  int thread_context;
} Entry_Params;

void entry(Entry_Params params);

#endif // OS_ENTRY_H