/* NOTE -- About this project
Today is valentine's day, and my girlfriend is on a class trip to Italy.
She said she wanted a handmade gift, so I have one week to make this game
for her before she gets back.
*/

/* TODO
- [X] Levels stored as text files
- [ ] Custom for expansion for looping over map
- [ ] Correctly handling window resizes
- [X] Prevent bunnyhopping
- [ ] Physics system
- [ ] Fixed frame rate
*/

AARect :: Vector4;

Entity :: struct {
  gen: u64;

  dir: float;
  pos: Vector2;
  bbox_dim := Vector2.{TILE_RESOLUTION, TILE_RESOLUTION};
  velocity: Vector2;
  mass: float;

  jumping: bool;
  can_jump: bool;

  texture: *Simp.Texture;

  #overlay (pos) bbox: AARect;
}

Entity_Handle :: struct {
  e: *Entity;
  gen: u64;
}

Camera :: struct {
  tracking: Entity_Handle;
  pos: Vector2;
  offset: Vector2;
}

Tile_Type :: enum {
  INVALID;
  EMPTY;
  GROUND;
}

Map :: struct {
  tiles: [] Tile_Type;
  width, height: int;
}

Force :: struct {
  magnitude: float;
  duration_seconds: float;
  time_counter: float;
}

operator > :: (a: Vector2, b: Vector2) -> bool {
  return a.x > b.x && a.y > b.y;
}

floor :: (v: Vector2) -> Vector2 {
  return {floor(v.x), floor(v.y)};
}

rects_intersect :: (a: Vector4, b: Vector4) -> bool, AARect {
  fulla := a.xy + a.zw;
  fullb := b.xy + b.zw;
  is_intersecting := (fulla > b.xy) && (fullb > a.xy);
  overlap: AARect;
  if is_intersecting {
    ip := max(a.xy, b.xy);
    ib := min(fulla, fullb);
    overlap = {xy = ip, zw = ib - ip};
  }

  return is_intersecting, overlap;
}

map_load :: () -> Map {
  map_path :: "W:/code/bean_game/level.bgdat"; // TODO: Should be provided by metaprogram
  raw_map, load_success := read_entire_file(map_path);
  assert(load_success, "Error reading level data!");
  rows := split(raw_map, #char "\n");
  pitch := rows[0].count;
  for rows {
    assert(it.count == pitch, "Error parsing level data on row %. Expected a line with % characters, recieved %.",
      it_index+1, pitch, it.count);
  }

  map: Map;
  map.width  = pitch;
  map.height = rows.count;
  map.tiles = NewArray(map.width * map.height, Tile_Type);
  for y: 0..rows.count-1 {
    for x: 0..pitch-1 {
      // Map data in memory is flipped vertically from file data
      c := raw_map.data[((rows.count-1)-y) * (pitch+1) + x];
      tile: Tile_Type;
      if c == {
        case #char "#"; tile = .EMPTY;
        case #char "g"; tile = .GROUND;
        case; assert(false, "Error parsing level data on row %. Unrecognized map symbol!", rows.count-y);
      }
      map.tiles[y * pitch + x] = tile;
    }
  }

  return map;
}

map_index :: (map: Map, p: Vector2) -> Tile_Type {
  if (p.y < 0 || p.y >= xx map.height) ||
     (p.x < 0 || p.x >= xx map.width) return .INVALID;

  return map.tiles[xx p.y * map.width + xx p.x];
}

make_handle :: (e: *Entity) -> Entity_Handle {
  return {e, e.gen};
}

get_entity :: (h: Entity_Handle) -> *Entity {
  e: *Entity;
  if h.gen == h.e.gen then e = h.e;
  return e;
}

cam_set_target :: (cam: *Camera, e: *Entity) {
  handle := make_handle(e);
  cam.tracking = handle;
}

cam_update_tracking :: (cam: *Camera, dt: float) {
  target := get_entity(cam.tracking);
  t := clamp(4.5 * dt, 0, 1);
  cam.pos = lerp(cam.pos, target.pos - cam.offset, t);
}

Texture_Atlas :: Table(string, Simp.Texture);
load_textures :: () -> Texture_Atlas {
  #import "File_Utilities";
  art_path :: "W:/assets/bean_game/sprites"; // TODO: Should be provided by metaprogram
  texture_files := file_list(art_path);
  atlas: Texture_Atlas;
  init(*atlas, texture_files.count);
  for texture_files {
    new_texture: Simp.Texture;
    tex_load_result := Simp.texture_load_from_file(*new_texture, it);
    assert(tex_load_result, "Error loading texture data!");
    _, key, _ := path_decomp(it);
    table_add(*atlas, key, new_texture);
  }

  return atlas;
}

sign :: (x: $T) -> T {
  if x == 0 return 0;
  return abs(x)/x;
}

TILE_NATIVE_RESOLUTION :: 64;
TILE_RESOLUTION :: TILE_NATIVE_RESOLUTION / 2;
RESOLUTION_W :: 640;
RESOLUTION_H :: 360;
ASPECT_RATIO :: cast(float) RESOLUTION_W / RESOLUTION_H;
PLAYER_MOVE_SPEED :: 350.0;
PLAYER_JUMP_VELOCITY :: 1000.0;
GRAVITY :: -3800.0;

main :: () {
  perm_arena: Arena.Flat_Pool;
  arena_allocator := Allocator.{proc=Arena.flat_pool_allocator_proc, data=*perm_arena};
  context.allocator = arena_allocator;

  main_window := WC.create_window(1280, 720, "Bean Game");
  window_width, window_height := Simp.get_render_dimensions(main_window);
  Simp.set_render_target(main_window);

  // NOTE: For now let's assume that the world is static
  // This is a very simple way to represent the world, and must be refined in the future.

  texture_atlas := load_textures();

  map := map_load();

  player: Entity;
  player.pos = xy(0, 250);
  player.jumping = true;
  player.texture = table_find_pointer(*texture_atlas, "character_roundRed");
  player.mass = 50;

  cam: Camera;
  cam.offset = {RESOLUTION_W/2, RESOLUTION_H/2};
  cam.offset -= player.bbox.zw / 2;
  cam_set_target(*cam, *player);

  grass_texture := table_find_pointer(*texture_atlas, "tile_grass");

  should_run := true;
  input_move_left, input_move_right, input_jump: bool;
  start := seconds_since_init();

  accel_force := Force.{magnitude = 220_000, duration_seconds = 0.1};
  move_force := Force.{magnitude = 75_000};
  friction_force := move_force;

  while should_run {
    reset_temporary_storage();
    Input.update_window_events();

    for Input.events_this_frame {
      if it.type == .QUIT should_run = false;

      if it.type == .KEYBOARD {
        if it.key_code == #char "A" then input_move_left  = xx it.key_pressed;
        if it.key_code == #char "D" then input_move_right = xx it.key_pressed;
        if it.key_code == .SPACEBAR then input_jump       = xx it.key_pressed;
        if it.key_code == #char "R" && it.key_pressed then player.pos = xy(0, 250);
      }
    }

    end := seconds_since_init();
    dt := cast(float, end - start);
    start = end;

    {
      old_player := player;

      if player.can_jump && input_jump && !player.jumping {
        player.jumping = true;
        player.can_jump = false;
        player.velocity.y = PLAYER_JUMP_VELOCITY;
      }

      if player.jumping {
        player.velocity.y += GRAVITY * dt;
      } else if input_jump == false {
        player.can_jump = true;
      }

      dir: float;
      if input_move_left  dir -= 1;
      if input_move_right dir += 1;
      if old_player.dir == 0 && dir != 0 {
        player.dir = dir;
        accel_force.time_counter = 0;
      }

      accel := 0.0;
      if accel_force.time_counter < accel_force.duration_seconds {
        accel += accel_force.magnitude * dir;
        accel_force.time_counter += dt;
      } else {
        accel += move_force.magnitude * dir;
      }
      if sign(player.velocity.x) != sign(player.dir) {
        player.dir = 0;
        player.velocity.x = 0;
      } else {
        accel += friction_force.magnitude * player.dir * -1;
      }
      accel /= player.mass;

      player.velocity.x += accel * dt;
      player.pos += player.velocity * dt;
      player_pos_diff := player.pos - old_player.pos;

      player_grid_pos := floor(player.pos / TILE_RESOLUTION);
      if !player.jumping {
        bottom_left  := map_index(map, player_grid_pos + {y = -1}) == .GROUND;
        bottom_right := map_index(map, player_grid_pos + {1, -1})  == .GROUND;
        floor_below := bottom_left || bottom_right;
        player.jumping = !floor_below;
      }
      for y: 0..1 {
        for x: 0..1 {
          test := player_grid_pos + xy(xx x, xx y);
          potential_collision := map_index(map, test);
          if potential_collision != .GROUND continue;
          tile := AARect.{xy = test * TILE_RESOLUTION, zw = xy(TILE_RESOLUTION)};
          colliding, overlap := rects_intersect(player.bbox, tile);
          if !colliding continue;
          vertical_collision, _ := rects_intersect(old_player.bbox + {y=player_pos_diff.y}, tile);
          if vertical_collision {
            if player.velocity.y < 0 {
              player.pos.y += overlap.w;
              player.jumping = false;
            } else {
              player.pos.y -= overlap.w;
            }
            player.velocity.y = 0;
          } else {
            player.pos.x += -sign(player_pos_diff.x) * overlap.z;
          }
        }
      }

      cam_update_tracking(*cam, dt);
    }

    {
      Simp.clear_render_target(1, 1, 1, 1);
      defer Simp.swap_buffers(main_window);

      Simp.set_shader_for_images(grass_texture);
      Simp.immediate_set_2d_projection(RESOLUTION_W, RESOLUTION_H);
      for y: 0..map.height-1 {
        for x: 0..map.width-1 {
          tile := map_index(map, {xx x, xx y});
          if tile == .GROUND {
            dsp_x := cast(float) x * TILE_RESOLUTION;
            dsp_y := cast(float) y * TILE_RESOLUTION;
            //Simp.immediate_quad((dsp_x + 1) - cam.pos.x, dsp_y - cam.pos.y, dsp_x + TILE_RESOLUTION - cam.pos.x, dsp_y + TILE_RESOLUTION - cam.pos.y, {0, 0, 0, 1});
            p0 := xy(dsp_x, dsp_y) - cam.pos;
            p1 := xy(dsp_x + TILE_RESOLUTION, dsp_y) - cam.pos;
            p2 := p0 + xy(TILE_RESOLUTION, TILE_RESOLUTION);
            p3 := xy(dsp_x, dsp_y + TILE_RESOLUTION) - cam.pos;
            Simp.immediate_quad(p0, p1, p2, p3);
          }
        }
      }
      /*
      Simp.immediate_quad(player.pos.x - cam.pos.x,
        player.pos.y - cam.pos.y,
        player.pos.x + TILE_RESOLUTION - cam.pos.x,
        player.pos.y + TILE_RESOLUTION - cam.pos.y,
        {.5, .5, .5, 1});
      */
      Simp.set_shader_for_images(player.texture);
      p0 := player.pos - cam.pos;
      p1 := xy(player.pos.x + player.bbox_dim.x, player.pos.y) - cam.pos;
      p2 := player.pos + player.bbox_dim - cam.pos;
      p3 := xy(player.pos.x, player.pos.y + player.bbox_dim.y) - cam.pos;
      Simp.immediate_quad(p0, p1, p2, p3);
    }
  }
}

#import "Basic";
#import "Math";
#import "String";
#import "File";
#import "Hash_Table";
WC    :: #import "Window_Creation";
Arena :: #import "Flat_Pool";
Input :: #import "Input";
Simp  :: #import "Simp";