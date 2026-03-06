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
- [X] Player flip direction
- [X] Autotiling
- [X] Parallax backgrounds
- [X] Coroutines
- [ ] Hands
- [ ] Cosmetics
- [X] Debug messages and font display (Custom logger that writes messages to the screen!)
- [X] Occlusion culling
- [ ] Main menu
- [X] Include user data in logger to change message display duration
- [ ] Audio
*/

AARect :: #type,isa Vector4;

Player :: struct {
  dir: float;
  facing := 1.0;
  pos: Vector2;
  bbox_dim := Vector2.{TILE_RESOLUTION, TILE_RESOLUTION};

  accel_per_frame: Vector2;
  current_accel_force: Force;
  mass: float;
  velocity: Vector2;

  can_jump: bool;
  coyote_time := PLAYER_COYOTE_TIME_GRACE_PERIOD;

  texture: *Simp.Texture;
  flip_start: float;
  flip_duration: float;
  flip_counter: float;

  #overlay (pos) bbox: AARect;
}

Input_Package :: struct {
  dir: float;
  jump: bool;
}

player_update :: (player: *Player, input: Input_Package, dt: float) {
  old_player: Player;
  while true {
    player_state_ground(*old_player);
    player_state_air(*old_player);
  }
}

player_state_ground :: (old_player: *Player) {
  while true {
    player, input, dt := co_yield(player_update);

    player_grid_pos := floor(player.pos / TILE_RESOLUTION);
    bottom_left  := map_index(map, player_grid_pos + {y = -1}).type == .GROUND;
    bottom_right := map_index(map, player_grid_pos + {1, -1}).type  == .GROUND;
    floor_below := bottom_left || bottom_right;
    if !floor_below {
      if !input.jump then player.coyote_time = 0;
      return;
    }

    if !input.jump {
      player.can_jump = true;
    }

    if player.can_jump && input.jump{
      player.can_jump = false;
      player.velocity.y = PLAYER_JUMP_VELOCITY;
      player.coyote_time = PLAYER_COYOTE_TIME_GRACE_PERIOD+1;
      return;
    }

    ground_collisions := get_collision(player, *map, .GROUND);
    for ground_collisions {
      player_pos_diff := player.pos - old_player.pos;
      if rects_intersect(old_player.bbox + {x=player_pos_diff.x}, it.bbox) {
        player.pos.x += -sign(player_pos_diff.x) * it.overlap.z;
      }
    }

    old_player.* = player.*;
  }
}

player_state_air :: (old_player: *Player) {
  while true {
    player, input, dt := co_yield(player_update);

    coyote_jump := false;
    if player.coyote_time < PLAYER_COYOTE_TIME_GRACE_PERIOD {
      player.coyote_time += dt;
      coyote_jump = true;
    }

    if coyote_jump && input.jump {
      player.velocity.y = PLAYER_JUMP_VELOCITY;
      player.coyote_time = PLAYER_COYOTE_TIME_GRACE_PERIOD+1;
    }

    ground_collisions := get_collision(player, *map, .[.GROUND, .WALL]);
    for ground_collisions {
      player_pos_diff := player.pos - old_player.pos;
      if rects_intersect(old_player.bbox + {y=player_pos_diff.y}, it.bbox) {
        defer player.velocity.y = 0;
        if player.velocity.y < 0 {
          player.pos.y += it.overlap.w;
          return;
        } else {
          player.pos.y -= it.overlap.w;
        }
      } else if rects_intersect(old_player.bbox + {x=player_pos_diff.x}, it.bbox) {
        player.pos.x += -sign(player_pos_diff.x) * it.overlap.z;
        if it.tile.type == .WALL && player.velocity.y < 0 {
          player_state_wall(old_player);
        }
      }
    }

    player.accel_per_frame.y += GRAVITY * player.mass;

    old_player.* = player.*;
  }
}

player_state_wall :: (old_player: *Player){
  while true {
    player, input, dt := co_yield(player_update);

    wall_collisions := get_collision(player, *map, .WALL);
    if wall_collisions.count == 0 || get_collision(player, *map, .GROUND) then return;
    for wall_collisions {
      player_pos_diff := player.pos - old_player.pos;
      if rects_intersect(old_player.bbox + {x=player_pos_diff.x}, it.bbox) {
        player.pos.x += -sign(player_pos_diff.x) * it.overlap.z;
      }
    }

    if !input.jump {
      player.can_jump = true;
    }

    if player.can_jump && input.jump {
      player.velocity.y = PLAYER_JUMP_VELOCITY;
      player.velocity.x = -player.dir * PLAYER_JUMP_VELOCITY;
      player.dir = -player.dir;
      player.coyote_time = PLAYER_COYOTE_TIME_GRACE_PERIOD+1;
      return;
    }

    player.accel_per_frame.y += GRAVITY * player.mass;
    player.accel_per_frame.y += wall_friction_force.magnitude;

    old_player.* = player.*;
  }
}

Camera :: struct {
  tracking: *Player;
  pos: Vector2;
  offset: Vector2;
}

cam_set_target :: (cam: *Camera, e: *Player) {
  cam.tracking = e;
}

cam_update_tracking :: (cam: *Camera, dt: float, attachment := 4.5) {
  target := cam.tracking;
  t := clamp(attachment * dt, 0, 1);
  cam.pos = lerp(cam.pos, target.pos - cam.offset, t);
}

base_asset_path :: "W:/assets/bean_game";

load_fonts :: (font_height_px: int) -> *Simp.Dynamic_Font {
  return Simp.get_font_at_size(base_asset_path, "font.ttf", font_height_px);
}

Texture_Atlas :: Table(string, Simp.Texture);
load_assets :: (font_height_px: int) -> Texture_Atlas, *Simp.Dynamic_Font {
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

  return atlas, load_fonts(font_height_px);
}

set_texture :: inline (tex: *Simp.Texture) {
  Simp.set_shader_for_images(tex);
  Simp.immediate_set_2d_projection(RESOLUTION_W, RESOLUTION_H);
}

make_quad :: inline (rect: AARect) -> Vector2, Vector2, Vector2, Vector2 {
  p0 := rect.xy;
  p1 := xy(rect.x + rect.z, rect.y);
  p2 := rect.xy + rect.zw;
  p3 := xy(rect.x, rect.y + rect.w);

  return p0, p1, p2, p3;
}

make_uv_counter_rotation :: inline (quadrants_to_rotate: u64) -> Vector2, Vector2, Vector2, Vector2 {
  quadrants_to_rotate %= 4;
  if quadrants_to_rotate == {
    case;   return {0,0}, {1,0}, {1,1}, {0,1};
    case 1; return {0,1}, {0,0}, {1,0}, {1,1};
    case 2; return {1,1}, {0,1}, {0,0}, {1,0};
    case 3; return {1,0}, {1,1}, {0,1}, {0,0};
  }
}

Collision :: struct {
  bbox: AARect;
  overlap: AARect;
  tile: *Tile;
}

get_collision :: (player: *Player, map: *Map, tile_types: [] Tile_Type) -> [] Collision {
  is_colliding := false;
  collisions: [..] Collision;
  collisions.allocator = temp;
  player_grid_pos := floor(player.pos / TILE_RESOLUTION);
  for y: 0..1 {
    for x: 0..1 {
      test := player_grid_pos + xy(xx x, xx y);
      potential_collision := map_index_pointer(map, test);
      if potential_collision == null || !array_find(tile_types, potential_collision.type) then continue;
      tile := AARect.{xy = test * TILE_RESOLUTION, zw = potential_collision.bbox_dim};
      colliding, overlap := rects_intersect(player.bbox, tile);
      if colliding {
        array_add(*collisions, {tile, overlap, potential_collision});
      }
    }
  }

  return collisions;
}

get_collision :: (player: *Player, map: *Map, tile_type: Tile_Type) -> [] Collision {
  return get_collision(player, map, .[tile_type]);
}

ENABLE_LEVEL_HOT_RELOAD :: true;
TILE_NATIVE_RESOLUTION :: 64;
TILE_RESOLUTION_DIVISOR :: 2;
TILE_RESOLUTION :: TILE_NATIVE_RESOLUTION / TILE_RESOLUTION_DIVISOR;
RESOLUTION_W :: 640;
RESOLUTION_H :: 360;
ASPECT_RATIO :: cast(float) RESOLUTION_W / RESOLUTION_H;
DT_MAX :: cast(float) 1/60;
FONT_HEIGHT_DIVISOR :: 24;

PLAYER_MOVE_SPEED :: 350.0;
PLAYER_JUMP_VELOCITY :: 1000.0;
PLAYER_BOUNCE_PAD_JUMP_VELOCITY :: 1500.0;
PLAYER_COYOTE_TIME_GRACE_PERIOD :: 0.1;
GRAVITY :: -3800.0;

floor_friction_force: Force;
wall_friction_force: Force;
move_force: Force;
ground_accel_force: Force;
air_accel_force: Force;


map: Map;

main :: () {
  level_arena_pool: Arena.Flat_Pool;
  level_arena_allocator := Allocator.{proc=Arena.flat_pool_allocator_proc, data=*level_arena_pool};
  bg_logger_init_and_equip();

  window_width  := 1280;
  window_height := 720;
  main_window := WC.create_window(window_width, window_height, "Bean Game");
  Simp.set_render_target(main_window);

  texture_atlas, font := load_assets(window_height / FONT_HEIGHT_DIVISOR);
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
  cosmetic_textures := *Simp.Texture.[table_find_pointer(*texture_atlas, "item_hat"),
                                      table_find_pointer(*texture_atlas, "item_hatTop"),
                                      table_find_pointer(*texture_atlas, "item_helmet"),
                                      table_find_pointer(*texture_atlas, "item_helmetModern")];
  should_run := true;
  start := seconds_since_init();
  // NOTE: For now let's assume that the world is static
  // This is a very simple way to represent the world, and must be refined in the future.
  apollo_start := to_apollo(start);
  map = map_load(texture_atlas, apollo_start,, level_arena_allocator);
  random_seed(cast,force(Some_Big_Time_Value) apollo_start);

  player: Player;
  player.pos = map.player_spawn;
  player.texture = table_find_pointer(*texture_atlas, "character_roundRed");
  player.mass = 50;
  player.flip_duration = 0.2;
  player_update_routine := co_create(player_update);

  friction_coefficient :: 2.0;
  floor_friction_force = {magnitude = -1 * GRAVITY * player.mass * friction_coefficient};
  wall_friction_force = {magnitude = 130_000};
  move_force = floor_friction_force;
  ground_accel_force.duration_seconds = 0.18;
  ground_accel_force.magnitude = (player.mass * (PLAYER_MOVE_SPEED / ground_accel_force.duration_seconds)) + floor_friction_force.magnitude;
  air_accel_force.duration_seconds = 2.0;
  air_accel_force.magnitude = (player.mass * (PLAYER_MOVE_SPEED / air_accel_force.duration_seconds)) + floor_friction_force.magnitude;

  map.cam.offset = {RESOLUTION_W/2, RESOLUTION_H/2};
  map.cam.offset -= player.bbox.zw / 2;
  cam_set_target(*map.cam, *player);

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
            player.pos = map.player_spawn;
            player.velocity = {};
          }
        }
      }

      for Input.get_window_resizes() {
        Simp.update_window(it.window);
        window_width = it.width;
        window_height = it.height;
        font = load_fonts(window_height / FONT_HEIGHT_DIVISOR);
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
            cam_set_target(*map.cam, *player);
          } else map.last_modification = modtime;
        }
      }
    }

    end := seconds_since_init();
    dt := cast(float, end - start);
    dt = min(dt, DT_MAX);
    start = end;

    {
      dir: float;
      if input_move_left  dir -= 1;
      if input_move_right dir += 1;
      if dir != 0 {
        if player.facing != dir {
          player.facing = dir;
          player.flip_start = xx seconds_since_init();
          player.flip_counter = 0;
        }
        if player.dir == 0 {
          player.dir = dir;
          player.current_accel_force.time_counter = 0;
        }
      }

      // NOTE: Player integration
      accel := player.accel_per_frame / player.mass;
      player.velocity += accel * dt;
      player.velocity.x = sign(player.velocity.x) * min(abs(player.velocity.x), PLAYER_MOVE_SPEED);
      player.pos += player.velocity * dt;
      player.accel_per_frame = {};

      input := Input_Package.{dir, input_jump};
      if player.current_accel_force.time_counter < player.current_accel_force.duration_seconds {
        player.accel_per_frame.x += player.current_accel_force.magnitude * input.dir;
        player.current_accel_force.time_counter += dt;
      } else if sign(input.dir) == sign(player.dir) {
        if values_are_close(abs(player.velocity.x), PLAYER_MOVE_SPEED) {
          player.accel_per_frame.x += move_force.magnitude * input.dir;
        }
      }
      if sign(player.velocity.x) != sign(player.dir) { // NOTE: Friction has caused us to stop
        player.dir = 0;
        player.velocity.x = 0;
      }
      player.accel_per_frame.x += floor_friction_force.magnitude * -sign(player.velocity.x);

      co_resume(player_update_routine, *player, input, dt);

      if get_collision(*player, *map, .BOUNCE_PAD) {
        player.velocity.y = PLAYER_BOUNCE_PAD_JUMP_VELOCITY;
        player.can_jump = false;
      }

      coins := get_collision(*player, *map, .COIN);
      for coins {
        if !it.tile.collected {
          it.tile.collected = true;
          map.coins_collected += 1;
        }
      }

      for * map {
        if it.collected then continue;
        if it.type == .COIN || it.type == .WATER {
          it.bob_counter += dt;
          it.bob_counter = fmod_cycling(it.bob_counter, (2 * PI) / it.bob_frequency);
        }
      }

      cam_update_tracking(*map.cam, dt);
    }

    {
      Simp.clear_render_target(1, 1, 1, 1);
      defer Simp.swap_buffers(main_window);

      for map.background {
        layer_effect := ifx it.layer > 1 then xx it.layer else 1.0;
        parallax_cam := map.cam.pos / layer_effect;
        cam_view := AARect.{xy=parallax_cam, zw=xy(RESOLUTION_W, RESOLUTION_H)};
        if !rects_intersect(cam_view, it.bbox) then continue;
        set_texture(it.tex);
        p0, p1, p2, p3 := make_quad({xy = it.pos - parallax_cam, zw = it.size});
        Simp.immediate_quad(p0, p1, p2, p3);
      }

      for :visible_only map {
        if it.tex == null then continue;
        if it.collected then continue;
        set_texture(it.tex);
        dsp := it_index * TILE_RESOLUTION;
        dsp.y += it.bob_amplitude * sin(it.bob_frequency * it.bob_counter);
        p0, p1, p2, p3 := make_quad({xy = dsp - map.cam.pos, zw = xy(TILE_RESOLUTION)});
        uv0, uv1, uv2, uv3 := make_uv_counter_rotation(it.decorative_uv_rotation);
        Simp.immediate_quad(p0, p1, p2, p3, {1,1,1,1}, uv0, uv1, uv2, uv3);
      }

      {
        using player;
        set_texture(texture);

        flip_counter += dt;
        flip := clamp(norm(flip_start + flip_counter, flip_start, flip_start + flip_duration), 0, 1);
        dsp_pos := pos - map.cam.pos;
        dsp_dim := bbox_dim;
        dsp_dim.x *= abs(cos(PI * flip));
        dsp_pos.x += (bbox_dim.x - dsp_dim.x) / 2;
        p0, p1, p2, p3 := make_quad({xy = dsp_pos, zw = dsp_dim});
        left, right: float;
        if player.facing > 0 {
          left = ifx flip > 0.5 then 0.0 else 1.0;
          right = ifx flip > 0.5 then 1.0 else 0.0;
        } else {
          left = ifx flip > 0.5 then 1.0 else 0.0;
          right = ifx flip > 0.5 then 0.0 else 1.0;
        }
        uv0 := Vector2.{left,0};
        uv1 := Vector2.{right,0};
        uv2 := Vector2.{right,1};
        uv3 := Vector2.{left,1};

        Simp.immediate_quad(p0, p1, p2, p3, {1,1,1,1}, uv0, uv1, uv2, uv3);
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
        t := norm(it.time_of_log + it.decay, it.time_of_log, it.time_of_log + it.time_to_decay);
        alpha := ifx t < 0.5 then 4 * pow(t, 3) else 1 - pow(-2 * t + 2, 3) / 2;
        Simp.draw_text(font, 50, 50 + it_index * (window_height/FONT_HEIGHT_DIVISOR), it.message, {w = 1 - alpha});
        it.decay += dt;
        if it.decay > it.time_to_decay remove it;
      }
    }
  }
}

Log_Message :: struct {
  message: string;
  time_of_log: float;
  decay: float;
  time_to_decay: float;
}

logs_to_display: [..] Log_Message;
bg_logger_init_and_equip :: () {
  context.logger = bg_logger;
}

bg_logger :: (message: string, data: *void, info: Log_Info) {
  if !message then return;
  write_string(message);
  if message[message.count-1] != #char "\n" then write_string("\n");

  if !(info.common_flags & .TO_FILE_ONLY) {
    new_message: Log_Message;
    new_message.message = copy_string(message,, logs_to_display.allocator);
    new_message.time_of_log = xx seconds_since_init(); // current_time_monotonic?
    new_message.time_to_decay = ifx info.common_flags & .ERROR then 10.0 else 5.0;
    array_insert_at(*logs_to_display, new_message, 0);
  }
}

#load "map.jai";
#load "common.jai";

#import "Basic";
#import "Math";
#import "Sloppy_Math";
#import "String";
#import "File";
#import "File_Utilities";
#import "Hash_Table";
#import "Random";
WC    :: #import "Window_Creation";
Arena :: #import "Flat_Pool";
Input :: #import "Input";
Simp  :: #import "Simp";