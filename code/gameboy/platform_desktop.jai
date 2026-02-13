#load "gb_instr_meta.jai";
#load "gb_cpu.jai";
#load "gb_gpu.jai";

GB_RESOLUTION_W :: 160;
GB_RESOLUTION_H :: 144;
GB_RESOLUTION_WINDOW_SCALE :: 4;

TARGET_FRAMES_PER_SECOND :: cast(float) GB_DMG_MASTER_CLOCK_RATE / GB_DOTS_PER_FRAME;
TARGET_MS_PER_FRAME :: 1000/TARGET_FRAMES_PER_SECOND;

Bitmap :: struct {
  width, height: int;
  pixels: *u32;
}

main :: () {
  // TODO: Where is the best place to put this?
  perm_arena: Flat_Pool;
  arena_allocator := Allocator.{proc=flat_pool_allocator_proc, data=*perm_arena};
  context.allocator = arena_allocator;
  format_int := *context.print_style.default_format_int;
  format_int.base = 16;
  format_int.minimum_digits = 2;

  init := SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
  assert(init);
  main_window := SDL_CreateWindow("Game Boy", GB_RESOLUTION_W * GB_RESOLUTION_WINDOW_SCALE, GB_RESOLUTION_H * GB_RESOLUTION_WINDOW_SCALE, 0);
  assert(main_window != null);

  framebuffer := Bitmap.{GB_RESOLUTION_W, GB_RESOLUTION_H,
                  alloc(GB_RESOLUTION_W * GB_RESOLUTION_H * size_of(u32))};
  gb_state := gb_init();

  window_surface := SDL_GetWindowSurface(main_window);
  framebuffer_surface := SDL_CreateSurfaceFrom(GB_RESOLUTION_W, GB_RESOLUTION_H, .XRGB8888, framebuffer.pixels, GB_RESOLUTION_W * size_of(u32));

  should_run := true;
  while should_run {
    reset_temporary_storage();
    start := seconds_since_init();

    e: SDL_Event;
    while SDL_PollEvent(*e) {
      if e.type == {
        case xx SDL_EventType.QUIT; should_run = false;
      }
    }

    {
      SDL_ClearSurface(framebuffer_surface, 1, 1, 1, 1);
      defer {
        SDL_BlitSurfaceScaled(framebuffer_surface, null, window_surface, null, .SDL_SCALEMODE_NEAREST);
        SDL_UpdateWindowSurface(main_window);
      }
      cycles_this_frame := 0;
      while cycles_this_frame < GB_DOTS_PER_FRAME {
        cycles_taken := gb_step(*gb_state);
        cycles_this_frame += cycles_taken;
        gb_ppu_process_frame(*gb_state, cycles_taken, cycles_this_frame, framebuffer);
      }
    }

    end := seconds_since_init();
    dt := (end - start) * 1000;
    if dt < TARGET_MS_PER_FRAME
      sleep_milliseconds(cast(s32, TARGET_MS_PER_FRAME - dt));
  }
}

#import "Basic";
#import "Flat_Pool";
#if OS == .WINDOWS {
  #import,file "extra_modules/sdl3/windows/windows.jai";
} else #if OS == .LINUX {
  #import,file "extra_modules/sdl3/linux/linux.jai";
}