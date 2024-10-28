#include <stdio.h>

#include "../base/include.h"
#include "../os/include.h"
#include "../base/include.c"
#include "../os/include.c"

core_function Json_Value* 
json_parse (Arena *arena, String8 json) {
    // Lexical Analysis
    Json_Token active_token = JSON_TOKEN_NULL;
    for (u64 i = 0; i < json->len; ++i) {
        u8 c = json->str[i];
        switch (active_token) {
            case JSON_TOKEN_NULL: {
                
            } break;
        }
    }
    
    // Parsing
}

// Quick test
int main (void) {
    // Read "test.json"
    Temp_Arena scratch = get_scratch();
    String8 file = os_read_file(scratch.arena, str8_lit("test.json"), false);
    Json_Value *value = json_parse(scratch, file);
    
    release_scratch(scratch);
}