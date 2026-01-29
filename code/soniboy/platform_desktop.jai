/*  Tests Passed
  -[X] 01-special
  -[ ] 02-interrupts
  -[X] 03-op sp,hl
  -[X] 04-op r,imm
  -[X] 05-op rp
  -[X] 06-ld r,r
  -[X] 07-jr,jp,call,ret,rst
  -[X] 08-misc instrs
  -[X] 09-op r,r
  -[X] 10-bit ops
  -[X] 11-op a,(hl)
*/

#load "gb_instr_meta.jai";
#load "emulator.jai";

GB_RESOLUTION_W :: 160;
GB_RESOLUTION_H :: 144;

main :: () {
  perm_arena: Flat_Pool;
  context.allocator.proc = flat_pool_allocator_proc;
  context.allocator.data = *perm_arena;
  format_int := *context.print_style.default_format_int;
  format_int.base = 16;
  format_int.minimum_digits = 2;

  // TODO: Horrible use of compile time, these should only be generated once and stored somewhere.
  gb_unprefixed_instrs, gb_cbprefixed_instrs :: #run gb_load_instruction_data();
  gb_unprefixed_instr_vtbl, gb_cbprefixed_instr_vtbl := gb_generate_instruction_vtable(gb_unprefixed_instrs, gb_cbprefixed_instrs);
  gb_state: GB_State;
  gb_state.base_mem = cast(*u8) alloc(0xFFFF);
  assert (gb_state.base_mem != null);

  test_rom, rom_load_result := read_entire_file(TEST_ROM_PATH);
  assert(rom_load_result);
  memcpy(gb_state.base_mem, test_rom.data, test_rom.count);
  gb_state.base_mem[0xFF44] = 0x90;
  gb_state.cpu.A = 0x01;
  gb_state.cpu.F = 0xB0;
  gb_state.cpu.B = 0x00;
  gb_state.cpu.C = 0x13;
  gb_state.cpu.D = 0x00;
  gb_state.cpu.E = 0xD8;
  gb_state.cpu.H = 0x01;
  gb_state.cpu.L = 0x4D;
  gb_state.cpu.SP = 0xFFFE;
  gb_state.cpu.PC = 0x0100;

  main_window := Desktop.create_window(GB_RESOLUTION_W * 4, GB_RESOLUTION_H * 4, "Soniboy");

  should_run := true;
  while should_run {
    reset_temporary_storage();

    Input.update_window_events();
    for Input.events_this_frame {
      if it.type == .QUIT then should_run = false;
    }

    gb_step(*gb_state, gb_unprefixed_instrs, gb_cbprefixed_instrs, gb_unprefixed_instr_vtbl, gb_cbprefixed_instr_vtbl);
  }
}

#import "Basic";
#import "File";
#import "Json";
#import "Flat_Pool";
Desktop :: #import "Window_Creation";
Input   :: #import "Input";