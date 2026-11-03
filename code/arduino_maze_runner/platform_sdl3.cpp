#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#define BASE_IMPLEMENTATION
#include "base.h"
#include "fix.h"
#include "trig.h"
#include "platform.h"

#include "fix.cpp"
#include "trig.cpp"
#include "game.cpp"

#define WINDOW_TILE_SIZE 64
#define WINDOW_WIDTH  (WINDOW_TILE_SIZE * MAP_WIDTH)
#define WINDOW_HEIGHT (WINDOW_TILE_SIZE * MAP_HEIGHT)

#define WALL_COLOR (u32)(255 << 8)
#define WALL_COLOR_DIM (u32)(128 << 8)

struct Bitmap {
  u32 *pixels;
  u64 width, height;
};

global struct {
  Arena *arena;

  Bitmap backbuffer;
  SDL_Window  *window;
  SDL_Surface *backbuffer_surface;
  SDL_Surface *display_surface;
} platform;

global bool game_running = true;
global bool render_2d_debug_view = true, render_debug_raycasts = false;

global bool move_forward, move_back, strafe_left, strafe_right;
global bool rotate_cw, rotate_ccw;


function void
r_circle_2d (Vec2 pos, s32 r, Vec3 color) {
  for (s32 y = -r; y <= r; ++y) {
    for (s32 x = -r; x <= r; ++x) {
      if (sqrtf((f32)(x*x + y*y)) <= r) {
        Vec2 p = v2add(pos, V2((f32)x,(f32)y));
        r_set_pixel_debug(p.x, p.y, color);
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
    r_set_pixel_debug(x,y, color);
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
    r_set_pixel_debug(x,y, color);
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
r_clear (bool white) {
  memset(platform.backbuffer.pixels, white ? 255 : 0, platform.backbuffer.width * platform.backbuffer.height * sizeof(u32));
}

function void
r_set_pixel_debug (u16 x, u16 y, Vec3 color) {
  if ((x >= 0 && x < platform.backbuffer.width) &&
      (y >= 0 && y < platform.backbuffer.height)) {
    u8 r = (u8)color.x;
    u8 g = (u8)color.y;
    u8 b = (u8)color.z;
    platform.backbuffer.pixels[y * platform.backbuffer.width + x] = (r << 24 | g << 16 | b << 8);
  }
}

function void
r_set_pixel (u16 x, u16 y, bool dim) {
  if ((x >= 0 && x < platform.backbuffer.width) &&
      (y >= 0 && y < platform.backbuffer.height)) {
    platform.backbuffer.pixels[y * platform.backbuffer.width + x] = dim ? WALL_COLOR_DIM : WALL_COLOR;
  }
}

int
main (int argc, char **argv) {
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
  platform.window = SDL_CreateWindow(
    "Arduino Maze Runner",
    WINDOW_WIDTH,
    WINDOW_HEIGHT,
    0
  );
  if (platform.window == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window: %s", SDL_GetError());
    return 1;
  }

  platform.arena  = arena_alloc_default();

  u32 *pixels = ArenaPush(platform.arena, u32, WINDOW_WIDTH * WINDOW_HEIGHT);
  platform.backbuffer = Bitmap{pixels, WINDOW_WIDTH, WINDOW_HEIGHT};
  platform.backbuffer_surface = SDL_CreateSurfaceFrom(WINDOW_WIDTH, WINDOW_HEIGHT, SDL_PIXELFORMAT_RGBX8888, (void*)pixels, sizeof(u32) * WINDOW_WIDTH);
  if (platform.backbuffer_surface == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create backbuffer_surface: %s", SDL_GetError());
    return 1;
  }
  Assert(SDL_MUSTLOCK(platform.backbuffer_surface) == 0, "Surface requires locking on this platform");
  platform.display_surface = SDL_GetWindowSurface(platform.window);
  if (platform.display_surface == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not obtain handle to display buffer: %s", SDL_GetError());
    return 1;
  }

  game_init(WINDOW_WIDTH, WINDOW_HEIGHT);

  u64 last_time = SDL_GetTicks();
  for (;game_running;) {
    for (SDL_Event e; SDL_PollEvent(&e);) {
      switch (e.type) {
        case SDL_EVENT_QUIT: game_running = false; break;

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
          SDL_KeyboardEvent key = e.key;
          if (key.scancode == SDL_SCANCODE_W) move_forward = key.down;
          if (key.scancode == SDL_SCANCODE_A) strafe_left  = key.down;
          if (key.scancode == SDL_SCANCODE_S) move_back    = key.down;
          if (key.scancode == SDL_SCANCODE_D) strafe_right = key.down;

          if (key.scancode == SDL_SCANCODE_Q) rotate_ccw = key.down;
          if (key.scancode == SDL_SCANCODE_E) rotate_cw  = key.down;

          if (key.scancode == SDL_SCANCODE_T && !key.down) render_2d_debug_view = !render_2d_debug_view;
          if (key.scancode == SDL_SCANCODE_R && !key.down) render_debug_raycasts = !render_debug_raycasts;
        } break;
      }
    }

    u64 current_time = SDL_GetTicks();
    f32 dt = current_time - last_time;
    last_time = current_time;

    Game_Input input = {move_forward, move_back, strafe_left, strafe_right, rotate_cw, rotate_ccw};
    game_tick(input, dt);

    r_clear(false);

    Debug_Render_Data *debug = game_render();

    if (render_2d_debug_view) {
      r_clear(false);

      // NOTE: Draw test map
      bool drew_vertical_gridlines = false;
      for (u64 y = 0; y < WINDOW_HEIGHT; ++y) {
        if (y % WINDOW_TILE_SIZE == 0)
          r_line_2d(V2(0,(f32)y), V2(WINDOW_WIDTH,(f32)y), V3());

        for (u64 x = 0; x < WINDOW_WIDTH; ++x) {
          if (!drew_vertical_gridlines && x % WINDOW_TILE_SIZE == 0)
            r_line_2d(V2((f32)x,0), V2((f32)x,WINDOW_HEIGHT), V3());

          u64 map_x = x / WINDOW_TILE_SIZE;
          u64 map_y = y / WINDOW_TILE_SIZE;

          if (test_map[map_y][map_x]) {
            r_set_pixel_debug(x, y, V3(255, 255, 255));
          }
        }
        drew_vertical_gridlines = true;
      }

      Vec2 player_pos = v2muls(v2fx16_to_v2(gs.player.pos), WINDOW_TILE_SIZE);
      r_circle_2d(player_pos, PLAYER_RADIUS, V3(0, 128, 255));

      if (render_debug_raycasts) {
        for EachInStaticArray(debug->rays) {
          r_line_2d(player_pos, it.angle, it.length * WINDOW_TILE_SIZE, V3(255, 0, 0));
        }
      } else {
        r_line_2d(player_pos, gs.player.view_angle * ANGLE_U16TOF32, WINDOW_TILE_SIZE, V3(255, 0, 0));
      }
    }

    SDL_BlitSurface(platform.backbuffer_surface, NULL, platform.display_surface, NULL);
    SDL_UpdateWindowSurface(platform.window);
  }

  return 0;
}