int
main (int argc, char **argv) {
  /*
  TODO:
  -[ ] Tune arenas
  -[ ] Create threads
  -[ ] Process command line
  */

  SYSTEM_INFO sys_info;
  GetSystemInfo(&sys_info);
  u64 page_size = sys_info.dwPageSize;
  u64 num_threads = sys_info.dwNumberOfProcessors;


  // entry(TODO);
}