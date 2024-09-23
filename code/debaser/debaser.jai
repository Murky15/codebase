#import "Basic";
#import "Window_Creation";
#import "Math";
#import "Flat_Pool";
Input :: #import "Input";
Simp  :: #import "Simp";

Tile :: struct {
  pos: Vector2;
  z_height: float;
}
TILE_SIZE :: #run Vector2.{32, 32} * 3;
tile_texture: Simp.Texture;
tiles: [100] Tile;

ISO_TRANSFORM :: Matrix2.{
  _11 = 1,     _12 = -1,
  _21 = 0.5,   _22 = 0.5
};
INVERSE_ISO_TRANSFORM :: #run inverse(ISO_TRANSFORM);

// @todo: Obviously clean this up
ASSET_DIR :: "W:/art/debaser";

should_quit   : bool = false;
main_window   : Window_Type;
window_width  : s32  = 1280;
window_height : s32  = 720;

time_first: float64;
view_offset: Vector2;
key_up, key_down, key_left, key_right: u32;

init :: () {
  main_window = create_window(window_width, window_height, "My little game");
  window_width, window_height = Simp.get_render_dimensions(main_window);
  Simp.set_render_target(main_window, .LEFT_HANDED);
}

simulate :: () {
  last := seconds_since_init();
  delta := last - time_first;
  dt := cast(float) delta;
  time_first = last;

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

    if it.type == .KEYBOARD {
      if it.key_code == {
        case .ARROW_UP;    key_up    = it.key_pressed;
        case .ARROW_DOWN;  key_down  = it.key_pressed;
        case .ARROW_LEFT;  key_left  = it.key_pressed;
        case .ARROW_RIGHT; key_right = it.key_pressed;
      }
    }
  }

  // @todo: There's gotta be a way to make this shorter
  new_cam_dir: Vector2;
  if key_up    then new_cam_dir.y += 1;
  if key_down  then new_cam_dir.y -= 1;
  if key_left  then new_cam_dir.x += 1;
  if key_right then new_cam_dir.x -= 1;
  if length(new_cam_dir) > 1.0 then new_cam_dir = normalize(new_cam_dir);

  view_offset += new_cam_dir * 200 * dt;
}

render :: () {
  cr, cg, cb, ca := rgb_decimal_to_float(135, 206, 235, 255);
  Simp.clear_render_target(cr, cg, cb, ca);

  Simp.set_shader_for_images(*tile_texture);
  for * tiles {
    it.z_height = 15.0 * cast(float) sin(it_index * tiles.count + 2.0 * seconds_since_init());
    draw(it);
  }

  Simp.swap_buffers(main_window);
}

main :: () {
  init();

  Simp.texture_load_from_file(*tile_texture, tprint("%/iso_tiles/tile_000.png", ASSET_DIR));

  // Spawn tiles
  axis := cast(int)sqrrt(tiles.count);
  for y: 0..axis-1 {
    for x: 0..axis-1 {
      tiles[y * axis + x].pos = .{xx x, xx y};
    }
  }

  time_first = seconds_since_init();
  while !should_quit {
    reset_temporary_storage();

    simulate();
    render();
  }
}

rgb_decimal_to_float :: (r: float, g: float, b: float, a: float) -> float, float, float, float {
  return r / 255, g / 255, b / 255, a / 255;
}

View :: struct {
  using pos: Vector2;
}

draw :: (tile: Tile) {
  iso_pos := ISO_TRANSFORM * tile.pos * (TILE_SIZE / 2);
  c := offset_origin(iso_pos);
  c += view_offset;

  c.y += tile.z_height;
  Simp.immediate_quad(
    c.x,
    c.y,
    c.x + TILE_SIZE.x,
    c.y + TILE_SIZE.y,
    .{1,1,1,1});
}

offset_origin :: (p: Vector2) -> Vector2 {
  origin_offset := Vector2.{xx window_width / 2.0, xx window_height / 2.0};
  result: Vector2;
  result.x = (p.x + origin_offset.x) - TILE_SIZE.x / 2;
  result.y = p.y + origin_offset.y;

  return result;
}

inverse :: (m: Matrix2) -> Matrix2 {
  return .{
    _11 = m._22, _12 = -m._12,
    _21 = -m._21, _22 = m._11
  };
}
