/*
  TODO: How do we want to handle window resizes?
*/

#load "gb_instr_meta.jai";
#load "gb_cpu.jai";

GB_RESOLUTION_W :: 160;
GB_RESOLUTION_H :: 144;
GB_RESOLUTION_WINDOW_SCALE :: 4;

TARGET_FRAMES_PER_SECOND :: 60;
TARGET_MS_PER_FRAME :: cast(float) 1000/TARGET_FRAMES_PER_SECOND;
CYCLES_PER_FRAME :: GB_DMG_MASTER_CLOCK_RATE / TARGET_FRAMES_PER_SECOND;

gl_swap_buffers :: (window: Desktop.Window_Type) { // Don't know why this isn't included in GL.jai

}

main :: () {
  // TODO: Where is the best place to put this?
  perm_arena: Flat_Pool;
  arena_allocator := Allocator.{proc=flat_pool_allocator_proc, data=*perm_arena};
  context.allocator = arena_allocator;
  format_int := *context.print_style.default_format_int;
  format_int.base = 16;
  format_int.minimum_digits = 2;

  main_window := Desktop.create_window(GB_RESOLUTION_W * GB_RESOLUTION_WINDOW_SCALE, GB_RESOLUTION_H * GB_RESOLUTION_WINDOW_SCALE, "Gameboy");
  _, _, window_width, window_height := Desktop.get_dimensions(main_window, true);
  gl_create_context(main_window);
  gl_load(*gl);
  using gl;
  vao: GLuint;
  glGenVertexArrays(1, *vao);
  glBindVertexArray(vao);
  vbo: GLuint;
  TEST_TRIANGLE_VERTICES :: float.[
    -1, -1, 0,
     1, -1, 0,
     0,  1, 0
  ];
  glGenBuffers(1, *vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, TEST_TRIANGLE_VERTICES.count * size_of(float), TEST_TRIANGLE_VERTICES.data, GL_STATIC_DRAW);

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

    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, null);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableVertexAttribArray(0);
    gl_swap_buffers(main_window);

    end := seconds_since_init();
    dt := (end - start) * 1000;
    if dt < TARGET_MS_PER_FRAME
      sleep_milliseconds(cast(s32, TARGET_MS_PER_FRAME - dt));
  }
}

#import "Basic";
#import "Flat_Pool";
#import "GL";
Desktop :: #import "Window_Creation";
Input   :: #import "Input";
