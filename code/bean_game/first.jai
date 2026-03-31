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
  message_loop(w, options.output_path, build_wasm);
  compiler_end_intercept(w);
  emcc_command: [..] string;
  //emcc_result := run_command();


  set_build_options_dc({do_output=false});
}

message_loop :: (w: Workspace, working_directory: string, build_wasm: bool) {
  if !build_wasm then return;

  while true {
    message := compiler_wait_for_message();
    if message.kind == {
      case .TYPECHECKED; {
        typechecked := cast(*Message_Typechecked) message;
        if !build_wasm then continue;
        for typed_body: typechecked.procedure_bodies {
          for typed_body.subexpressions {
            if it.kind == .IF {
              if_node := cast(*Code_If) it;
              if if_node.if_flags & .IS_STATIC {
                log("%", if_node.condition.*);
              }
            }
          }

          /*
          if contains_inline_asm {
            body := proc_body.expression;
            header := body.header;
            log("Removing body of procedure: % because it contains #asm", header.name, loc = make_location(body), flags = .WARNING);
            empty_returns: [..] *Code_Declaration;
            header.returns = empty_returns;
            empty_statements: [..] *Code_Node;
            body.block.statements = empty_statements;
            compiler_modify_procedure(w, body);
          }
          */
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