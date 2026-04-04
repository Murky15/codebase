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

  if build_wasm {
    log("Building WASM target with Emscripten backend...");
    options.output_type = .OBJECT_FILE;
    options.runtime_support_definitions = .ENTRY_POINT_AND_INIT;
    options.backtrace_on_crash = .OFF;
    options.llvm_options.output_bitcode = true;
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
    add_build_file(join(#filepath, "main.jai"), w);
    message_loop(build_wasm);
  }
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
          proc := it.expression;
          header := proc.header;

          uses_asm := false;
          for it.subexpressions {
            if it.kind == .ASM then uses_asm = true;
          }

          if uses_asm {
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
            dummy_return.arguments_unsorted = arguments_unsorted;
            dummy_return.arguments_sorted = arguments_sorted;
            array_add(*new_block, dummy_return);
            proc.block.statements = new_block;

            compiler_modify_procedure(typechecked.workspace, proc);
          }
        }
      }

      case .COMPLETE; break;
    }
  }
}

#import "Basic";
#import "Compiler";
#import "String";
#import "Process";