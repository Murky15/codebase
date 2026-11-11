/* TODO:
  What if maze generation was animated after the player clicks start?
  - [X] Fixed point
  - [X] Trig meta tables
  - [X] Collision
*/

#define RAYCAST_RESOLUTION 32

#define PLAYER_RADIUS 0.15625_fx16
#define PLAYER_SPEED  0.005_fx16
#define PLAYER_ROTATE_SPEED 65
struct Player {
  Vec2_Fix16 pos;
  u16 view_angle;
};

struct Ray {
  Vec2_Fix16 pos;
  Vec2_Fix16 dir;
};

enum Raycast_Collision_Type : u32 {
  RAYCAST_NULL,
  RAYCAST_WALL_NS,
  RAYCAST_WALL_EW,

  RAYCAST_COUNT
};

struct Raycast_Result {
  Raycast_Collision_Type collision;
  fix16 length;
};

global struct {
  Player player;
  Player old_player;
} gs;

#define MAP_WIDTH  10
#define MAP_HEIGHT 10
StaticAssert(MAP_WIDTH == MAP_HEIGHT, check_map_dimensions);
global const PROGMEM u8 test_map[MAP_WIDTH * MAP_HEIGHT] = {
  1,0,0,0,0,1,0,0,0,1,
  0,0,0,1,0,0,1,1,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,1,1,1,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,1,0,0,0,
  0,0,0,0,0,0,1,0,0,0,
  0,1,0,0,0,0,1,0,0,0,
  0,0,0,1,1,0,1,0,0,1,
  1,0,0,0,0,0,0,0,0,1,
};

function void
r_vert (u16 x, u16 y0, u16 y1, bool dim) {
  Assert(y0 <= y1);
  for (u16 y = y0; y <= y1; ++y) {
    r_set_pixel(x, y, dim);
  }
}

function bool
map_index (s16 x, s16 y) {
  return PGMAccessU8(test_map + (y * MAP_WIDTH + x));
}

function bool
map_index (Vec2_Fix16 pos) {
  return map_index(pos.x.to_int(), pos.y.to_int());
}

function Raycast_Result
raycast (Ray ray) {
  // NOTE: Apply digital differential analysis to find wall intersections.
  s16 map_x = ray.pos.x.to_int(), map_y = ray.pos.y.to_int();
  s16 step_x = 0, step_y = 0;

  fix32 ray_pos_x = fix32::from_raw(ray.pos.x.val);
  fix32 ray_pos_y = fix32::from_raw(ray.pos.y.val);
  fix32 ray_dir_x = fix32::from_raw(ray.dir.x.val);
  fix32 ray_dir_y = fix32::from_raw(ray.dir.y.val);

  fix32 dx = ray.dir.x == 0.0_fx16 ? fix32::max() : fix32_abs(fix32_div_lazy(1.0_fx32,ray_dir_x));
  fix32 dy = ray.dir.y == 0.0_fx16 ? fix32::max() : fix32_abs(fix32_div_lazy(1.0_fx32,ray_dir_y));
  fix32 length_x = fix32::max(), length_y = fix32::max();

  if (ray.dir.x > 0.0_fx16) {
    step_x = 1;
    length_x = fix32_mul_lazy(fix32_sub(fix32::from_int(map_x + 1), ray_pos_x), dx);
  } else if (ray.dir.x < 0.0_fx16) {
    step_x = -1;
    length_x = fix32_mul_lazy(fix32_sub(ray_pos_x, fix32::from_int(map_x)), dx);
  }
  if (ray.dir.y > 0.0_fx16) {
    step_y = 1;
    length_y = fix32_mul_lazy(fix32_sub(fix32::from_int(map_y + 1), ray_pos_y), dy);
  } else if (ray.dir.y < 0.0_fx16) {
    step_y = -1;
    length_y = fix32_mul_lazy(fix32_sub(ray_pos_y, fix32::from_int(map_y)), dy);
  }

  bool collided = false, side = 0; // NOTE: 0 - Vertical boundary, 1 - Horizontal boundary
  for (;;) {
    if (length_x < length_y) {
      length_x.val += dx.val;
      map_x += step_x;
      side = 0;
    } else {
      length_y.val += dy.val;
      map_y += step_y;
      side = 1;
    }

    bool outside_bounds = (map_x < 0 || map_x >= MAP_WIDTH || map_y < 0 || map_y >= MAP_HEIGHT);
    if (outside_bounds || map_index(map_x, map_y)) {
      collided = !outside_bounds;
      break;
    }
  }

  Raycast_Result result = {};
  if (collided) {
    if (side == 0) {
      result.collision = RAYCAST_WALL_NS;
      result.length = fix16::from_raw(length_x.val - dx.val);
    } else {
      result.collision = RAYCAST_WALL_EW;
      result.length = fix16::from_raw(length_y.val - dy.val);
    }
  }

  return result;
}

global s16 resolution_width, resolution_height;

function void
game_init (u16 width, u16 height) {
  gs.player.pos = v2fx16(5.0_fx16,5.0_fx16);
  resolution_width = width;
  resolution_height = height;
}

function void
game_tick (Game_Input input, u16 dt) {
  u16 rotation = 0;
  if (input.rotate_ccw) rotation -= PLAYER_ROTATE_SPEED * dt;
  if (input.rotate_cw)  rotation += PLAYER_ROTATE_SPEED * dt;
  gs.player.view_angle += rotation;

  Vec2_Fix16 move_amt = {};
  fix16 x = 0.0_fx16, y = 0.0_fx16;
  if (input.move_forward) x += 1.0_fx16;
  if (input.strafe_left)  y -= 1.0_fx16;
  if (input.move_back)    x -= 1.0_fx16;
  if (input.strafe_right) y += 1.0_fx16;
  fix16 dx = x * fix16_cos(gs.player.view_angle) - y * fix16_sin(gs.player.view_angle);
  fix16 dy = x * fix16_sin(gs.player.view_angle) + y * fix16_cos(gs.player.view_angle);
  move_amt.x = dx;
  move_amt.y = dy;
  if (v2fx16_len(move_amt) > 1.0_fx16)
    move_amt = v2fx16_norm(move_amt);
  gs.player.pos = v2fx16_add(gs.player.pos, v2fx16_muls(move_amt, PLAYER_SPEED * fix16::from_int(dt)));

  Vec2_Fix16 map_pos = v2fx16(fix16_floor(gs.player.pos.x), fix16_floor(gs.player.pos.y));
  Vec2_Fix16 collision_queries[] = {
    v2fx16(map_pos.x+1.0_fx16, map_pos.y),
    v2fx16(map_pos.x-1.0_fx16, map_pos.y),
    v2fx16(map_pos.x, map_pos.y+1.0_fx16),
    v2fx16(map_pos.x, map_pos.y-1.0_fx16),

    v2fx16(map_pos.x-1.0_fx16, map_pos.y+1.0_fx16),
    v2fx16(map_pos.x+1.0_fx16, map_pos.y+1.0_fx16),
    v2fx16(map_pos.x-1.0_fx16, map_pos.y-1.0_fx16),
    v2fx16(map_pos.x+1.0_fx16, map_pos.y-1.0_fx16),
  };
  for EachInStaticArray(collision_queries) {
    if (!map_index(it)) continue;

    Rect_Fix16 tile;
    tile.xy = it;
    tile.wh = v2fx16(1.0_fx16,1.0_fx16);

    if (rect_circle_intersect(tile, gs.player.pos, PLAYER_RADIUS)) {
      Vec2_Fix16 diff = v2fx16_sub(gs.player.pos, gs.old_player.pos);
      Vec2_Fix16 test_x = v2fx16_add(gs.old_player.pos, {diff.x, 0.0_fx16});
      Vec2_Fix16 test_y = v2fx16_add(gs.old_player.pos, {0.0_fx16, diff.y});

      fix16 buffer = 0.01_fx16;
      fix16 overlap_x, overlap_y;
      if (rect_circle_intersect(tile, test_y, PLAYER_RADIUS, &overlap_y)) {
        overlap_y += buffer;
        if (diff.y > 0.0_fx16) {
          gs.player.pos.y -= overlap_y;
        } else {
          gs.player.pos.y += overlap_y;
        }
      } else if (rect_circle_intersect(tile, test_x, PLAYER_RADIUS, &overlap_x)) {
        overlap_x += buffer;
        if (diff.x > 0.0_fx16) {
          gs.player.pos.x -= overlap_x;
        } else {
          gs.player.pos.x += overlap_x;
        }
      }
    }
  }

  gs.old_player = gs.player;
}

struct Debug_Render_Data {
  struct {
    f32 angle, length;
  } rays[RAYCAST_RESOLUTION];
};

function Debug_Render_Data*
game_render () {
  Vec2_Fix16 local_pos = gs.player.pos;

#if OS_WINDOWS
  local_persist Debug_Render_Data debug = {};
#endif

  for (u16 ray_idx = 0, x = 0; ray_idx < RAYCAST_RESOLUTION; ++ray_idx, x += PIXELS_PER_RAYCAST) {
    u16 relative_angle = PGMAccessU16(ray_angle_lookup + ray_idx);
    u16 angle = gs.player.view_angle + relative_angle;

    Vec2_Fix16 dir = v2fx16(fix16_cos(angle), fix16_sin(angle));
    Ray ray = {local_pos, dir};
    Raycast_Result cast = raycast(ray);
    fix16 dist = cast.length;
#if OS_WINDOWS
    debug.rays[ray_idx] = {angle * ANGLE_U16TOF32, dist == 0.0_fx16 ? 100.f : dist.to_f32()};
#endif
    if (cast.collision == RAYCAST_NULL) continue;

    fix16 perp_dist = dist * fix16_cos(relative_angle);
    perp_dist = Max(fix16::from_raw(1), perp_dist);

    s16 wall_height = fix32_div_lazy(CAMERA_FOCAL_LENGTH, fix32::from_raw(perp_dist.val)).to_int();
    s16 y0 = Max(resolution_height/2 - wall_height/2, 0);
    s16 y1 = Min(resolution_height/2 + wall_height/2, resolution_height);

    for (u16 p = 0; p < PIXELS_PER_RAYCAST; ++p)
      r_vert(x+p, y0, y1, cast.collision == RAYCAST_WALL_EW);
  }

#if OS_WINDOWS
  return &debug;
#else
  return nullptr;
#endif
}