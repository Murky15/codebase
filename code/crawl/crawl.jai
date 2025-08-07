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

MAX_ANIMATION_FRAMES :: 4;
Animation :: struct {
  frames: [] Atlas_Index;
  width, height: float;
  seconds_per_frame: float;
}

Texture_Registry :: struct {
  texture:      Simp.Texture;
  environment:  Table(Environment_Texture_Key,Sprite);
  characters:   Table(string,Animation);
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

Cardinal_Dir :: enum_flags {
  NORTH;
  EAST;
  SOUTH;
  WEST;
}

Entity :: struct {
  using pos: Vector3;
  facing: Cardinal_Dir;
  tweaks: Entity_Tweaks;
  anim: Animation;
}

Map :: struct {
  width, height: int;
  data: [] Environment_Texture_Key;
}

WINDOW_WIDTH :: 1280;
WINDOW_HEIGHT :: 720;

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

MOVE_SPEED :: 500;
SPRITE_SCALE :: 3;
TILE_SCALE :: 3;

arena: Flat_Pool;
textures: Texture_Registry;
view_matrix: Matrix4;

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

draw_map :: (map: Map) {
  GRID_LEEWAY :: 2;
  for j: 0..map.height-1{
    for i: 0..map.width-1 {
      x,y,z: float;
      // Draw floor
      key := map.data[j*map.width+i];
      success, texcoords := table_find_new(*textures.environment, key);
      if success {
        x = cast(float)i * texcoords.width * TILE_SCALE;
        y = cast(float)j * texcoords.height * TILE_SCALE;
        z = 0.0;

        p0 := view_matrix * xyzw(x,y,z,1);
        p1 := view_matrix * xyzw(x+texcoords.width*TILE_SCALE,y,z,1);
        p2 := view_matrix * xyzw(x+texcoords.width*TILE_SCALE,y+texcoords.height*TILE_SCALE,z,1);
        p3 := view_matrix * xyzw(x,y+texcoords.height*TILE_SCALE,z,1);

        Simp.immediate_quad(p0.xyz,p1.xyz,p2.xyz,p3.xyz, .{1,1,1,1}, texcoords.uv0, texcoords.uv1, texcoords.uv2, texcoords.uv3);
      }

      success, texcoords = table_find_new(*textures.environment, .WALL_MID);
      if success {
        if i == map.width-1 {
          p0 := view_matrix * xyzw(x+texcoords.width*TILE_SCALE,y,z,1);
          p1 := view_matrix * xyzw(x+texcoords.width*TILE_SCALE,y,z-texcoords.height*TILE_SCALE,1);
          p2 := view_matrix * xyzw(x+texcoords.width*TILE_SCALE,y+texcoords.height*TILE_SCALE,z-texcoords.height*TILE_SCALE,1);
          p3 := view_matrix * xyzw(x+texcoords.width*TILE_SCALE,y+texcoords.height*TILE_SCALE,z,1);
          Simp.immediate_quad(p0.xyz,p1.xyz,p2.xyz,p3.xyz,.{1,1,1,1}, texcoords.uv1, texcoords.uv2, texcoords.uv3, texcoords.uv0);
        }

        if j == map.height-1 {
          p0 := view_matrix * xyzw(x,y+texcoords.height*TILE_SCALE,z,1);
          p1 := view_matrix * xyzw(x,y+texcoords.height*TILE_SCALE,z-texcoords.height*TILE_SCALE,1);
          p2 := view_matrix * xyzw(x+texcoords.width*TILE_SCALE,y+texcoords.height*TILE_SCALE,z-texcoords.height*TILE_SCALE,1);
          p3 := view_matrix * xyzw(x+texcoords.width*TILE_SCALE,y+texcoords.height*TILE_SCALE,z,1);
          Simp.immediate_quad(p0.xyz,p1.xyz,p2.xyz,p3.xyz,.{1,1,1,1}, texcoords.uv0, texcoords.uv3, texcoords.uv2, texcoords.uv1);
        }
      }
    }
  }
}

draw_entity :: (e: Entity) {
  anim := e.anim;
  frame := anim.frames[0];
  angle := cast(float)seconds_since_init() * 100; // Calculate this from entity dir
  rotation: Quaternion;
  set_from_axis_and_angle(*rotation, .{0,0,1}, angle * (PI/180.0));
  rot_matrix := rotation_matrix(Matrix4, rotation);
  p0 := view_matrix * rot_matrix * xyzw(e.x,e.y,e.z,1);
  p1 := view_matrix * rot_matrix * xyzw(e.x+anim.width*SPRITE_SCALE,e.y,e.z,1);
  p2 := view_matrix * rot_matrix * xyzw(e.x+anim.width*SPRITE_SCALE,e.y,e.z+anim.height*SPRITE_SCALE,1);
  p3 := view_matrix * rot_matrix * xyzw(e.x,e.y,e.z+anim.height*SPRITE_SCALE,1);

  Simp.immediate_quad(p0.xyz,p1.xyz,p2.xyz,p3.xyz,.{1,1,1,1},frame.uv3,frame.uv2,frame.uv1,frame.uv0);
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
  player.anim = anim;

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

    update_camera_tracking(*cam);

    view_matrix = make_translation_matrix4(.{cast(float)render_width/2, cast(float) render_height/2, 0}) * make_look_at_matrix(cam.pos, cam.target, .{0,0,1}, false);
    Simp.clear_render_target(.15, .15, .2, 0);
    Simp.set_shader_for_images(*textures.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    Simp.immediate_begin();
    {
      draw_map(TEST_MAP);
      draw_entity(player);
    }
    Simp.swap_buffers(window);
  }
}

#import "Basic";
#import "Reflection";
#import "Math";
#import "String";
#import "Flat_Pool";
#import "Hash_Table";
#import "Window_Creation";
#import "GL";
Input :: #import "Input";
Simp  :: #import "Simp";