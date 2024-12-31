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
};

// @todo: Add line + column info for better error messages
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
    JSON_OBJECT = 1,
    JSON_ARRAY,
    JSON_STRING,
    JSON_NUMBER,
    JSON_KEYWORD,
};

typedef u32 Json_Keyword;
enum {
    JSON_KEYWORD_TRUE  = 1,
    JSON_KEYWORD_FALSE = 0,
    JSON_KEYWORD_NULL  = 0,
};

typedef struct Json_Object     Json_Object;
typedef struct Json_Array      Json_Array;
typedef union  Json_Value      Json_Value;
typedef struct Json_Value_Node Json_Value_Node;
typedef struct Json_Value_List Json_Value_List;
typedef struct Json_Set        Json_Set;

struct Json_Object {
    Json_Set *table;
    u64 count;
    u64 total_slots;
};

struct Json_Value_List {
    Json_Value_Node *first, *last;
    u64 count;
};

struct Json_Array {
    Json_Value_List values;
};

struct Json_Value {
    Json_Type type;
    union {
        Json_Object object;
        Json_Array array;
        String8 string;
        f64 number;
        Json_Keyword keyword;
    };
};

struct Json_Value_Node {
    Json_Value_Node *next;
    Json_Value value;
};

struct Json_Set {
    String8 key;
    Json_Value value;
};

//- Lexing functions
core_function void json_token_list_push(Arena *arena, Json_Token_List *list, Json_Token token);
core_function Json_Token_List json_lex(Arena *arena, String8 json);
core_function void json_dump_lex(Json_Token_List *tokens, String8 json);

//- @incomplete Object/Array manipulation
core_function Json_Value json_object_fetch(Json_Object *object, String8 key);
#define json_fetch_num(j,T,str) (T)json_object_fetch(j,str).number
#define json_fetch_str(j,str) json_object_fetch(j,str).string
#define json_fetch_obj(j,str) json_object_fetch(j,str).object
#define json_fetch_arr(j,str) json_object_fetch(j,str).array
#define json_fetch_keyword(j,str) json_object_fetch(j,str).keyword

core_function void json_value_list_push(Arena *arena, Json_Value_List *list, Json_Value value);

//- @incomplete Serialization functions & better program-side data mutability
core_function void json_print(Json_Value value);

//- Parsing functions
// @note: Trailing commas are *not supported*
// @todo: This section needs a lot of work. Instead of returning the value directly, it should return
// a b32 indicating success or failure so we don't spiral out of control with a chain of errors.
core_function Json_Object json_process_object(Arena *arena, Json_Token_Node **token);
core_function Json_Array  json_process_array(Arena *arena, Json_Token_Node **token);
core_function Json_Value  json_process_token(Arena *arena, Json_Token_Node **token_stream);
core_function Json_Value  json_parse(Arena *arena, String8 json);

#endif //JSON_H
