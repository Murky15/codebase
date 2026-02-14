main :: () {
  perm_arena: Flat_Pool;
  arena_allocator := Allocator.{proc=flat_pool_allocator_proc, data=*perm_arena};
  context.allocator = arena_allocator;

  main_window := create_window(1280, 720, "Bean Game");
  window_width, window_height := Simp.get_render_dimensions(main_window);
  Simp.set_render_target(main_window);

  should_run := true;
  while should_run {
    reset_temporary_storage();
    Input.update_window_events();

    for Input.events_this_frame {
      if it.type == .QUIT should_run = false;
    }

    Simp.clear_render_target(1, 1, 1, 1);
    Simp.set_shader_for_color();
    Simp.immediate_quad(0, 0, 50, 50, {.5, .5, .5, 1});
    Simp.swap_buffers(main_window);
  }
}

#import "Basic";
#import "Window_Creation";
#import "Flat_Pool";
Input :: #import "Input";
Simp  :: #import "Simp";