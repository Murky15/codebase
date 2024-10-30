#ifndef JSON_H
#define JSON_H

//- Lexical Analysis
typedef u32 Json_Token_Type;
enum {
    JSON_TOKEN_NULL,
    JSON_TOKEN_PUNCTUATOR,
    JSON_TOKEN_STRING,
    JSON_TOKEN_NUMBER,
    JSON_TOKEN_KEYWORD,
    
    JSON_TOKEN_COUNT
};

typedef struct Json_Token {
    Json_Token_Type type;
    String8 value;
} Json_Token;

typedef struct Json_Token_Node {
    struct Json_Token_Node *next;
    Json_Token token;
} Json_Token_Node;

typedef struct Json_Token_List {
    Json_Token_Node *first, *last;
    u64 count;
} Json_Token_List;

//- Parsed Data
typedef u32 Json_Type;
enum {
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_NUMBER,
    JSON_REAL_NUMBER,
    
    JSON_TYPE_COUNT
};

typedef struct Json_Object Json_Object;
typedef struct Json_Array  Json_Array;
typedef union  Json_Value  Json_Value;
typedef struct Json_Set    Json_Set;

struct Json_Object {
    // Open-addressed hash table based on "key-value" pairs
    // total_slots = count + count/2 just in case the user wants to append values to the object
    // Json_Set(s) should only be returned by value just in case we need to do a full rehash 
    // Maybe we should be chaining instead, who knows.
    Json_Type type;
    Json_Set *table;
    u64 count;
    u64 total_slots;
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
    };
    struct {
        Json_Type __pad2;
        u64 number;
    };
    struct {
        Json_Type __pad3;
        f64 real_number;
    };
};

struct Json_Set {
    String8 key;
    Json_Value value;
};

//- Lexing functions
core_function void json_token_list_push(Arena *arena, Json_Token_List *list, Json_Token token);
core_function Json_Token_List json_lex(Arena *arena, String8 json);
core_function void json_dump_lex(Json_Token_List *tokens, String8 json);

//- Parsing functions
core_function Json_Value* json_parse(Arena *arena, String8 json);

#endif //JSON_H
