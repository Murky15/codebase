OUTPUT_NAME :: "BeanGame";

#run {
  global_opts := get_build_options();
  global_opts.text_output_flags = xx 0;

  args := global_opts.compile_time_command_line;
  build_wasm    := array_find(args, "wasm");
  build_website := array_find(args, "website");

  // NOTE: Website build
  if build_website {
    w := compiler_create_workspace("Website");

    web_opts := get_build_options(w);
    copy_commonly_propagated_fields(global_opts, *web_opts);
    {
      using web_opts;
      output_path                         = #filepath; // TODO: Temporary!
      output_type                         = .EXECUTABLE;
      backend                             = .LLVM;
      os_target                           = .WASM;
      cpu_target                          = .CUSTOM;
      emit_debug_info                     = .DWARF;
      backtrace_on_crash                  = .OFF;
      runtime_support_definitions         = .OMIT;
      output_executable_name              = "index";
      llvm_options.target_system_features = "+bulk-memory"; // NOTE: Not working?
      llvm_options.enable_split_modules   = false;
      llvm_options.function_sections      = true;

      walloc_object_file_path := join(#filepath, "modules/Walloc/walloc.o");
      additional_linker_arguments = .[walloc_object_file_path];
    }

    import_paths: [..] string;
    array_add(*import_paths, join(#filepath, "modules"));
    for web_opts.import_path array_add(*import_paths, it);
    web_opts.import_path = import_paths;

    set_build_options(web_opts, w);
    remap_import(w, "*", "Default_Allocator", "Walloc");
    add_build_file(join(#filepath, "website.jai"), w);
  } 

  // NOTE: Game build
  // TODO: For emscripten builds, compile to WASM object file and then call emcc on that, not the llvm bc
  {
    w := compiler_create_workspace("Game");

    options := get_build_options(w);
    copy_commonly_propagated_fields(global_opts, *options);
    options.output_path = join(#filepath, "../../build");

    // TODO: This process will need to be expanded obviously.
    art_path, map_path: string;
    ifx OS == {
      case .WINDOWS; {
        art_path = "W:/assets/bean_game/sprites";
        map_path = "W:/code/bean_game/level.bgdat";
      }
    }

    exposed_art_path := art_path;
    exposed_map_path := map_path;

    options.backend = .LLVM;
    options.output_executable_name = OUTPUT_NAME;
    set_optimization(*options, .VERY_DEBUG);
    set_build_options(options, w);

    add_build_string(tprint("art_path :: \"%\";\nmap_path :: \"%\";", exposed_art_path, exposed_map_path), w);
    add_build_file(join(#filepath, "main.jai"), w);

    /*
    emscripten: [..] string;
    array_add(*emscripten, "emcc.bat");
    array_add(*emscripten, "-sMEMORY64=1");
    array_add(*emscripten, "-sUSE_SDL=3");
    array_add(*emscripten, "--use-preload-plugins");
    array_add(*emscripten, "--preload-file");
    array_add(*emscripten, tprint("%@sprites", art_path));
    array_add(*emscripten, "--preload-file");
    array_add(*emscripten, tprint("%@level.bgdat", map_path));
    array_add(*emscripten, tprint("%/%.bc", options.output_path, OUTPUT_NAME));
    array_add(*emscripten, "-o");
    array_add(*emscripten, tprint("%/%.html", options.output_path, OUTPUT_NAME));
    cmd(..emscripten);
    */
  }

  set_build_options_dc({do_output=false});
}

cmd :: (args: .. string, redirect_stdout_to_file := "", loc := #caller_location) {
  log("[CMD] %\n", get_quoted_command_string(args));

  result, output_string, error_string := run_command(..args, capture_and_return_output = redirect_stdout_to_file != "");
  if result.exit_code {
      if output_string then log("%", output_string);
      if error_string then log("%", error_string);
      compiler_report(tprint("Command failed with exit code '%'.\n", result.exit_code), loc);
      exit(result.exit_code);
  }

  if redirect_stdout_to_file then write_entire_file(redirect_stdout_to_file, output_string);
}

#import "Basic";
#import "Compiler";
#import "String";
#import "Process";
#import "File";