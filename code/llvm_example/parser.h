// @note: Lexer for the "Kaleidoscope" language spec by the LLVM council

typedef u32 Token_Kind;
enum {
  TOKEN_KIND_NULL,
  TOKEN_KIND_DEFINITION,
  TOKEN_KIND_EXTERN,
  TOKEN_KIND_IDENTIFIER,
  TOKEN_KIND_NUM_LITERAL,
  TOKEN_KIND_EOF,

  TOKEN_KIND_COUNT,
};

struct Token_Range {
  u64 first;
  u64 opl;
};

struct Token {
  Token_Kind type;
  Token_Range loc;
};

struct Token_Node {
  Token_Node *next;
  Token token;
};

struct Token_List {
  u64 num_tokens;
  Token_Node *first;
  Token_Node *last;
};

function String8 str8_from_token_kind(Token_Kind type);
function String8 str8_sub_from_token_range(String8 string, Token_Range range);

function void token_list_push_node(Token_List *list, Token_Node *node);
function void token_list_push(Arena *arena, Token_List *list, Token token);

function Token_List lex(Arena *arena, String8 input);