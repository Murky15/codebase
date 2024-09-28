#include <windows.h>

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
    if (hFile) {
        LARGE_INTEGER sz;
        GetFileSizeEx(hFile, &sz);
        u64 file_size = sz.QuadPart;
        u8 *buffer = arena_pushn(arena, u8, file_size+1);
        buffer[file_size] = '\0';
        u64 bytes_read;
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