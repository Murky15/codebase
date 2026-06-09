AT_COMPILE_TIME :: true;

SOURCE_DIR :: "clay_source";
SOURCE_NAME :: "clay.h";
LIBRARY_BASE_NAME :: "clay";

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

  target_os := OS;
  if target_wasm then target_os = .WASM;

  if target_os == {
    case .WINDOWS; library_dir = "windows";
    case .LINUX;   library_dir = "linux";
    case .MACOS;   library_dir = "macos";
    case .WASM;    library_dir = "wasm";
  }

  success := true;
  if compile {
    if !is_directory(SOURCE_DIR) {
      log_error("Unable to locate clay source. Ensure that you have git cloned the repo into this directory under the proper name: \"clay_source\"");
      return false;
    }
    source_file = join(SOURCE_DIR, "/", SOURCE_NAME);

    make_directory_if_it_does_not_exist(library_dir, recursive=true);

    extra: [..] string;
    array_add(*extra, "-DCLAY_IMPLEMENTATION");
    if target_os == .WINDOWS {
      array_add(*extra, "-Tc");
    } else {
      array_add(*extra, "-x", "c");
    }

    library_location := join(library_dir, "/", LIBRARY_BASE_NAME);
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
  }

  return generate_bindings(opts, "clay_bindings.jai");
}

FOOTER :: #string EOF

#if OS == .WINDOWS {
    %1 :: #library,no_dll "windows/%1";
} else #if OS == .LINUX {
    %1 :: #library,no_dll "linux/%1";
} else #if OS == .MACOS {
    %1 :: #library,no_dll "macos/%1";
} else #if OS == .WASM {
    %1 :: #library,no_dll "wasm/%1";
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
#import "String";