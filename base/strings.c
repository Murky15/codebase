// @note: Char functions
// Helpful resource: https://www.cs.cmu.edu/~pattis/15-1XX/common/handouts/ascii.html

link b32
char_is_alpha (u8 c) {
  return char_is_alpha_upper(c) || char_is_alpha_lower(c);
}

link b32
char_is_alpha_upper (u8 c) {
  return (c >= 65 && c <= 90);
}

link b32
char_is_alpha_lower (u8 c) {
  return (c >= 97 && c <= 122);
}

link b32
char_is_digit (u8 c) {
  return (c >= 48 && c <= 57);
}

link b32
char_is_symbol (u8 c) {
  return (c >= 33 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 96) || (c >= 123 && c < 127);
}

link b32
char_is_control (u8 c) {
  return (c <= 31);
}

link b32
char_is_space (u8 c) {
  return (c == 32) || (c == 127);
}

link u8
char_to_upper (u8 c) {
  return char_is_alpha_lower(c) ? c - 32 : c;
}

link u8
char_to_lower (u8 c) {
  return char_is_alpha_upper(c) ? c + 32 : c;
}

link u8
char_to_forward_slash (u8 c) {
  return c == '\\' ? '/' : c;
}

link u64
cstr_length (char *cstr) {
  char *c;
  for (c = cstr; *c; ++c);
  return c - cstr;
}

// @note: String functions

// Constructors
link String8
str8 (u8 *str, u64 len) {
  return (String8){str, len};
}

link String8
str8_range (u8 *first, u8 *opl) {
  return (String8){first, (u64)(opl-first)};
}

link String16
str16 (u16 *str, u64 len) {
  return (String16){str, len};
}

link String32
str32 (u32 *str, u64 len) {
  return (String32){str, len};
}

// Substrings
link String8
str8_sub (String8 string, u64 first, u64 opl) {
  String8 result = zero_struct;
  first = min(string.len, first);
  opl = min(string.len, opl);
  if (first > opl) swap(u64, first, opl);
  result.str = string.str + first;
  result.len = opl - first;

  return result;
}

link String8
str8_skip (String8 string, u64 amount) {
  amount = min(amount, string.size);
  return (String8){string.str + amount, string.len - amount};
}

link String8
str8_chop (String8 string, u64 amount) {
  amount = min(amount, string.size);
  return (String8){string.str, string.len - amount};
}

link String8
str8_prefix (String8 string, u64 size) {
  size = min(size, string.len);
  return (String8){string.str, size};
}

link String8
str8_postfix (String8 string, u64 size) {
  size = min(size, string.len);
  u64 skip = string.len - size;
  return (String8){string.str + skip, size};
}

// Match
link b32
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

// Allocation
link String8
str8_push_copy (Arena *arena, String8 string) {
  u64 len = string.len;
  u8 *str = arena_pushn(arena, u8, len + 1);
  memory_copy(str, string.str, len);
  return (String8){str, len};
}

link String8
str8_pushfv (Arena *arena, char *fmt, va_list args) {

}

link String8
str8_pushf (Arena *arena, char *fmt, ...) {
  String8 result;
  va_list args;
  va_start(args, fmt);
  result = str8_pushfv(arena, fmt, args);
  va_end(args);

  return result;
}


// String lists
link void
str8_list_push_node (String8List *list, String8Node *node) {

}

link void
str8_list_push_node_front (String8List *list, String8Node *node) {

}

link void
str8_list_push (Arena *arena, String8List *list, String8 string) {

}

link void
str8_list_push_front (Arena *arena, String8List *list, String8 string) {

}

link void
str8_list_pushf (Arena *arena, String8List *list, char *fmt, ...) {

}

link void
str8_list_concat (String8List *base, String8List *to_append) {

}

link String8List
str8_split_by_strings (Arena *arena, String8 string, u64 num_splits, String8 *splits) {

}

link String8List
str8_split_by_chars (Arena *arena, String8 string, u64 num_splits, char *splits) {

}

link String8
str8_list_join (Arena *arena, String8List list, String8Join *opt_join_params) {

}

// Conversions
link String8Array
str8_list_to_array (String8List *list) {

}