#define base_function extern
#include "base.h"
#undef OS_WINDOWS

#include <windows.h>
#include <atlbase.h>
#include <dxcapi.h>
#include <d3d12shader.h>

#include "runtime_shader_compiler.h"

function void
win32_append_last_error_to_list (Arena *arena, Str8_List *list) {
  DWORD open_file_error = GetLastError();
  DWORD format_flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  u8 *error_buffer = 0;
  FormatMessage(format_flags, NULL, open_file_error, 0, (LPSTR)&error_buffer, 0, NULL);

  Str8 error_message = str8_cstring((const char*)error_buffer);
  str8_list_push(arena, list, str8_push_copy(arena, error_message));
  LocalFree(error_buffer);
}

// TODO: Return errors/warnings as an array of strings
Compiled_Shader_Data
compile_shader_from_file (Arena *arena, Str8 file_path, Str8 entry_point, Str8 profile, Str8_List *messages_out) {
  /*
  ScratchBlock(&arena,1) {
    u8 *cstr_file_path = str8_to_cstr(scratch.arena, file_path);
    HANDLE hFile = CreateFile(
      (LPCSTR)cstr_file_path,
      GENERIC_READ,
      0,
      NULL,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL,
      NULL
    );
    if (hFile == INVALID_HANDLE_VALUE)
      win32_append_last_error_to_list(arena, messages_out);

    LARGE_INTEGER full_file_size = {};
    if (!GetFileSizeEx(hFile, &full_file_size))
      win32_append_last_error_to_list(arena, messages_out);
    Assert(full_file_size.QuadPart <= UINT32_MAX, "Shader source file is too big!");
    DWORD file_size = full_file_size.LowPart;

    u8 *source_buffer = ArenaPush(scratch.arena, u8, file_size);
    DWORD bytes_read;
    if (!ReadFile(hFile, source_buffer, file_size, &bytes_read, NULL))
      win32_append_last_error_to_list(arena, messages_out);

    Assert(bytes_read == file_size);
    Str8 shader_source = str8(source_buffer, file_size);
    CloseHandle(hFile);

    CComPtr<IDxcUtils> utils;
    CComPtr<IDxcCompiler3> compiler;
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    CComPtr<IDxcIncludeHandler> include_handler;
    utils->CreateDefaultIncludeHandler(&include_handler);
  }
  */

  CComPtr<IDxcUtils> utils;
  CComPtr<IDxcCompiler3> compiler;
  DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
  DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
  CComPtr<IDxcIncludeHandler> include_handler;
  utils->CreateDefaultIncludeHandler(&include_handler);

  ScratchBlock(&arena, 1) {
    Str16 wstr_file_path = str8_to_str16(scratch.arena, file_path);
    CComPtr<IDxcBlobEncoding> file = nullptr;
    utils->LoadFile((LPCWSTR)wstr_file_path.str, nullptr, &file);
    DxcBuffer shader_source = {};
    shader_source.Ptr      = file->GetBufferPointer();
    shader_source.Size     = file->GetBufferSize();
    shader_source.Encoding = DXC_CP_ACP;

    CComPtr<IDxcCompilerArgs> args;
    utils->BuildArguments(
      (LPCWSTR)wstr_file_path.str, // TODO: Shorten to just file name?
      (LPCWSTR)str8_to_str16(scratch.arena, entry_point).str,
      (LPCWSTR)str8_to_str16(scratch.arena, profile).str,
      NULL, 0,
      NULL, 0,
      &args
    );

    CComPtr<IDxcResult> results;
    compiler->Compile(
      &shader_source,
      args->GetArguments(),
      args->GetCount(),
      include_handler,
      IID_PPV_ARGS(&results)
    );

    CComPtr<IDxcBlobUtf8> errors;
    results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

  }

  return {};
}