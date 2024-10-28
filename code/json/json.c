#include <stdio.h>

#include "../base/include.h"
#include "../os/include.h"
#include "../base/include.c"
#include "../os/include.c"
#include "json.h"

core_function void
json_token_list_push (Arena *arena, Json_Token_List *list, Json_Token token) {
    Json_Token_Node *node = arena_pushn(arena, Json_Token_Node, 1);
    node->token = token;
    sll_queue_push(list->first, list->last, node);
    list->count++;
}

core_function Json_Value* 
json_parse (Arena *arena, String8 json) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    
    //- Lexer
    // @todo: This lexer is evaluationless, merely including tokens which fit the regex. MUST FIX!!!!
    Json_Token_List tokens = {0};
    
    Json_Token_Type active_token_type = JSON_TOKEN_NULL;
    u64 word_idx = 0;
    for (u64 i = 0, j = 1; i < json.len; ++i, ++j) {
        u8 c = json.str[i];
        u8 nc = j < json.len ? json.str[j] : 0;
        switch (active_token_type) {
            case JSON_TOKEN_NULL: {
                if (char_is_alpha(c)) {
                    active_token_type = JSON_TOKEN_KEYWORD;
                    word_idx = i;
                    continue;
                } else if (char_is_symbol(c)) {
                    if (c == 34) {
                        active_token_type = JSON_TOKEN_STRING;
                        word_idx = i + 1;
                        continue;
                    } else if (c == '{' || c == '}' 
                               || c == '[' || c == ']' 
                               || c == ','
                               || c == ':') {
                        Json_Token new_token = {JSON_TOKEN_PUNCTUATOR, v2i(i, j)};
                        json_token_list_push(scratch.arena, &tokens, new_token);
                        continue;
                    } else {
                        // @todo: Atrocious error handling
                        fprintf(stderr, "Json parse failed! Unrecognized Symbol.");
                        return 0;
                    }
                } else if (char_is_digit(c) || c == '-') {
                    active_token_type = JSON_TOKEN_NUMBER;
                    word_idx = i;
                    continue;
                } else if (char_is_space(c) || char_is_control(c)) {
                    continue;
                }
            } break;
            
            case JSON_TOKEN_STRING: {
                if (nc != 32) {
                    continue;
                } else { 
                    // @todo: Handle escaped-string processing
                    goto make_new_token;
                }
            } break;
            
            case JSON_TOKEN_NUMBER: {
                if (char_is_digit(nc) 
                    || nc == 'E' || nc == 'e' 
                    || nc == '.' || nc == '+'
                    || nc == '-') {
                    continue;
                } else {
                    goto make_new_token;
                }
            } break;
            
            case JSON_TOKEN_KEYWORD: {
                if (char_is_alpha(nc)) {
                    continue;
                } else {
                    String8 keyword = str8_sub(json, word_idx, j);
                    if (str8_match(keyword, str8_lit("true"),0) ||
                        str8_match(keyword, str8_lit("false"),0) ||
                        str8_match(keyword, str8_lit("null"),0)) {
                        goto make_new_token;
                    } else {
                        fprintf(stderr, "Json parse failed! Unrecognized Keyword: %.*s", str8_expand(keyword));
                        return 0;
                    }
                }
            } break;
        }
        
        make_new_token:
        if (active_token_type != JSON_TOKEN_NULL) {
            Json_Token new_token = {active_token_type, v2i(word_idx, j)};
            json_token_list_push(scratch.arena, &tokens, new_token);
            active_token_type = JSON_TOKEN_NULL;
        }
    }
    
    //- Parsing
    
    release_scratch(scratch);
}

// Quick test
int main (void) {
    // Read "test.json"
    Temp_Arena scratch = get_scratch(0,0);
    String8 file = os_read_file(scratch.arena, str8_lit("test.json"), false);
    Json_Value *value = json_parse(scratch.arena, file);
    
    release_scratch(scratch);
}