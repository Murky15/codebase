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

  assert(options.cpu_target == .X64, "Only X64 targets are currently supported!");
  if build_wasm {
    log("Building WASM target with Emscripten backend...");
    options.output_type = .OBJECT_FILE;
    options.llvm_options.output_bitcode = true;
  }
  options.backend = .LLVM;
  options.text_output_flags = xx 0;
  options.output_executable_name = OUTPUT_NAME;
  options.output_path = join(#filepath, "../../build");
  set_optimization(*options, .VERY_DEBUG);
  set_build_options(options, w);
  compiler_begin_intercept(w);
  add_build_file(join(#filepath, "main.jai"), w);
  message_loop(build_wasm);
  compiler_end_intercept(w);
  emcc_command: [..] string;
  //emcc_result := run_command();


  set_build_options_dc({do_output=false});
}

message_loop :: (build_wasm: bool) {
  if !build_wasm then return;

  while true {
    message := compiler_wait_for_message();
    if message.kind == {
      case .TYPECHECKED; {
        typechecked := cast(*Message_Typechecked) message;

        for typechecked.procedure_bodies {
          proc_body := it.expression;
          proc_name := proc_body.header.name;
          if array_find(problematic_procedures, proc_name) then continue;
          for it.subexpressions {
            if it.kind != .IF then continue;
            potential_cpu_check := cast(*Code_If) it;
            if (potential_cpu_check.if_flags & .IS_STATIC) && (potential_cpu_check.static_if_flags & .EVALUATED_AS_TRUE) {
              found_inline_asm := false;
              for potential_cpu_check.then_block.statements {
                if it.kind == .ASM {
                  found_inline_asm = true;
                  array_add_if_unique(*problematic_procedures, proc_name);
                }
              }

              if found_inline_asm {
                report := tprint("This #if in procedure % will be deleted because it contains inline assembly.", proc_name);
                compiler_report(report, make_location(potential_cpu_check), .WARNING);
                if proc_name == ">>" {
                  potential_cpu_check.then_block = potential_cpu_check.else_block;
                } else {
                  empty_block: [] *Code_Node;
                  potential_cpu_check.then_block.statements = empty_block;
                }
                compiler_modify_procedure(message.workspace, proc_body);
              }
            }
          }
        }
      }

      case .COMPLETE; break;
    }
  }
}

problematic_procedures: [..] string;

#import "Basic";
#import "Compiler";
#import "String";
#import "Process";