// @note: Unity build

#include <stdio.h>

#define ENABLE_ASSERT 1

// Headers
#include "base/include.h"

// Source
#include "base/include.c"

function void
test_arena (void) {
    printf("sizeof(Arena): %llu\n", sizeof(Arena));
    
    // Test arena allocation and release
    Arena *arena = arena_alloc();
    assert(arena != NULL);
    printf("Arena allocated successfully.\n");
    
    // Test arena push and alignment
    int *int_ptr = (int *)arena_push(arena, sizeof(int), align_of(int));
    assert(int_ptr != NULL);
    *int_ptr = 42;
    assert(*int_ptr == 42);
    printf("Pushed an integer successfully.\n");
    
    // Test arena pushn
    int *int_array = arena_pushn(arena, int, 10);
    assert(int_array != NULL);
    for (int i = 0; i < 10; ++i) {
        int_array[i] = i;
    }
    for (int i = 0; i < 10; ++i) {
        assert(int_array[i] == i);
    }
    printf("Pushed an array of integers successfully.\n");
    
    // Test arena position
    u64 pos_before = arena_pos(arena);
    printf("Arena position before additional push: %llu\n", pos_before);
    
    // Test arena pop
    arena_pop(arena, sizeof(int) * 10);
    u64 pos_after_pop = arena_pos(arena);
    assert(pos_after_pop == pos_before - sizeof(int) * 10);
    printf("Arena pop successful. Position after pop: %llu\n", pos_after_pop);
    
    // Test arena clear
    arena_clear(arena);
    u64 pos_after_clear = arena_pos(arena);
    assert(pos_after_clear == sizeof(Arena));
    printf("Arena clear successful. Position after clear: %llu\n", pos_after_clear);
    
    // Test temporary arena
    Temp_Arena temp = temp_arena(arena);
    int *temp_int_ptr = (int *)arena_push(temp.arena, sizeof(int), align_of(int));
    assert(temp_int_ptr != NULL);
    *temp_int_ptr = 99;
    assert(*temp_int_ptr == 99);
    printf("Temporary arena push successful.\n");
    
    // End temporary arena
    temp_arena_end(temp);
    u64 pos_after_temp = arena_pos(arena);
    assert(pos_after_temp == sizeof(Arena)); // Since temp_arena_end should restore position
    printf("Temporary arena end successful. Position after temp_arena_end: %llu\n", pos_after_temp);
    
    // Test scratch arena
    Arena *conflicts[] = {arena};
    Temp_Arena scratch = get_scratch(conflicts, 1);
    int *scratch_int_ptr = (int *)arena_push(scratch.arena, sizeof(int), align_of(int));
    assert(scratch_int_ptr != NULL);
    *scratch_int_ptr = 77;
    assert(*scratch_int_ptr == 77);
    printf("Scratch arena push successful.\n");
    
    // End scratch arena
    release_scratch(scratch);
    u64 pos_after_scratch = arena_pos(arena);
    assert(pos_after_scratch == sizeof(Arena));
    printf("Scratch arena end successful. Position after release_scratch: %llu\n", pos_after_scratch);
    
    // Release the main arena
    arena_release(arena);
    printf("Arena released successfully.\n");
    printf("All arena tests passed\n");
}

function void
test_strings (Arena *arena) {
    // Test string constructors
    char *test_cstr = "Hello, world!";
    String8 str = str8_cstring(test_cstr);
    assert(str.len == strlen(test_cstr));
    assert(memcmp(str.str, test_cstr, str.len) == 0);
    printf("String constructor (cstring) test passed.\n");
    
    // Test str8_lit
    String8 lit_str = str8_lit("Literal");
    assert(lit_str.len == 7);
    assert(memcmp(lit_str.str, "Literal", 7) == 0);
    printf("String literal constructor test passed.\n");
    
    // Test str8_sub
    String8 sub_str = str8_sub(str, 0, 5);
    assert(sub_str.len == 5);
    assert(memcmp(sub_str.str, "Hello", 5) == 0);
    printf("Substring test passed.\n");
    
    // Test str8_push_copy
    String8 copied_str = str8_push_copy(arena, str);
    assert(copied_str.len == str.len);
    assert(memcmp(copied_str.str, str.str, str.len) == 0);
    printf("String copy test passed.\n");
    
    // Test str8_match
    String8 str1 = str8_cstring("Test");
    String8 str2 = str8_cstring("test");
    assert(str8_match(str1, str2, MATCH_CASE_INSENSITIVE));
    assert(!str8_match(str1, str2, 0));
    printf("String match test passed.\n");
    
    // Test str8_list_push and str8_list_to_array
    String8List list = {0};
    str8_list_push(arena, &list, str1);
    str8_list_push(arena, &list, str2);
    assert(list.num_nodes == 2);
    String8Array array = str8_list_to_array(arena, &list);
    assert(array.count == 2);
    assert(memcmp(array.strings[0].str, "Test", 4) == 0);
    assert(memcmp(array.strings[1].str, "test", 4) == 0);
    printf("String list and array conversion test passed.\n");
    
    // Test str8_split
    String8 split_str = str8_cstring("one,two,three");
    String8List split_list = str8_split(arena, split_str, 1, ",");
    assert(split_list.num_nodes == 3);
    String8Array split_array = str8_list_to_array(arena, &split_list);
    assert(memcmp(split_array.strings[0].str, "one", 3) == 0);
    assert(memcmp(split_array.strings[1].str, "two", 3) == 0);
    assert(memcmp(split_array.strings[2].str, "three", 5) == 0);
    printf("String split test passed.\n");
    
    // Test str8_list_join
    String8Join join_params = {str8_lit("["), str8_lit(", "), str8_lit("]")};
    String8 joined_str = str8_list_join(arena, split_list, &join_params);
    assert(memcmp(joined_str.str, "[one, two, three]", 17) == 0);
    printf("String join test passed.\n");
    
    // Test char functions
    assert(char_is_alpha('a'));
    assert(char_is_digit('1'));
    assert(char_to_upper('a') == 'A');
    assert(char_to_lower('A') == 'a');
    printf("Character functions test passed.\n");
    
    printf("All strings tests passed\n");
}

int
main (void) {
    Arena *arena = arena_alloc();
    
    print_context();
    
    // Run tests
    test_arena();
    test_strings(arena);
    return 0;
}