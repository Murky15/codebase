AT_COMPILE_TIME :: true;

SOURCE_DIR :: "clay_source";
SOURCE_NAME :: "clay.h";
LIBRARY_BASE_NAME :: "clay";

target_os := OS;
source_file: string;
library_dir: string;

#if AT_COMPILE_TIME {
  #run,stallable {
    set_build_options_dc(.{do_output=false});
    options := get_build_options();
    args := options.compile_time_command_line;
    if !build_library(args) || !generate_bindings() {
      compiler_set_workspace_status(.FAILED);
    }
  }
} else {
  #import "System";

  main :: () {
    set_working_directory(path_strip_filename(get_path_of_running_executable()));
    if !build_library(get_command_line_arguments()) || !generate_bindings() {
      exit(1);
    }
  }
}

build_library :: (args: [] string) -> bool {
  compile       := array_find(args, "-compile");
  compile_debug := array_find(args, "-debug");
  target_wasm   := array_find(args, "-wasm");

  if target_wasm then target_os = .WASM;

  if target_os == {
    case .WINDOWS; library_dir = "windows";
    case .LINUX;   library_dir = "linux";
    case .MACOS;   library_dir = "macos";
    case .WASM;    library_dir = "wasm";
  }

  source_file = join(SOURCE_DIR, "/", SOURCE_NAME);
  if !file_exists(source_file) {
    log_error("Unable to locate clay source. Ensure that you have git cloned the repo into this directory under the proper name: \"clay_source\"");
    return false;
  }

  success := true;
  if compile {
    make_directory_if_it_does_not_exist(library_dir, recursive=true);

    library_location := join(library_dir, "/", LIBRARY_BASE_NAME);
    if target_os == .WASM {
      build_clay_wasm: [..] string;
      array_add(*build_clay_wasm, "clang");
      array_add(*build_clay_wasm, "-Os");
      array_add(*build_clay_wasm, "-DCLAY_IMPLEMENTATION");
      array_add(*build_clay_wasm, "-DCLAY_WASM");
      array_add(*build_clay_wasm, "-mbulk-memory");
      array_add(*build_clay_wasm, "--target=wasm64-unknown-unknown");
      array_add(*build_clay_wasm, "-m64");
      array_add(*build_clay_wasm, "-nostdlib");
      array_add(*build_clay_wasm, "-c");
      array_add(*build_clay_wasm, "-o");
      array_add(*build_clay_wasm, join(library_location, ".o"));
      array_add(*build_clay_wasm, "-x");
      array_add(*build_clay_wasm, "c");
      array_add(*build_clay_wasm, source_file);
      success &&= cmd(..build_clay_wasm);
    }
    extra: [..] string;
    array_add(*extra, "-DCLAY_IMPLEMENTATION");
    if target_os == .WINDOWS {
      array_add(*extra, "-Tc");
    } else {
      array_add(*extra, "-x", "c");
    }
    success &&= build_cpp_static_lib(library_location, source_file, target = target_os, extra = extra);
  }

  return success;
}

generate_bindings :: () -> bool {
  opts: Generate_Bindings_Options;
  {
    using opts;

    array_add(*library_search_paths, library_dir);
    array_add(*libraries, {filename=LIBRARY_BASE_NAME});
    array_add(*source_files, source_file);

    generate_library_declarations = false;
    footer = tprint(FOOTER, LIBRARY_BASE_NAME);

    generate_compile_time_struct_checks = false;

    visitor = clay_visitor;
  }

  return generate_bindings(opts, "clay_bindings.jai");
}

clay_visitor :: (decl: *Declaration, parent_decl: *Declaration) -> result: Declaration_Visit_Result = .RECURSE {
  if target_os == .WASM && decl.library {
    decl.library.identifier = "";
  }

  return;
}

cmd :: (args: .. string, redirect_stdout_to_file := "", loc := #caller_location) -> bool {
  log("[CMD] %\n", get_quoted_command_string(args));

  result, output_string, error_string := run_command(..args, capture_and_return_output = redirect_stdout_to_file != "");
  if result.exit_code {
    if output_string then log("%", output_string);
    if error_string then log("%", error_string);
    compiler_report(tprint("Command failed with exit code '%'.\n", result.exit_code), loc);
    return false;
  }

  if redirect_stdout_to_file then write_entire_file(redirect_stdout_to_file, output_string);

  return true;
}

FOOTER :: #string EOF
#scope_module
#if OS == .WINDOWS {
  %1 :: #library,no_dll "windows/%1";
} else #if OS == .LINUX {
  %1 :: #library,no_dll "linux/%1";
} else #if OS == .MACOS {
  %1 :: #library,no_dll "macos/%1";
} else #if OS == .WASM {
  // NOTE: Do nothing.
} else {
  #assert false;
}
EOF

#import "Basic";
#import "Bindings_Generator";
#import "BuildCpp";
#import "Compiler";
#import "File";
#import "File_Utilities";
#import "Process";
#import "String";