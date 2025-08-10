Environment_Texture_Key :: enum {
  NULL :: 0;
  HOLE;
  FLOOR_LADDER;
  FLOOR_STAIRS;
  FLOOR_1;
  FLOOR_2;
  FLOOR_3;
  FLOOR_4;
  FLOOR_5;
  FLOOR_6;
  FLOOR_7;
  FLOOR_8;
  FLASK_BIG_BLUE;
  FLASK_BIG_RED;
  FLASK_BIG_GREEN;
  FLASK_BIG_YELLOW;
  FLASK_BLUE;
  FLASK_RED;
  FLASK_GREEN;
  FLASK_YELLOW;
  CRATE;
  EDGE_DOWN;
  SKULL;
  UI_HEART_EMPTY;
  UI_HEART_FULL;
  UI_HEART_HALF;
  COLUMN;
  COLUMN_WALL;
  WALL_GOO_BASE;
  WALL_GOO;
  WALL_HOLE_1;
  WALL_HOLE_2;
  WALL_BANNER_BLUE;
  WALL_BANNER_RED;
  WALL_BANNER_GREEN;
  WALL_BANNER_YELLOW;
  WALL_LEFT;
  WALL_MID;
  WALL_RIGHT;
  WALL_TOP_LEFT;
  WALL_TOP_MID;
  WALL_TOP_RIGHT;
  BUTTON_RED_UP;
  BUTTON_RED_DOWN;
  BUTTON_BLUE_UP;
  BUTTON_BLUE_DOWN;
  LEVER_LEFT;
  LEVER_RIGHT;
  DOORS_FRAME_LEFT;
  DOORS_FRAME_RIGHT;
  DOORS_FRAME_TOP;
  DOORS_LEAF_CLOSED;
  DOORS_LEAF_OPEN;
  WALL_EDGE_BOTTOM_LEFT;
  WALL_EDGE_BOTTOM_RIGHT;
  WALL_EDGE_MID_LEFT;
  WALL_EDGE_TOP_LEFT;
  WALL_EDGE_LEFT;
  WALL_EDGE_TOP_RIGHT;
  WALL_EDGE_RIGHT;
  WALL_EDGE_MID_RIGHT;
  WALL_EDGE_TSHAPE_BOTTOM_RIGHT;
  WALL_EDGE_TSHAPE_BOTTOM_LEFT;
  WALL_EDGE_TSHAPE_RIGHT;
  WALL_EDGE_TSHAPE_LEFT;
  WALL_OUTER_FRONT_RIGHT;
  WALL_OUTER_FRONT_LEFT;
  WALL_OUTER_MID_LEFT;
  WALL_OUTER_TOP_LEFT;
  WALL_OUTER_TOP_RIGHT;
  WALL_OUTER_MID_RIGHT;
  WALL_FOUNTAIN_TOP_1;
  WALL_FOUNTAIN_TOP_2;
  WALL_FOUNTAIN_TOP_3;
}

Atlas_Index :: union {
  struct { uv0, uv1, uv2, uv3: Vector2; }
  uv: [4] Vector2;
}

Sprite :: struct {
  using frame: Atlas_Index;
  width, height: float;
}

Animation :: struct {
  active: bool;
  started_at: float;
  seconds_to_complete: float;
}

Transform_Animation :: struct (T: Type) {
  #assert T != string;
  #as using animation: Animation;
  old: T;
  new: T;
  current: T;
}

MAX_ANIMATION_FRAMES :: 4;
Sprite_Animation :: struct {
  #as using animation: Animation;
  frames: [] Atlas_Index;
  width, height: float;
}

Texture_Registry :: struct {
  texture:      Simp.Texture;
  environment:  Table(Environment_Texture_Key,Sprite);
  characters:   Table(string,Sprite_Animation);
  items:        Table(string,Sprite);
}

Camera_Track_Mode :: enum {
  STATIONARY;
  FOLLOW;
}

Camera :: struct {
  tracking: Camera_Track_Mode;
  using pos: Vector3;
  target: *Vector3;
  desired_distance_from_target: Vector3;
  zoom: float;
}

Entity_Tweaks :: enum_flags {
  PLACEHOLDER;
}

Entity :: struct {
  tweaks: Entity_Tweaks;
  using pos: Vector3;
  dir: Vector2;
  sprite_anim: Sprite_Animation;
  rotation_anim: Transform_Animation(float);
}

Map :: struct {
  width, height: int;
  data: [] Environment_Texture_Key;
}


TEST_MAP :: Map.{
  width = 16,
  height = 16,
  data = .[
    4,4,4,1,4,7,4,4,4,4,4,1,4,7,4,4,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    4,7,4,4,4,8,4,4,4,7,4,4,4,8,4,4,
    4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    4,4,7,4,4,4,4,4,4,4,7,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    4,4,10,7,4,4,4,4,4,4,9,7,4,4,4,4,
    4,4,4,1,4,7,4,9,4,4,4,1,4,7,4,4,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    4,7,4,4,4,8,4,4,4,7,4,4,4,8,4,4,
    4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    4,4,7,4,4,4,4,4,4,4,7,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    4,4,11,7,4,4,4,4,4,4,11,7,4,4,4,4
  ]
};
WINDOW_WIDTH :: 1280;
WINDOW_HEIGHT :: 720;
MOVE_SPEED :: 500;
SPRITE_SCALE :: 3;
TILE_SCALE :: 3;

arena: Flat_Pool;
textures: Texture_Registry;

load_textures :: (path_to_folder: string) -> Texture_Registry {
  make_index :: (in: string, texture_dim: Vector2) -> Atlas_Index, float, float #expand {
    x, y, width, height: float;
    success := scan2(in, "% % % %", *x, *y, *width, *height);
    if !success {
      print("Unable to parse UV coordinates when loading texture atlas!");
      break `outer;
    }

    using result: Atlas_Index;
    uv0 = .{x, y};
    uv1 = .{x+width,y};
    uv2 = .{x+width,y+height};
    uv3 = .{x,y+height};
    for * uv {
      if context.simp.coordinate_system == .RIGHT_HANDED {
        it.y = texture_dim.y - it.y;
      }
      it.* /= texture_dim;
    }

    return result, width, height;
  }

  result: Texture_Registry;
  assert(Simp.texture_load_from_file(*result.texture, tprint("%/0x72_DungeonTilesetII_v1.7.png", path_to_folder)));
  texture_dim := Vector2.{xx result.texture.width, xx result.texture.height};

  #import "Text_File_Handler";
  atlas := Text_File_Handler.{do_version_number=false};
  defer deinit(*atlas);

  start_file(*atlas, "Atlas Parser", tprint("%/tile_list_v1.7", path_to_folder));
  line, found := consume_next_line(*atlas);
  if !found return .{};

  left, right := break_by_spaces(line);
  while outer := true {
    if ends_with(left, "f0") {
      name := copy_string(slice(left,0,left.count-3));
      index, width, height := make_index(right, texture_dim);
      frames: [..] Atlas_Index;
      array_add(*frames, index);

      anim := table_add(*result.characters, name, .{width=width, height=height});
      for 1..MAX_ANIMATION_FRAMES-1 {
        line, found = consume_next_line(*atlas);
        if !found break outer;

        left, right = break_by_spaces(line);
        if !ends_with(left, tprint("f%",it)) {
          anim.frames = frames;
          continue outer;
        }
        index, _, _ := make_index(right, texture_dim);
        array_add(*frames, index);
      }
      anim.frames = frames;
    } else if begins_with(left, "weapon") {
      name := copy_string(slice(left,7,left.count));
      index, width, height := make_index(right, texture_dim);
      table_add(*result.items, name, .{index, width, height});
    } else {
      key_names := enum_names(Environment_Texture_Key);
      for name: key_names {
        if to_lower_copy(name,,temp) == left {
          key, success := enum_name_to_value(Environment_Texture_Key, name);
          if !success {
            print("This texture does not have a numeric key!");
            continue outer;
          }
          index, width, height := make_index(right, texture_dim);
          table_add(*result.environment, key, .{index, width, height});
        }
      }
    }

    line, found = consume_next_line(*atlas);
    if !found break;

    left, right = break_by_spaces(line);
  }

  return result;
}

draw_map :: (map: Map, view_matrix: Matrix4) {
  prep_render :: (x: float, y: float, z: float) -> Vector4 #expand {
    return `view_matrix * xyzw(x,y,z,1);
  }
  for j: 0..map.height-1{
    for i: 0..map.width-1 {
      // Draw floor
      key := map.data[j*map.width+i];
      _, texcoords := table_find_new(*textures.environment, key);
      tile_width := texcoords.width * TILE_SCALE;
      tile_height := texcoords.height * TILE_SCALE;
      x := cast(float)i * tile_width;
      y := cast(float)j * tile_height;
      z := 0.0;

      p0 := prep_render(x,y,z);
      p1 := prep_render(x+tile_width,y,z);
      p2 := prep_render(x+tile_width,y+tile_height,z);
      p3 := prep_render(x,y+tile_height,z);

      Simp.immediate_quad(p0.xyz,p1.xyz,p2.xyz,p3.xyz, .{1,1,1,1}, texcoords.uv0, texcoords.uv1, texcoords.uv2, texcoords.uv3);

      _, texcoords = table_find_new(*textures.environment, .WALL_MID);
      if i == map.width-1 {
        p0 := prep_render(x+tile_width,y,z);
        p1 := prep_render(x+tile_width,y,z-tile_height);
        p2 := prep_render(x+tile_width,y+tile_height,z-tile_height);
        p3 := prep_render(x+tile_width,y+tile_height,z);
        Simp.immediate_quad(p0.xyz,p1.xyz,p2.xyz,p3.xyz,.{1,1,1,1}, texcoords.uv1, texcoords.uv2, texcoords.uv3, texcoords.uv0);
      }

      if j == map.height-1 {
        p0 := prep_render(x,y+tile_height,z);
        p1 := prep_render(x,y+tile_height,z-tile_height);
        p2 := prep_render(x+tile_width,y+tile_height,z-tile_height);
        p3 := prep_render(x+tile_width,y+tile_height,z);
        Simp.immediate_quad(p0.xyz,p1.xyz,p2.xyz,p3.xyz,.{1,1,1,1}, texcoords.uv0, texcoords.uv3, texcoords.uv2, texcoords.uv1);
      }
    }
  }
}

draw_entity :: (e: *Entity, view_matrix: Matrix4) {
  prep_render :: (x: float, y: float, z: float) -> Vector4 #expand {
    return `view_matrix * `rot_matrix * xyzw(x,y,z,1);
  }

  anim := e.sprite_anim;
  frame := anim.frames[0];

  sprite_width := anim.width*SPRITE_SCALE;
  sprite_width_rotated := sprite_width/sqrt(2);
  sprite_height := anim.height*SPRITE_SCALE;
  initial_rotation_vector := xy(sprite_width_rotated,sprite_width_rotated);
  pivot := xyz(e.pos.xy+initial_rotation_vector/2,e.z);

  angle_from_initial := get_rotation_angle(e.dir,initial_rotation_vector);
  if is_approximately_zero(fmod_cycling(angle_from_initial, PI)) { // Check if we are trying to move parallel to the view dir
    angle_from_initial -= PI/2;
  }
  angle_from_initial = normalize_angle(angle_from_initial);

  if !values_are_close(angle_from_initial, normalize_angle(e.rotation_anim.new)) {
    e.rotation_anim.old = normalize_angle(e.rotation_anim.current);
    e.rotation_anim.new = e.rotation_anim.old + closest_rotation_on_circle(e.rotation_anim.old, angle_from_initial);
    start_animation(*e.rotation_anim, 0.15);
  }

  angle := update_transform_animation(*e.rotation_anim);
  rotation: Quaternion;
  set_from_axis_and_angle(*rotation, .{0,0,1}, angle);

  // TODO: Optimize! (We can probably bake/cache this somehow)
  rot_matrix := make_translation_matrix4(pivot) * rotation_matrix(Matrix4, rotation) * make_translation_matrix4(-pivot);
  p0 := prep_render(e.x,e.y,e.z);
  p1 := prep_render(e.x+sprite_width_rotated,e.y+sprite_width_rotated,e.z);
  p2 := prep_render(e.x+sprite_width_rotated,e.y+sprite_width_rotated,e.z+sprite_height);
  p3 := prep_render(e.x,e.y,e.z+sprite_height);

  Simp.immediate_quad(p0.xyz,p1.xyz,p2.xyz,p3.xyz,.{1,1,1,1},frame.uv3,frame.uv2,frame.uv1,frame.uv0);
}

get_rotation_angle :: (terminal: Vector2, initial: Vector2 = .{1,0}) -> float {
  return atan2(terminal.y,terminal.x) - atan2(initial.y, initial.x);
}

closest_rotation_on_circle :: (t0: float, t1: float) -> float {
  diff := t1 - t0;
  if diff > PI {
    diff -= 2*PI;
  } else if diff < -PI {
    diff += 2*PI;
  }

  return diff;
}

normalize_angle :: (angle: float) -> float {
  return fmod_cycling(angle,2*PI);
}

normalize :: (min: float, max: float, x: float) -> float {
  result := (x-min)/(max-min);
  return clamp(result, 0, 1);
}

update_camera_tracking :: (cam: *Camera) {
  if cam.tracking == {
    case .STATIONARY; return;
    case .FOLLOW;
      distance_from_target := cam.target.* - cam.pos;
      adjustment := distance_from_target - cam.desired_distance_from_target;
      cam.pos += adjustment;
  }
}

start_animation :: (anim: *$T/Animation, seconds_to_complete: float) {
  anim.active = true;
  anim.started_at = cast(float) seconds_since_init(); // Should this be more precise?
  anim.seconds_to_complete = seconds_to_complete;
}

update_transform_animation :: (anim: *Transform_Animation) -> anim.T {
  if anim.active {
    now := cast(float) seconds_since_init();
    t := normalize(anim.started_at, anim.started_at+anim.seconds_to_complete, now);
    anim.current = lerp(anim.old, anim.new, t);
    if t > 1 {
      anim.active = false;
    }
  }

  return anim.current;
}

main :: () {
  window := create_window(WINDOW_WIDTH, WINDOW_HEIGHT, "Crawl");
  render_width, render_height := Simp.get_render_dimensions(window);
  Simp.set_render_target(window, .RIGHT_HANDED);

  //context.allocator.proc = flat_pool_allocator_proc;
  //context.allocator.data = *arena;

  textures = load_textures("W:/assets/crawl/0x72_DungeonTilesetII_v1.7");

  success, anim := table_find_new(*textures.characters, "wizzard_m_idle_anim");
  if !success {
    print("Unable to locate player animation!\n");
    return;
  }

  player: Entity;
  player.sprite_anim = anim;

  cam: Camera;
  cam.tracking = .FOLLOW;
  cam.pos = #run xyz(1/sqrt(2),1/sqrt(2),sin(atan(1/sqrt(2))));
  cam.desired_distance_from_target = cam.pos;
  cam.target = *player.pos;

  move_up, move_down, move_left, move_right: bool;
  ts1 := seconds_since_init();

  running := true;
  while running {
    reset_temporary_storage();
    Input.update_window_events();
    for e: Input.events_this_frame {
      if e.type == {
        case .QUIT; running = false;
        case .KEYBOARD;
          key_pressed := cast(bool) e.key_pressed;
          if e.key_code == #char "W" move_up    = key_pressed;
          if e.key_code == #char "A" move_left  = key_pressed;
          if e.key_code == #char "S" move_down  = key_pressed;
          if e.key_code == #char "D" move_right = key_pressed;
      }
    }

    ts2 := seconds_since_init();
    dt := cast(float) (ts2 - ts1);
    ts1 = ts2;

    move_dir: Vector2;
    if move_up    move_dir.y -= 1;
    if move_down  move_dir.y += 1;
    if move_left  move_dir.x -= 1;
    if move_right move_dir.x += 1;
    if length(move_dir) > 1 normalize(*move_dir);
    player.pos += xyz(move_dir,0) * MOVE_SPEED * dt;
    if move_dir != .{0,0}  {
      player.dir = move_dir;
    }
    update_camera_tracking(*cam);

    view_matrix := make_translation_matrix4(.{cast(float)render_width/2, cast(float) render_height/2, 0}) * make_look_at_matrix(cam.pos, cam.target, .{0,0,1}, false);
    Simp.clear_render_target(.15, .15, .2, 0);
    Simp.set_shader_for_images(*textures.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    Simp.immediate_begin();
    {
      draw_map(TEST_MAP, view_matrix);
      draw_entity(*player, view_matrix);
    }
    Simp.swap_buffers(window);
  }
}

#import "Basic";
#import "Reflection";
#import "Math";
#import "Sloppy_Math";
#import "String";
#import "Flat_Pool";
#import "Hash_Table";
#import "Window_Creation";
#import "GL";
Input :: #import "Input";
Simp  :: #import "Simp";