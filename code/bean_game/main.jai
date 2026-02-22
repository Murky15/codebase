/* NOTE -- About this project
Today is valentine's day, and my girlfriend is on a class trip to Italy.
She said she wanted a handmade gift, so I have one week to make this game
for her before she gets back.
*/

/* TODO
- [X] Levels stored as text files
- [X] Custom for expansion for looping over map
- [X] Correctly handling window resizes
- [X] Prevent bunnyhopping
- [X] Physics system
- [X] Hot reloading levels
- [X] Bounce pads
- [X] Coins
- [ ] Wall jumping / friction
- [ ] Grappling hook
- [ ] Flip animation like paper mario
- [X] Autotiling
- [ ] Parallax backgrounds
- [ ] Multi-Core by default (maybe)
- [ ] Hands and weapons
- [X] Debug messages and font display (Custom logger that writes messages to the screen!)
- [ ] Occlusion culling
*/

ENABLE_LEVEL_HOT_RELOAD :: true;

AARect :: #type,isa Vector4;

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
  PLAYER_SPAWN;
  BOUNCE_PAD;
  COIN;
}

Tile :: struct {
  type: Tile_Type;
  tex: *Simp.Texture;
  bbox_dim := Vector2.{TILE_RESOLUTION, TILE_RESOLUTION};

  coin_bob_counter: float;
  collected: bool;
}

Map :: struct {
  version: int;
  name: string;
  author: string;
  description: string;

  #if ENABLE_LEVEL_HOT_RELOAD {
    path: string;
    last_modification: Apollo_Time;
  }

  tiles: [] Tile;
  width, height: int;
  player_spawn: Vector2;
  coins_collected: int;
}

for_expansion :: (map: Map, body: Code, flags: For_Flags) #expand {
  #assert !(flags & .REVERSE);
  if map.tiles.data != null {
    for y: 0..map.height-1 {
      for x: 0..map.width-1 {
        index := y * map.width + x;
        #if flags & .POINTER {
          `it := *map.tiles[index];
        } else {
          `it := map.tiles[index];
        }
        `it_index := Vector2.{xx x, xx y};

        #insert (break=break y) body;
      }
    }
  }
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

map_load :: (textures: Texture_Atlas, last_modification := APOLLO_TIME_INVALID) -> Map {
  map_path :: "W:/code/bean_game/level.bgdat"; // TODO: Should be provided by metaprogram
  rows_in_header :: 4;
  tis_map :: cast(*Type_Info_Struct) type_info(Map);

  map: Map;
  raw_map, load_success := read_entire_file(map_path,, temp);
  if !load_success {
    log("Error reading level data!");
    return {};
  }
  #if ENABLE_LEVEL_HOT_RELOAD {
    map.path = map_path;
    map.last_modification = last_modification;
  }

  rows := split(raw_map, #char "\n");
  for 0..rows_in_header-1 {
    meta_tag := rows[it];
    success, tag, value := split_from_left(meta_tag, #char ":");
    if !success {
      log("Error parsing level header on row %.", it+1);
      return {};
    }
    to_lower_in_place(tag);
    value = eat_spaces(value);
    for tis_map.members {
      if tag != it.name continue;
      if it.type.type == .STRING {
        value_cpy := copy_string(value);
        memcpy(cast(*void)*map + it.offset_in_bytes, cast(*void)*value_cpy, it.type.runtime_size);
      } else if it.type.type == .INTEGER {
        integer_value := to_integer(value);
        memcpy(cast(*void)*map + it.offset_in_bytes, cast(*void)*integer_value, it.type.runtime_size);
      }
    }
    raw_map.count -= meta_tag.count+1;
    raw_map.data += meta_tag.count+1;
  }
  rows.count -= rows_in_header;
  rows.data += rows_in_header;

  pitch := rows[0].count;
  for rows {
    if it.count != pitch {
      log("Error parsing level data on row %. Expected a line with % characters, recieved %.", it_index+rows_in_header+1, pitch, it.count);
      return {};
    }
  }

  map.width  = pitch;
  map.height = rows.count;
  map.tiles = NewArray(map.width * map.height, Tile);
  for * map {
    // Map data in memory is flipped vertically from file data
    c := raw_map.data[((rows.count-1)- xx it_index.y) * (pitch+1) + xx it_index.x];
    type := Tile_Type.EMPTY;
    if c == {
      case #char "#"; type = .EMPTY;
      case #char "g"; type = .GROUND;
      case #char "b"; type = .BOUNCE_PAD;
      case #char "c"; type = .COIN;
      case #char "p"; {
        type = .PLAYER_SPAWN;
        map.player_spawn = it_index * TILE_RESOLUTION;
      }
      case; {
        log("Error parsing level data on row %. Unrecognized map symbol: %", (rows.count+rows_in_header)-it_index.y, to_string(*c,1));
        return {};
      }
    }

    it.type = type;
  }

  // NOTE: Texture pass
  Autotile :: struct {
    tex: *Simp.Texture;
    probability: float;
  }
  atile :: (tex_name: string, probability := 100.0) -> Autotile #expand {
    return {table_find_pointer(*`textures, tex_name), probability};
  }

  above_ground_tiles := Autotile.[atile("tile_grass"), atile("tile_top")];
  below_ground_tiles := Autotile.[atile("tile", 50), atile("tile_diagonal", 50.0/3), atile("tile_stone", 50.0/3), atile("tile_brick", 50.0/3)];
  bounce_pad_texture := table_find_pointer(*textures, "tile_belt");
  // TODO: A lot of textures have alternate counterparts (coins/gems spikes/cogs)
  // Should we autotile these or keep them as seperate entities?
  coin_texture := table_find_pointer(*textures, "tile_coin");
  for * map {
    if it.type == {
      case .GROUND; {
        above := map_index(map, it_index + {0, 1}).type;
        below := map_index(map, it_index + {0,-1}).type;
        it.tex = ifx above != .GROUND {
          ifx below != .GROUND then above_ground_tiles[1].tex else above_ground_tiles[0].tex;
        } else {
          rand := cast(float) (random_get() % 100) + 1;
          tex: *Simp.Texture;
          for tile: below_ground_tiles {
            rand -= tile.probability;
            if rand < 0 {
              tex = tile.tex;
              break;
            }
          }

          ifx rand > 0 then below_ground_tiles[0].tex else tex;
        }
      }
      case .BOUNCE_PAD; {
        it.tex = bounce_pad_texture;
        it.bbox_dim = {TILE_RESOLUTION, TILE_RESOLUTION/3};
      }
      case .COIN; it.tex = coin_texture;
    }
  }

  return map;
}

map_index_pointer :: (map: Map, p: Vector2) -> *Tile {
  if (p.y < 0 || p.y >= xx map.height) ||
     (p.x < 0 || p.x >= xx map.width) return null;

  return *map.tiles[xx p.y * map.width + xx p.x];
}

map_index :: (map: Map, p: Vector2) -> Tile {
  tile := inline map_index_pointer(map, p);
  return ifx tile == null then Tile.{type=.INVALID} else tile.*;
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

cam_update_tracking :: (cam: *Camera, dt: float, attachment := 4.5) {
  target := get_entity(cam.tracking);
  t := clamp(attachment * dt, 0, 1);
  cam.pos = lerp(cam.pos, target.pos - cam.offset, t);
}

Texture_Atlas :: Table(string, Simp.Texture);
load_assets :: (font_height_px: int) -> Texture_Atlas, *Simp.Dynamic_Font {
  base_asset_path :: "W:/assets/bean_game";
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

  return atlas, Simp.get_font_at_size(base_asset_path, "font.ttf", font_height_px);
}

sign :: inline (x: $T) -> T {
  if x == 0 return 0;
  return abs(x)/x;
}

norm :: inline (x: float, min: float, max: float) -> float {
  return (x-min)/(max-min);
}

get_digits_right_to_left_on_temp :: (x: int) -> [] int {
  push_allocator(temp);
  digits: [..] int;
  if x == 0 then array_add(*digits, 0);
  while x > 0 {
    digit := x % 10;
    array_add(*digits, digit);
    x /= 10;
  }

  return digits;
}

TILE_NATIVE_RESOLUTION :: 64;
TILE_RESOLUTION :: TILE_NATIVE_RESOLUTION / 2;
RESOLUTION_W :: 640;
RESOLUTION_H :: 360;
ASPECT_RATIO :: cast(float) RESOLUTION_W / RESOLUTION_H;
FONT_HEIGHT_PX :: RESOLUTION_H / 12;

PLAYER_MOVE_SPEED :: 350.0;
PLAYER_JUMP_VELOCITY :: 1000.0;
PLAYER_BOUNCE_PAD_JUMP_VELOCITY :: 1500.0;
GRAVITY :: -3800.0;

set_texture :: (tex: *Simp.Texture) {
  Simp.set_shader_for_images(tex);
  Simp.immediate_set_2d_projection(RESOLUTION_W, RESOLUTION_H);
}

make_quad :: (rect: AARect) -> Vector2, Vector2, Vector2, Vector2 {
  p0 := rect.xy;
  p1 := xy(rect.x + rect.z, rect.y);
  p2 := rect.xy + rect.zw;
  p3 := xy(rect.x, rect.y + rect.w);

  return p0, p1, p2, p3;
}

main :: () {
  level_arena_pool: Arena.Flat_Pool;
  level_arena_allocator := Allocator.{proc=Arena.flat_pool_allocator_proc, data=*level_arena_pool};
  bg_logger_init_and_equip();

  main_window := WC.create_window(1280, 720, "Bean Game");
  Simp.set_render_target(main_window);

  texture_atlas, font := load_assets(FONT_HEIGHT_PX);
  coin_texture := table_find_pointer(*texture_atlas, "tile_coin"); // Duplicate
  x_texture := table_find_pointer(*texture_atlas, "ui_numX");
  number_textures := *Simp.Texture.[table_find_pointer(*texture_atlas, "ui_num0"),
                                   table_find_pointer(*texture_atlas, "ui_num1"),
                                   table_find_pointer(*texture_atlas, "ui_num2"),
                                   table_find_pointer(*texture_atlas, "ui_num3"),
                                   table_find_pointer(*texture_atlas, "ui_num4"),
                                   table_find_pointer(*texture_atlas, "ui_num5"),
                                   table_find_pointer(*texture_atlas, "ui_num6"),
                                   table_find_pointer(*texture_atlas, "ui_num7"),
                                   table_find_pointer(*texture_atlas, "ui_num8"),
                                   table_find_pointer(*texture_atlas, "ui_num9")];
  should_run := true;
  input_move_left, input_move_right, input_jump: bool;
  start := seconds_since_init();
  // NOTE: For now let's assume that the world is static
  // This is a very simple way to represent the world, and must be refined in the future.
  map := map_load(texture_atlas, to_apollo(start),, level_arena_allocator);

  player: Entity;
  player.pos = map.player_spawn;
  player.jumping = true;
  player.texture = table_find_pointer(*texture_atlas, "character_roundRed");
  player.mass = 50;

  cam: Camera;
  cam.offset = {RESOLUTION_W/2, RESOLUTION_H/2};
  cam.offset -= player.bbox.zw / 2;
  cam_set_target(*cam, *player);

  // NOTE: Should we also have a special "pivot" force?
  friction_coefficient :: 2.0;
  friction_force := Force.{magnitude = -1 * GRAVITY * player.mass * friction_coefficient};
  move_force := friction_force;
  accel_force: Force;
  accel_force.duration_seconds = 0.18;
  accel_force.magnitude = (player.mass * (PLAYER_MOVE_SPEED / accel_force.duration_seconds)) + friction_force.magnitude;

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
            player.pos = map.player_spawn;
            player.velocity = {};
          }
        }
      }

      for Input.get_window_resizes() Simp.update_window(it.window);

      #if ENABLE_LEVEL_HOT_RELOAD {
        modtime, _, success := file_modtime_and_size(map.path);
        if modtime > map.last_modification {
          // TODO: This is very naive and should definitely be optimized in the near future
          log("Reloading level...");
          new_map := map_load(texture_atlas,, temp);
          if new_map.tiles.data != null {
            Arena.reset(*level_arena_pool, overwrite_memory=true);
            map = map_load(texture_atlas, modtime,, level_arena_allocator);
          } else map.last_modification = modtime;
        }
      }
    }

    end := seconds_since_init();
    dt := cast(float, end - start);
    start = end;

    {
      old_player := player;

      if !player.jumping {
        if !input_jump {
          player.can_jump = true;
        } else if player.can_jump {
          player.jumping = true;
          player.can_jump = false;
          player.velocity.y = PLAYER_JUMP_VELOCITY;
        }
      }

      dir: float;
      if input_move_left  dir -= 1;
      if input_move_right dir += 1;
      if old_player.dir == 0 && dir != 0 {
        player.dir = dir;
        accel_force.time_counter = 0;
      }

      accel: Vector2;
      if accel_force.time_counter < accel_force.duration_seconds {
        accel.x += accel_force.magnitude * dir;
        accel_force.time_counter += dt;
      } else if sign(dir) == sign(player.dir) {
        // TODO: There is a bug here where if we release the move key (dir == 0) and then press it again
        // before we fully deccelerate we resume moving at a much lower speed. This feels very janky and
        // needs to be fixed.
        accel.x += move_force.magnitude * dir;
      }
      if sign(player.velocity.x) != sign(player.dir) { // NOTE: Friction has caused us to stop
        player.dir = 0;
        player.velocity.x = 0;
      }
      accel.x += friction_force.magnitude * -sign(player.velocity.x);
      if player.jumping accel.y += GRAVITY * player.mass;
      accel /= player.mass;
      player.velocity += accel * dt;
      player.velocity.x = sign(player.velocity.x) * min(abs(player.velocity.x), PLAYER_MOVE_SPEED);
      player.pos += player.velocity * dt;

      player_pos_diff := player.pos - old_player.pos;
      player_grid_pos := floor(player.pos / TILE_RESOLUTION);
      if !player.jumping {
        bottom_left  := map_index(map, player_grid_pos + {y = -1}).type == .GROUND;
        bottom_right := map_index(map, player_grid_pos + {1, -1}).type  == .GROUND;
        floor_below := bottom_left || bottom_right;
        player.jumping = !floor_below;
      }

      for y: 0..1 {
        for x: 0..1 {
          test := player_grid_pos + xy(xx x, xx y);
          potential_collision := map_index_pointer(map, test);
          tile := AARect.{xy = test * TILE_RESOLUTION, zw = potential_collision.bbox_dim};
          colliding, overlap := rects_intersect(player.bbox, tile);
          if !colliding continue;
          if potential_collision.type == {
            case .GROUND; {
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

            case .BOUNCE_PAD; {
              player.velocity.y = PLAYER_BOUNCE_PAD_JUMP_VELOCITY;
              player.jumping = true;
              player.can_jump = false;
            }

            case .COIN; {
              if !potential_collision.collected {
                potential_collision.collected = true;
                map.coins_collected += 1;
              }
            }
          }
        }
      }

      cam_update_tracking(*cam, dt);
    }

    {
      Simp.clear_render_target(1, 1, 1, 1);
      defer Simp.swap_buffers(main_window);

      for * map {
        if it.tex == null then continue;
        set_texture(it.tex);
        dsp := it_index * TILE_RESOLUTION;
        if it.type == .COIN {
          BOB_AMPLITUDE :: 3.0;
          BOB_FREQUENCY :: PI;
          if it.collected then continue;
          it.coin_bob_counter += dt;
          it.coin_bob_counter = fmod_cycling(it.coin_bob_counter, (2 * PI) / BOB_FREQUENCY);
          dsp.y += 3 * sin(PI * it.coin_bob_counter);
        }
        p0, p1, p2, p3 := make_quad({xy = dsp - cam.pos, zw = xy(TILE_RESOLUTION)});
        Simp.immediate_quad(p0, p1, p2, p3);
      }

      {
        set_texture(player.texture);
        p0, p1, p2, p3 := make_quad({xy = player.pos - cam.pos, zw = player.bbox_dim});
        Simp.immediate_quad(p0, p1, p2, p3);
      }

      {
        scoreboard_x := 0.0;
        SCOREBOARD_Y :: 315;
        SCOREBOARD_RESOLUTION :: TILE_RESOLUTION * 1.5;
        set_texture(coin_texture);
        cp0, cp1, cp2, cp3 := make_quad({xy = xy(scoreboard_x, SCOREBOARD_Y), zw = xy(SCOREBOARD_RESOLUTION)});
        Simp.immediate_quad(cp0, cp1, cp2, cp3);
        scoreboard_x += TILE_RESOLUTION;
        set_texture(x_texture);
        xp0, xp1, xp2, xp3 := make_quad({xy = xy(scoreboard_x, SCOREBOARD_Y), zw = xy(SCOREBOARD_RESOLUTION)});
        Simp.immediate_quad(xp0, xp1, xp2, xp3);
        scoreboard_x += TILE_RESOLUTION;

        coin_digits := get_digits_right_to_left_on_temp(map.coins_collected);
        for < coin_digits {
          set_texture(number_textures[it]);
          dp0, dp1, dp2, dp3 := make_quad({xy = xy(scoreboard_x, SCOREBOARD_Y), zw = xy(SCOREBOARD_RESOLUTION)});
          Simp.immediate_quad(dp0, dp1, dp2, dp3);
          scoreboard_x += TILE_RESOLUTION;
        }
      }

      Simp.set_shader_for_color();
      for * logs_to_display {
        t := norm(it.time_of_log + it.decay, it.time_of_log, it.time_of_log + LOG_MESSAGE_FADE_DURATION);
        alpha := ifx t < 0.5 then 4 * pow(t, 3) else 1 - pow(-2 * t + 2, 3) / 2;
        Simp.draw_text(font, 50, 50 + it_index * FONT_HEIGHT_PX, it.message, {w = 1 - alpha});
        it.decay += dt;
        if it.decay > LOG_MESSAGE_FADE_DURATION {
          free(it.message);
          remove it;
        }
      }
    }
  }
}

LOG_MESSAGE_FADE_DURATION :: 5;
Log_Message :: struct {
  message: string;
  time_of_log: float;
  decay: float;
}

logs_to_display: [..] Log_Message;
bg_logger_init_and_equip :: () {
  context.logger = bg_logger;
}

bg_logger :: (message: string, data: *void, info: Log_Info) {
  if !message then return;
  write_string(message);
  if message[message.count-1] != #char "\n" then write_string("\n");

  new_message: Log_Message;
  new_message.message = copy_string(message,, logs_to_display.allocator);
  new_message.time_of_log = xx seconds_since_init(); // current_time_monotonic?
  array_insert_at(*logs_to_display, new_message, 0);
}

#import "Basic";
#import "Math";
#import "String";
#import "File";
#import "File_Utilities";
#import "Hash_Table";
#import "Random";
WC    :: #import "Window_Creation";
Arena :: #import "Flat_Pool";
Input :: #import "Input";
Simp  :: #import "Simp";