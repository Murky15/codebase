Tile :: struct {
  pos: Vector2;
  z_height: float;
}
tile_size :: #run Vector2.{32, 32} * 3;

// @todo: Obviously clean this up
ASSET_DIR :: "W:/art/debaser";

iso_transform :: Matrix2.{
  _11 = 1,     _12 = -1,
  _21 = 0.5,   _22 = 0.5
};
inverse_iso_transform :: #run inverse(iso_transform);

should_quit   : bool = false;
window_width  : s32  = 1280;
window_height : s32  = 720;

main :: () {
  main_window := create_window(window_width, window_height, "My little game");
  window_width, window_height = Simp.get_render_dimensions(main_window);
  Simp.set_render_target(main_window, .LEFT_HANDED);

  tile_texture: Simp.Texture;
  Simp.texture_load_from_file(*tile_texture, tprint("%/iso_tiles/tile_023.png", ASSET_DIR));

  // Spawn tiles
  tiles: [100] Tile;
  axis := cast(int)sqrt(tiles.count);
  for y: 0..axis-1 {
    for x: 0..axis-1 {
      tiles[y * axis + x].pos = iso_transform * .{xx x, xx y} * (tile_size / 2);
    }
  }

  first := seconds_since_init();
  while !should_quit {
    reset_temporary_storage();

    Input.update_window_events();
    for Input.get_window_resizes() {
      Simp.update_window(it.window);

      if it.window == main_window {
        window_width = it.width;
        window_height = it.height;
      }
    }
    for Input.events_this_frame {
      if it.type == .QUIT then should_quit = true;
    }

    last := seconds_since_init();
    delta := last - first;
    dt := cast(float) delta;
    first = last;

    cr, cg, cb, ca := rgb_decimal_to_float(135, 206, 235, 255);
    Simp.clear_render_target(cr, cg, cb, ca);

    Simp.set_shader_for_images(*tile_texture);
    for * tiles {
      it.z_height = 10.0 * cast(float) sin(it_index * tiles.count + 2.0 * seconds_since_init());

      c := offset_coordinate(it.pos);
      c.y += it.z_height;
      Simp.immediate_quad(c.x, c.y, c.x + tile_size.x, c.y + tile_size.y, .{1, 1, 1, 1});
    }

    Simp.swap_buffers(main_window);
  }
}

offset_coordinate :: (p: Vector2) -> Vector2 {
  origin_offset := Vector2.{xx window_width / 2.0, xx window_height / 2.0};
  result: Vector2;
  result.x = (p.x + origin_offset.x) - tile_size.x / 2;
  result.y = p.y + origin_offset.y;

  return result;
}

inverse :: (m: Matrix2) -> Matrix2 {
  return .{
    _11 = m._22, _12 = -m._12,
    _21 = -m._21, _22 = m._11
  };
}

rgb_decimal_to_float :: (r: float, g: float, b: float, a: float) -> float, float, float, float {
  return r / 255, g / 255, b / 255, a / 255;
}

#import "Basic";
#import "Window_Creation";
#import "Math";
Input :: #import "Input";
Simp  :: #import "Simp";