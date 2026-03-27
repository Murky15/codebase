set_vsync :: (enable: bool) {
  #import "GL";
  #if OS == .WINDOWS {
    wglSwapIntervalEXT(xx enable);
  }
}

main :: () {
  #if OS == .WINDOWS {
    #import "Windows";
    timeBeginPeriod(1);
  }

  window_width  := 1280;
  window_height := 720;
  window := WC.create_window(window_width, window_height, "Bean Game");
  Simp.set_render_target(window);
  set_vsync(true);

  should_run := true;
  start := seconds_since_init();
  gs := bg_init(window_width, window_height, xx start);

  input_move_left, input_move_right, input_jump: bool;
  while should_run {
    reset_temporary_storage();
    Input.update_window_events();

    {
      for Input.events_this_frame {
        if it.type == .QUIT should_run = false;

        if it.type == .KEYBOARD {
          if it.key_code == #char "A" || it.key_code == .ARROW_LEFT  then input_move_left  = xx it.key_pressed;
          if it.key_code == #char "D" || it.key_code == .ARROW_RIGHT then input_move_right = xx it.key_pressed;
          if it.key_code == .SPACEBAR || it.key_code == .ARROW_UP || it.key_code == #char "W" then input_jump = xx it.key_pressed;
          if it.key_code == #char "R" && it.key_pressed {
            //player.pos = map.player_spawn;
            //player.vel = {};
          }
        }
      }

      for Input.get_window_resizes() {
        Simp.update_window(it.window);
        window_width = it.width;
        window_height = it.height;
        //font = load_fonts(window_height / FONT_HEIGHT_DIVISOR);
      }

      #if ENABLE_LEVEL_HOT_RELOAD {
        modtime, _, success := file_modtime_and_size(map.path);
        if modtime > map.last_modification {
          // TODO: This is very naive and should definitely be optimized in the near future
          log("Reloading level...");
          new_map := map_load(texture_atlas,, temp);
          if new_map.tiles.data != null {
            Arena.reset(*level_arena_pool, overwrite_memory=true);
            map = map_load(texture_atlas, modtime,, level_arena_allocator);
            map.cam.offset = {RESOLUTION_W/2, RESOLUTION_H/2};
            map.cam.offset -= player.bbox.zw / 2;
            player = *map.player;
            cam_set_target(*map.cam, player);
          } else map.last_modification = modtime;
        }
      }
    }

    end := seconds_since_init();
    dt := cast(float, end - start);
    //dt = min(dt, DT_MAX);
    start = end;

    dir: float;
    if input_move_left  dir -= 1;
    if input_move_right dir += 1;
    input := Input_Package.{dir, input_jump};

    bg_tick(gs, input, dt);
  }
}

#import "Basic";
WC    :: #import "Window_Creation";
Input :: #import "Input";
Simp  :: #import "Simp";