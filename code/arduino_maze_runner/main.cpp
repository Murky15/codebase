#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#define BASE_IMPLEMENTATION
#include "base.h"

#define RENDER_DEBUG_2D_VIEW 1

#define WINDOW_SCALE 2
#define WINDOW_WIDTH  (MAP_TILE_SIZE * MAP_WIDTH)
#define WINDOW_HEIGHT (MAP_TILE_SIZE * MAP_HEIGHT)

#define RAYCAST_FOV (M_PI/3.f)
#define RAYCAST_RESOLUTION 32
#define PIXELS_PER_RAYCAST (WINDOW_WIDTH / RAYCAST_RESOLUTION)

#define CAMERA_FOCAL_LENGTH ((WINDOW_WIDTH/2.f) / tanf(RAYCAST_FOV/2.f))

struct Bitmap {
  u32 *pixels;
  u64 width, height;
};

struct Player {
  Vec2 pos;
  f32  view_angle;
};
#define PLAYER_RADIUS 10
#define PLAYER_SPEED  250
#define PLAYER_ROTATE_SPEED (90.f*DEG2RAD)

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

#define MAP_TILE_SIZE (32 * WINDOW_SCALE)
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

global struct {
  Arena *perm;
  Arena *frame;

  Bitmap backbuffer;
  SDL_Window  *window;
  SDL_Surface *backbuffer_surface;
  SDL_Surface *display_surface;

  Player player;
} gs;

global bool game_running = true;
global bool move_up, move_down, strafe_left, strafe_right;
global bool rotate_cw, rotate_ccw;

function void
r_test_gradient (Bitmap bitmap) {
  u8 gradient_x = 0, gradient_y = 0;
  for (u64 y = 0; y < bitmap.height; ++y) {
    for (u64 x = 0; x < bitmap.width; ++x) {
      u32 *pixel = &bitmap.pixels[y * bitmap.width + x];
      *pixel = (gradient_x << 24 | gradient_y << 16 | 0 << 8);
      gradient_x++;
    }
    gradient_y++;
  }
}

function void
r_clear (Bitmap bitmap, bool white) {
  memset(bitmap.pixels, white ? 255 : 0, bitmap.width * bitmap.height * sizeof(u32));
}

function void
r_set_pixel (Bitmap bitmap, Vec2 pos, Vec3 color) {
  if ((pos.x >= 0 && pos.x < bitmap.width) &&
      (pos.y >= 0 && pos.y < bitmap.height)) {
    u64 x = (u64)pos.x, y = (u64)pos.y;
    u8 r = (u8)color.x;
    u8 g = (u8)color.y;
    u8 b = (u8)color.z;
    bitmap.pixels[y * bitmap.width + x] = (r << 24 | g << 16 | b << 8);
  }
}

function void
r_circle_2d (Bitmap bitmap, Vec2 pos, s32 r, Vec3 color) {
  for (s32 y = -r; y <= r; ++y) {
    for (s32 x = -r; x <= r; ++x) {
      if (sqrtf((f32)(x*x + y*y)) <= r) {
        r_set_pixel(bitmap, v2add(pos, V2((f32)x,(f32)y)), color);
      }
    }
  }
}

function void
r_impl_line_2d_low (Bitmap bitmap, Vec2 p0, Vec2 p1, Vec3 color) {
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
    r_set_pixel(bitmap, V2(x,y), color);
    if (d > 0) {
      y += yi;
      d += (2.f * (dy - dx));
    } else {
      d += 2.f * dy;
    }
  }
}

function void
r_impl_line_2d_high (Bitmap bitmap, Vec2 p0, Vec2 p1, Vec3 color) {
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
    r_set_pixel(bitmap, V2(x,y), color);
    if (d > 0) {
      x += xi;
      d += (2.f * (dx - dy));
    } else {
      d += 2.f * dx;
    }
  }
}

function void
r_line_2d (Bitmap bitmap, Vec2 p0, Vec2 p1, Vec3 color) {
  if (fabs(p1.y - p0.y) < fabs(p1.x - p0.x)) {
    if (p0.x > p1.x)
      r_impl_line_2d_low(bitmap, p1, p0, color);
    else
      r_impl_line_2d_low(bitmap, p0, p1, color);
  } else {
    if (p0.y > p1.y)
      r_impl_line_2d_high(bitmap, p1, p0, color);
    else
      r_impl_line_2d_high(bitmap, p0, p1, color);
  }
}

function void
r_line_2d (Bitmap bitmap, Vec2 p0, f32 angle, f32 length, Vec3 color) {
  f32 x = p0.x, y = p0.y;
  x = cosf(angle) * length;
  y = sinf(angle) * length;

  r_line_2d(bitmap, p0, v2add(p0, V2(x,y)), color);
}

function void
r_vert (Bitmap bitmap, f32 x, f32 y0, f32 y1, Vec3 color) {
  Assert(y0 <= y1);
  for (f32 y = round(y0); y <= round(y1); ++y) {
    r_set_pixel(bitmap, V2((f32)x,(f32)y), color);
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
        (test_map[(u64)map_pos.y][(u64)map_pos.x])) break;
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

int
main (int argc, char **argv) {
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
  gs.window = SDL_CreateWindow(
    "Arduino Maze Runner",
    WINDOW_WIDTH,
    WINDOW_HEIGHT,
    0
  );
  if (gs.window == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window: %s", SDL_GetError());
    return 1;
  }

  gs.perm  = arena_alloc_default();
  gs.frame = arena_alloc_default();

  u32 *pixels = ArenaPush(gs.perm, u32, WINDOW_WIDTH * WINDOW_HEIGHT);
  gs.backbuffer = Bitmap{pixels, WINDOW_WIDTH, WINDOW_HEIGHT};
  gs.backbuffer_surface = SDL_CreateSurfaceFrom(WINDOW_WIDTH, WINDOW_HEIGHT, SDL_PIXELFORMAT_RGBX8888, (void*)pixels, sizeof(u32) * WINDOW_WIDTH);
  if (gs.backbuffer_surface == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create backbuffer_surface: %s", SDL_GetError());
    return 1;
  }
  Assert(SDL_MUSTLOCK(gs.backbuffer_surface) == 0, "Surface requires locking on this platform");
  gs.display_surface = SDL_GetWindowSurface(gs.window);
  if (gs.display_surface == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not obtain handle to display buffer: %s", SDL_GetError());
    return 1;
  }

  gs.player.pos = V2(WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f);
  gs.player.view_angle = M_PI/2.f;

  u64 last_time = SDL_GetTicks();
  for (;game_running;) {
    for (SDL_Event e; SDL_PollEvent(&e);) {
      switch (e.type) {
        case SDL_EVENT_QUIT: game_running = false; break;

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
          SDL_KeyboardEvent key = e.key;
          if (key.scancode == SDL_SCANCODE_W) move_up      = key.down;
          if (key.scancode == SDL_SCANCODE_A) strafe_left  = key.down;
          if (key.scancode == SDL_SCANCODE_S) move_down    = key.down;
          if (key.scancode == SDL_SCANCODE_D) strafe_right = key.down;

          if (key.scancode == SDL_SCANCODE_Q) rotate_ccw = key.down;
          if (key.scancode == SDL_SCANCODE_E) rotate_cw  = key.down;
        } break;
      }
    }

    u64 current_time = SDL_GetTicks();
    f32 dt = (current_time - last_time) / 1000.f;
    last_time = current_time;

    // NOTE: Process input & move player

    f32 rotation = 0;
    if (rotate_ccw) rotation -= PLAYER_ROTATE_SPEED * dt;
    if (rotate_cw)  rotation += PLAYER_ROTATE_SPEED * dt;
    f32 new_angle = gs.player.view_angle + rotation;
    gs.player.view_angle = fmod_cycling(new_angle, 2.f * M_PI);

    Vec2 move_amt = V2();
    f32 x = 0, y = 0;
    if (move_up)      x += 1;
    if (strafe_left)  y -= 1;
    if (move_down)    x -= 1;
    if (strafe_right) y += 1;
    move_amt.x = x * cosf(gs.player.view_angle) - y * sinf(gs.player.view_angle);
    move_amt.y = x * sinf(gs.player.view_angle) + y * cosf(gs.player.view_angle);
    if (v2len(move_amt) > 1.f)
      move_amt = v2norm(move_amt);
    gs.player.pos = v2add(gs.player.pos, v2muls(move_amt, PLAYER_SPEED * dt));

    r_clear(gs.backbuffer, false);

    Vec2 local_pos = V2(gs.player.pos.x / MAP_TILE_SIZE,
                      gs.player.pos.y / MAP_TILE_SIZE);

    struct {
      f32 angle, length;
    } debug_rays[RAYCAST_RESOLUTION];

    for (u64 ray_idx = 0, x = 0; ray_idx < RAYCAST_RESOLUTION; ++ray_idx, x += PIXELS_PER_RAYCAST) {
      f32 proj_x = (f32)(x + PIXELS_PER_RAYCAST/2.f) - WINDOW_WIDTH/2.f;
      f32 relative_angle = atan2(proj_x, CAMERA_FOCAL_LENGTH);
      f32 angle = gs.player.view_angle + relative_angle;

      Vec2 dir = V2(cosf(angle), sinf(angle));
      Ray ray = {local_pos, dir};
      Raycast_Result cast = raycast(ray);

      f32 dist = cast.length;
      f32 perp_dist = dist * cosf(relative_angle);

      f32 wall_height = CAMERA_FOCAL_LENGTH / perp_dist;
      f32 y0 = Max(WINDOW_HEIGHT/2.f - wall_height/2.f, 0);
      f32 y1 = Min(WINDOW_HEIGHT/2.f + wall_height/2.f, WINDOW_HEIGHT);
      Vec3 color = V3(0, 255, 0);
      if (cast.collision == RAYCAST_WALL_EW) color = v3muls(color, 0.5f);

#if RENDER_DEBUG_2D_VIEW
      debug_rays[ray_idx] = {angle, cast.length};
#else
      for (u64 p = 0; p < PIXELS_PER_RAYCAST; ++p)
        r_vert(gs.backbuffer, (f32)(x+p), y0, y1, color);
#endif
    }

#if RENDER_DEBUG_2D_VIEW
    // NOTE: Draw test map
    bool drew_vertical_gridlines = false;
    for (u64 y = 0; y < WINDOW_HEIGHT; ++y) {
      if (y % MAP_TILE_SIZE == 0)
        r_line_2d(gs.backbuffer, V2(0,(f32)y), V2(WINDOW_WIDTH,(f32)y), V3());

      for (u64 x = 0; x < WINDOW_WIDTH; ++x) {
        if (!drew_vertical_gridlines && x % MAP_TILE_SIZE == 0)
          r_line_2d(gs.backbuffer, V2((f32)x,0), V2((f32)x,WINDOW_HEIGHT), V3());

        u64 map_x = x / MAP_TILE_SIZE;
        u64 map_y = y / MAP_TILE_SIZE;

        if (test_map[map_y][map_x]) {
          r_set_pixel(gs.backbuffer, V2((f32)x,(f32)y), V3(255, 255, 255));
        }
      }
      drew_vertical_gridlines = true;
    }

    r_circle_2d(gs.backbuffer, gs.player.pos, PLAYER_RADIUS, V3(0, 128, 255));

    for EachInStaticArray(debug_rays) {
      r_line_2d(gs.backbuffer, gs.player.pos, it.angle, it.length * MAP_TILE_SIZE, V3(255, 0, 0));
    }
#endif

    SDL_BlitSurface(gs.backbuffer_surface, NULL, gs.display_surface, NULL);
    SDL_UpdateWindowSurface(gs.window);
  }

  return 0;
}