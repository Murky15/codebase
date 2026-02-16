/* NOTE -- About this project
Today is valentine's day, and my girlfriend is on a class trip to Italy.
She said she wanted a handmade gift, so I have one week to make this game
for her before she gets back.
*/

AARect :: struct {
  using #as v: Vector4;
  #overlay (z) width: float;
  #overlay (w) height: float;
}

Entity :: struct {
  pos: Vector2;
  bbox_dim := #run xy(TILE_RESOLUTION);
  velocity: Vector2;
  jumping: bool;

  #overlay (pos) bbox: AARect;
}

Tile_Type :: enum {
  NONE;
  GROUND;
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

map_index :: (map: [] Tile_Type, ) {

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
  map_data := NewArray(playfield_width * playfield_height, Tile_Type);
  for y: 0..playfield_height-1 {
    for x: 0..playfield_width-1 {
      if y == 2 && x < 6 {
        map_data[y * playfield_width + x] = .GROUND;
      }
      if y == 2 && x > 13 {
        map_data[y * playfield_width + x] = .GROUND;
      }
      if y == 6 && x > 6 && x < 13 {
        map_data[y * playfield_width + x] = .GROUND;
      }
    }
  }

  player: Entity;
  player.pos = xy(0, 150);

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
      }
    }

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
    for x: 0..1 {
      test := player_grid_pos + xy(xx x, 0);
      if test.y < 0 || test.y >= xx playfield_height  break;
      if test.x < 0 || test.x >= xx playfield_width   continue;
      potential_collision := map_data[xx test.y * playfield_width + xx test.x];
      if potential_collision == .GROUND {
        tile := AARect.{xy = test * TILE_RESOLUTION, zw = xy(TILE_RESOLUTION)};
        colliding, overlap := rects_intersect(player.bbox, tile);
        if colliding {
          player.pos.y += overlap.height;
          player.velocity.y = 0;
          player.jumping = false;
        }
      }
    }

    Simp.clear_render_target(1, 1, 1, 1);
    Simp.set_shader_for_color();
    for y: 0..playfield_height-1 {
      for x: 0..playfield_width-1 {
        tile := map_data[y * playfield_width + x];
        if tile == .GROUND {
          dsp_x := cast(float) x * TILE_RESOLUTION;
          dsp_y := cast(float) y * TILE_RESOLUTION;
          Simp.immediate_quad(dsp_x + 1, dsp_y, dsp_x + TILE_RESOLUTION, dsp_y + TILE_RESOLUTION, {0, 0, 0, 1});
        }
      }
    }
    Simp.immediate_quad(player.pos.x, player.pos.y, player.pos.x + TILE_RESOLUTION, player.pos.y + TILE_RESOLUTION, {.5, .5, .5, 1});
    Simp.swap_buffers(main_window);
  }
}

#import "Basic";
#import "Window_Creation";
#import "Flat_Pool";
#import "Math";
Input :: #import "Input";
Simp  :: #import "Simp";