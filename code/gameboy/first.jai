build :: () {
  w := compiler_create_workspace();
  options := get_build_options();
  options.output_executable_name = "gameboy";
  options.output_path = join(#filepath, "../../build");

  extra_modules := join(#filepath, "extra_modules");
  import_paths: [..] string;
  array_add(*import_paths, extra_modules);
  array_add(*import_paths, ..options.import_path);
  options.import_path = import_paths;

  set_optimization(*options, .VERY_DEBUG);
  set_build_options(options, w);

  add_build_file(join(#filepath, "platform_desktop.jai"), w);

  test_rom_path := join(#filepath, "test_roms/02-interrupts.gb");
  opcode_desc_path := join(#filepath, "gb_opcodes.json");
  add_build_string(tprint("TEST_ROM_PATH :: \"%\"; OPCODE_DESCRIPTOR_PATH :: \"%\";", test_rom_path, opcode_desc_path), w);


  print_to_doctor := false;
  for options.compile_time_command_line {
    if it == {
      case "doctor"; print_to_doctor = true;
    }
  }
  add_build_string(tprint("GB_DOCTOR_LOG :: %;", print_to_doctor), w);

  set_build_options_dc({do_output=false});
}

#run build();
#import "Basic";
#import "Compiler";
#import "String";