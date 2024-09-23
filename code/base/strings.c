// @note: Char functions
// Helpful resource: https://www.cs.cmu.edu/~pattis/15-1XX/common/handouts/ascii.html

core_function b32
char_is_alpha (u8 c) {
    return char_is_alpha_upper(c) || char_is_alpha_lower(c);
}

core_function b32
char_is_alpha_upper (u8 c) {
    return (c >= 65 && c <= 90);
}

core_function b32
char_is_alpha_lower (u8 c) {
    return (c >= 97 && c <= 122);
}

core_function b32
char_is_digit (u8 c) {
    return (c >= 48 && c <= 57);
}

core_function b32
char_is_alpha_numeric(u8 c) {
    return char_is_alpha(c) || char_is_digit(c);
}

core_function b32
char_is_symbol (u8 c) {
    return (c >= 33 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 96) || (c >= 123 && c < 127);
}

core_function b32
char_is_control (u8 c) {
    return (c <= 31);
}

core_function b32
char_is_space (u8 c) {
    return (c == 32) || (c == 127);
}

core_function u8
char_to_upper (u8 c) {
    return char_is_alpha_lower(c) ? c - 32 : c;
}

core_function u8
char_to_lower (u8 c) {
    return char_is_alpha_upper(c) ? c + 32 : c;
}

core_function u8
char_to_forward_slash (u8 c) {
    return c == '\\' ? '/' : c;
}

core_function u64
cstr_length (char *cstr) {
    char *c;
    for (c = cstr; *c; ++c);
    return c - cstr;
}

// @note: String functions

// Constructors
core_function String8
str8 (u8 *str, u64 len) {
    return comp_lit(String8, str, len);
}

core_function String8
str8_range (u8 *first, u8 *opl) {
    return comp_lit(String8, first, (u64)(opl-first));
}

core_function String16
str16 (u16 *str, u64 len) {
    return comp_lit(String16, str, len);
}

core_function String32
str32 (u32 *str, u64 len) {
    return comp_lit(String32, str, len);
}

// Substrings
core_function String8
str8_sub (String8 string, u64 first, u64 opl) {
    String8 result = zero_struct;
    first = min(string.len, first);
    opl = min(string.len, opl);
    if (first > opl) swap(u64, first, opl);
    result.str = string.str + first;
    result.len = opl - first;
    
    return result;
}

core_function String8
str8_skip (String8 string, u64 amount) {
    amount = min(amount, string.len);
    return comp_lit(String8, string.str + amount, string.len - amount);
}

core_function String8
str8_chop (String8 string, u64 amount) {
    amount = min(amount, string.len);
    return comp_lit(String8, string.str, string.len - amount);
}

core_function String8
str8_prefix (String8 string, u64 size) {
    size = min(size, string.len);
    return comp_lit(String8, string.str, size);
}

core_function String8
str8_postfix (String8 string, u64 size) {
    size = min(size, string.len);
    u64 skip = string.len - size;
    return comp_lit(String8, string.str + skip, size);
}

// Match
core_function b32
str8_match (String8 a, String8 b, String8_Matchflags flags) {
    b32 result = 0;
    if (a.len == b.len || flags & MATCH_RIGHT_SIDE_SLOPPY) {
        result = 1;
        for (int i = 0; i < a.len; ++i) {
            b32 match = a.str[i] == b.str[i];
            if (flags & MATCH_CASE_INSENSITIVE) match |= (char_to_lower(a.str[i]) == char_to_lower(b.str[i]));
            if (flags & MATCH_SLASH_INSENSITIVE) match |= (char_to_forward_slash(a.str[i]) == char_to_forward_slash(b.str[i]));
            if (match == 0) {
                result = 0;
                break;
            }
        }
    }
    
    return result;
}

core_function u64
str8_find (String8 haystack, String8 needle, u64 start_pos, String8_Matchflags flags) {
    b32 found = 0;
    u64 found_pos = 0;
    for (u64 i = start_pos; i < haystack.len; ++i) {
        if (needle.len <= haystack.len - i) {
            found = str8_match(str8_sub(haystack, i, i + needle.len), needle, flags);
            if (found) {
                found_pos = i;
                break;
            }
        }
    }
    
    return found_pos;
}

// Allocation
core_function String8
str8_push_copy (Arena *arena, String8 string) {
    u64 len = string.len;
    u8 *str = arena_pushn(arena, u8, len + 1);
    memory_copy(str, string.str, len);
    return comp_lit(String8, str, len);
}

core_function String8
str8_pushfv (Arena *arena, char *fmt, va_list args) {
    va_list backup_args;
    va_copy(backup_args, args);
    
    u64 try_size = Kilobytes(1);
    u8 *buffer = arena_pushn(arena, u8, try_size);
    u64 actual_size = vsnprintf((char*)buffer, try_size, fmt, args);
    actual_size += 1;
    if (actual_size > try_size) {
        arena_pop(arena, try_size);
        buffer = arena_pushn(arena, u8, actual_size);
        vsnprintf((char*)buffer, actual_size, fmt, backup_args);
    } else {
        arena_pop(arena, try_size - actual_size);
    }
    va_end(backup_args);
    
    return comp_lit(String8, buffer, actual_size);
}

core_function String8
str8_pushf (Arena *arena, char *fmt, ...) {
    String8 result;
    va_list args;
    va_start(args, fmt);
    result = str8_pushfv(arena, fmt, args);
    va_end(args);
    
    return result;
}

// String lists
core_function void
str8_list_push_node (String8List *list, String8Node *node) {
    sll_queue_push(list->first, list->last, node);
    list->num_nodes++;
    list->total_len += node->string.len;
}

core_function void
str8_list_push_node_front (String8List *list, String8Node *node) {
    sll_queue_push_front(list->first, list->last, node);
    list->num_nodes++;
    list->total_len += node->string.len;
}

core_function void
str8_list_push (Arena *arena, String8List *list, String8 string) {
    String8Node *node = arena_pushn(arena, String8Node, 1);
    node->string = string;
    str8_list_push_node(list, node);
}

core_function void
str8_list_push_front (Arena *arena, String8List *list, String8 string) {
    String8Node *node = arena_pushn(arena, String8Node, 1);
    node->string = string;
    str8_list_push_node_front(list, node);
}

core_function void
str8_list_pushf (Arena *arena, String8List *list, char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    String8 string = str8_pushfv(arena, fmt, args);
    va_end(args);
    
    str8_list_push(arena, list, string);
}

core_function void
str8_list_concat (String8List *base, String8List *appending) {
    if (appending->first) {
        base->num_nodes += appending->num_nodes;
        base->total_len += appending->total_len;
        if (base->last == 0) {
            *base = *appending;
        } else {
            base->last->next = appending->first;
            base->last = appending->last;
        }
    }
    memory_zero(appending, sizeof(String8List));
}

core_function String8List
str8_split (Arena *arena, String8 string, u64 num_splits, char *splits) {
    String8List result = zero_struct;
    
    u8 *ptr = string.str;
    u8 *word = ptr;
    u8 *opl = ptr + string.len;
    for (;ptr < opl; ++ptr) {
        u8 byte = *ptr;
        b32 is_split = false;
        for (u64 split = 0; split < num_splits; ++split) {
            if (byte == splits[split]) {
                is_split = true;
                break;
            }
        }
        
        if (is_split) {
            if (word < ptr) {
                str8_list_push(arena, &result, str8_range(word, ptr));
            }
            word = ptr + 1;
        }
    }
    
    if (word < ptr) {
        str8_list_push(arena, &result, str8_range(word, ptr));
    }
    
    return result;
}

core_function String8
str8_list_join (Arena *arena, String8List list, String8Join *opt_join_params) {
    String8Join join = zero_struct;
    if (opt_join_params) memory_copy(&join, opt_join_params, sizeof(String8Join));
    
    u64 size = join.pre.len + join.post.len + join.sep.len * (list.num_nodes - 1) + list.total_len;
    u8 *buffer = arena_pushn(arena, u8, size + 1);
    u8 *ptr = buffer;
    String8 result = {buffer, size};
    
    memory_copy(ptr, join.pre.str, join.pre.len);
    ptr += join.pre.len;
    for (String8Node *node = list.first; node != 0; node = node->next) {
        memory_copy(ptr, node->string.str, node->string.len);
        ptr += node->string.len;
        if (node != list.last) {
            memory_copy(ptr, join.sep.str, join.sep.len);
            ptr += join.sep.len;
        }
    }
    memory_copy(ptr, join.post.str, join.post.len);
    
    return result;
}

// Conversions
core_function u8* 
str8_to_cstr(Arena *arena, String8 string) {
    u8 *buffer = arena_pushn(arena, u8, string.len + 1);
    memory_copy(buffer, string.str, string.len);
    buffer[string.len] = '\0';
    
    return buffer;
}

core_function String8Array
str8_list_to_array (Arena *arena, String8List *list) {
    String8Array result;
    result.count = list->num_nodes;
    result.strings = arena_pushn(arena, String8, result.count);
    u64 idx = 0;
    for (String8Node *node = list->first; node != 0; node = node->next, ++idx) {
        result.strings[idx] = node->string;
    }
    
    return result;
}