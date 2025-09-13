core_function String8
os_read_file (Arena *arena, String8 path, b32 create_if_not_exist) {
    Temp_Arena scratch = get_scratch(&arena,1);
    String8 result = zero_struct;

    const char *path_str = (char*)str8_to_cstr(scratch.arena, path);
    HANDLE hFile = CreateFile(path_str,
                              GENERIC_READ,
                              FILE_SHARE_READ,
                              0,
                              (create_if_not_exist > 0) ? OPEN_ALWAYS : OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                              0);
    if (hFile && hFile != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER sz;
        //GetFileSizeEx(hFile, &sz);
        sz.LowPart = GetFileSize(hFile, (LPDWORD)&sz.HighPart); // Because TCC complains
        u64 file_size = sz.QuadPart;
        u8 *buffer = arena_pushn(arena, u8, file_size+1);
        buffer[file_size] = '\0';
        u64 bytes_read = 0;
        if (file_size > u32_max) {
            u32 diff = (u32)(file_size - u32_max);
            DWORD first_read, second_read;
            ReadFile(hFile, buffer, u32_max, &first_read, 0);
            ReadFile(hFile, buffer, diff, &second_read, 0);
            bytes_read = first_read + second_read;
        } else {
            ReadFile(hFile, buffer, (u32)file_size, (LPDWORD)&bytes_read, 0);
        }

        if (bytes_read == file_size) {
            result = comp_lit(String8, buffer, file_size);
        }

        CloseHandle(hFile);
    }

    release_scratch(scratch);
    return result;
}

core_function b32
os_write_file (String8 path, String8 to_write, b32 create_if_not_exist) {
    Temp_Arena scratch = get_scratch(0,0);

    b32 success = false;
    const char *path_str = (char*)str8_to_cstr(scratch.arena, path);
    HANDLE hFile = CreateFile(path_str,
                              GENERIC_WRITE,
                              0,
                              0,
                              (create_if_not_exist > 0) ? OPEN_ALWAYS : OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              0);
    if (hFile) {
        DWORD bytes_written;
        BOOL result = WriteFile(hFile, to_write.str, (u32)to_write.len, &bytes_written, 0);
        if (result == TRUE && bytes_written == to_write.len)
            success = true;

        CloseHandle(hFile);
    }

    release_scratch(scratch);
    return success;
}

core_function void
os_push_directory_search_result (Arena *arena, Directory_Search_Results *results, Directory_Search_Result result) {
    Directory_Search_Result_Node *node = arena_pushn(arena, Directory_Search_Result_Node, 1);
    node->result = result;
    sll_queue_push(results->first, results->last, node);
    results->count++;
}

core_function Directory_Search_Results
os_search_directory_and_read_files (Arena *arena, String8 path, String8 query) {
    Temp_Arena scratch = get_scratch(&arena, 1);

    Directory_Search_Results results = zero_struct;

    String8List query_str_list = zero_struct;
    str8_list_push(scratch.arena, &query_str_list, path);
    str8_list_push(scratch.arena, &query_str_list, query);
    String8Join join_opts = comp_lit(String8Join, .sep=str8_lit("/"));
    String8 query_str = str8_list_join(scratch.arena, query_str_list, &join_opts);

    WIN32_FIND_DATA data;
    HANDLE cursor = FindFirstFile(query_str.str, &data);
    if (cursor != INVALID_HANDLE_VALUE) {
        do {
            String8 file_name = str8_push_copy(arena, str8_cstring(data.cFileName));

            String8List file_path_list = zero_struct;
            str8_list_push(scratch.arena, &file_path_list, path);
            str8_list_push(scratch.arena, &file_path_list, file_name);
            String8Join join_opts = comp_lit(String8Join, .sep=str8_lit("/"));
            String8 file_path = str8_list_join(scratch.arena, file_path_list, &join_opts);

            // Pass dir and query as seperate parameters for easy join
            String8 file_data = os_read_file(arena, file_path, false);
            Directory_Search_Result result = comp_lit(Directory_Search_Result, file_name, file_data);
            os_push_directory_search_result(arena, &results, result);
        } while (FindNextFile(cursor, &data));
        FindClose(cursor);
    }

    release_scratch(scratch);
    return results;
}