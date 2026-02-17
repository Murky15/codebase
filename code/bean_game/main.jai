/* NOTE -- About this project
Today is valentine's day, and my girlfriend is on a class trip to Italy.
She said she wanted a handmade gift, so I have one week to make this game
for her before she gets back.
*/

/* TODO
- [ ] Levels stored as text files
*/

AARect :: #type,isa Vector4;

Entity :: struct {
  pos: Vector2;
  bbox_dim := Vector2.{TILE_RESOLUTION, TILE_RESOLUTION};
  velocity: Vector2;
  jumping: bool;

  #overlay (pos) bbox: AARect;
}

Tile_Type :: enum {
  INVALID;
  NONE;
  GROUND;
}

Map :: struct {
  tiles: [] Tile_Type;
  width, height: int;
}

TILE_RESOLUTION :: 64;
PLAYER_MOVE_SPEED :: 500.0;
PLAYER_JUMP_VELOCITY :: 1600.0;
GRAVITY :: -4200.0;

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

map_create :: (playfield_width: int, playfield_height: int) -> Map {
  map_data := NewArray(playfield_width * playfield_height, Tile_Type);
  return {map_data, playfield_width, playfield_height};
}

map_index :: (map: Map, p: Vector2) -> Tile_Type {
  if (p.y < 0 || p.y >= xx map.height) ||
     (p.x < 0 || p.x >= xx map.width) return .INVALID;

  return map.tiles[xx p.y * map.width + xx p.x];
}

main :: () {
  perm_arena: Flat_Pool;
  arena_allocator := Allocator.{proc=flat_pool_allocator_proc, data=*perm_arena};
  context.allocator = arena_allocator;

  main_window := create_window(1280, 720, "Bean Game");
  window_width, window_height := Simp.get_render_dimensions(main_window);
  Simp.set_render_target(main_window);

  // NOTE: For now let's assume that the world is static
  // This is a very simple way to represent the world, and must be refined in the future.
  playfield_width := window_width / TILE_RESOLUTION;
  playfield_height := window_height / TILE_RESOLUTION;
  map := map_create(playfield_width, playfield_height);

  for y: 0..playfield_height-1 {
    for x: 0..playfield_width-1 {
      if y == 2 && x < 6 {
        map.tiles[y * playfield_width + x] = .GROUND;
      }
      if y == 2 && x > 13 {
        map.tiles[y * playfield_width + x] = .GROUND;
      }
      if (y == 6 || y == 5 || y == 4) && x > 6 && x < 13 {
        map.tiles[y * playfield_width + x] = .GROUND;
      }
    }
  }

  player: Entity;
  player.pos = xy(0, 250);
  player.jumping = true;

  should_run := true;
  input_move_left, input_move_right, input_jump: bool;
  start := seconds_since_init();
  while should_run {
    reset_temporary_storage();
    Input.update_window_events();

    for Input.events_this_frame {
      if it.type == .QUIT should_run = false;

      if it.type == .KEYBOARD {
        if it.key_code == #char "A" then input_move_left  = xx it.key_pressed;
        if it.key_code == #char "D" then input_move_right = xx it.key_pressed;
        if it.key_code == .SPACEBAR then input_jump       = xx it.key_pressed;
        if it.key_code == #char "R" then { player.pos = xy(0, 250); player.jumping = true; };
      }
    }

    {
      end := seconds_since_init();
      dt := cast(float, end - start);
      start = end;

      old_player := player;

      if input_jump && !player.jumping {
        player.jumping = true;
        player.velocity.y = PLAYER_JUMP_VELOCITY;
      }

      if player.jumping {
        player.velocity.y += GRAVITY * dt;
      }

      dir: float;
      if input_move_left  dir -= 1;
      if input_move_right dir += 1;

      player.pos.x += dir * PLAYER_MOVE_SPEED * dt;
      player.pos.y += player.velocity.y * dt;
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
            player.pos.x += -dir * overlap.z;
          }
        }
      }
    }

    {
      Simp.clear_render_target(1, 1, 1, 1);
      defer Simp.swap_buffers(main_window);

      Simp.set_shader_for_color();
      for y: 0..playfield_height-1 {
        for x: 0..playfield_width-1 {
          tile := map_index(map, {xx x, xx y});
          if tile == .GROUND {
            dsp_x := cast(float) x * TILE_RESOLUTION;
            dsp_y := cast(float) y * TILE_RESOLUTION;
            Simp.immediate_quad(dsp_x + 1, dsp_y, dsp_x + TILE_RESOLUTION, dsp_y + TILE_RESOLUTION, {0, 0, 0, 1});
          }
        }
      }
      Simp.immediate_quad(player.pos.x, player.pos.y, player.pos.x + TILE_RESOLUTION, player.pos.y + TILE_RESOLUTION, {.5, .5, .5, 1});
    }
  }
}

#import "Basic";
#import "Window_Creation";
#import "Flat_Pool";
#import "Math";
Input :: #import "Input";
Simp  :: #import "Simp";