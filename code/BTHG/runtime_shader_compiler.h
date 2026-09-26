#ifndef RUNTIME_SHADER_COMPILER_H
#define RUNTIME_SHADER_COMPILER_H

enum {
  SHADER_FORMAT_SPIRV,
  SHADER_FORMAT_DXIL,

  SHADER_FORMAT_COUNT
};
typedef u32 Shader_Format;

typedef struct Compiled_Shader_Data {
  Shader_Format format;
  u32 *data;
  u64 count;
  bool succeeded;
} Compiled_Shader_Data;

#if LANG_CPP
extern "C" {
#endif

Compiled_Shader_Data compile_shader_from_file (Arena *arena, Str8 file_path, Str8 entry_point, Str8 profile, Str8_List *messages_out);

#if LANG_CPP
}
#endif

#endif // RUNTIME_SHADER_COMPILER_H