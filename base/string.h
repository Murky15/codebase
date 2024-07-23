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

// @note: String structures

typedef struct String8Node {
  String8Node *next;
  String8 string;
} String8Node;

typedef struct String8List {

}

#endif // BASE_STRINGS_H