#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#define BASE_IMPLEMENTATION
#include "base.h"
#include "platform.h"

#include "game.cpp"

#define WINDOW_WIDTH  (MAP_TILE_SIZE * MAP_WIDTH)
#define WINDOW_HEIGHT (MAP_TILE_SIZE * MAP_HEIGHT)

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
r_clear (bool white) {
  memset(platform.backbuffer.pixels, white ? 255 : 0, platform.backbuffer.width * platform.backbuffer.height * sizeof(u32));
}

function void
r_set_pixel (Vec2 pos, Vec3 color) {
  if ((pos.x >= 0 && pos.x < platform.backbuffer.width) &&
      (pos.y >= 0 && pos.y < platform.backbuffer.height)) {
    u64 x = (u64)pos.x, y = (u64)pos.y;
    u8 r = (u8)color.x;
    u8 g = (u8)color.y;
    u8 b = (u8)color.z;
    platform.backbuffer.pixels[y * platform.backbuffer.width + x] = (r << 24 | g << 16 | b << 8);
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

  game_init();

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
    f32 dt = (current_time - last_time) / 1000.f;
    last_time = current_time;

    Game_Input input = {move_forward, move_back, strafe_left, strafe_right, rotate_cw, rotate_ccw};
    game_tick(input, dt);

    r_clear(false);

    Debug_Render_Data debug = game_render(WINDOW_WIDTH, WINDOW_HEIGHT);

    if (render_2d_debug_view) {
      r_clear(false);

      // NOTE: Draw test map
      bool drew_vertical_gridlines = false;
      for (u64 y = 0; y < WINDOW_HEIGHT; ++y) {
        if (y % MAP_TILE_SIZE == 0)
          r_line_2d(V2(0,(f32)y), V2(WINDOW_WIDTH,(f32)y), V3());

        for (u64 x = 0; x < WINDOW_WIDTH; ++x) {
          if (!drew_vertical_gridlines && x % MAP_TILE_SIZE == 0)
            r_line_2d(V2((f32)x,0), V2((f32)x,WINDOW_HEIGHT), V3());

          u64 map_x = x / MAP_TILE_SIZE;
          u64 map_y = y / MAP_TILE_SIZE;

          if (test_map[map_y][map_x]) {
            r_set_pixel(V2((f32)x,(f32)y), V3(255, 255, 255));
          }
        }
        drew_vertical_gridlines = true;
      }

      r_circle_2d(gs.player.pos, PLAYER_RADIUS, V3(0, 128, 255));

      if (render_debug_raycasts) {
        for EachInStaticArray(debug.rays) {
          r_line_2d(gs.player.pos, it.angle, it.length * MAP_TILE_SIZE, V3(255, 0, 0));
        }
      } else {
        r_line_2d(gs.player.pos, gs.player.view_angle, MAP_TILE_SIZE, V3(255, 0, 0));
      }
    }

    SDL_BlitSurface(platform.backbuffer_surface, NULL, platform.display_surface, NULL);
    SDL_UpdateWindowSurface(platform.window);
  }

  return 0;
}