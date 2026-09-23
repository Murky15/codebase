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
} Compiled_Shader_Data;

Compiled_Shader_Data compile_shader_from_file (Arena *arena, String8 file_path);

#endif // RUNTIME_SHADER_COMPILER_H