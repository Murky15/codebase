#import "Basic";
#import "Window_Creation";
#import "Input";
#import "Simp";

main :: () {
  window := create_window(1280, 720, "Untitled");

  while true {
    update_window_events();

    for events_this_frame {
      if it.type == .QUIT then break;
    }
  }
}