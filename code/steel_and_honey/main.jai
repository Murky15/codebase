#import "Basic";
#import "Window_Creation";
#import "Input";
#import "GL";

main :: () {
  window := create_window(1280, 720, "Steel and Honey");
  gl_create_context(window);
  gl_major, gl_minor := gl_get_version();
  print("Created OpenGL context with version: %.%", gl_major, gl_minor);

  gl_load(*gl);
  using gl;


  running := true;
  while running {
    update_window_events();

    for events_this_frame {
      if it.type == .QUIT then running = false;
    }
  }
}