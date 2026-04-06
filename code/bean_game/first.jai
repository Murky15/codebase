OUTPUT_NAME :: "BeanGame";

#run {
  w := compiler_create_workspace();
  options := get_build_options();

  args := options.compile_time_command_line;

  build_wasm := false;
  for arg: args {
    if arg == {
      case "wasm"; build_wasm = true;
    }
  }

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

  if build_wasm {
    log("Building WASM target with Emscripten backend...");

    exposed_art_path = "sprites";
    exposed_map_path = "level.bgdat";

    options.output_type = .OBJECT_FILE;
    options.runtime_support_definitions = .ENTRY_POINT_AND_INIT;
    options.backtrace_on_crash = .OFF;
    options.llvm_options.output_bitcode = true;
    remap_import(w, "", "Default_Allocator", "Emscripten_Allocator");
  }
  options.backend = .LLVM;
  options.text_output_flags = xx 0;
  options.output_executable_name = OUTPUT_NAME;
  options.output_path = join(#filepath, "../../build");
  set_optimization(*options, .VERY_DEBUG);
  set_build_options(options, w);

  compiler_begin_intercept(w);
  {
    add_build_string(tprint("EMSCRIPTEN :: %;", build_wasm), w);
    add_build_string(tprint("art_path :: \"%\";\nmap_path :: \"%\";", exposed_art_path, exposed_map_path), w);
    add_build_file(join(#filepath, "main.jai"), w);
    message_loop(build_wasm);
  }
  compiler_end_intercept(w);

  //emcc_result := run_command();
  //emcc -sMEMORY64=1 -sUSE_SDL=3 --use-preload-plugins --preload-file ..\assets\bean_game\sprites@sprites --preload-file ..\code\bean_game\level.bgdat@level.bgdat BeanGame.bc -o test.html

  if build_wasm {
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

message_loop :: (build_wasm: bool) {
  if !build_wasm then return;

  while true {
    message := compiler_wait_for_message();
    if message.kind == {
      case .TYPECHECKED; {
        typechecked := cast(*Message_Typechecked) message;

        for typechecked.procedure_bodies {
          proc := it.expression;
          header := proc.header;

          uses_asm := false;
          for it.subexpressions {
            if it.kind == .ASM then uses_asm = true;
          }

          if uses_asm {
            if header.name == "print_to_builder" {
              proc.block = xx compiler_get_nodes(print_to_builder_no_asm);
            } else {
              compiler_report("Removing body of this procedure because it contains inline assembly!", make_location(proc), .WARNING);
              new_block: [..] *Code_Node;
              dummy_return := New(Code_Return);
              arguments_unsorted: [..] Code_Argument;
              arguments_sorted: [..] *Code_Node;
              for header.returns {
                literal := New(Code_Literal);
                if it.type.type == {
                  case .INTEGER; {
                    literal.value_type = .NUMBER;
                    literal.value_flags = .IS_A_NUMBER;
                  }
                  case .BOOL; {
                    literal.value_type = .BOOLEAN;
                  }
                  case .POINTER; {
                    literal.value_type = .POINTER;
                    literal.pointer_literal_info = New(Code_Pointer_Literal_Info);
                    literal.pointer_literal_info.pointer_literal_type = .CONSTANT_VALUE;
                  }
                  case; {
                    literal.value_type = .STRUCT;
                    literal.struct_literal_info = New(Code_Struct_Literal_Info);
                  }
                }

                argument := Code_Argument.{literal, null};
                array_add(*arguments_unsorted, argument);
                array_add(*arguments_sorted, literal);
              }
              import := proc.enclosing_load.enclosing_import;
              if import && import.module_name != "Runtime_Support" {
                warning_log :: #code log("You are calling the function %, but its body has been removed due to the presence of inline ASM!", #location(#this), flags = .WARNING);
                array_add(*new_block, compiler_get_nodes(warning_log));
              }

              dummy_return.arguments_unsorted = arguments_unsorted;
              dummy_return.arguments_sorted = arguments_sorted;
              array_add(*new_block, dummy_return);

              proc.block.statements = new_block;
            }

            compiler_modify_procedure(typechecked.workspace, proc);
          }
        }
      }

      case .COMPLETE; break;
    }
  }
}

print_to_builder_no_asm :: #code {
  implicit_index_cursor := 0;

  cursor := 0;
  printed := 0;
  while cursor < format_string.count {
      c := format_string.data[cursor];

      if c != #char "%" {
          if c == 31 {
              append(builder, format_string.data + printed, cursor - printed);
              append(builder, "%");
              cursor += 1;
              printed = cursor;
              continue;
          }

          cursor += 1;
          continue;
      }

      append(builder, format_string.data + printed, cursor - printed);
      cursor += 1;  // Skip the %.

      value := implicit_index_cursor; // Will get bumped below.

      if cursor < format_string.count {
          next := format_string.data[cursor];
          if my_is_digit(next) {
              if next == #char "0" {  // %00 outputs the empty string; %0 is like a regular %.
                  cursor += 1;
                  if (cursor < format_string.count) && (format_string.data[cursor] == #char "0") {
                      // Empty string.
                      cursor += 1;
                      printed = cursor;
                      continue;
                  } else {
                      // Fall through and do the thing.
                  }
              } else {
                  // @Robustness: We probably want to test that parse_number does not overflow!
                  cursor, value = parse_number(format_string, cursor);
                  value -= 1;      // Arguments are 0-based indices from here on.
              }
          }
      }


      if value < 0 {
          if context.print_style.log_runtime_errors {
              log_error("Invalid negative \% index % in the format string at character %. Skipping.\n", value, cursor);
          }

          implicit_index_cursor = 0;
          printed = cursor;
          continue;
      }

      if value >= args.count {
          if context.print_style.log_runtime_errors {
              do_free := false;
              arguments_string := "(There are no insertable arguments.)";
              if value == 1 {
                  arguments_string = "(There is only 1 insertable argument.)";
              } else {
                  arguments_string = sprint("(There are only % insertable arguments.)", value);
                  do_free = true;
              }

              log_error("Invalid \% index % in the format string \"%\" at character %. %\n", value+1, format_string, cursor, arguments_string);
              if do_free free(arguments_string);
          }

          implicit_index_cursor = 0;
          printed = cursor;
          continue;
      }

      print_item_to_builder(builder, args[value], false);
      implicit_index_cursor = value + 1;

      printed = cursor;   // Next time we append, start here.
  }

  append(builder, format_string.data + printed, cursor - printed);

  if builder.failed return false;
  return true;
}

#import "Basic";
#import "Compiler";
#import "String";
#import "Process";
#import "File";