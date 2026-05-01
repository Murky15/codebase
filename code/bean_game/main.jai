/* TODO
X = completed
O = Scrapped / Pushed back to later release

- [X] Levels stored as text files
- [X] Custom for expansion for looping over map
- [X] Correctly handling window resizes
- [X] Prevent bunnyhopping
- [X] Physics system
- [X] Hot reloading levels
- [X] Bounce pads
- [X] Coins
- [X] Wall jumping / friction
- [X] Environmental ropes
- [X] Player flip direction
- [X] Autotiling
- [X] Parallax backgrounds
- [X] Coroutines
- [X] Debug messages and font display (Custom logger that writes messages to the screen!)
- [X] Occlusion culling
- [X] Include user data in logger to change message display duration
- [X] Physics system overhaul
- [X] WASM build with Emscripten + SDL3
- [X] SDL transition
- [X] Remove coroutines
- [X] Fixed Arena implementation
- [X] Bounce pad bug where the player collides for multiple frames
- [X] Fixed timestep
- [X] Spikes & traps
- [O] Grappling hook
- [O] using,map to clean up SDL3 imports
- [O] Maybe coroutines could be useful actually
- [ ] Hands
- [ ] Signs with user-specified text
- [ ] Cosmetics
- [ ] Main menu
- [ ] Audio
- [ ] Downloading level files from the web and checking for updates
- [ ] To communicate player health instead of hearts we could switch player sprite from green -> yellow -> red -> dead
- [ ] Bordered rects could be drawn with SDL_Rect whatever in a for loop increasing the offset and decreasing the dim each time
- [ ] Fix fullscreen projection, or figure out how to draw in the borders for odd aspect ratios
*/

AARect :: #type,isa Vector4;

Rigid_Body :: struct {
  mass: float;

  pos: Vector2;
  bbox_dim: Vector2;
  vel: Vector2;
  force: Vector2;

  #overlay (pos) bbox: AARect;
}

apply_force :: inline (body: *Rigid_Body, mag: Vector2) {
  body.force += mag;
}

apply_impulse :: inline (body: *Rigid_Body, mag: Vector2) {
  if mag.x != 0 body.vel.x = mag.x;
  if mag.y != 0 body.vel.y = mag.y;
}

integrate :: (body: *Rigid_Body, dt: float) {
  acc := body.force / body.mass;
  body.vel += acc*dt;
  body.pos += body.vel*dt;
  body.force = xy(0,0);
}

Player_State :: enum {
  GROUND;
  AIR;
  WALL;
  SWING;
}

Player :: struct {
  #as using body: Rigid_Body;
  body.bbox_dim = Vector2.{TILE_RESOLUTION, TILE_RESOLUTION};

  state: Player_State;
  health := PLAYER_MAX_LIVES - 1;

  can_jump:        bool;
  used_bounce_pad: bool;

  coyote_counter               := PLAYER_COYOTE_TIME_GRACE_PERIOD;
  wall_jump_immobility_counter := PLAYER_TIME_IMMOBILE;
  acceleration_counter         := PLAYER_TIME_TO_ACCELERATE;
  damage_cooldown              := PLAYER_GRACE_PERIOD;

  texture: [PLAYER_MAX_LIVES] *SDL_Texture;
  dir:      float;
  wall_dir: float;
  flip:     float;
  facing := 1.0;

  swinging_from: *Env_Rope;
}

Rope :: struct {
  #as using n: Linked_List_Header;

  Particle :: struct {
    pos: Vector2;
    old_pos: Vector2;
    invmass := 1.0;
    fixed: bool;

    accel := Vector2.{0, GRAVITY};
  }

  PARTICLES_PER_CHUNK :: 50;
  Particle_Chunk :: struct {
    #as using n: Linked_List_Header;
    p: [PARTICLES_PER_CHUNK] Particle;
    count: int;
  }

  // Visual settings
  thickness := 5.0;
  padding := 1.0;

  desired_particle_dist: float;
  particle_density: float;
  jakobsen_iterations: int;
  particles: Linked_List(Particle_Chunk);
  num_particles: int;
}

operator *[] :: (chunks: Linked_List(Rope.Particle_Chunk), index: int) -> *Rope.Particle {
  chunk_idx := index / Rope.PARTICLES_PER_CHUNK;
  assert(chunk_idx < chunks.count, "Index out of range!");
  local_idx := index;
  for * chunks {
    if it_index == chunk_idx {
      assert(local_idx < it.count, "Index out of range!");
      return *it.p[local_idx];
    }
    local_idx -= Rope.PARTICLES_PER_CHUNK;
  }

  return null;
}

particles :: (rope: Rope, body: Code, flags: For_Flags) #expand {
  for <=cast(bool, flags & .REVERSE) i: 0..rope.num_particles-1 {
    #if flags & .POINTER {
      `it := *rope.particles[i];
    } else {
      `it := rope.particles[i];
    }
    `it_index := i;

    #insert body;
  }
}

Input_Package :: struct {
  dir: float;
  rope_slide_dir: float;
  jump: bool;
}

rope_init :: (rope: *Rope, particle_density := 0.3, jakobsen_iterations := 50) {
  rope.particle_density = particle_density;
  rope.jakobsen_iterations = jakobsen_iterations;
  rope.desired_particle_dist = 1.0 / particle_density;
}

rope_push_particle :: (arena: *Arena, rope: *Rope, particle_reserves: *Linked_List(Rope.Particle_Chunk), particle: Rope.Particle) {
  next_chunk :: () -> *Rope.Particle_Chunk #expand {
    result := particle_reserves.first;
    if result {
      particle_reserves.first = xx particle_reserves.first.next;
      particle_reserves.count -= 1;
      memset(result, 0, size_of(Rope.Particle_Chunk));
    } else {
      result = arena_push(arena, Rope.Particle_Chunk);
    }
    sll_queue_push(*rope.particles, result);

    return result;
  }

  current_chunk := rope.particles.last;
  if current_chunk == null || current_chunk.count == Rope.PARTICLES_PER_CHUNK then current_chunk = next_chunk();
  current_chunk.p[current_chunk.count] = particle;
  current_chunk.count += 1;
  rope.num_particles += 1;
}

rope_integrate :: (rope: *Rope, dt: float) {
  for * :particles rope {
    if it.fixed then continue;
    old_pos := it.pos;
    it.pos = 2 * it.pos - it.old_pos + it.accel * square(dt);
    it.old_pos = old_pos;
  }

  // Enforce constraints
  for 0..rope.jakobsen_iterations {
    for i: 1..rope.num_particles-1 {
      p0 := *rope.particles[i-1];
      p1 := *rope.particles[i];
      delta := p1.pos - p0.pos;
      length := sqrt(dot(delta, delta));
      diff := (length - rope.desired_particle_dist)
             /(length * (p0.invmass + p1.invmass));
      p0.pos += p0.invmass * delta * diff;
      p1.pos -= p1.invmass * delta * diff;
    }
  }
}

rope_destroy :: (rope: *Rope, particle_reserves: *Linked_List(Rope.Particle_Chunk)) {
  chunk, next: *Rope.Particle_Chunk;
  chunk = rope.particles.first;
  for 0..rope.particles.count-1 {
    next = xx chunk.next;
    sll_queue_push(particle_reserves, chunk);
    chunk = next;
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
  t := saturate(attachment * dt);
  cam.pos = lerp(cam.pos, target.pos - cam.offset, t);
}

Texture_Atlas :: Table(string, *SDL_Texture);
load_assets :: (renderer: *SDL_Renderer) -> Texture_Atlas {
  Bitmap :: struct {
    name: string;
    width, height, channels: s32;
    pixels: *u8;
  }
  tex_visitor :: (userdata: *void, dirname: *u8, fname: *u8) -> SDL_EnumerationResult #c_call {
    push_context,defer_pop;
    bitmaps := cast(*[..] Bitmap) userdata;
    name := to_string(fname);
    full_path := join(to_string(dirname), name);
    bitmap: Bitmap;
    bitmap.name = copy_string(path_strip_extension(name));
    #if EMSCRIPTEN {
      bitmap.pixels = emscripten_get_preloaded_image_data(temp_c_string(full_path), *bitmap.width, *bitmap.height);
      bitmap.channels = 4; // Hope this is correct...
    } else {
      using STB :: #import "stb_image";
      bitmap.pixels = stbi_load(temp_c_string(full_path), *bitmap.width, *bitmap.height, *bitmap.channels, 0);
    }
    if bitmap.pixels == null || bitmap.channels != 4 then return .SDL_ENUM_FAILURE;
    array_add(bitmaps, bitmap);

    return .SDL_ENUM_CONTINUE;
  }
  bitmaps: [..] Bitmap;
  bitmaps.allocator = temp;
  texture_read_success := SDL_EnumerateDirectory(art_path, tex_visitor, *bitmaps);
  assert(texture_read_success, "Error loading textures!");

  atlas: Texture_Atlas;
  init(*atlas, bitmaps.count);
  for bitmaps {
    surface := SDL_CreateSurfaceFrom(it.width, it.height, .ABGR8888, it.pixels, it.width * 4);
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

draw_quad :: (renderer: *SDL_Renderer, p0: Vector2, p1: Vector2, p2: Vector2, p3: Vector2, color: Vector4) {
  quad_indices :: s32.[0,1,2,0,2,3];

  p := Vector2.[p0, p1, p2, p3];
  v: [4] SDL_Vertex;
  for * v {
    it.position.x = p[it_index].x;
    it.position.y = RESOLUTION_H - p[it_index].y;
    it.color = cast,force(SDL_FColor) color;
  }

  result := SDL_RenderGeometry(renderer, null, v.data, v.count, quad_indices.data, quad_indices.count);
  assert(result, tprint("%", to_string(SDL_GetError())));
}

draw_circle_debug :: (renderer: *SDL_Renderer, center: Vector2, radius: int, color: Vector4) {
  SDL_SetRenderDrawColorFloat(renderer, color.x, color.y, color.z, color.w);
  for y_int: -radius..radius {
    for x_int: -radius..radius {
      y := cast(float) y_int;
      x := cast(float) x_int;
      if square(x) + square(y) <= square(cast(float) radius) {
        dsp_x := center.x + x;
        dsp_y := RESOLUTION_H - (center.y + y);
        SDL_RenderPoint(renderer, dsp_x, dsp_y);
      }
    }
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

ENABLE_LEVEL_HOT_RELOAD :: !EMSCRIPTEN;
TILE_NATIVE_RESOLUTION :: 64;
TILE_RESOLUTION_DIVISOR :: 2;
TILE_RESOLUTION :: TILE_NATIVE_RESOLUTION / TILE_RESOLUTION_DIVISOR;
RESOLUTION_W :: 640;
RESOLUTION_H :: 360;
ASPECT_RATIO :: cast(float) RESOLUTION_W / RESOLUTION_H;
FONT_HEIGHT_DIVISOR :: 24;

PLAYER_MAX_LIVES :: 3;
PLAYER_MOVE_SPEED :: 350.0;
PLAYER_JUMP_VELOCITY :: 1000.0;
PLAYER_ROPE_JUMP_MIN_VELOCITY :: PLAYER_JUMP_VELOCITY / 3;
PLAYER_BOUNCE_PAD_JUMP_VELOCITY :: 1200.0;
PLAYER_COYOTE_TIME_GRACE_PERIOD :: 0.1;
PLAYER_TIME_TO_ACCELERATE :: 0.18;
PLAYER_TIME_IMMOBILE :: 0.3;
PLAYER_GRACE_PERIOD  :: 0.5;
PLAYER_BLINK_TIME    :: 0.1;

ROPE_PARTICLE_IMPULSE_SCALE :: 2;

GRAVITY :: -3800.0;

// NOTE: Some of these things could probably be better organized somewhere else in the future,
// but for now this works fine.
globals: struct {
  game_state: enum {
    NONE;
    MENU;
    PLAY;
    PAUSE;
  } = .MENU;

  // Platform handles
  renderer: *SDL_Renderer;

  // Map & memory management
  level_arena: *Arena;
  map: Map;
  old_map: Map;

  // Physics constants
  floor_friction_force: Vector2;
  wall_friction\_force: Vector2;
  move\_force: Vector2;
  accel_force: Vector2;

  // Input collection & collision detection
  input_move_left, input_move_right: bool;
  input_slide_up , input_slide_down: bool;
  input_jump: bool;
  collision_event_memory: [9] Collision_Event;
  collision_events: [] Collision_Event;

  // Texture cache
  texture_atlas: Texture_Atlas;

  coin_texture:         *SDL_Texture;
  x_texture:            *SDL_Texture;
  ui_box_texture:       *SDL_Texture;
  ui_select_texture:    *SDL_Texture;
  number_textures:   [] *SDL_Texture;
  cosmetic_textures: [] *SDL_Texture;

  // Timing
  should_run: bool;
  start: u64;
  accumulator: float64;

  // Misc
  player_flip: Tween_State;
}

collect_input :: () {
  e: SDL_Event;
  while SDL_PollEvent(*e) {
    if cast(SDL_EventType) e.type == {
      case .SDL_EVENT_QUIT; globals.should_run = false;

      case .SDL_EVENT_KEY_UP; #through;
      case .SDL_EVENT_KEY_DOWN; {
        key := e.key.key;
        down := e.key.down;
        if key == #char "a" || key == SDLK_LEFT  then globals.input_move_left  = down;
        if key == #char "d" || key == SDLK_RIGHT then globals.input_move_right = down;
        if key == #char "w" || key == SDLK_UP    then globals.input_slide_up   = down;
        if key == #char "s" || key == SDLK_DOWN  then globals.input_slide_down = down;
        if key == SDLK_SPACE then globals.input_jump = down;
        if key == #char "r" && down {
          globals.map.player.pos = globals.map.player_spawn;
          globals.map.player.vel = {};
          log("Resetting player");
        }
      }
    }
  }
}

bean_game_tick :: (map: *Map, dt: float) {
  player := *map.player;

  collect_input();

  dir: float;
  if globals.input_move_left  dir -= 1;
  if globals.input_move_right dir += 1;
  if dir != 0 {
    if player.facing != dir {
      player.facing = dir;
      globals.player_flip = tween_init(0.2, tween_norm);
    }
    if player.dir == 0 {
      player.dir = dir;
      player.acceleration_counter = 0;
    }
  }

  rope_slide_dir: float;
  if globals.input_slide_up   rope_slide_dir += 1;
  if globals.input_slide_down rope_slide_dir -= 1;

  input := Input_Package.{dir, rope_slide_dir, globals.input_jump};

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

      if player.can_jump && input.jump {
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
      }

      for globals.collision_events {
        if it == .FLOOR {
          player.can_jump = false;
          player.state = .GROUND;
        }
        if it == .WALL_SLIDE then player.state = .WALL;
      }

      // TODO: Should we work this into the main collision system?
      for * map.environmental_ropes {
        if player.vel.y < 0 && check_env_rope_collision_and_attach(it, player, dt) {
          player.state = .SWING;
          player.vel = xy(0,0);
          player.swinging_from = it;
        }
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
      for globals.collision_events {
        if it == .WALL_SLIDE {
          wall_collision = true;
        } else if it == .FLOOR then player.state = .AIR;
      }
      if !wall_collision then player.state = .AIR;
      if player.state == .AIR then player.can_jump = false;

      apply_force(player, xy(0, GRAVITY*player.mass) + globals.wall_friction_force);
    }

    case .SWING; { // TODO: Momentum carry after jumping -> steal velocity from the particle we were on and add it to the impulse.
      if input.rope_slide_dir > 0 {
        env_rope_move_body_up(player.swinging_from);
      } else if input.rope_slide_dir < 0 {
        env_rope_move_body_down(player.swinging_from);
      }

      if input.jump {
        env_rope_detach_body(player.swinging_from, dt);
        player.swinging_from = null;
        // TODO: Pull this out into function?
        player.can_jump = false;
        player.coyote_counter = PLAYER_COYOTE_TIME_GRACE_PERIOD;
        player.state = .AIR;
      }
    }
  }

  // TODO: These motion physics could probably be more finely tuned for when the player is swinging...
  if player.wall_jump_immobility_counter < PLAYER_TIME_IMMOBILE {
    apply_force(player, globals.move_force * player.dir);
    player.wall_jump_immobility_counter += dt;
  } else if player.acceleration_counter < PLAYER_TIME_TO_ACCELERATE {
    apply_force(player, globals.accel_force * input.dir);
    player.acceleration_counter += dt;
  } else if sign(input.dir) == sign(player.dir) {
    if values_are_close(abs(player.vel.x), PLAYER_MOVE_SPEED) {
      apply_force(player, globals.move_force * input.dir);
    }
  }
  apply_force(player, globals.floor_friction_force * -player.dir);

  old_player := player.*;
  if player.state != .SWING then integrate(player, dt);
  if sign(player.vel.x) != sign(player.dir) { // NOTE: Friction has caused us to stop
    player.dir = 0;
    player.vel.x = 0;
  }
  player.vel.x = sign(player.vel.x) * min(abs(player.vel.x), PLAYER_MOVE_SPEED);

  // NOTE: Collisions
  if player.damage_cooldown < PLAYER_GRACE_PERIOD {
    player.damage_cooldown += dt;
  }

  globals.collision_events.count = 0;
  obstacle_collisions := get_collision(player, map, .[.GROUND, .WALL]);
  for obstacle_collisions {
    player_pos_diff := player.pos - old_player.pos;
    if rects_intersect(old_player.bbox + {y=player_pos_diff.y}, it.bbox) {
      if player.vel.y < 0 {
        player.pos.y += it.overlap.w;
        globals.collision_events.count += 1;
        globals.collision_events[globals.collision_events.count-1] = .FLOOR;
      } else player.pos.y -= it.overlap.w;
      player.vel.y = 0;
    } else if rects_intersect(old_player.bbox + {x=player_pos_diff.x}, it.bbox) {
      player.pos.x += -sign(player_pos_diff.x) * it.overlap.z;
      player.dir = 0;
      player.vel.x = 0;
      if it.tile.type == .WALL && player.vel.y < 0 {
        globals.collision_events.count += 1;
        globals.collision_events[globals.collision_events.count-1] = .WALL_SLIDE;
      }
    }
  }

  if get_collision(player, map, .BOUNCE_PAD) {
    if !player.used_bounce_pad {
      apply_impulse(player, xy(0, PLAYER_BOUNCE_PAD_JUMP_VELOCITY));
      player.can_jump = false;
      player.used_bounce_pad = true;
    }
  } else player.used_bounce_pad = false;

  if player.damage_cooldown >= PLAYER_GRACE_PERIOD {
    for get_collision(player, map, .[.SPIKE, .SPIKE_SPINNING, .SPIKE_TRAP]) {
      tile := it.tile;
      if tile.type == .SPIKE_TRAP && tile.trap_state == .HIDDEN {
        tile.trap_animation = tween_init(TILE_SPIKE_TRAP_RELEASE_TIME, tween_norm);
        tile.trap_state = .SPRING;
      }
      player.health -= 1;
      player.damage_cooldown = 0;
    }
  }

  if player.health < PLAYER_MAX_LIVES - 1 {
    for get_collision(player, map, .HEART) {
      if !it.tile.collected {
        it.tile.collected = true;
        player.health = min(player.health + 1, PLAYER_MAX_LIVES - 1);
      }
    }
  }

  for get_collision(player, map, .COIN) {
    if !it.tile.collected {
      it.tile.collected = true;
      map.coins_collected += 1;
    }
  }

  for * map.environmental_ropes {
    still_changing_rope_position := it == player.swinging_from && input.rope_slide_dir != 0;
    simulate_env_rope(it, dt, still_changing_rope_position);
  }

  cam_update_tracking(*map.cam, dt);
}

bean_game_render :: (renderer: *SDL_Renderer, map: *Map, old_map: *Map, frame_time: float, alpha: float) {
  player := *map.player;

  cam_pos := lerp(old_map.cam.pos, map.cam.pos, alpha);

  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderClear(renderer);
  defer SDL_RenderPresent(renderer);

  for map.background {
    layer_effect := ifx it.layer > 1 then xx it.layer else 1.0;
    parallax_cam := cam_pos / layer_effect;
    cam_view := AARect.{xy=parallax_cam, zw=xy(RESOLUTION_W, RESOLUTION_H)};
    if !rects_intersect(cam_view, it.bbox) then continue;
    draw_quad(renderer, it.tex, {xy = it.pos - parallax_cam, zw = it.size}, rotation_angle = it.rotation);
  }

  for rope: map.environmental_ropes {
    for :particles rope {
      if it_index < rope.num_particles-1 {
        next_particle := rope.particles[it_index + 1];
        segment := next_particle.pos - it.pos;
        padding := unit_vector(segment) * rope.padding;
        perpendicular := xy(-segment.y, segment.x);
        perpendicular = unit_vector(perpendicular);
        half_thick := perpendicular * rope.thickness / 2;
        full_thick := perpendicular * rope.thickness;
        p0 := -padding + (it.pos - cam_pos) - half_thick;
        p1 :=  padding + (next_particle.pos - cam_pos) - half_thick;
        p2 := p1 + full_thick;
        p3 := p0 + full_thick;
        draw_quad(renderer, p0, p1, p2, p3, {176.0/255, 105.0/255, 219.0/255, 1});
      }
      //draw_circle_debug(renderer, it.pos - cam_pos, 2, {1, 0, 0, 1});
    }
  }

  for * map {
    if it.collected then continue;
    it.rotation += it.angular_speed * frame_time;
    if it.bobs {
      it.bob_counter += frame_time;
      it.bob_counter = fmod_cycling(it.bob_counter, (2 * PI) / it.bob_frequency);
    }

    // TODO: Now here is a situation where coroutines could be useful
    if it.type == .SPIKE_TRAP {
      if it.trap_state == {
        case .SPRING; {
          t := it.trap_animation.proc(*it.trap_animation, frame_time);
          it.trap_display_offset = lerp(TILE_SPIKE_TRAP_HIDDEN_OFFSET, 0, t);
          if !it.trap_animation.active {
            it.trap_state = .ACTIVE;
            it.trap_animation = tween_init(TILE_SPIKE_TRAP_ACTIVE_DURATION, tween_norm);
          }
        }

        case .ACTIVE; {
          it.trap_animation.proc(*it.trap_animation, frame_time);
          if !it.trap_animation.active {
            it.trap_state = .RETRACT;
            it.trap_animation = tween_init(TILE_SPIKE_TRAP_RETRACT_TIME, tween_norm);
          }
        }

        case .RETRACT; {
          t := it.trap_animation.proc(*it.trap_animation, frame_time);
          it.trap_display_offset = lerp(0, TILE_SPIKE_TRAP_HIDDEN_OFFSET, t);
          if !it.trap_animation.active {
            it.trap_state = .HIDDEN;
          }
        }
      }
    }
  }

  for :visible_only map {
    if it.tex == null then continue;
    if it.collected then continue;
    dsp := it_index * TILE_RESOLUTION;
    dsp.y += it.bob_amplitude * sin(it.bob_frequency * it.bob_counter);
    dsp.y -= it.trap_display_offset;
    draw_quad(renderer, it.tex, {xy = dsp - cam_pos, zw = xy(TILE_RESOLUTION)}, rotation_angle = it.rotation);
  }

  {
    player.flip = globals.player_flip.proc(*globals.player_flip, frame_time);
    dsp_pos := lerp(old_map.player.pos, player.pos, alpha) - cam_pos;
    dsp_dim := player.bbox_dim;
    dsp_dim.x *= abs(cos(PI * player.flip));
    dsp_pos.x += (player.bbox_dim.x - dsp_dim.x) / 2;
    toggle := player.facing > 0;
    if player.flip > 0.5 then toggle = !toggle;
    show := true;
    if player.damage_cooldown < PLAYER_GRACE_PERIOD {
      blink := cast(int) (player.damage_cooldown / PLAYER_BLINK_TIME);
      if blink % 2 == 0 then show = false; else show = true;
    }
    if show then draw_quad(renderer, player.texture[player.health], {xy = dsp_pos, zw = dsp_dim}, flip_x = toggle);
  }

  {
    scoreboard_x := 0.0;
    SCOREBOARD_Y :: 315;
    SCOREBOARD_RESOLUTION :: TILE_RESOLUTION * 1.5;
    draw_quad(renderer, globals.coin_texture, {xy = xy(scoreboard_x, SCOREBOARD_Y), zw = xy(SCOREBOARD_RESOLUTION)});
    scoreboard_x += TILE_RESOLUTION;
    draw_quad(renderer, globals.x_texture, {xy = xy(scoreboard_x, SCOREBOARD_Y), zw = xy(SCOREBOARD_RESOLUTION)});
    scoreboard_x += TILE_RESOLUTION;

    coin_digits := get_digits_right_to_left_on_temp(map.coins_collected);
    for < coin_digits {
      draw_quad(renderer, globals.number_textures[it], {xy = xy(scoreboard_x, SCOREBOARD_Y), zw = xy(SCOREBOARD_RESOLUTION)});
      scoreboard_x += TILE_RESOLUTION / 1.8;
    }
  }

  for logs_to_display {
    alpha := it.fade.proc(*it.fade, frame_time);
    SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 1 - alpha);
    SDL_RenderDebugText(renderer, 20.0, RESOLUTION_H - (20.0 + it_index * 10), temp_c_string(it.message));
    if alpha <= 0.0 {
      free(it.message);
      remove it;
    }
  }

  SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 1);
  SDL_RenderDebugTextFormat(renderer, RESOLUTION_W - 100, 20, "FPS: %", cast(int,1.0/frame_time));
}

bean_game_main_menu_tick_and_render :: (renderer: *SDL_Renderer) {
  collect_input();
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderClear(renderer);
  defer SDL_RenderPresent(renderer);


}

main_loop :: (ctx: *void) #c_call {
  push_context,defer_pop cast(*#Context) ctx;
  if !globals.should_run {
    #if EMSCRIPTEN {
      emscripten_cancel_main_loop();
    } else {
      exit(0);
    }
  }

  reset_temporary_storage();

  dt :: 0.01; // 100 hz
  end := SDL_GetTicks();
  frame_time := (end - globals.start) / 1000.0;
  globals.start = end;

  if globals.game_state == .MENU {
    bean_game_main_menu_tick_and_render(globals.renderer);
  } else if globals.game_state == .PLAY {
    globals.accumulator += frame_time;
    while globals.accumulator >= dt {
      globals.old_map = globals.map; // TODO: Expensive state copy, can we change this to be a pointer swap?
      bean_game_tick(*globals.map, dt);
      globals.accumulator -= dt;
    }

    alpha := saturate(globals.accumulator / dt);
    bean_game_render(globals.renderer, *globals.map, *globals.old_map, frame_time, xx alpha);
  }
}

main :: () {
  using globals;

  bg_logger_init_and_equip();
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

  window_width  := RESOLUTION_W * 2;
  window_height := RESOLUTION_H * 2;
  window := SDL_CreateWindow("Bean Game", xx window_width, xx window_height, SDL_WINDOW_RESIZABLE);
  renderer = SDL_CreateRenderer(window, null);
  SDL_SetRenderLogicalPresentation(renderer, RESOLUTION_W, RESOLUTION_H, .INTEGER_SCALE);
  SDL_SetRenderVSync(renderer, 1);

  texture_atlas = load_assets(renderer);
  coin_texture = atlas_fetch(*texture_atlas, "tile_coin"); // Duplicate
  x_texture = atlas_fetch(*texture_atlas, "ui_numX");
  ui_box_texture = atlas_fetch(*texture_atlas, "ui_box");
  ui_select_texture = atlas_fetch(*texture_atlas, "ui_select");

  number_textures = *SDL_Texture.[atlas_fetch(*texture_atlas, "ui_num0"),
                                    atlas_fetch(*texture_atlas, "ui_num1"),
                                    atlas_fetch(*texture_atlas, "ui_num2"),
                                    atlas_fetch(*texture_atlas, "ui_num3"),
                                    atlas_fetch(*texture_atlas, "ui_num4"),
                                    atlas_fetch(*texture_atlas, "ui_num5"),
                                    atlas_fetch(*texture_atlas, "ui_num6"),
                                    atlas_fetch(*texture_atlas, "ui_num7"),
                                    atlas_fetch(*texture_atlas, "ui_num8"),
                                    atlas_fetch(*texture_atlas, "ui_num9")];
  cosmetic_textures = *SDL_Texture.[atlas_fetch(*texture_atlas, "item_hat"),
                                      atlas_fetch(*texture_atlas, "item_hatTop"),
                                      atlas_fetch(*texture_atlas, "item_helmet"),
                                      atlas_fetch(*texture_atlas, "item_helmetModern")];
  start = SDL_GetTicks();
  random_seed(cast,trunc(u32) SDL_GetPerformanceCounter());
  backing_buffer_size := 1048576; // 1 mebibyte
  backing_buffer := alloc(backing_buffer_size);
  level_arena = arena_alloc(backing_buffer, backing_buffer_size);
  map = map_load(level_arena, texture_atlas);

  player := *map.player;

  FRICTION_COEFFICIENT :: 1.8;
  floor_friction_force = xy(-1 * GRAVITY * player.mass * FRICTION_COEFFICIENT, 0);
  wall_friction_force = xy(0, 3000 * player.mass);
  move_force = floor_friction_force;
  accel_force = player.mass * xy(PLAYER_MOVE_SPEED / PLAYER_TIME_TO_ACCELERATE, 0) + floor_friction_force;

  player_flip = tween_init(0.2, tween_norm);

  map.cam.offset = {RESOLUTION_W/2, RESOLUTION_H/2};
  map.cam.offset -= player.bbox.zw / 2;
  cam_set_target(*map.cam, player);

  collision_events = collision_event_memory;
  should_run = true;

  #if EMSCRIPTEN {
    emscripten_set_main_loop_arg(main_loop, *context, 0, true);
  } else {
    while true {
      #if ENABLE_LEVEL_HOT_RELOAD {
        using File_Utilities :: #import "File_Utilities";
        modtime, _, success := file_modtime_and_size(map.path);
        if modtime > map.last_modification {
          // TODO: This is very naive and should definitely be optimized in the near future
          log("Reloading level...");
          backup_pos := arena_pos(level_arena);
          new_map := map_load(level_arena, texture_atlas);
          if new_map.tiles.data != null {
            old_player_pos := map.player.pos;
            arena_clear(level_arena);
            map = map_load(level_arena, texture_atlas, modtime);
            map.cam.offset = {RESOLUTION_W/2, RESOLUTION_H/2};
            map.cam.offset -= player.bbox.zw / 2;
            player = *map.player;
            cam_set_target(*map.cam, player);
            player.pos = old_player_pos;
          } else {
            arena_pop_to(level_arena, backup_pos);
            map.last_modification = modtime;
          }
        }
      }
      main_loop(*context);
    }
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

  write_string(message);
  if message[message.count-1] != #char "\n" then write_string("\n");

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
  #import "emscripten";
}