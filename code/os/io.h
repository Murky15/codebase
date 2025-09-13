#ifndef OS_IO_H
#define OS_IO_H

/*
@todo:
-[X] Reading & writing to files synchronously
-[ ] Std-free writing to console output (error handling) & Debug printing
-[X] Directory traversal / scanning
-[ ] Message-box Assertions
*/

typedef struct Directory_Search_Result {
    String8 name, data;
} Directory_Search_Result;

typedef struct Directory_Search_Result_Node {
    struct Directory_Search_Result_Node *next;
    Directory_Search_Result result;
} Directory_Search_Result_Node;

typedef struct Directory_Search_Results {
    Directory_Search_Result_Node *first, *last;
    u64 count;
} Directory_Search_Results;

//- @note: Basic file IO
core_function String8 os_read_file(Arena *arena, String8 path, b32 create_if_not_exist);
core_function b32     os_write_file(String8 path, String8 to_write, b32 create_if_not_exist);

//- @note: Directory traversal
core_function void os_push_directory_search_result(Arena *arena, Directory_Search_Results *results, Directory_Search_Result result);
core_function Directory_Search_Results os_search_directory_and_read_files(Arena *arena, String8 path, String8 query);

#endif //OS_IO_H
