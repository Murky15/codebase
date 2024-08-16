#ifndef BASE_STRINGS_H
#define BASE_STRINGS_H

// @note: Basic string types

typedef struct String8 {
  u8* str;
  u64 len;
} String8;

typedef struct String16 {
  u16* str;
  u64 len;
} String16;

typedef struct String32 {
  u32* str;
  u64 len;
} String32;

// @note: String structure types

typedef struct String8Node {
  struct String8Node *next;
  String8 string;
} String8Node;

typedef struct String8List {
  String8Node *first;
  String8Node *last;
  u64 num_nodes;
  u64 total_len;
} String8List;

typedef struct String8Array {
  u64 count;
  String8 *strings;
} String8Array;

// @note: String operation types

typedef struct String8Join {
  String8 pre, sep, post;
} String8Join;

typedef struct Decoded_Codepoint {
  u32 codepoint;
  u32 advance;
} Decoded_Codepoint;

typedef u32 String8_Matchflags;
enum {
  MATCH_CASE_INSENSITIVE = (1 << 0),
  MATCH_RIGHT_SIDE_SLOPPY = (1 << 1),
  MATCH_SLASH_INSENSITIVE = (1 << 2),
};

// @note: Char functions

core_function b32 char_is_alpha(u8 c);
core_function b32 char_is_alpha_upper(u8 c);
core_function b32 char_is_alpha_lower(u8 c);
core_function b32 char_is_digit(u8 c);
core_function b32 char_is_alpha_numeric(u8 c);
core_function b32 char_is_symbol(u8 c);
core_function b32 char_is_control(u8 c);
core_function b32 char_is_space(u8 c);
core_function u8  char_to_upper(u8 c);
core_function u8  char_to_lower(u8 c);
core_function u8  char_to_forward_slash(u8 c);
core_function u64 cstr_length(char *cstr);

// @note: String functions

// Constructors
core_function String8 str8(u8 *str, u64 len);
#define str8_zero() str8(0, 0)
#define str8_cstring(cstr) str8((u8*)cstr, cstr_length(cstr))
#define str8_lit(s) str8((u8*)s, sizeof(s)-1)
core_function String8 str8_range(u8 *first, u8 *opl);
core_function String16 str16(u16 *str, u64 len);
core_function String32 str32(u32 *str, u64 len);

//@note: Use %.*s in format string
#define str8_expand(s) (int)((s).len), (char*)((s).str)

// Substrings
core_function String8 str8_sub(String8 string, u64 first, u64 opl);
core_function String8 str8_skip(String8 string, u64 amount);
core_function String8 str8_chop(String8 string, u64 amount);
core_function String8 str8_prefix(String8 string, u64 size);
core_function String8 str8_postfix(String8 string, u64 size);

// Match
core_function b32 str8_match(String8 a, String8 b, String8_Matchflags flags);
core_function u64 str8_find(String8 haystack, String8 needle, u64 start_pos, String8_Matchflags flags);

// Allocation
core_function String8 str8_push_copy(Arena *arena, String8 string);
core_function String8 str8_pushfv(Arena *arena, char *fmt, va_list args);
core_function String8 str8_pushf(Arena *arena, char *fmt, ...);

// String lists
core_function void str8_list_push_node(String8List *list, String8Node *node);
core_function void str8_list_push_node_front(String8List *list, String8Node *node);
core_function void str8_list_push(Arena *arena, String8List *list, String8 string);
core_function void str8_list_push_front(Arena *arena, String8List *list, String8 string);
core_function void str8_list_pushf(Arena *arena, String8List *list, char *fmt, ...);
core_function void str8_list_concat(String8List *base, String8List *appending);
core_function String8List str8_split(Arena *arena, String8 string, u64 num_splitters, char *splits);
core_function String8 str8_list_join(Arena *arena, String8List list, String8Join *opt_join_params);

// Conversions
core_function String8Array str8_list_to_array(Arena *arena, String8List *list);

// @todo: Unicode conversions

#endif // BASE_STRINGS_H