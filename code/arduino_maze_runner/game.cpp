/* TODO:
  What if maze generation was animated after the player clicks start?
  - [X] Fixed point
  - [X] Trig meta tables
  - [ ] Collision
*/

#define RAYCAST_FOV (M_PI/2.f)
#define RAYCAST_RESOLUTION 32

#define PLAYER_RADIUS 10
#define PLAYER_SPEED  0.005f
#define PLAYER_ROTATE_SPEED 65
struct Player {
  Vec2_Fix16 pos;
  u16  view_angle;
};

struct Ray {
  Vec2_Fix16 pos;
  Vec2_Fix16 dir;
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
  fix16 length;
};

global struct {
  Player player;
} gs;

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
r_vert (u16 x, u16 y0, u16 y1, bool dim) {
  Assert(y0 <= y1);
  for (u16 y = y0; y <= y1; ++y) {
    r_set_pixel(x, y, dim);
  }
}

/*
function f32
fmod_cycling (f32 x, f32 y) {
  Assert(y != 0, "Cannot fmod_cycling with denominator 0.");
  f32 remainder = x - (floor(x/y) * y);

  return remainder;
}
*/

function Raycast_Result
raycast (Ray ray) {
  // NOTE: Apply digital differential analysis to find wall intersections.
  Vec2_Fix16 map_pos = v2fx16(fix16_floor(ray.pos.x), fix16_floor(ray.pos.y));
  fix16 dx = ray.dir.x == 0 ? FIX16_MAX : fix16_abs(1/ray.dir.x);
  fix16 dy = ray.dir.y == 0 ? FIX16_MAX : fix16_abs(1/ray.dir.y);
  Vec2_Fix16 step = {}, length = {};
  if (ray.dir.x > 0) {
    step.x = 1;
    length.x = ((map_pos.x + 1) - ray.pos.x) * dx;
  } else {
    step.x = -1;
    length.x = (ray.pos.x - map_pos.x) * dx;
  }
  if (ray.dir.y > 0) {
    step.y = 1;
    length.y = ((map_pos.y + 1) - ray.pos.y) * dy;
  } else {
    step.y = -1;
    length.y = (ray.pos.y - map_pos.y) * dy;
  }

  int side = 0; // NOTE: 0 - Vertical boundary, 1 - Horizontal boundary
  for (;;) {
    if (length.x < length.y) {
      length.x  += dx;
      map_pos.x += step.x;
      side = 0;
    } else {
      length.y  += dy;
      map_pos.y += step.y;
      side = 1;
    }

    if ((map_pos.x < 0 || map_pos.x >= MAP_WIDTH || map_pos.y < 0 || map_pos.y >= MAP_HEIGHT) ||
        (test_map[Fix16ToInt(map_pos.y)][Fix16ToInt(map_pos.x)])) break;
  }

  Raycast_Result result = {};
  if (side == 0) {
    result.collision = RAYCAST_WALL_NS;
    result.length = length.x - dx;
  } else {
    result.collision = RAYCAST_WALL_EW;
    result.length = length.y - dy;
  }

  return result;
}

global u16 resolution_width, resolution_height;
global u16 pixels_per_raycast;
global s16 camera_focal_length;
// TODO: Could be made smaller
global u16 ray_angle_lookup[RAYCAST_RESOLUTION];

function void
game_init (u16 width, u16 height) {
  gs.player.pos = v2fx16((s16)5,5);

  // NOTE: Precompute projection angles
  pixels_per_raycast = width / RAYCAST_RESOLUTION;
  camera_focal_length = (width/2) / tanf(RAYCAST_FOV/2.f);

  for (u16 ray_idx = 0; ray_idx < RAYCAST_RESOLUTION; ++ray_idx) {
    f32 proj_x = (f32)((ray_idx * pixels_per_raycast) + pixels_per_raycast/2.f) - width/2.f;
    ray_angle_lookup[ray_idx] = atan2(proj_x, camera_focal_length) * ANGLE_F32TOU16;
  }

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
  fix16 x = 0, y = 0;
  if (input.move_forward) x += 1;
  if (input.strafe_left)  y -= 1;
  if (input.move_back)    x -= 1;
  if (input.strafe_right) y += 1;
  fix16 dx = x * fix16_cos(gs.player.view_angle) - y * fix16_sin(gs.player.view_angle);
  fix16 dy = x * fix16_sin(gs.player.view_angle) + y * fix16_cos(gs.player.view_angle);
  move_amt.x = dx;
  move_amt.y = dy;
  if (v2fx16_len(move_amt) > 1)
    move_amt = v2fx16_norm(move_amt);
  gs.player.pos = v2fx16_add(gs.player.pos, v2fx16_muls(move_amt, PLAYER_SPEED * dt));
}

struct Debug_Render_Data {
  struct {
    f32 angle, length;
  } rays[RAYCAST_RESOLUTION];
};

function Debug_Render_Data*
game_render () {
  Vec2_Fix16 local_pos = gs.player.pos;

  local_persist Debug_Render_Data debug = {};

  for (u16 ray_idx = 0, x = 0; ray_idx < RAYCAST_RESOLUTION; ++ray_idx, x += pixels_per_raycast) {
    u16 relative_angle = ray_angle_lookup[ray_idx];
    u16 angle = gs.player.view_angle + relative_angle;

    Vec2_Fix16 dir = v2fx16(fix16_cos(angle), fix16_sin(angle));
    Ray ray = {local_pos, dir};
    Raycast_Result cast = raycast(ray);

    fix16 dist = cast.length;
    fix16 perp_dist = dist * fix16_cos(relative_angle);

    u16 wall_height = camera_focal_length / Fix16ToF32(perp_dist);
    u16 y0 = Max(resolution_height/2 - wall_height/2, 0);
    u16 y1 = Min(resolution_height/2 + wall_height/2, resolution_height);

#ifdef OS_WINDOWS
    debug.rays[ray_idx] = {angle * ANGLE_U16TOF32, Fix16ToF32(dist)};
#endif

    for (u16 p = 0; p < pixels_per_raycast; ++p)
      r_vert(x+p, y0, y1, cast.collision == RAYCAST_WALL_EW);
  }

  return &debug;
}