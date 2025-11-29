#ifndef OS_ENTRY_H
#define OS_ENTRY_H

typedef struct Thread_Init_Package {
  Thread_Context tctx;
} Thread_Init_Package;

s32 os_bootstrap_thread(void *params);

void os_entry(void);

#endif // OS_ENTRY_H