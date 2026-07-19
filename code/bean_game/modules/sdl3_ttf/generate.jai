LIBRARY_BASE_NAME :: "sdl3_ttf";

#run {
  set_build_options_dc({do_output=false});
  if !generate_bindings() {
    compiler_set_workspace_status(.FAILED);
    return;
  }
}

generate_bindings :: () -> bool {
  os, cpu: string;
  #if OS == {
    case .WINDOWS; os = "windows";
    case .LINUX;   os = "linux";
    case .MACOS;   os = "macos";
  }
  #if CPU == {
    case .X64;   cpu = "x64";
    case .ARM64; cpu = "arm64";
  }
  library_path := join(os, "/", cpu);

  opts: Generate_Bindings_Options;
  {
    using opts;

    array_add(*library_search_paths, library_path);
    array_add(*libraries, {filename=LIBRARY_BASE_NAME});

    // TODO: Not sure how I feel about using relative paths...
    array_add(*system_include_paths, "../sdl3/include/");
    array_add(*source_files, "include/SDL3_ttf/SDL_ttf.h");
    array_add(*extra_clang_arguments, "-D__PRFCHWINTRIN_H");

    generate_library_declarations = false;
    footer = tprint(FOOTER, LIBRARY_BASE_NAME);

    generate_compile_time_struct_checks = false;
  }

  output_filename := "bindings.jai";
  return generate_bindings(opts, output_filename);
}

FOOTER :: #string EOF
#if OS == .WINDOWS {
  #if CPU == .X64 {
    %1 :: #library,no_dll "windows/x64/%1";
  } else #if CPU == .ARM64 {
    %1 :: #library,no_dll "windows/arm64/%1";
  }
}
EOF

#import "Basic";
#import "Bindings_Generator";
#import "Compiler";
#import "String";