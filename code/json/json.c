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

core_function Json_Token_List
json_lex (Arena *arena, String8 json) {
    // @todo: This lexer is evaluationless, merely including tokens which fit the basic regex. MUST FIX!!!!
    // Also all these "continue"'s are mad annoying
    Json_Token_List tokens = zero_struct;
    
    Json_Token_Type active_token_type = JSON_TOKEN_NULL;
    u64 word_idx = 0;
    b32 token_ready = false;
    for (u64 i = 0, j = 1; i < json.len; ++i, ++j) {
        u8 c = json.str[i];
        u8 nc = j < json.len ? json.str[j] : 0;
        switch (active_token_type) {
            case JSON_TOKEN_NULL: {
                if (char_is_alpha(c)) {
                    active_token_type = JSON_TOKEN_KEYWORD;
                    word_idx = i;
                } else if (char_is_symbol(c)) {
                    if (c == 34) {
                        active_token_type = JSON_TOKEN_STRING;
                        word_idx = i + 1;
                    } else if (c == '{' || c == '}' 
                               || c == '[' || c == ']' 
                               || c == ','
                               || c == ':') {
                        Json_Token new_token = {JSON_TOKEN_PUNCTUATOR, str8_sub(json, i, j)};
                        json_token_list_push(arena, &tokens, new_token);
                    } else {
                        // @todo: Atrocious error handling
                        fprintf(stderr, "Json lex failed! Unrecognized Symbol.\n");
                        return comp_zero(Json_Token_List);
                    }
                } else if (char_is_digit(c) || c == '-') {
                    active_token_type = JSON_TOKEN_NUMBER;
                    word_idx = i;
                } else if (char_is_space(c) || char_is_control(c)) {
                    continue;
                }
            } break;
            
            case JSON_TOKEN_STRING: {
                if (c == 34) { // (")
                    // @todo: Handle escaped-string processing
                    token_ready = true;
                    goto make_new_token;
                }
            } break;
            
            case JSON_TOKEN_NUMBER: {
                b32 cond = char_is_digit(nc) || nc == 'E' || nc == 'e' || nc == '.' || nc == '+' || nc == '-';
                if (!cond) {
                    token_ready = true;
                    goto make_new_token;
                }
            } break;
            
            case JSON_TOKEN_KEYWORD: {
                if (!char_is_alpha(nc)) {
                    String8 keyword = str8_sub(json, word_idx, j);
                    if (str8_match(keyword, str8_lit("true"),0) ||
                        str8_match(keyword, str8_lit("false"),0) ||
                        str8_match(keyword, str8_lit("null"),0)) {
                        token_ready = true;
                        goto make_new_token;
                    } else {
                        fprintf(stderr, "Json lex failed! Unrecognized Keyword: %.*s\n", str8_expand(keyword));
                        return comp_zero(Json_Token_List);
                    }
                }
            } break;
        }
        
        make_new_token:
        if (active_token_type != JSON_TOKEN_NULL && token_ready) {
            u64 end_idx = active_token_type == JSON_TOKEN_STRING ? i : j;
            Json_Token new_token = {active_token_type, str8_sub(json, word_idx, end_idx)};
            json_token_list_push(arena, &tokens, new_token);
            active_token_type = JSON_TOKEN_NULL;
            token_ready = false;
        }
    }
    
    return tokens;
}

core_function void
json_dump_lex (Json_Token_List *tokens, String8 json) {
    for (Json_Token_Node *node = tokens->first; node; node = node->next) {
        Json_Token token = node->token;
        switch (token.type) {
            case JSON_TOKEN_PUNCTUATOR: printf("Punctuator: "); break;
            case JSON_TOKEN_STRING:     printf("String:     "); break;
            case JSON_TOKEN_NUMBER:     printf("Number:     "); break;
            case JSON_TOKEN_KEYWORD:    printf("Keyword:    "); break;
        }
        printf("%.*s\n", str8_expand(token.value));
    }
}

core_function Json_Set 
json_object_fetch (Json_Object *object, String8 key) {
    u64 hash = str8_hash(key) & object->total_slots;
    while (!str8_match(object->table[hash].key, key, 0)) {
        hash++;
        hash %= object->total_slots;
    }
    
    return object->table[hash];
}

core_function void
json_value_list_push (Arena *arena, Json_Value_List *list, Json_Value value) {
    Json_Value_Node *node = arena_pushn(arena, Json_Value_Node, 1);
    node->value = value;
    sll_queue_push(list->first, list->last, node);
    list->count++;
}

core_function Json_Object
json_process_object (Arena *arena, Json_Token_Node **token) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    Json_Object result = zero_struct;
    
    if ((*token)->token.value.str[0] == '{') {
        Json_Set *object_sets = 0;
        u64 num_sets = 0;
        Json_Token_Node *next = (*token);
        for (Json_Token_Node *key = (*token)->next; next->token.value.str[0] != '}'; key = next) {
            
            // Parse set
            if (key->token.type == JSON_TOKEN_STRING) {
                Json_Set *new_set = arena_pushn(scratch.arena, Json_Set, 1);
                num_sets++;
                object_sets = object_sets == 0 ? new_set : object_sets;
                new_set->key = key->token.value;
                Json_Token_Node *seperator = key->next;
                if (seperator->token.value.str[0] == ':') {
                    Json_Token_Node *value_token = seperator->next;
                    Json_Value value = json_process_token(arena, &value_token);
                    new_set->value = value;
                    if (value_token->token.value.str[0] == ',') {
                        next = value_token->next;
                    } else if (value_token->token.value.str[0] == '}') {
                        next = value_token;
                    } else {
                        fprintf(stderr, "Json parse error: Unexpected end of object found!\n");
                        goto end;
                    }
                } else {
                    fprintf(stderr, "Json parse error: Invalid separator found in object!\n");
                    goto end;
                }
            } else {
                fprintf(stderr, "Json parse error: Invalid key in object!\n");
                goto end;
            }
        }
        
        *token = next;
        result.type  = JSON_OBJECT;
        result.count = num_sets;
        result.total_slots = num_sets; // num_sets * 1.5f;
        // @todo: Maybe we should allocate more slots in case the user wants to mutate the data,
        // but in most cases the user just reads it. IDK I have to profile this
        result.table = arena_pushn(arena, Json_Set, result.total_slots);
        for (u64 i = 0; i < result.count; ++i) {
            Json_Set new_set = object_sets[i];
            u64 hash = str8_hash(new_set.key) % result.total_slots;
            while (result.table[hash].key.str != 0) {
                hash += 1;
                hash %= result.total_slots;
            }
            result.table[hash] = new_set;
        }
        
    } else {
        fprintf(stderr, "Json parse error: Unrecognized token!\n");
    }
    
    end:
    release_scratch(scratch);
    return result;
}

core_function Json_Array
json_process_array (Arena *arena, Json_Token_Node **token) {
    Json_Array result = zero_struct;
    result.type = JSON_ARRAY;
    if ((*token)->token.value.str[0] == '[') {
        for () {
            Json_Token_Node *element = (*token)->next;
            Json_Value value_to_add = json_process_token(arena, );
        }
    } else {
        fprintf(stderr, "Json parse error: Unrecognized token!\n");
    }
    
    return result;
}

core_function Json_Value
json_process_token (Arena *arena, Json_Token_Node **token_stream) {
    Json_Value value = zero_struct;
    Json_Token token = (*token_stream)->token;
    switch (token.type) {
        case JSON_TOKEN_STRING: {
            value.type = JSON_STRING;
            value.string = token.value;
        } break;
        
        case JSON_TOKEN_NUMBER: { 
            value.type = JSON_NUMBER;
            value.number = f64_from_str8(token.value);
        } break;
        
        case JSON_TOKEN_KEYWORD: {
            value.type = JSON_KEYWORD;
            // @todo: We already make this comparison in the tokenizer, why are we doing this twice.
            if (token.value.str[0] == 't') {
                value.keyword = JSON_KEYWORD_TRUE;
            } else if (token.value.str[0] == 'f') {
                value.keyword = JSON_KEYWORD_FALSE;
            } else if (token.value.str[0] == 'n') {
                value.keyword = JSON_KEYWORD_NULL;
            }
        } break;
        
        case JSON_TOKEN_PUNCTUATOR: {
            if (token.value.str[0] == '{') {
                value = (Json_Value)json_process_object(arena, token_stream);
            } else if (token.value.str[0] == '[') {
                value = (Json_Value)json_process_array(arena, token_stream);
            } else {
                fprintf(stderr, "Json parse error: Unexpected symbol: %.*s\n", str8_expand(token.value));
            }
        } break;
        
        default: fprintf(stderr, "Json parse error: Invalid token found!\n"); break;
    }
    
    *token_stream = (*token_stream)->next;
    return value;
}

core_function void
json_print (Json_Value value) {
    local_persist int depth;
    depth++;
    
    switch (value.type) {
        case JSON_OBJECT: {
            printf("Object:\n");
            Json_Object *object = (Json_Object*)&value.object;
            for (u64 i = 0; i < object->count; ++i) {
                Json_Set set = object->table[i];
                for (int j = 0; j < depth; ++j) {
                    printf("\t");
                }
                printf("%.*s: ", str8_expand(set.key));
                json_print(set.value);
            }
        } break;
        
        case JSON_ARRAY: {
            
        } break;
        
        case JSON_STRING: {
            printf("\"%.*s\"\n", str8_expand(value.string));
        } break;
        
        case JSON_NUMBER: {
            printf("%f\n", value.number);
        } break;
        
        case JSON_KEYWORD: {
            printf("%d\n", value.keyword);
        } break;
    }
    depth--;
}

core_function Json_Value
json_parse (Arena *arena, String8 json) {
    Json_Value result = zero_struct;
    Temp_Arena scratch = get_scratch(&arena, 1);
    
    Json_Token_List tokens = json_lex(scratch.arena, json);
    Json_Token_Node *token_stream = tokens.first;
    if (tokens.first != 0) {
        result = json_process_token(arena, &token_stream);
    }
    
    release_scratch(scratch);
    return result;
}

// Quick test
int main (void) {
    // Read "test.json"
    Temp_Arena scratch = get_scratch(0,0);
    
    String8 file = os_read_file(scratch.arena, str8_lit("test.json"), false);
    Json_Value value = json_parse(scratch.arena, file);
    json_print(value);
    
    release_scratch(scratch);
}