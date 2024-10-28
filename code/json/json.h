#ifndef JSON_H
#define JSON_H

//- Lexical Analysis
typedef u32 Json_Token;
enum {
    JSON_TOKEN_NULL,
    
    JSON_TOKEN_COUNT
};

//- Parsed Data

typedef u32 Json_Type;
enum {
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_NUMBER,
    
    JSON_TYPE_COUNT
};

typedef struct Json_Object Json_Object;
typedef struct Json_Array  Json_Array;
typedef union  Json_Value  Json_Value;

typedef struct Json_Set {
    String8 key;
    Json_Value value;
} Json_Set;

struct Json_Object {
    // Open-addressed hash table based on "key-value" pairs
    Json_Type type;
    Json_Set *table;
    u64 count;
};

struct Json_Array {
    Json_Type type;
    Json_Value *values;
    u64 count;
};

union Json_Value {
    Json_Type type;
    Json_Object object;
    Json_Array array;
    struct {
        Json_Type __pad1;
        String8 string;
    }
    struct {
        Json_Type __pad2;
        u64 number;
    }
};

core_function Json_Value* json_parse(Arena *arena, String8 json);

#endif //JSON_H
