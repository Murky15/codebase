
function String8
str8_from_token_kind (Token_Kind type) {
  String8 string;
  switch (type) {
    default: string = str8_lit("null"); break;
    case TOKEN_KIND_DEFINITION: string = str8_lit("definition"); break;
    case TOKEN_KIND_EXTERN: string = str8_lit("extern"); break;
    case TOKEN_KIND_IDENTIFIER: string = str8_lit("identifier"); break;
    case TOKEN_KIND_NUM_LITERAL: string = str8_lit("num literal"); break;
    case TOKEN_KIND_EOF: string = str8_lit("end of file"); break;
  }

  return string;
}

function String8
str8_sub_from_token_range (String8 string, Token_Range range) {
  return str8_sub(string, range.first, range.opl);
}

function void
token_list_push_node (Token_List *list, Token_Node *node) {
  sll_queue_push(list->first, list->last, node);
  list->num_tokens++;
}

function void
token_list_push (Arena *arena, Token_List *list, Token token) {
  Token_Node *token_to_add = arena_pushn(arena, Token_Node, 1);
  token_to_add->token = token;
  token_list_push_node(list, token_to_add);
}

// Yummy spaghetti code just like grandma used to make
function Token_List
lex (Arena *arena, String8 input) {
  Token_List result = {};
  Token_Kind active_token_type = TOKEN_KIND_NULL;
  u64 lexeme = 0;

  // @note: All these gotos are redundant but do they help readability?
  for (u64 pos = 0; pos < input.len; pos += 1) {
    u64 next_pos = pos + 1;
    u8 byte = input.str[pos];
    u8 next_byte = next_pos < input.len ? input.str[next_pos] : 0;

    switch (active_token_type) {
      case TOKEN_KIND_NULL: {
        if (char_is_space(byte) || char_is_control(byte)) {
          lexeme++;
          continue;
        } else if (char_is_alpha(byte)) {
          active_token_type = TOKEN_KIND_IDENTIFIER;
          if (char_is_alpha_numeric(next_byte))
            continue;
        } else if (char_is_digit(byte) || byte == '.') {
          active_token_type = TOKEN_KIND_NUM_LITERAL;
          if (char_is_digit(next_byte))
            continue;
        } else if (byte == EOF) {
          active_token_type = TOKEN_KIND_EOF;
        }
        // @todo: Handle comments
      } break;

      case TOKEN_KIND_IDENTIFIER: {
        if (char_is_alpha_numeric(next_byte))
          continue;

        String8 token_str = str8_sub(input, lexeme, next_pos);
        if (str8_match(token_str, str8_lit("def"), 0))
          active_token_type = TOKEN_KIND_DEFINITION;
        else if (str8_match(token_str, str8_lit("extern"), 0))
          active_token_type = TOKEN_KIND_EXTERN;
      } break;

      case TOKEN_KIND_NUM_LITERAL: {
        // @note: Little bug here where you can have multiple decimal points
        if (char_is_digit(next_byte) || next_byte == '.')
          continue;
      } break;
    }

    Token_Range range = {lexeme, next_pos};
    token_list_push(arena, &result, {active_token_type, range});
    lexeme = next_pos;
    active_token_type = TOKEN_KIND_NULL;
  }

  return result;
}