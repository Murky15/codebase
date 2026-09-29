#define base_function extern
#include "base.h"
#undef OS_WINDOWS

#include <windows.h>
#include <atlbase.h>
#include <dxcapi.h>
#include <d3d12shader.h>

#include <SDL3/SDL_gpu.h>

#include "runtime_shader_compiler.h"

typedef HRESULT (*dxc_create_instance_sig)(REFCLSID,REFIID,LPVOID);
dxc_create_instance_sig DxcCreateInstance_;
#define DxcCreateInstance DxcCreateInstance_

global HMODULE dxcompiler_dll;

function void
win32_append_last_error_to_list (Arena *arena, Str8_List *list) {
  DWORD open_file_error = GetLastError();
  DWORD format_flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  u8 *error_buffer = 0;
  FormatMessage(format_flags, NULL, open_file_error, 0, (LPSTR)&error_buffer, 0, NULL);

  Str8 error_message = Str8CStr((const char*)error_buffer);
  str8_list_push(arena, list, str8_push_copy(arena, error_message));
  LocalFree(error_buffer);
}

function bool
win32_load_dxcompiler (Arena *arena, Str8_List *messages_out) {
  dxcompiler_dll = LoadLibrary("dxcompiler.dll");
  if (!dxcompiler_dll) return false;
  DxcCreateInstance = (dxc_create_instance_sig) GetProcAddress(dxcompiler_dll, "DxcCreateInstance");

  return !!DxcCreateInstance;
}

// TODO: Add debug mode and SPIR-V option
Compiled_Shader_Data
compile_shader_from_file (Arena *arena, Str8 file_path, Str8 entry_point, Str8 profile, Str8_List *messages_out) {
  Compiled_Shader_Data result = {};

  if (!dxcompiler_dll && !win32_load_dxcompiler(arena, messages_out)) {
    win32_append_last_error_to_list(arena, messages_out);
    return result;
  }

  CComPtr<IDxcUtils> utils;
  CComPtr<IDxcCompiler3> compiler;
  DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
  DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
  CComPtr<IDxcIncludeHandler> include_handler;
  utils->CreateDefaultIncludeHandler(&include_handler);

  CComPtr<IDxcCompilerArgs> args;
  CComPtr<IDxcBlobEncoding> file = nullptr;
  ScratchBlock(&arena, 1) {
    Str16 wstr_file_path = str8_to_str16(scratch.arena, file_path);
    utils->LoadFile((LPCWSTR)wstr_file_path.str, nullptr, &file);
    utils->BuildArguments(
      (LPCWSTR)wstr_file_path.str, // TODO: Shorten to just file name?
      (LPCWSTR)str8_to_str16(scratch.arena, entry_point).str,
      (LPCWSTR)str8_to_str16(scratch.arena, profile).str,
      NULL, 0,
      NULL, 0,
      &args
    );
  }
  if (!file) {
    str8_list_pushf(arena, messages_out, "Unable to find shader source file: %.*s", Str8Expand(file_path));
    return result;
  }

  DxcBuffer shader_source = {};
  shader_source.Ptr       = file->GetBufferPointer();
  shader_source.Size      = file->GetBufferSize();
  shader_source.Encoding  = DXC_CP_ACP;
  CComPtr<IDxcResult> results;
  compiler->Compile(
    &shader_source,
    args->GetArguments(),
    args->GetCount(),
    include_handler,
    IID_PPV_ARGS(&results)
  );

  CComPtr<IDxcBlobUtf8> messages;
  results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&messages), nullptr);
  if (messages && messages->GetStringLength() > 0) {
    str8_list_pushf(arena, messages_out, "%s", messages->GetStringPointer());
  }
  HRESULT compilation_status;
  results->GetStatus(&compilation_status);
  if (FAILED(compilation_status)) {
    str8_list_push(arena, messages_out, Str8Lit("Failure"));
    return result;
  }

  CComPtr<IDxcBlob> output;
  CComPtr<IDxcBlobUtf16> output_name;
  results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&output), &output_name);
  u64 output_count = output->GetBufferSize();
  u8 *output_buffer = ArenaPush(arena, u8, output_count);
  memcpy(output_buffer, output->GetBufferPointer(), output_count);
  result.format = SDL_GPU_SHADERFORMAT_DXIL;
  result.data = output_buffer;
  result.count = output_count;
  str8_list_push(arena, messages_out, Str8Lit("Success"));

  return result;
}