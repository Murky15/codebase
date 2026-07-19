window_width:  s32 = 1280;
window_height: s32 = 720;

handle_clay_errors :: (error_data: Clay_ErrorData) #c_call {
  // TODO: Fix this
  push_context {
    log("%", error_data.errorText.chars);
  }
}

main :: () {
  window := SDL_CreateWindow("Bean Game Website", window_width, window_height, 0);
  renderer := SDL_CreateRenderer(window, null);

  required_memory := Clay_MinMemorySize();
  clay_arena := Clay_CreateArenaWithCapacityAndMemory(required_memory, alloc(required_memory));
  Clay_Initialize(clay_arena, {xx window_width, xx window_height}, {handle_clay_errors, null});

  should_run := true;
  start := SDL_GetTicks();
  while should_run {
    end := SDL_GetTicks();
    dt := (end - start) / 1000.0;
    start = end;

    e: SDL_Event;
    while SDL_PollEvent(*e) {
      if cast(SDL_EventType) e.type == {
        case .SDL_EVENT_QUIT; should_run = false;
      }
    }

    Clay_SetLayoutDimensions({xx window_width, xx window_height});
    Clay_SetPointerState({0,0}, false);
    Clay_UpdateScrollContainers(true, {0,0}, dt);

    clay_render_commands: Clay_RenderCommandArray;
    {
      Clay_BeginLayout();
      defer clay_render_commands = Clay_EndLayout(dt);

      if Clay_PushUI(Clay_Id("OuterContainer"), {layout = {sizing = {Clay_SizingGrow(), }}}) {

      }
    }
  }
}

#import "Basic";

#import "clay";
#import "sdl3";
#import "sdl3_ttf";
