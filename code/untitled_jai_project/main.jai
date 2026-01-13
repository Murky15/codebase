#import "Basic";
#import "Window_Creation";
#import "Input";

main :: () {
  window := create_window(1280, 720, "Untitled");

  running := true;
  while running {
    update_window_events();
    for events_this_frame {
      if it.type == .QUIT then running = false;
    }
  }
}