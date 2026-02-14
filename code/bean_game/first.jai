#run {
  w := compiler_create_workspace();
  options := get_build_options();
  options.output_executable_name = "BeanGame";
  options.output_path = join(#filepath, "../../build");

  set_optimization(*options, .VERY_DEBUG);
  set_build_options(options, w);

  add_build_file(join(#filepath, "main.jai"), w);
  set_build_options_dc({do_output=false});
}

#import "Basic";
#import "Compiler";
#import "String";