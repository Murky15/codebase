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
#load "gb_cpu.jai";

Bitmap :: struct {
  width, height: int;
  pixels: *u32;
  platform_data: *void;
}

desktop_create_framebuffer :: (width: int, height: int) -> Bitmap {
  result: Bitmap;
  #if OS == .WINDOWS {
    #import "Windows";
    dib_info := New(BITMAPINFO);
    dib_info.bmiHeader.biSize = size_of(BITMAPINFOHEADER);
    dib_info.bmiHeader.biWidth = xx width;
    dib_info.bmiHeader.biHeight = xx height;
    dib_info.bmiHeader.biPlanes = 1;
    dib_info.bmiHeader.biBitCount = 32;
    dib_info.bmiHeader.biCompression = BI_RGB;
    result.platform_data = dib_info;
  }
  result.width = width;
  result.height = height;
  result.pixels = alloc(width * height * size_of(u32));

  return result;
}

desktop_blt_buffer :: (window: Desktop.Window_Type, bitmap: Bitmap, window_width: int, window_height: int) {
  #if OS == .WINDOWS {
    dib_info := cast(*BITMAPINFO) bitmap.platform_data;
    client_dc := GetDC(window);
    gdi :: #library,system "Gdi32";
    StretchDIBits :: (
      hdc: HDC,
      xDest: s32, yDest: s32,
      DestWidth: s32, DestHeight: s32,
      xSrc: s32, ySrc: s32,
      SrcWidth: s32, SrcHeight: s32,
      lpBits: *void, lpbmi: *BITMAPINFO,
      iUsage: u32, rop: ROP) -> s32 #foreign gdi;

    StretchDIBits(
      client_dc,
      0, 0,
      xx window_width,
      xx window_height,
      0, 0,
      xx bitmap.width,
      xx bitmap.height,
      bitmap.pixels,
      dib_info,
      DIB_RGB_COLORS,
      .SRCCOPY
    );
    ReleaseDC(window, client_dc);
  }
}

GB_RESOLUTION_W :: 160;
GB_RESOLUTION_H :: 144;
GB_RESOLUTION_WINDOW_SCALE :: 4;

main :: () {
  perm_arena: Flat_Pool;
  context.allocator.proc = flat_pool_allocator_proc;
  context.allocator.data = *perm_arena;
  format_int := *context.print_style.default_format_int;
  format_int.base = 16;
  format_int.minimum_digits = 2;

  // TODO: Horrible use of compile time, these should only be generated once and stored somewhere.
  gb_unprefixed_instrs, gb_cbprefixed_instrs := gb_load_instruction_data();
  gb_unprefixed_instr_vtbl, gb_cbprefixed_instr_vtbl := gb_generate_instruction_vtable();
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

  main_window := Desktop.create_window(GB_RESOLUTION_W * GB_RESOLUTION_WINDOW_SCALE, GB_RESOLUTION_H * GB_RESOLUTION_WINDOW_SCALE, "Soniboy");
  _, _, window_width, window_height := Desktop.get_dimensions(main_window, true);
  frame_buffer := desktop_create_framebuffer(GB_RESOLUTION_W, GB_RESOLUTION_H);

  should_run := true;
  while should_run {
    reset_temporary_storage();

    Input.update_window_events();
    for Input.events_this_frame {
      if it.type == .QUIT then should_run = false;
    }

    gb_step(*gb_state, gb_unprefixed_instrs, gb_cbprefixed_instrs, gb_unprefixed_instr_vtbl, gb_cbprefixed_instr_vtbl);

    x_grad: u8;
    y_grad: u8;
    for y: 0..frame_buffer.height-1 {
      x_grad = 0;
      for x: 0..frame_buffer.width-1 {
        // 0xAARRGGBB
        frame_buffer.pixels[y * frame_buffer.width + x] = (cast(u32, y_grad) << 16) | (cast(u32, x_grad) << 8);
        x_grad += 1;
      }
      y_grad += 1;
    }
    desktop_blt_buffer(main_window, frame_buffer, window_width, window_height);
  }
}

#import "Basic";
#import "File";
#import "Json";
#import "Flat_Pool";
Desktop :: #import "Window_Creation";
Input   :: #import "Input";