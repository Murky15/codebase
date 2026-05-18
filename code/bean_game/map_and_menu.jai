Tile_Type :: enum {
  INVALID;
  EMPTY;
  BOUNCE_PAD;
  COIN;
  GROUND;
  HEART;
  PLAYER_SPAWN;
  ROPE;
  ROPE_LONG;
  SPIKE;
  SPIKE_SPINNING;
  SPIKE_TRAP;
  WALL;
  WATER;
}

Collision_Event :: enum {
  NONE;
  FLOOR;
  WALL_SLIDE;
}

Tile :: struct {
  type: Tile_Type;
  tex: *SDL_Texture;
  bbox_dim := Vector2.{TILE_RESOLUTION, TILE_RESOLUTION};
  collected: bool;

  rotation: float;
  angular_speed: float;

  bobs: bool;
  bob_counter: float;
  bob_amplitude: float;
  bob_frequency: float;

  trap_animation: Coroutine(*Tile, float);
  trap_display_offset: float;
}

TILE_TILT_AMPLITUDE :: 5.0;
TILE_SPIKE_SPINNING_ANGULAR_SPEED :: 500.0;
TILE_SPIKE_TRAP_HIDDEN_OFFSET :: TILE_RESOLUTION/1.5;
TILE_SPIKE_TRAP_RELEASE_TIME :: 0.2;
TILE_SPIKE_TRAP_ACTIVE_DURATION :: 2.0;
TILE_SPIKE_TRAP_RETRACT_TIME :: 1.0;

Env_Rope :: struct {
  #as using rope: Rope;
  length: float;

  body: *Rigid_Body; // TODO: Expand this to multiple bodies?
  body_location      := -1;
  body_location_next := -1;
  body_transition: Tween_State;

  transition_velocity := 100.0;
  partial_transition_time: float;

  max_attachment_distance := 10.0;
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

  max_parallax_layers :: 3;
  Parallax_Effect :: struct {
    #as using n: Linked_List_Header;

    layer: u64;
    pos: Vector2;
    size: Vector2;
    rotation: float;
    tex: *SDL_Texture;

    #overlay (pos) bbox: AARect;
  }
  background: Linked_List(Parallax_Effect);

  cam: Camera;
  player: Player;

  free_particle_chunks: Linked_List(Rope.Particle_Chunk);
  environmental_ropes:  Linked_List(Env_Rope);

  tiles: [] Tile;
  width, height: int;
  player_spawn: Vector2;
  coins_collected: int;
}

// NOTE: The `<` on the y loops in these macros is a little hack I came up with
// so that traps would render behind the tiles below them.

for_expansion :: (map: Map, body: Code, flags: For_Flags) #expand {
  #assert !(flags & .REVERSE);
  if map.tiles.data == null then return;
  for < y: 0..map.height-1 {
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

visible_only :: (map: Map, body: Code, flags: For_Flags) #expand {
  #assert !(flags & .REVERSE);
  if map.tiles.data == null return;
  cam_min_bounds := map.cam.pos;
  cam_max_bounds := cam_min_bounds + xy(RESOLUTION_W, RESOLUTION_H);
  cam_min_bounds /= TILE_RESOLUTION;
  cam_max_bounds /= TILE_RESOLUTION;
  min_x := cast(int) cam_min_bounds.x;
  min_y := cast(int) cam_min_bounds.y;
  max_x := cast(int) cam_max_bounds.x;
  max_y := cast(int) cam_max_bounds.y;
  for < y: min_y..max_y {
    for x: min_x..max_x {
      `it_index := Vector2.{xx x, xx y};
      #if flags & .POINTER {
        `it := map_index_pointer(map, it_index);
        if it == null then continue;
      } else {
        `it := map_index(map, it_index);
        if it.type == .INVALID then continue;
      }

      #insert (break=break y) body;
    }
  }
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

// Copy so we don't use inline assembly
my_split_from_left :: (s: string, byte: u8) -> (left: string, right: string) {
  cursor := 0;
  while 1 {
      // If we hit the end, we didn't find it. Bail!
      if cursor >= s.count  return s, "";   // >= in case of data corruption.
      if s[cursor] == byte  break;

      cursor += 1;
  }

  left := s;
  left.count = cursor;

  return left, advance(s, cursor);
}

atlas_fetch :: inline (atlas: *Texture_Atlas, key: string) -> *SDL_Texture {
  return (.*)table_find_pointer(atlas, key);
}

map_load :: (arena: *Arena, textures: Texture_Atlas, last_modification := APOLLO_TIME_INVALID) -> Map {
  // NOTE: Parse text file
  rows_in_header :: 4;
  tis_map :: cast(*Type_Info_Struct) type_info(Map);

  raw_map_cstr_len: u64;
  raw_map_cstr := SDL_LoadFile(map_path, *raw_map_cstr_len);
  raw_map := to_string(raw_map_cstr, xx raw_map_cstr_len);

  map: Map;
  #if ENABLE_LEVEL_HOT_RELOAD {
    map.path = map_path;
    map.last_modification = last_modification;
  }

  #if OS == .WINDOWS {
    newline_size :: 2;
    rows := split(raw_map, "\r\n");
  } else {
    newline_size :: 1;
    rows := split(raw_map, #char "\n");
  }

  for 0..rows_in_header-1 {
    meta_tag := rows[it];
    tag, value := my_split_from_left(meta_tag, #char ":");
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
    raw_map.count -= meta_tag.count+newline_size;
    raw_map.data += meta_tag.count+newline_size;
  }
  rows.count -= rows_in_header;
  rows.data += rows_in_header;

  pitch := rows[0].count;
  for rows {
    if it.count != pitch {
      log_error("Error parsing level data on row %. Expected a line with % characters, recieved %.", it_index+rows_in_header+1, pitch, it.count);
      return {};
    }
  }

  map.width  = pitch;
  map.height = rows.count;
  map.tiles = arena_push(arena, map.width * map.height, Tile);
  for * map {
    // Map data in memory is flipped vertically from file data
    c := raw_map.data[((rows.count-1) - xx it_index.y) * (pitch+newline_size) + xx it_index.x];
    type := Tile_Type.INVALID;
    if c == {
      case #char "#"; type = .EMPTY;
      case #char "b"; type = .BOUNCE_PAD;
      case #char "c"; type = .COIN;
      case #char "g"; type = .GROUND;
      case #char "r"; type = .ROPE; #through;
      case #char "R"; {
        if type == .EMPTY then type = .ROPE_LONG;
        length := ifx type == .ROPE then TILE_RESOLUTION * 3.0 else TILE_RESOLUTION * 6.0;
        anchor_pos := it_index * TILE_RESOLUTION;
        anchor_pos.x += TILE_RESOLUTION / 2;
        anchor_pos.y += 1; // Padding
        create_env_rope(arena, *map, length, anchor_pos);
      }
      case #char "S"; type = .SPIKE_SPINNING;
      case #char "s"; type = .SPIKE;
      case #char "H"; type = .HEART;
      case #char "h"; type = .SPIKE_TRAP;
      case #char "t"; type = .WALL;
      case #char "w"; type = .WATER;
      case #char "p"; {
        type = .PLAYER_SPAWN;
        map.player_spawn = it_index * TILE_RESOLUTION;
      }
      case; {
        log_error("Error parsing level data on row %. Unrecognized map symbol: %", (rows.count+rows_in_header)-it_index.y, to_string(*c,1));
        return {};
      }
    }

    it.type = type;
  }

  // NOTE: Texture pass
  Autotile :: struct {
    tex: *SDL_Texture;
    probability: float;
    default: bool;
    parallax_layer: u64;
  }
  atile :: (tex_name: string, probability := 100.0, default := false, layer := 1) -> Autotile #expand {
    #assert layer <= Map.max_parallax_layers;
    return {atlas_fetch(*textures, tex_name), probability, default, layer};
  }
  atile_choose :: inline (tiles: [] Autotile) -> *SDL_Texture, u64 {
    default: *Autotile;
    rand := cast(float) (random_get() % 100) + 1;
    for tiles {
      rand -= it.probability;
      if rand <= 0 {
        return it.tex, it.parallax_layer;
      } else if it.default then default = *tiles[it_index];
    }

    if default != null {
      return default.tex, default.parallax_layer;
    } else return null, 0;
  }

  // NOTE: Associated with tiles
  above_ground_tiles      := Autotile.[atile("tile_grass"), atile("tile_top"), atile("tile_water")];
  below_ground_tiles      := Autotile.[atile("tile", 50.0, true), atile("tile_diagonal", 16.0), atile("tile_stone", 16.0), atile("tile_border", 16.0)];
  coin_textures           := Autotile.[atile("tile_coin", 50), atile("tile_gem", 50)];
  bounce_pad_texture      := atlas_fetch(*textures, "tile_belt");
  tower_texture           := atlas_fetch(*textures, "tile_brick");
  env_rope_anchor_texture := atlas_fetch(*textures, "tile_crateSmall");
  spike_texture           := atlas_fetch(*textures, "tile_spikes");
  spike_spinning_texture  := atlas_fetch(*textures, "tile_spike");
  heart_texture           := atlas_fetch(*textures, "tile_heart");


  // NOTE: Not associated with tiles
  // TODO: I'm temporarily removing `atile("background_treeLarge", 4.0, layer=2), atile("background_tree", 4.0, layer=2)` from the following list because I can't figure out
  // how to make them look good with parallax scrolling
  environment_textures := Autotile.[atile("tile_tree", 8.0, layer=1), atile("tile_bushHalf", 8.0, layer=1), atile("tile_bush", 8.0, layer=1)];
  cloud_textures       := Autotile.[atile("background_cloudA", 50.0, layer=3), atile("background_cloudB", 50.0, layer=3)];

  for * map {
    above := map_index(map, it_index + {0, 1}).type;
    below := map_index(map, it_index + {0,-1}).type;
    if it.type == {
      case .EMPTY; {
        if below != .GROUND then continue;
        foliage_texture, parallax_layer := atile_choose(environment_textures);
        if foliage_texture == null then continue;
        // NOTE: Can be tweaked
        x_offset := random_get_within_range(-TILE_RESOLUTION/2, TILE_RESOLUTION/2);
        y_offset := random_get_within_range(-TILE_RESOLUTION/4, 0);
        foliage := arena_push(arena, Map.Parallax_Effect);
        foliage.layer = parallax_layer;
        foliage.pos = (it_index * TILE_RESOLUTION) + xy(x_offset, y_offset);
        scale := random_get_within_range(0.7,1.5);
        w, h: float;
        SDL_GetTextureSize(foliage_texture, *w, *h);
        foliage.size = xy(w  * scale /TILE_RESOLUTION_DIVISOR,
                          h * scale /TILE_RESOLUTION_DIVISOR);
        foliage.tex = foliage_texture;
        foliage.rotation = random_get_within_range(-TILE_TILT_AMPLITUDE, TILE_TILT_AMPLITUDE);
        sll_queue_push(*map.background, foliage);
      }
      case .GROUND; {
        it.rotation = random_get_within_range(-TILE_TILT_AMPLITUDE, TILE_TILT_AMPLITUDE);
        it.tex = ifx above != .GROUND && above != .WATER {
          ifx below != .GROUND then above_ground_tiles[1].tex else above_ground_tiles[0].tex;
        } else {
          it.rotation += 90.0 * (random_get() % 4);
          atile_choose(below_ground_tiles);
        }
      }
      case .WATER; {
        it.tex = above_ground_tiles[2].tex;
        it.bobs = true;
        it.bob_frequency = PI / 2;
        it.bob_counter = random_get_within_range(0, (2 * PI) / it.bob_frequency);
        it.bob_amplitude = 5.0;
      }
      case .BOUNCE_PAD; {
        it.tex = bounce_pad_texture;
        it.bbox_dim = {TILE_RESOLUTION, 1.0 /*TILE_RESOLUTION/3*/};
      }
      case .COIN; {
        it.tex = atile_choose(coin_textures);
        it.bobs = true;
        it.bob_frequency = PI;
        it.bob_counter = random_get_within_range(0, (2 * PI) / it.bob_frequency);
        it.bob_amplitude = 3.0;
      }
      case .HEART; {
        it.tex = heart_texture;
        it.bobs = true;
        it.bob_frequency = 2 * PI;
        it.bob_counter = random_get_within_range(0, (2 * PI) / it.bob_frequency);
        it.bob_amplitude = 3.0;
      }
      case .SPIKE_SPINNING; {
        it.tex = spike_spinning_texture;
        it.angular_speed = TILE_SPIKE_SPINNING_ANGULAR_SPEED;
      }

      case .WALL; it.tex = tower_texture;
      case .ROPE_LONG; #through;
      case .ROPE; it.tex = env_rope_anchor_texture;

      case .SPIKE_TRAP; {
        it.trap_display_offset = TILE_SPIKE_TRAP_HIDDEN_OFFSET;
      } #through;
      case .SPIKE; it.tex = spike_texture;
    }
  }

  num_clouds := 40;
  for 1..num_clouds {
    cloud_texture, layer := atile_choose(cloud_textures);
    assert(cloud_texture != null);
    cloud := arena_push(arena, Map.Parallax_Effect);
    cloud.layer = layer;
    w, h: float;
    SDL_GetTextureSize(cloud_texture, *w, *h);
    cloud.size = xy(w,h);
    x := random_get_within_range(-cloud.size.x, (map.width-1.0) * TILE_RESOLUTION);
    y := random_get_within_range(-cloud.size.y, (map.height-1.0) * TILE_RESOLUTION);
    cloud.pos = xy(x,y);
    cloud.tex = cloud_texture;
    sll_queue_push_front(*map.background, cloud);
  }

  map.player.pos = map.player_spawn;
  map.player.texture[2] = atlas_fetch(*textures, "character_roundRed");
  map.player.texture[1] = atlas_fetch(*textures, "character_roundYellow");
  map.player.texture[0] = atlas_fetch(*textures, "character_roundPurple");
  map.player.mass = 10;

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

create_env_rope :: (arena: *Arena, map: *Map, length: float, anchor: Vector2) -> *Env_Rope {
  rope := arena_push(arena, Env_Rope);
  rope_init(rope);
  num_particles := cast(int) round(rope.particle_density * length);
  rope.length = rope.desired_particle_dist * num_particles;
  for i: 0..num_particles-1 {
    particle: rope.Particle;
    if i == 0 {
      particle.invmass = 0;
      particle.fixed = true;
    }
    particle.pos = anchor + xy(i * rope.desired_particle_dist, 0);
    particle.old_pos = particle.pos;
    rope_push_particle(arena, rope, *map.free_particle_chunks, particle);
  }
  sll_queue_push(*map.environmental_ropes, rope);

  rope.partial_transition_time = (1.0 / rope.transition_velocity) * rope.desired_particle_dist;

  return rope;
}

check_env_rope_collision_and_attach :: (rope: *Env_Rope, body: *Rigid_Body, dt: float) -> bool {
  closest_particle := -1;
  closest_distance := rope.max_attachment_distance;
  for :particles rope {
    new_distance := distance(it.pos, body.pos + body.bbox_dim / 2);
    if new_distance < closest_distance {
      closest_particle = it_index;
      closest_distance = new_distance;
    }
  }

  if closest_particle != -1 {
    env_rope_attach_at(rope, body, closest_particle, dt);
    return true;
  }

  return false;
}

env_rope_move_body_up :: (rope: *Env_Rope) {
  if rope.body == null || rope.body_transition.active then return;
  rope.body_transition = tween_init(rope.partial_transition_time, tween_norm_unsaturated);
  rope.body_location_next = max(rope.body_location - 1, 0);
}

env_rope_move_body_down :: (rope: *Env_Rope) {
  if rope.body == null || rope.body_transition.active then return;
  rope.body_transition = tween_init(rope.partial_transition_time, tween_norm_unsaturated);
  rope.body_location_next = min(rope.body_location + 1, rope.num_particles - 1);
}

env_rope_attach_at :: (rope: *Env_Rope, body: *Rigid_Body, index: int, dt: float) {
  particle := *rope.particles[index];
  if !particle.fixed {
    particle.old_pos = particle.pos - body.vel * dt;
  }
  rope.body_location = index;
  rope.body = body;
}

env_rope_detach_body :: (rope: *Env_Rope, dt: float) {
  particle := *rope.particles[rope.body_location];
  particle_vel := (particle.pos - particle.old_pos) / dt;
  particle_vel.y = max(PLAYER_ROPE_JUMP_MIN_VELOCITY, particle_vel.y);
  apply_impulse(rope.body, particle_vel * ROPE_PARTICLE_IMPULSE_SCALE);

  rope.body = null;
  rope.body_location = -1;
  rope.body_location_next = -1;
  rope.body_transition.active = false;
}

simulate_env_rope :: (rope: *Env_Rope, dt: float, still_changing_rope_position: bool) {
  if rope.body != null {
    binding_particle := *rope.particles[rope.body_location];
    additional_accel: Vector2;
    if !binding_particle.fixed {
      additional_accel = rope.body.force / rope.body.mass;
      binding_particle.accel += additional_accel;
      binding_particle.invmass = 1 / rope.body.mass;
    }
    rope_integrate(rope, dt);
    body_pos_p0 := binding_particle.pos - rope.body.bbox_dim / 2;
    rope.body.pos = body_pos_p0;

    if rope.body_transition.active {
      next_particle := *rope.particles[rope.body_location_next];
      body_pos_p1 := next_particle.pos - rope.body.bbox_dim / 2;
      t := rope.body_transition.proc(*rope.body_transition, dt);
      rope.body.pos = lerp(body_pos_p0, body_pos_p1, t);

      // Ended this frame
      if !rope.body_transition.active {
        // NOTE: This is a crappy way to get around a timing bug where the time to scale the rope increases
        // directly with the particle density due to the accumulation of incorrect timekeeping here.
        // There has to be a better way to do this.
        direction := rope.body_location_next - rope.body_location;
        rope.body_location = rope.body_location_next;
        rope.body_location_next = -1;
        if still_changing_rope_position && direction != 0 {
          overflow := (t - 1.0) * rope.body_transition.duration;
          ended_at := rope.body_transition.started_at + rope.body_transition.duration;
          if direction == -1 {
            env_rope_move_body_up(rope);
          } else if direction == 1 {
            env_rope_move_body_down(rope);
          }
          rope.body_transition.started_at = ended_at;
          rope.body_transition.index += overflow;
        }
      }
    }
    if !binding_particle.fixed {
      binding_particle.accel -= additional_accel; // Should this be an assignment to avoid floating point errors?
      binding_particle.invmass = 1;
    }

    rope.body.force = xy(0,0);
  } else rope_integrate(rope, dt);
}

Menu_Box_Click_Event_Proc :: #type (*void);
Menu_Box :: struct {
  rect: SDL_FRect;
  draw_border := true;
  filled: bool;

  union {
    text: string;
    sprite: *SDL_Texture;
  }

  content_pos: Vector2;
  content_scale: float;

  interactive: bool;
  user_data: *void;
  on_click: Menu_Box_Click_Event_Proc;
}

Menu :: struct {
  dim: SDL_FRect;
  font: Bitmap_Font;
  boxes: [..] Menu_Box;
}

MENU_LAYOUT_CENTER :: -1;
Menu_Autolayout :: enum {
  NONE;
  HORIZONTAL;
  VERTICAL;
}

menu_frame_begin :: (menu: *Menu, dim: SDL_FRect, font: Bitmap_Font) {

}

menu_push_autolayout :: (menu: *Menu, anchor: float, spacing: float) {

}

menu_add_box :: (menu: *Menu, x: float, y: float, text: string, scale := 1.0) -> *Menu_Box {

}

menu_add_box  :: (menu: *Menu, dim: Vector2, text: string) -> *Menu_Box {

}

menu_add_box  :: (menu: *Menu, dim: Vector2, text: string, on_click: Menu_Box_Click_Event_Proc, user_data := null) -> *Menu_Box {

}

menu_submit_and_process_input :: () {

}