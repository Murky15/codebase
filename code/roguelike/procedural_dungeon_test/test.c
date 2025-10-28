#pragma comment(lib, "raylib")

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "raylib/include/raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raylib/include/raygui.h"
#define BASE_MEMORY_MINIMAL
#include "../../base/include.h"
#include "../../base/include.c"

#include "../dungeon.h"
#include "../dungeon.c"

/*
  Simple workshop for building a dungeon generation algorithm b/c my D3D11 renderer is still in its infancy and
  too specialized to conviniently display this. Oops.

  For when I go 3D, here are some links relating to extending the bowyer-watson algorithm:
  https://en.wikipedia.org/wiki/Tetrahedron# (Circumradius & circumcenter for the circumsphere are defined in this article)
*/

global u64 window_width = 1280, window_height = 720;

function Vector2
v2raylib (Vec2 v) {
  return (Vector2){v.x, v.y};
}

function void
raylib_draw_grid (Camera2D cam, Vec2 dim, s64 spacing) {
  Vec2 full_dim = v2muls(dim, 2.f);
  f32 num_columns = full_dim.x / spacing;
  f32 onside_columns = num_columns / 2.f;
  f32 num_rows = full_dim.y / spacing;
  f32 onside_rows = num_rows / 2.f;
  for (s64 x = -onside_columns; x <= onside_columns; ++x) {
    f32 px = x * spacing;
    f32 thick = x == 0 ? 3.f : 1.f;
    Vector2 p0 = {px, -dim.y};
    Vector2 p1 = {px,  dim.y};
    DrawLineEx(p0, p1, thick/cam.zoom, BLACK);
  }
  for (s64 y = -onside_rows; y <= onside_rows; ++y) {
    f32 py = y * spacing;
    f32 thick = y == 0 ? 3.f : 1.f;
    Vector2 p0 = {-dim.x, py};
    Vector2 p1 = { dim.x,  py};
    DrawLineEx(p0, p1, thick/cam.zoom, BLACK);
  }
}

int
main (void) {
  InitWindow(window_width, window_height, "Procedural dungeon generation workshop");
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  srand(time(0));

  Arena *arena = arena_alloc();

  Camera2D cam = {0};
  cam.zoom = 1.f;
  cam.offset = v2raylib(v2(window_width/2.f, window_height/2.f));

  u64 map_width = 512, map_height = 512;
  u64 grid_dim = 16;
  Dungeon dungeon = d_create(arena,
    .target_room_count = 500,
    .grid_dim   = grid_dim,
    .map_width  = map_width,
    .map_height = map_height,
    .room_width_mean = 48,
    .room_width_deviation = 10,
    .room_height_mean = 48,
    .room_height_deviation = 18,
    .hallway_width = 5,
    .room_width_floor = 15,
    .room_height_floor = 15,
    .percent_edges_included = 10);


  while (!WindowShouldClose())
  {
    Vector2 mouse_world_pos = GetScreenToWorld2D(GetMousePosition(), cam);

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
    {
      Vec2 delta = v2(GetMouseDelta().x, GetMouseDelta().y);
      delta = v2muls(delta, -1.0f/cam.zoom);
      cam.target = v2raylib(v2add(v2(cam.target.x, cam.target.y), delta));
    }

    f32 wheel = GetMouseWheelMove();
    if (wheel != 0)
    {
      cam.offset = GetMousePosition();
      cam.target = mouse_world_pos;
      f32 scale = 0.2f*wheel;
      cam.zoom = clamp(expf(logf(cam.zoom)+scale), 0.1f, 64);
    }

    BeginDrawing();
    ClearBackground(RAYWHITE);
    BeginMode2D(cam);

    for (u64 y = 0; y < dungeon.height; ++y) {
      for (u64 x = 0; x < dungeon.width; ++x) {
        Dungeon_Tile tile = dungeon.tiles[y * dungeon.width + x];
        Vec2 world_pos = d_grid_to_world(&dungeon, v2(x,y));
        if (tile.flags) {
          Color c;
          if (tile.flags & DUNGEON_TILE_ROOM)
            c = BLUE;
          if (tile.flags & DUNGEON_TILE_HALLWAY)
            c = RED;
          DrawRectangleV(v2raylib(world_pos), v2raylib(v2(dungeon.grid_dim, dungeon.grid_dim)), c);
        }
      }
    }
#if 0
    // raylib_draw_grid(cam, v2muls(v2(map_width,map_height), 0.5f * grid_dim), grid_dim);
    foreach (room, &dungeon) {
      DrawRectangleV(v2raylib(room->world_pos), v2raylib(room->world_size), BLUE);
    }
    foreach (edge, &bw_result) {
      DrawLineEx(v2raylib(edge->p0), v2raylib(edge->p1), 2.f/cam.zoom, GREEN);
    }
    foreach (edge, &pathway) {
      DrawLineEx(v2raylib(edge->p0), v2raylib(edge->p1), 2.f/cam.zoom, RED);
    }
#endif

    EndMode2D();
    Vector2 cursor = GetScreenToWorld2D(GetMousePosition(), cam);
    DrawText(TextFormat("[%f, %f]", cursor.x, cursor.y), GetMouseX() + 15, GetMouseY(), 20, BLACK);

    EndDrawing();
  }

  CloseWindow();
  return 0;
}