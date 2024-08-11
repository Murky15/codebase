main :: () {
  WINDOW_WIDTH :: 1280;
  WINDOW_HEIGHT :: 720;
  main_window := create_window(WINDOW_WIDTH, WINDOW_HEIGHT, "Tracer");

  quit: bool;
  while !quit {
    update_window_events();

    for events_this_frame {
      if it.type == .QUIT then quit = true;
    }
  }
}

#scope_file
#import "Basic";
#import "Window_Creation";
#import "Input";