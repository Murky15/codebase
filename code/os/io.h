#ifndef OS_IO_H
#define OS_IO_H

/*
@todo:
-[X] Reading & writing to files synchronously 
-[ ] Std-free writing to console output (error handling) & Debug printing
-[ ] Directory managment
-[ ] Assertions
*/

#if !DEBUG
# define os_debug_print(string) 
#endif

typedef void* Console;

core_function String8 os_read_file(Arena *arena, String8 path, b32 create_if_not_exist);
core_function b32     os_write_file(String8 path, String8 to_write, b32 create_if_not_exist);
//core_function void    os_debug_print (String8 string);

#endif //OS_IO_H
