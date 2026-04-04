/* TODO
- [X] Levels stored as text files
- [X] Custom for expansion for looping over map
- [X] Correctly handling window resizes
- [X] Prevent bunnyhopping
- [X] Physics system
- [X] Hot reloading levels
- [X] Bounce pads
- [X] Coins
- [X] Wall jumping / friction
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
- [ ] Downloading level files from the web and checking for updates
- [X] Physics system overhaul
- [ ] WASM build with Emscripten + SDL3
- [X] SDL transition
- [ ] using,map to clean up SDL3 imports
- [X] Remove coroutines
*/

AARect :: #type,isa Vector4;

Rigid_Body :: struct {
  mass: float;

  pos: Vector2;
  bbox_dim: Vector2;
  vel: Vector2;
  acc: Vector2;

  #overlay (pos) bbox: AARect;
}

apply_force :: inline (body: *Rigid_Body, mag: Vector2) {
  body.acc += mag;
}

apply_impulse :: inline (body: *Rigid_Body, mag: Vector2) {
  if mag.x != 0 body.vel.x = mag.x;
  if mag.y != 0 body.vel.y = mag.y;
}

// NOTE: ASSUMING CONSTANT FORCES
integrate :: (body: *Rigid_Body, dt: float) {
  acc := body.acc / body.mass;
  body.pos += body.vel*dt + 0.5*acc*square(dt);
  body.vel += acc*dt;
  body.acc = xy(0,0);
}

Player_State :: enum {
  GROUND;
  AIR;
  WALL;
}

Player :: struct {
  #as using body: Rigid_Body;
  body.bbox_dim = Vector2.{TILE_RESOLUTION, TILE_RESOLUTION};

  state: Player_State;

  can_jump: bool;
  coyote_counter := PLAYER_COYOTE_TIME_GRACE_PERIOD;
  wall_jump_immobility_counter := PLAYER_TIME_IMMOBILE;
  acceleration_counter := PLAYER_TIME_TO_ACCELERATE;

  grappling_hook: Rope(0.1);

  texture: *SDL_Texture;
  dir: float;
  wall_dir: float;
  flip: float;
  facing := 1.0;
}

Rope :: struct (particle_density := 1.0, jakobsen_iterations := 50) {
  Particle :: struct {
    pos: Vector2;
    old_pos: Vector2;

    fixed: bool;
  }

  length: float;
  particles: [..] Particle; // Should this be dynamic? Maybe we want to grow or shrink the rope mid swing.
}

Input_Package :: struct {
  dir: float;
  jump: bool;
}

Tween_Proc :: #type (state: *Tween_State, dt: float) -> float;
Tween_State :: struct {
  duration: float;
  started_at: float;
  index: float;

  proc: Tween_Proc;
}

tween_init :: (duration: float, proc: Tween_Proc) -> Tween_State {
  result: Tween_State;
  result.duration = duration;
  result.started_at = SDL_GetTicks() / 1000.0;
  result.proc = proc;

  return result;
}

tween_lerp :: (using state: *Tween_State, dt: float) -> float {
  index += dt;
  return saturate(norm(started_at + index, started_at, started_at + duration));
}

tween_ease :: (using state: *Tween_State, dt: float) -> float {
  t := tween_lerp(state, dt);
  return ifx t < 0.5 then 4 * pow(t, 3) else 1 - pow(-2 * t + 2, 3) / 2;
}

begin_swing :: (rope: *Rope, length: float, anchor: Vector2) {
  num_particles := cast(int) round(rope.particle_density * length);
  particle_dist := 1.0 / rope.particle_density;
  rope.length = particle_dist * num_particles;
  for i: 0..num_particles-1 {
    particle: rope.Particle;
    if i == 0 then particle.fixed = true;
    particle.pos = anchor + xy(i * particle_dist, 0);
    particle.old_pos = particle.pos;
    array_add(*rope.particles, particle);
  }
}

end_swing :: (rope: *Rope) {
  array_reset(*rope.particles);
  rope.length = 0;
}

Player_Update_Inputs :: struct {
  player: *Player;
  input: Input_Package;
  dt: float;

  collision_events: [] Collision_Event;
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
  t := saturate(attachment * dt);
  cam.pos = lerp(cam.pos, target.pos - cam.offset, t);
}

// TODO: These should all be provided by metaprogram
base_asset_path :: "W:/assets/bean_game";
art_path :: "W:/assets/bean_game/sprites";
map_path :: "W:/code/bean_game/level.bgdat";
Baked_Bitmap :: struct {
  name: string;
  width, height, channels: s32;
  pixels: [] u32;
}
baked_sprites_raw, baked_map_raw :: #run bake_assets();

bake_assets :: () -> [] Baked_Bitmap, string #compile_time {
  using file :: #import "File";
  using file_util :: #import "File_Utilities";
  using stb :: #import "stb_image";

  texture_files := file_list(art_path);
  baked_sprites := NewArray(texture_files.count, Baked_Bitmap);
  for texture_files {
    _, key, _ := path_decomp(it);
    bitmap: Baked_Bitmap;
    bitmap.name = key;
    pixels := stbi_load(to_c_string(it), *bitmap.width, *bitmap.height, *bitmap.channels, 0);
    assert(bitmap.channels == 4);
    bitmap_size := bitmap.width * bitmap.height * bitmap.channels;
    bitmap.pixels = NewArray(bitmap.width * bitmap.height, u32);
    memcpy(bitmap.pixels.data, pixels, bitmap_size);
    baked_sprites[it_index] = bitmap;
  }

  raw_map, load_success := read_entire_file(map_path);
  if !load_success {
    log_error("Error reading level data!");
  }

  return baked_sprites, raw_map;
}

Texture_Atlas :: Table(string, *SDL_Texture);
load_assets :: (renderer: *SDL_Renderer) -> Texture_Atlas {
  atlas: Texture_Atlas;
  init(*atlas, baked_sprites_raw.count);
  for baked_sprites_raw {
    surface := SDL_CreateSurfaceFrom(it.width, it.height, .ABGR8888, it.pixels.data, it.width * 4);
    texture := SDL_CreateTextureFromSurface(renderer, surface);
    assert(texture != null, "Error loading texture data!");
    table_add(*atlas, it.name, texture);
  }

  return atlas;
}

make_frect :: inline (rect: AARect) -> SDL_FRect {
  return SDL_FRect.{rect.x, rect.y, rect.z, rect.w};
}

draw_quad :: (renderer: *SDL_Renderer, tex: *SDL_Texture, rect: AARect, flip_x := false, flip_y := false, rotation_angle := 0.0) {
  quad := rect;
  quad.y = RESOLUTION_H - (quad.y + quad.w);
  flip: SDL_FlipMode;
  if flip_x then flip |= .HORIZONTAL;
  if flip_y then flip |= .VERTICAL;
  SDL_RenderTextureRotated(renderer, tex, null, *make_frect(quad), rotation_angle, null, flip);
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
  for y: -1..1 {
    for x: -1..1 {
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
PLAYER_TIME_TO_ACCELERATE :: 0.18;
PLAYER_TIME_IMMOBILE :: 0.3;
GRAVITY :: -3800.0;

FLOOR_FRICTION_FORCE: Vector2;
WALL_FRICTION\_FORCE: Vector2;
MOVE\_FORCE: Vector2;
ACCEL_FORCE: Vector2;

map: Map;
input_move_left, input_move_right, input_jump: bool;
collision_event_memory: [9] Collision_Event;
collision_events: [] Collision_Event;
should_run: bool;
start: u64;
player_flip: Tween_State;
renderer: *SDL_Renderer;
coin_texture: *SDL_Texture;
x_texture: *SDL_Texture;
number_textures: [] *SDL_Texture;
cosmetic_textures: [] *SDL_Texture;

main_loop :: (ctx: *void) #c_call {
  #if EMSCRIPTEN {
    if !should_run then emscripten_cancel_main_loop();
  }

  push_context cast(*#Context) ctx {
    reset_temporary_storage();

    player := *map.player;

    {
      e: SDL_Event;
      while SDL_PollEvent(*e) {
        if cast(SDL_EventType) e.type == {
          case .SDL_EVENT_QUIT; should_run = false;

          case .SDL_EVENT_KEY_UP; #through;
          case .SDL_EVENT_KEY_DOWN; {
            key := e.key.key;
            down := e.key.down;
            if key == #char "a" || key == SDLK_LEFT  then input_move_left  = down;
            if key == #char "d" || key == SDLK_RIGHT then input_move_right = down;
            if key == SDLK_SPACE || key == SDLK_UP || key == #char "w" then input_jump = down;
            if key == #char "r" && down {
              player.pos = map.player_spawn;
              player.vel = {};
            }
          }
        }
      }

      /*
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
            array_free(map.tiles);
            // TODO: Also must free parallax effects
            map = map_load(texture_atlas, modtime);
            map.cam.offset = {RESOLUTION_W/2, RESOLUTION_H/2};
            map.cam.offset -= player.bbox.zw / 2;
            player = *map.player;
            cam_set_target(*map.cam, player);
          } else map.last_modification = modtime;
        }
      }
      */
    }

    end := SDL_GetTicks();
    dt := (end - start) / 1000.0;
    start = end;

    {
      dir: float;
      if input_move_left  dir -= 1;
      if input_move_right dir += 1;
      if dir != 0 {
        if player.facing != dir {
          player.facing = dir;
          player_flip = tween_init(0.2, tween_lerp);
        }
        if player.dir == 0 {
          player.dir = dir;
          player.acceleration_counter = 0;
        }
      }

      input := Input_Package.{dir, input_jump};

      old_player := player.*;
      if player.state == {
        case .GROUND; {
          player_grid_pos := floor(player.pos / TILE_RESOLUTION);
          bottom_left_type := map_index(map, player_grid_pos + {y = -1}).type;
          bottom_right_type := map_index(map, player_grid_pos + {1, -1}).type;
          bottom_left := bottom_left_type  == .GROUND || bottom_left_type == .WALL;
          bottom_right := bottom_right_type == .GROUND || bottom_right_type == .WALL;
          floor_below := bottom_left || bottom_right;
          if !floor_below {
            if !input.jump then player.coyote_counter = 0;
            player.can_jump = false;
            player.state = .AIR;
          }

          if !input.jump {
            player.can_jump = true;
          }

          if player.can_jump && input.jump{
            player.can_jump = false;
            apply_impulse(player, xy(0, PLAYER_JUMP_VELOCITY));
            player.coyote_counter = PLAYER_COYOTE_TIME_GRACE_PERIOD;
            player.state = .AIR;
          }
        }

        case .AIR; {
          player.wall_dir = 0;

          coyote_jump := false;
          if player.coyote_counter < PLAYER_COYOTE_TIME_GRACE_PERIOD {
            player.coyote_counter += dt;
            coyote_jump = true;
          }

          if coyote_jump && input.jump {
            apply_impulse(player, xy(0, PLAYER_JUMP_VELOCITY));
            player.coyote_counter = PLAYER_COYOTE_TIME_GRACE_PERIOD;
            player.can_jump = false;
          }

          for collision_events {
            if it == .FLOOR then player.state = .GROUND;
            if it == .WALL_SLIDE then player.state = .WALL;
          }

          apply_force(player, xy(0, GRAVITY*player.mass));
        }

        case .WALL; {
          if player.wall_dir == 0 then player.wall_dir = player.dir;

          if !input.jump {
            player.can_jump = true;
          }

          if player.can_jump && input.jump {
            player.dir = -player.wall_dir;
            apply_impulse(player, xy(player.dir * PLAYER_MOVE_SPEED, PLAYER_JUMP_VELOCITY));
            player.coyote_counter = PLAYER_COYOTE_TIME_GRACE_PERIOD;
            player.can_jump = false;
            player.wall_jump_immobility_counter = 0;
            player.acceleration_counter = PLAYER_TIME_TO_ACCELERATE;
            player.state = .AIR;
          }

          wall_collision := false;
          for collision_events {
            if it == .WALL_SLIDE {
              wall_collision = true;
            } else if it == .FLOOR then player.state = .AIR;
          }
          if !wall_collision then player.state = .AIR;
          if player.state == .AIR then player.can_jump = false;

          apply_force(player, xy(0, GRAVITY*player.mass) + WALL_FRICTION_FORCE);
        }
      }

      if player.wall_jump_immobility_counter < PLAYER_TIME_IMMOBILE {
        apply_force(player, MOVE_FORCE * player.dir);
        player.wall_jump_immobility_counter += dt;
      } else if player.acceleration_counter < PLAYER_TIME_TO_ACCELERATE {
        apply_force(player, ACCEL_FORCE * input.dir);
        player.acceleration_counter += dt;
      } else if sign(input.dir) == sign(player.dir) {
        if values_are_close(abs(player.vel.x), PLAYER_MOVE_SPEED) {
          apply_force(player, MOVE_FORCE * input.dir);
        }
      }
      apply_force(player, FLOOR_FRICTION_FORCE * -player.dir);

      integrate(player, dt);
      if sign(player.vel.x) != sign(player.dir) { // NOTE: Friction has caused us to stop
        player.dir = 0;
        player.vel.x = 0;
      }
      player.vel.x = sign(player.vel.x) * min(abs(player.vel.x), PLAYER_MOVE_SPEED);

      // NOTE: Collisions
      collision_events.count = 0;
      obstacle_collisions := get_collision(player, *map, .[.GROUND, .WALL]);
      for obstacle_collisions {
        player_pos_diff := player.pos - old_player.pos;
        if rects_intersect(old_player.bbox + {y=player_pos_diff.y}, it.bbox) {
          if player.vel.y < 0 {
            player.pos.y += it.overlap.w;
            collision_events.count += 1;
            collision_events[collision_events.count-1] = .FLOOR;
          } else player.pos.y -= it.overlap.w;
          player.vel.y = 0;
        } else if rects_intersect(old_player.bbox + {x=player_pos_diff.x}, it.bbox) {
          player.pos.x += -sign(player_pos_diff.x) * it.overlap.z;
          player.dir = 0;
          player.vel.x = 0;
          if it.tile.type == .WALL && player.vel.y < 0 {
            collision_events.count += 1;
            collision_events[collision_events.count-1] = .WALL_SLIDE;
          }
        }
      }

      if get_collision(player, *map, .BOUNCE_PAD) {
        apply_impulse(player, xy(0, PLAYER_BOUNCE_PAD_JUMP_VELOCITY));
        player.can_jump = false;
      }

      coins := get_collision(player, *map, .COIN);
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
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      SDL_RenderClear(renderer);
      defer SDL_RenderPresent(renderer);

      for map.background {
        layer_effect := ifx it.layer > 1 then xx it.layer else 1.0;
        parallax_cam := map.cam.pos / layer_effect;
        cam_view := AARect.{xy=parallax_cam, zw=xy(RESOLUTION_W, RESOLUTION_H)};
        if !rects_intersect(cam_view, it.bbox) then continue;
        draw_quad(renderer, it.tex, {xy = it.pos - parallax_cam, zw = it.size});
      }

      for :visible_only map {
        if it.tex == null then continue;
        if it.collected then continue;
        dsp := it_index * TILE_RESOLUTION;
        dsp.y += it.bob_amplitude * sin(it.bob_frequency * it.bob_counter);
        draw_quad(renderer, it.tex, {xy = dsp - map.cam.pos, zw = xy(TILE_RESOLUTION)}, rotation_angle = it.decorative_rotation);
      }

      {
        using player;

        /*
        rope_width :: 5;
        for grappling_hook.particles {
          p0 := it.pos - map.cam.pos;
          if it_index < grappling_hook.particles.count-1 {
            next_particle := grappling_hook.particles[it_index + 1];
            segment := next_particle.pos - it.pos;
            perpendicular := xy(-segment.y, segment.x);
            perpendicular = unit_vector(perpendicular);
            perpendicular *= rope_width;
            p1 := next_particle.pos - map.cam.pos;
            p2 := p1 + perpendicular;
            p3 := p0 + perpendicular;
            //set_color();
            Simp.immediate_quad(p0, p1, p2, p3, {0, 0, 1, 1});
          }
          draw_circle(p0, 2, 100, {1, 0, 0, 1});
        }
        */

        flip = player_flip.proc(*player_flip, dt);
        dsp_pos := pos - map.cam.pos;
        dsp_dim := bbox_dim;
        dsp_dim.x *= abs(cos(PI * flip));
        dsp_pos.x += (bbox_dim.x - dsp_dim.x) / 2;
        toggle := facing > 0;
        if flip > 0.5 then toggle = !toggle;
        draw_quad(renderer, texture, {xy = dsp_pos, zw = dsp_dim}, flip_x = toggle);
      }

      {
        scoreboard_x := 0.0;
        SCOREBOARD_Y :: 315;
        SCOREBOARD_RESOLUTION :: TILE_RESOLUTION * 1.5;
        draw_quad(renderer, coin_texture, {xy = xy(scoreboard_x, SCOREBOARD_Y), zw = xy(SCOREBOARD_RESOLUTION)});
        scoreboard_x += TILE_RESOLUTION;
        draw_quad(renderer, x_texture, {xy = xy(scoreboard_x, SCOREBOARD_Y), zw = xy(SCOREBOARD_RESOLUTION)});
        scoreboard_x += TILE_RESOLUTION;

        coin_digits := get_digits_right_to_left_on_temp(map.coins_collected);
        for < coin_digits {
          draw_quad(renderer, number_textures[it], {xy = xy(scoreboard_x, SCOREBOARD_Y), zw = xy(SCOREBOARD_RESOLUTION)});
          scoreboard_x += TILE_RESOLUTION / 1.8;
        }
      }

      for logs_to_display {
        alpha := it.fade.proc(*it.fade, dt);
        SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 1 - alpha);
        SDL_RenderDebugText(renderer, 20.0, RESOLUTION_H - (20.0 + it_index * 10), temp_c_string(it.message));
        if alpha <= 0.0 {
          free(it.message);
          remove it;
        }
      }

      SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 1);
      SDL_RenderDebugTextFormat_CFormat(renderer, RESOLUTION_W - 100, 20, "FPS: %d", cast(int,1.0/dt));
    }
  }
}

main :: () {
  bg_logger_init_and_equip();

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

  window_width  := 1280;
  window_height := 720;
  window := SDL_CreateWindow("Bean Game", xx window_width, xx window_height, 0);
  renderer = SDL_CreateRenderer(window, null);
  SDL_SetRenderLogicalPresentation(renderer, RESOLUTION_W, RESOLUTION_H, .INTEGER_SCALE);
  SDL_SetRenderVSync(renderer, 1);

  texture_atlas := load_assets(renderer);
  coin_texture = (.*)table_find_pointer(*texture_atlas, "tile_coin"); // Duplicate
  x_texture = (.*)table_find_pointer(*texture_atlas, "ui_numX");

  number_textures = *SDL_Texture.[(.*)table_find_pointer(*texture_atlas, "ui_num0"),
                                    (.*)table_find_pointer(*texture_atlas, "ui_num1"),
                                    (.*)table_find_pointer(*texture_atlas, "ui_num2"),
                                    (.*)table_find_pointer(*texture_atlas, "ui_num3"),
                                    (.*)table_find_pointer(*texture_atlas, "ui_num4"),
                                    (.*)table_find_pointer(*texture_atlas, "ui_num5"),
                                    (.*)table_find_pointer(*texture_atlas, "ui_num6"),
                                    (.*)table_find_pointer(*texture_atlas, "ui_num7"),
                                    (.*)table_find_pointer(*texture_atlas, "ui_num8"),
                                    (.*)table_find_pointer(*texture_atlas, "ui_num9")];
  cosmetic_textures = *SDL_Texture.[(.*)table_find_pointer(*texture_atlas, "item_hat"),
                                      (.*)table_find_pointer(*texture_atlas, "item_hatTop"),
                                      (.*)table_find_pointer(*texture_atlas, "item_helmet"),
                                      (.*)table_find_pointer(*texture_atlas, "item_helmetModern")];
  start = SDL_GetTicks();
  // NOTE: For now let's assume that the world is static
  // This is a very simple way to represent the world, and must be refined in the future.
  random_seed(cast,trunc(u32) SDL_GetPerformanceCounter());
  map = map_load(texture_atlas);

  player := *map.player;

  FRICTION_COEFFICIENT :: 1.8;
  FLOOR_FRICTION_FORCE = xy(-1 * GRAVITY * player.mass * FRICTION_COEFFICIENT, 0);
  WALL_FRICTION_FORCE = xy(0, 130_000);
  MOVE_FORCE = FLOOR_FRICTION_FORCE;
  ACCEL_FORCE = player.mass * xy(PLAYER_MOVE_SPEED / PLAYER_TIME_TO_ACCELERATE, 0) + FLOOR_FRICTION_FORCE;

  player_flip = tween_init(0.2, tween_lerp);

  map.cam.offset = {RESOLUTION_W/2, RESOLUTION_H/2};
  map.cam.offset -= player.bbox.zw / 2;
  cam_set_target(*map.cam, player);

  begin_swing(*player.grappling_hook, 100, player.pos + xy(0, 50));

  collision_events = collision_event_memory;
  should_run = true;

  #if EMSCRIPTEN {
    emscripten_set_main_loop_arg(main_loop, *context, 0, true);
  } else {
    while should_run main_loop(*context);
  }
}

Log_Message :: struct {
  message: string;
  fade: Tween_State;
}

logs_to_display: [..] Log_Message;
bg_logger_init_and_equip :: () {
  context.logger = bg_logger;
}

bg_logger :: (message: string, data: *void, info: Log_Info) {
  if !message then return;
  /*
  write_string(message);
  if message[message.count-1] != #char "\n" then write_string("\n");
  */

  if !(info.common_flags & .TO_FILE_ONLY) {
    new_message: Log_Message;
    new_message.message = copy_string(message);
    duration := ifx info.common_flags & .ERROR then 15.0 else 5.0;
    new_message.fade = tween_init(duration, tween_ease);
    array_insert_at(*logs_to_display, new_message, 0);
  }
}

#load "map.jai";
#load "common.jai";

#import "Basic";
#import "Math";
#import "Sloppy_Math";
#import "String";
#import "Hash_Table";
#import "PCG";

#if OS == .WINDOWS {
  #import,file "modules/sdl3/windows/windows.jai";
} else #if OS == .LINUX {
  #import,file "modules/sdl3/linux/linux.jai";
} else #if OS == .MACOS {
  #import,file "modules/sdl3/macos/macos.jai";
}
#if EMSCRIPTEN {
  #import,file "modules/emscripten.jai";
}