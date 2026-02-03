/*
  TODO: How do we want to handle window resizes?
*/

#load "gb_instr_meta.jai";
#load "gb_cpu.jai";

Bitmap :: struct {
  width, height: int;
  pixels: *u32;
  platform_data: *void;
}

#if OS == .LINUX {
  X11_Blt_State :: struct {
    fb: [2] XImage;
    gc: GC;
  }
}

desktop_create_framebuffer :: (window: Desktop.Window_Type, width: int, height: int) -> Bitmap {
  result: Bitmap;
  result.width = width;
  result.height = height;
  result.pixels = alloc(width * height * size_of(u32));
  #if OS == .WINDOWS {
    dib_info := New(BITMAPINFO);
    dib_info.bmiHeader.biSize = size_of(BITMAPINFOHEADER);
    dib_info.bmiHeader.biWidth = xx width;
    dib_info.bmiHeader.biHeight = xx height;
    dib_info.bmiHeader.biPlanes = 1;
    dib_info.bmiHeader.biBitCount = 32;
    dib_info.bmiHeader.biCompression = BI_RGB;
    result.platform_data = dib_info;
  } else #if OS == .LINUX {
    LSBFirst :: 0;
    blt_state := New(X11_Blt_State);
    blt_state.fb[0].width  = xx width;
    blt_state.fb[0].height = xx height;
    blt_state.fb[0].format = ZPixmap;
    blt_state.fb[0].data = xx result.pixels;
    blt_state.fb[0].byte_order = LSBFirst;
    blt_state.fb[0].bitmap_unit = 32;
    blt_state.fb[0].bitmap_bit_order = LSBFirst;
    blt_state.fb[0].bitmap_pad = 32;
    blt_state.fb[0].depth = 24;
    blt_state.fb[0].bytes_per_line = xx(4 * width);
    blt_state.fb[0].bits_per_pixel = 32;
    blt_state.fb[0].red_mask   = 0x00FF0000;
    blt_state.fb[0].green_mask = 0x0000FF00;
    blt_state.fb[0].blue_mask  = 0x000000FF;

    blt_state.fb[1] = blt_state.fb[0];
    blt_state.fb[1].width  = xx(width  * GB_RESOLUTION_WINDOW_SCALE);
    blt_state.fb[1].height = xx(height * GB_RESOLUTION_WINDOW_SCALE);
    blt_state.fb[1].data = alloc(blt_state.fb[1].width * blt_state.fb[1].height * size_of(u32));
    blt_state.fb[1].bytes_per_line = xx(4 * blt_state.fb[1].width);

    assert(xx XInitImage(*blt_state.fb[0]), "X11: Unable to allocate framebuffer!");
    assert(xx XInitImage(*blt_state.fb[1]), "X11: Unable to allocate framebuffer!");

    blt_state.gc = XCreateGC(x_global_display, window, 0, null);
    assert(xx blt_state.gc, "X11: Unable to create graphics context!");
    result.platform_data = blt_state;
  }

  return result;
}

desktop_blt_buffer :: (window: Desktop.Window_Type, bitmap: Bitmap, window_width: int, window_height: int) {
  #if OS == .WINDOWS {
    dib_info := cast(*BITMAPINFO) bitmap.platform_data;
    client_dc := GetDC(window);
    defer ReleaseDC(window, client_dc);
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
  } else #if OS == .LINUX {
    blt_state := cast(*X11_Blt_State) bitmap.platform_data;
    src := cast(*u32) blt_state.fb[0].data;
    dest := cast(*u32) blt_state.fb[1].data;
    for y: 0..window_height {
      for x: 0..window_width {
        dest[y * window_width + x] = src[(y / GB_RESOLUTION_WINDOW_SCALE) * bitmap.width + (x / GB_RESOLUTION_WINDOW_SCALE)];
      }
    }
    XPutImage(x_global_display, window, blt_state.gc, *blt_state.fb[1], 0, 0, 0, 0, xx window_width, xx window_height);
  }
}

GB_RESOLUTION_W :: 160;
GB_RESOLUTION_H :: 144;
GB_RESOLUTION_WINDOW_SCALE :: 4;

TARGET_FRAMES_PER_SECOND :: 60;
TARGET_MS_PER_FRAME :: cast(float) 1000/TARGET_FRAMES_PER_SECOND;
CYCLES_PER_FRAME :: GB_DMG_MASTER_CLOCK_RATE / TARGET_FRAMES_PER_SECOND;

main :: () {
  // TODO: Where is the best place to put this?
  perm_arena: Flat_Pool;
  context.allocator.proc = flat_pool_allocator_proc;
  context.allocator.data = *perm_arena;
  format_int := *context.print_style.default_format_int;
  format_int.base = 16;
  format_int.minimum_digits = 2;

  main_window := Desktop.create_window(GB_RESOLUTION_W * GB_RESOLUTION_WINDOW_SCALE, GB_RESOLUTION_H * GB_RESOLUTION_WINDOW_SCALE, "Soniboy");
  _, _, window_width, window_height := Desktop.get_dimensions(main_window, true);
  frame_buffer := desktop_create_framebuffer(main_window, GB_RESOLUTION_W, GB_RESOLUTION_H);

  gb_state := gb_init();

  should_run := true;
  while should_run {
    reset_temporary_storage();
    start := seconds_since_init();

    Input.update_window_events();
    for Input.events_this_frame {
      if it.type == .QUIT then should_run = false;
    }

    cycles_this_frame := 0;
    while cycles_this_frame < CYCLES_PER_FRAME {
      cycles_this_frame += gb_step(*gb_state);
    }

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

    end := seconds_since_init();
    dt := (end - start) * 1000;
    if dt < TARGET_MS_PER_FRAME
      sleep_milliseconds(cast(s32, TARGET_MS_PER_FRAME - dt));
  }
}

#import "Basic";
#import "File";
#import "Json";
#import "Flat_Pool";
Desktop :: #import "Window_Creation";
Input   :: #import "Input";

#if OS == .WINDOWS
  #import "Windows";
else #if OS == .LINUX
  #import "X11";