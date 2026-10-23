/* TODO:
  What if maze generation was animated after the player clicks start?
*/

#define RAYCAST_FOV (M_PI/3.f)
#define RAYCAST_RESOLUTION 32

#define PLAYER_RADIUS 10
#define PLAYER_SPEED  250
#define PLAYER_ROTATE_SPEED (180.f*DEG2RAD)
struct Player {
  Vec2 pos;
  f32  view_angle;
};

struct Ray {
  Vec2 pos;
  Vec2 dir;
};

typedef u32 Raycast_Collision_Type;
enum {
  RAYCAST_NULL,
  RAYCAST_WALL_NS,
  RAYCAST_WALL_EW,

  RAYCAST_COUNT
};

struct Raycast_Result {
  Raycast_Collision_Type collision;
  f32 length;
};

global struct {
  Player player;
} gs;

#define MAP_SCALE 2
#define MAP_TILE_SIZE (32 * MAP_SCALE)
#define MAP_WIDTH  10
#define MAP_HEIGHT 10
StaticAssert(MAP_WIDTH == MAP_HEIGHT, check_map_dimensions);
global u8 test_map[MAP_HEIGHT][MAP_WIDTH] = {
  {1,0,0,0,0,1,0,0,0,1},
  {0,0,0,1,0,0,1,1,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,1,1,1,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,1,0,0,0},
  {0,0,0,0,0,0,1,0,0,0},
  {0,1,0,0,0,0,1,0,0,0},
  {0,0,0,1,1,0,1,0,0,1},
  {1,0,0,0,0,0,0,0,0,1},
};

function void
r_circle_2d (Vec2 pos, s32 r, Vec3 color) {
  for (s32 y = -r; y <= r; ++y) {
    for (s32 x = -r; x <= r; ++x) {
      if (sqrtf((f32)(x*x + y*y)) <= r) {
        r_set_pixel(v2add(pos, V2((f32)x,(f32)y)), color);
      }
    }
  }
}

function void
r_impl_line_2d_low (Vec2 p0, Vec2 p1, Vec3 color) {
  f32 dx = p1.x - p0.x;
  f32 dy = p1.y - p0.y;
  f32 yi = 1.f;
  if (dy < 0) {
    yi = -1.f;
    dy = -dy;
  }
  f32 d = 2.f*dy - dx;
  f32 y = p0.y;

  for (f32 x = p0.x; x <= p1.x; ++x) {
    r_set_pixel(V2(x,y), color);
    if (d > 0) {
      y += yi;
      d += (2.f * (dy - dx));
    } else {
      d += 2.f * dy;
    }
  }
}

function void
r_impl_line_2d_high (Vec2 p0, Vec2 p1, Vec3 color) {
  f32 dx = p1.x - p0.x;
  f32 dy = p1.y - p0.y;
  f32 xi = 1.f;
  if (dx < 0) {
    xi = -1.f;
    dx = -dx;
  }
  f32 d = 2.f*dx - dy;
  f32 x = p0.x;

  for (f32 y = p0.y; y <= p1.y; ++y) {
    r_set_pixel(V2(x,y), color);
    if (d > 0) {
      x += xi;
      d += (2.f * (dx - dy));
    } else {
      d += 2.f * dx;
    }
  }
}

function void
r_line_2d (Vec2 p0, Vec2 p1, Vec3 color) {
  if (fabs(p1.y - p0.y) < fabs(p1.x - p0.x)) {
    if (p0.x > p1.x)
      r_impl_line_2d_low(p1, p0, color);
    else
      r_impl_line_2d_low(p0, p1, color);
  } else {
    if (p0.y > p1.y)
      r_impl_line_2d_high(p1, p0, color);
    else
      r_impl_line_2d_high(p0, p1, color);
  }
}

function void
r_line_2d (Vec2 p0, f32 angle, f32 length, Vec3 color) {
  f32 x = p0.x, y = p0.y;
  x = cosf(angle) * length;
  y = sinf(angle) * length;

  r_line_2d(p0, v2add(p0, V2(x,y)), color);
}

function void
r_vert (f32 x, f32 y0, f32 y1, Vec3 color) {
  Assert(y0 <= y1);
  for (f32 y = round(y0); y <= round(y1); ++y) {
    r_set_pixel(V2((f32)x,(f32)y), color);
  }
}

function f32
fmod_cycling (f32 x, f32 y) {
  Assert(y != 0, "Cannot fmod_cycling with denominator 0.");
  f32 remainder = x - (floor(x/y) * y);

  return remainder;
}

function Raycast_Result
raycast (Ray ray) {
  // NOTE: Apply digital differential analysis to find wall intersections.
  Vec2 map_pos = V2(floorf(ray.pos.x), floorf(ray.pos.y));
  Vec2 delta = V2(fabsf(1.f/ray.dir.x), fabsf(1.f/ray.dir.y));
  Vec2 step = {}, length = {};
  if (ray.dir.x > 0) {
    step.x = 1;
    length.x = ((map_pos.x + 1.f) - ray.pos.x) * delta.x;
  } else if (ray.dir.x != 0) {
    step.x = -1;
    length.x = (ray.pos.x - map_pos.x) * delta.x;
  }
  if (ray.dir.y > 0) {
    step.y = 1;
    length.y = ((map_pos.y + 1.f) - ray.pos.y) * delta.y;
  } else if (ray.dir.y != 0) {
    step.y = -1;
    length.y = (ray.pos.y - map_pos.y) * delta.y;
  }

  int side = 0; // NOTE: 0 - Vertical boundary, 1 - Horizontal boundary
  for (;;) {
    if (length.x < length.y) {
      length.x  += delta.x;
      map_pos.x += step.x;
      side = 0;
    } else {
      length.y  += delta.y;
      map_pos.y += step.y;
      side = 1;
    }

    if ((map_pos.x < 0 || map_pos.x >= MAP_WIDTH || map_pos.y < 0 || map_pos.y >= MAP_HEIGHT) ||
        (test_map[(u16)map_pos.y][(u16)map_pos.x])) break;
  }

  Raycast_Result result = {};
  if (side == 0) {
    result.collision = RAYCAST_WALL_NS;
    result.length = length.x - delta.x;
  } else {
    result.collision = RAYCAST_WALL_EW;
    result.length = length.y - delta.y;
  }

  return result;
}

function void
game_init () {
  gs.player.pos = V2((MAP_WIDTH * MAP_TILE_SIZE) / 2.f, (MAP_HEIGHT * MAP_TILE_SIZE) / 2.f);
  gs.player.view_angle = M_PI/2.f;
}

function void
game_tick (Game_Input input, f32 dt) {
  f32 rotation = 0;
  if (input.rotate_ccw) rotation -= PLAYER_ROTATE_SPEED * dt;
  if (input.rotate_cw)  rotation += PLAYER_ROTATE_SPEED * dt;
  f32 new_angle = gs.player.view_angle + rotation;
  gs.player.view_angle = fmod_cycling(new_angle, 2.f * M_PI);

  Vec2 move_amt = V2();
  f32 x = 0, y = 0;
  if (input.move_forward) x += 1;
  if (input.strafe_left)  y -= 1;
  if (input.move_back)    x -= 1;
  if (input.strafe_right) y += 1;
  move_amt.x = x * cosf(gs.player.view_angle) - y * sinf(gs.player.view_angle);
  move_amt.y = x * sinf(gs.player.view_angle) + y * cosf(gs.player.view_angle);
  if (v2len(move_amt) > 1.f)
    move_amt = v2norm(move_amt);
  gs.player.pos = v2add(gs.player.pos, v2muls(move_amt, PLAYER_SPEED * dt));
}

struct Debug_Render_Data {
  struct {
    f32 angle, length;
  } rays[RAYCAST_RESOLUTION];
};

function Debug_Render_Data*
game_render (u16 resolution_width, u16 resolution_height) {
  Vec2 local_pos = V2(gs.player.pos.x / MAP_TILE_SIZE,
                      gs.player.pos.y / MAP_TILE_SIZE);

  u16 pixels_per_raycast = resolution_width / RAYCAST_RESOLUTION;
  f32 camera_focal_length = (resolution_width/2.f) / tanf(RAYCAST_FOV/2.f);

  local_persist Debug_Render_Data debug = {};

  for (u16 ray_idx = 0, x = 0; ray_idx < RAYCAST_RESOLUTION; ++ray_idx, x += pixels_per_raycast) {
    f32 proj_x = (f32)(x + pixels_per_raycast/2.f) - resolution_width/2.f;
    f32 relative_angle = atan2(proj_x, camera_focal_length);
    f32 angle = gs.player.view_angle + relative_angle;

    Vec2 dir = V2(cosf(angle), sinf(angle));
    Ray ray = {local_pos, dir};
    Raycast_Result cast = raycast(ray);

    f32 dist = cast.length;
    f32 perp_dist = dist * cosf(relative_angle);

    f32 wall_height = camera_focal_length / perp_dist;
    f32 y0 = Max(resolution_height/2.f - wall_height/2.f, 0);
    f32 y1 = Min(resolution_height/2.f + wall_height/2.f, resolution_height);
    Vec3 color = V3(0, 255, 0);
    //if (cast.collision == RAYCAST_WALL_EW) color = v3muls(color, 0.5f);

    debug.rays[ray_idx] = {angle, cast.length};

    for (u16 p = 0; p < pixels_per_raycast; ++p)
      r_vert((f32)(x+p), y0, y1, color);
  }

  return &debug;
}