#ifndef RUNTIME_SHADER_COMPILER_H
#define RUNTIME_SHADER_COMPILER_H

typedef struct Compiled_Shader_Data {
  SDL_GPUShaderFormat format;
  u8 *data;
  u64 count;
} Compiled_Shader_Data;

#if LANG_CPP
extern "C" {
#endif

Compiled_Shader_Data compile_shader_from_file (Arena *arena, Str8 file_path, Str8 entry_point, Str8 profile, Str8_List *messages_out);

#if LANG_CPP
}
#endif

#endif // RUNTIME_SHADER_COMPILER_H