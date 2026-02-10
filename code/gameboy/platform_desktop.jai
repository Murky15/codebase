#load "gb_instr_meta.jai";
#load "gb_cpu.jai";

GB_RESOLUTION_W :: 160;
GB_RESOLUTION_H :: 144;
GB_RESOLUTION_WINDOW_SCALE :: 4;

TARGET_FRAMES_PER_SECOND :: 60;
TARGET_MS_PER_FRAME :: cast(float) 1000/TARGET_FRAMES_PER_SECOND;
CYCLES_PER_FRAME :: GB_DMG_MASTER_CLOCK_RATE / TARGET_FRAMES_PER_SECOND;

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

  window_surface := SDL_GetWindowSurface(main_window);
  framebuffer := SDL_CreateSurface(GB_RESOLUTION_W, GB_RESOLUTION_H, .XRGB8888);
  gb_state := gb_init();

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

    cycles_this_frame := 0;
    while cycles_this_frame < CYCLES_PER_FRAME {
      cycles_this_frame += gb_step(*gb_state);
    }

    SDL_ClearSurface(framebuffer, 1, 1, 1, 1);
    SDL_LockSurface(framebuffer);
    {
      defer {
        SDL_UnlockSurface(framebuffer);
        SDL_BlitSurfaceScaled(framebuffer, null, window_surface, null, .SDL_SCALEMODE_NEAREST);
        SDL_UpdateWindowSurface(main_window);
      }
      x_grad: u8;
      y_grad: u8;
      pixels := cast(*u32) framebuffer.pixels;
      for y: 0..framebuffer.h-1 {
        x_grad = 0;
        for x: 0..framebuffer.w-1 {
          // 0xXXRRGGBB
          pixels[y * framebuffer.w + x] = (cast(u32, y_grad) << 16) | (cast(u32, x_grad));
          x_grad += 1;
        }
        y_grad += 1;
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