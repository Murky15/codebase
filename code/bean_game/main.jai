/* NOTE -- About this project
Today is valentine's day, and my girlfriend is on a class trip to Italy.
She said she wanted a handmade gift, so I have one week to make this game
for her before she gets back.
*/

AARect :: struct {
  using v: Vector4;
  #overlay (z) width: float;
  #overlay (w) height: float;
}

Entity :: struct {
  pos: Vector2;
  bbox_dim: Vector2;
  velocity: Vector2;
  jumping: bool;

  #overlay (pos) bbox: AARect;
}

Tile_Type :: enum {
  NONE;
  GROUND;
}

PLAYER_MOVE_SPEED :: 500.0;
PLAYER_JUMP_VELOCITY :: 1400.0;
GRAVITY :: -4000.0;

operator > :: (a: Vector2, b: Vector2) -> bool {
  return a.x > b.x && a.y > b.y;
}

rects_intersect :: (a: AARect, b: AARect) -> bool, AARect {
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

main :: () {
  perm_arena: Flat_Pool;
  arena_allocator := Allocator.{proc=flat_pool_allocator_proc, data=*perm_arena};
  context.allocator = arena_allocator;

  main_window := create_window(1280, 720, "Bean Game");
  window_width, window_height := Simp.get_render_dimensions(main_window);
  Simp.set_render_target(main_window);

  // NOTE: For now let's assume that the world is static
  // This is a very simple way to represent the world, and must be refined in the future.
  tile_resolution :: 64;
  playfield_width := window_width / tile_resolution;
  playfield_height := window_height / tile_resolution;
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

    if input_jump && !player.jumping {
      player.jumping = true;
      player.velocity.y = PLAYER_JUMP_VELOCITY;
    }
    if player.jumping {
      player.velocity.y += GRAVITY * dt;
    }
    player.pos.y = max(0.0, player.pos.y);
    if player.pos.y == 0 then player.jumping = false;

    dir: float;
    if input_move_left  dir -= 1;
    if input_move_right dir += 1;
    player.pos.x += dir * PLAYER_MOVE_SPEED * dt;
    player.pos.y += player.velocity.y * dt;

    Simp.clear_render_target(1, 1, 1, 1);
    Simp.set_shader_for_color();
    for y: 0..playfield_height-1 {
      for x: 0..playfield_width-1 {
        tile := map_data[y * playfield_width + x];
        if tile == .GROUND {
          dsp_x := cast(float) x * tile_resolution;
          dsp_y := cast(float) y * tile_resolution;
          Simp.immediate_quad(dsp_x + 1, dsp_y, dsp_x + tile_resolution, dsp_y + tile_resolution, {0, 0, 0, 1});
        }
      }
    }
    Simp.immediate_quad(player.pos.x, player.pos.y, player.pos.x + tile_resolution, player.pos.y + tile_resolution, {.5, .5, .5, 1});
    Simp.swap_buffers(main_window);
  }
}

#import "Basic";
#import "Window_Creation";
#import "Flat_Pool";
#import "Math";
Input :: #import "Input";
Simp  :: #import "Simp";