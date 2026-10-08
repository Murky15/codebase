#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#define BASE_IMPLEMENTATION
#include "base.h"

#define WINDOW_SCALE 2
#define WINDOW_WIDTH  MAP_TILE_SIZE * MAP_WIDTH  * WINDOW_SCALE
#define WINDOW_HEIGHT MAP_TILE_SIZE * MAP_HEIGHT * WINDOW_SCALE

struct Bitmap {
  u32 *pixels;
  u64 width, height;
};

struct Player {
  Vec2 pos;
  f32 fov;
  s32 r;
  u64 num_rays;
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

#define MAP_TILE_SIZE 32
#define MAP_WIDTH  10
#define MAP_HEIGHT 10
StaticAssert(MAP_WIDTH == MAP_HEIGHT, check_map_dimensions);

u8 test_map[MAP_HEIGHT][MAP_WIDTH] = {
  {1,0,0,0,0,1,0,0,0,1},
  {0,0,0,1,0,0,1,1,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,1,1,1,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,1,0,0,0},
  {0,0,0,0,0,0,1,0,0,0},
  {0,1,0,0,0,0,1,0,0,0},
  {0,0,0,0,0,0,1,0,0,0},
  {1,0,0,0,0,0,0,0,0,1},
};

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
  Assert(pos.x >= 0 && pos.x < bitmap.width, "Requested pixel location is outside horizontal bounds");
  Assert(pos.y >= 0 && pos.y < bitmap.height, "Requested pixel location is outside horizontal bounds");

  u64 x = (u64)pos.x, y = (u64)pos.y;
  u8 r = (u8)color.x;
  u8 g = (u8)color.y;
  u8 b = (u8)color.z;
  bitmap.pixels[y * bitmap.width + x] = (r << 24 | g << 16 | b << 8);
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
  gs.player.r = 10;

  bool game_running = true;
  for (;game_running;) {
    for (SDL_Event e; SDL_PollEvent(&e);) {
      switch (e.type) {
        case SDL_EVENT_QUIT: game_running = false; break;
      }
    }

    r_clear(gs.backbuffer, true);

    // NOTE: Draw test map
    for (u64 y = 0; y < WINDOW_WIDTH; ++y) {
      for (u64 x = 0; x < WINDOW_HEIGHT; ++x) {
        u64 map_x = x / (MAP_TILE_SIZE * WINDOW_SCALE);
        u64 map_y = y / (MAP_TILE_SIZE * WINDOW_SCALE);

        if (test_map[map_y][map_x]) {
          r_set_pixel(gs.backbuffer, V2((f32)x,(f32)y), V3());
        }
      }
    }

    r_circle_2d(gs.backbuffer, gs.player.pos, gs.player.r, V3(0, 128, 255));

    SDL_BlitSurface(gs.backbuffer_surface, NULL, gs.display_surface, NULL);
    SDL_UpdateWindowSurface(gs.window);
  }

  return 0;
}