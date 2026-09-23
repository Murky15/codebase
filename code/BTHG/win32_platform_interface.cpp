#define base_function extern
#include "base.h"

#include <windows.h>
#include <dxcapi.h>

#include "runtime_shader_compiler.h"

// TODO: Return errors/warnings as an array of strings
Compiled_Shader_Data
compile_shader_from_file (Arena *arena, String8 file_path) {
  ScratchBlock(&arena,1) {
    //HANDLE hFile = CreateFile();

  }
  return {};
}