// Unity build

// @note: Headers
#include "base/include.h"
#include "lexer.h"

// @note: Source
#include "base/include.c"
#include "lexer.cpp"

char *program = R"(
def fib(x)
  if x < 3 then
    1
  else
    fib(x-1)+fib(x-2)

fib(40)
)";

int
main (int argc, char **argv) {
  Arena *arena = arena_alloc();
  String8 program_string = str8_cstring(program);
  Token_List tokens = lex(arena, program_string);
  printf("Lex report:\n\n");
  for (Token_Node *node = tokens.first; node != 0; node = node->next) {
    Token *token = &node->token;
    String8 type = str8_from_token_kind(token->type);
    String8 val  = str8_sub_from_token_range(program_string, token->loc);
    printf("Type: %.*s, value: %.*s\n", str8_expand(type), str8_expand(val));
  }
}