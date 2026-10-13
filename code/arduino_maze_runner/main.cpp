#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#define BASE_IMPLEMENTATION
#include "base.h"

#define WINDOW_SCALE 2
#define WINDOW_WIDTH  (MAP_TILE_SIZE * MAP_WIDTH)
#define WINDOW_HEIGHT (MAP_TILE_SIZE * MAP_HEIGHT)

#define RAYCAST_FOV (M_PI/2.f)
#define RAYCAST_RESOLUTION 9
#define RAYCAST_ANGLE_STEP (RAYCAST_FOV / (f32)(RAYCAST_RESOLUTION-1))

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

function f32
fmod_cycling (f32 x, f32 y) {
  Assert(y != 0, "Cannot fmod_cycling with denominator 0.");
  f32 remainder = x - (floor(x/y) * y);

  return remainder;
}

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
    Vec2 move_amt = V2();
    if (move_up)      move_amt.y -= PLAYER_SPEED * dt;
    if (strafe_left)  move_amt.x -= PLAYER_SPEED * dt;
    if (move_down)    move_amt.y += PLAYER_SPEED * dt;
    if (strafe_right) move_amt.x += PLAYER_SPEED * dt;
    if (v2len(move_amt) > 1.f)
      move_amt = v2norm(move_amt);
    gs.player.pos = v2add(gs.player.pos, move_amt);

    f32 rotation = 0;
    if (rotate_ccw) rotation -= PLAYER_ROTATE_SPEED * dt;
    if (rotate_cw)  rotation += PLAYER_ROTATE_SPEED * dt;
    f32 new_angle = gs.player.view_angle + rotation;
    gs.player.view_angle = fmod_cycling(new_angle, 2.f * M_PI);

    r_clear(gs.backbuffer, true);

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
          r_set_pixel(gs.backbuffer, V2((f32)x,(f32)y), V3());
        }
      }
      drew_vertical_gridlines = true;
    }

    r_circle_2d(gs.backbuffer, gs.player.pos, PLAYER_RADIUS, V3(0, 128, 255));
    f32 angle = gs.player.view_angle - RAYCAST_FOV/2.f;
    Vec2 local_pos = V2(gs.player.pos.x / MAP_TILE_SIZE,
                      gs.player.pos.y / MAP_TILE_SIZE);
    for (u64 i = 0; i < RAYCAST_RESOLUTION; ++i, angle += RAYCAST_ANGLE_STEP) {
      // NOTE: Apply digital differential analysis to find wall intersections.
      Vec2 map_pos = V2(floorf(local_pos.x), floorf(local_pos.y));
      Vec2 dir   = V2(cosf(angle), sinf(angle));
      Vec2 delta = V2(fabsf(1.f/dir.x), fabsf(1.f/dir.y));
      Vec2 step = {}, length = {};
      if (dir.x > 0) {
        step.x = 1;
        length.x = ((map_pos.x + 1.f) - local_pos.x) * delta.x;
      } else {
        step.x = -1;
        length.x = (local_pos.x - map_pos.x) * delta.x;
      }
      if (dir.y > 0) {
        step.y = 1;
        length.y = ((map_pos.y + 1.f) - local_pos.y) * delta.y;
      } else {
        step.y = -1;
        length.y = (local_pos.y - map_pos.y) * delta.y;
      }

      int side = 0; // 0 - Vertical boundary, 1 - Horizontal boundary
      for (;map_pos.x >= 0 && map_pos.x < MAP_WIDTH && map_pos.y >= 0 && map_pos.y < MAP_HEIGHT;) {
        if (length.x < length.y) {
          length.x  += delta.x;
          map_pos.x += step.x;
          side = 0;
        } else {
          length.y  += delta.y;
          map_pos.y += step.y;
          side = 1;
        }

        if (test_map[(u64)map_pos.y][(u64)map_pos.x]) break;
      }

      f32 debug_line_length = 0;
      if (side == 0) {
        debug_line_length = (length.x - delta.x) * MAP_TILE_SIZE;
      } else {
        debug_line_length = (length.y - delta.y) * MAP_TILE_SIZE;
      }
      r_line_2d(gs.backbuffer, gs.player.pos, angle, debug_line_length, V3(255, 0, 0));

      /*
      if (m > 0) {
        f32 grid_x = ceil(gs.player.pos.x / MAP_TILE_SIZE) * MAP_TILE_SIZE;
        f32 grid_y = ceil(gs.player.pos.y / MAP_TILE_SIZE) * MAP_TILE_SIZE;

        f32 side_x = Clamp(gs.player.pos.x + ((grid_y - gs.player.pos.y) / m), 0, WINDOW_WIDTH);
        f32 side_y = Clamp(gs.player.pos.y + (m * (grid_x - gs.player.pos.x)), 0, WINDOW_HEIGHT);

        f32 dx = (side_x - grid_x);
        f32 dy = (side_y - grid_y);

        Vec2 p1 = {}, step = {};
        if (dx < dy) {
          p1 = V2(side_x, grid_y);
          step = V2();
        } else if (dy <= dx) {
          p1 = V2(grid_x, side_y);
          step = V2();
        }

        for (;0;) {

        }

      }
      ------------------------------------------------------------------
      Vec2 p1 = {}, step = {};
      if (m >= 0 && m <= 1) {
        f32 grid_x = ceil(gs.player.pos.x / MAP_TILE_SIZE) * MAP_TILE_SIZE;
        f32 grid_y = gs.player.pos.y + (m * (grid_x - gs.player.pos.x));
        p1 = V2(grid_x, grid_y);
        step = v2muls(V2(1, m), MAP_TILE_SIZE);
      } else if (m > 1) {,
        f32 grid_y = ceil(gs.player.pos.y / MAP_TILE_SIZE) * MAP_TILE_SIZE;
        f32 grid_x = gs.player.pos.x + ((grid_y - gs.player.pos.y) / m);
        p1 = V2(grid_x, grid_y);
        step = v2muls(V2(1/m, 1), MAP_TILE_SIZE);
      }
      for (;p1.x >= 0 && p1.x < WINDOW_WIDTH && p1.y >= 0 && p1.y < WINDOW_HEIGHT;
              p1 = v2add(p1, step)) {
        r_circle_2d(gs.backbuffer, p1, 5, V3(0,128,0));
        u64 map_x = (u64)p1.x / MAP_TILE_SIZE;
        u64 map_y = (u64)p1.y / MAP_TILE_SIZE;
        if (test_map[map_y][map_x]) break;
      }
      */


    }

    SDL_BlitSurface(gs.backbuffer_surface, NULL, gs.display_surface, NULL);
    SDL_UpdateWindowSurface(gs.window);
  }

  return 0;
}