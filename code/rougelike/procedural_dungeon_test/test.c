#include <stdlib.h>
#include <stdio.h>
#include "raylib.h"

#define BASE_MEMORY_MINIMAL
#include "base/include.h"
#include "base/include.c"

/*
  Simple workshop for building a dungeon generation algorithm b/c my D3D11 renderer is still in its infancy and
  too specialized to conviniently display this. Oops.
*/

typedef Vec2 Edge[2];

typedef struct Triangle {
  struct Triangle *next;
  u64 gen;
  b32 marked_for_delete;
  Vec2 p[3];

  Vec2 circum_center;
  f32  circum_radius;
} Triangle;

typedef struct Triangle_Mesh {
  Triangle *first, *last;
  u64 count;
} Triangle_Mesh;

global u32 window_width = 1280, window_height = 720;

function void
mesh_push_triangle (Arena *arena, Triangle_Mesh *mesh, Triangle triangle) {
  Triangle *node = arena_pushn(arena, Triangle, 1);
  *node = triangle;
  sll_queue_push(mesh->first, mesh->last, node);
  mesh->count++;
}

function Triangle
make_triangle (Vec2 p0, Vec2 p1, Vec2 p2) {
  Triangle result = {0};
  result.p[0] = p0;
  result.p[1] = p1;
  result.p[2] = p2;

  Vec2 A = v2(0,0);
  Vec2 B = v2sub(p1, p0);
  Vec2 C = v2sub(p2, p0);
  f32  D = 2.f * v2cross(B,C);
  Vec2 center;
  f32  radius;
  center.x = (C.y*(sqr(B.x)+sqr(B.y)) - B.y*(sqr(C.x)+sqr(C.y))) / D;
  center.y = (B.x*(sqr(C.x)+sqr(C.y)) - C.x*(sqr(B.x)+sqr(B.y))) / D;
  radius = v2len(center);
  center = v2add(center, p0);
  result.circum_center = center;
  result.circum_radius = radius;

  return result;
}

int
main (void) {
  InitWindow(window_width, window_height, "Procedural dungeon generation workshop");
  srand(0);

  Vec2 test_points[15];
  for (u32 i = 0; i < array_count(test_points); ++i) {
    test_points[i].x = rand() % window_width;
    test_points[i].y = rand() % window_height;
  }

  // Bowyer-Watson implementation
  Temp_Arena scratch;
  ldefer(scratch=get_scratch(0,0),release_scratch(scratch)) {
    Triangle_Mesh delaunay = {0};
    Triangle super = make_triangle(v2(-1000, -1000), v2(0, 1000), v2(1000, -1000));
    mesh_push_triangle(scratch.arena, &delaunay, super);
    for (u32 i = 0; i < array_count(test_points); ++i) {
      Vec2 p = test_points[i];
      Triangle_Mesh bad_triangles = {0};
      foreach (triangle, &delaunay) {
        if (!triangle->marked_for_delete) {
          f32 dist = v2dist(p, triangle->circum_center);
          if (dist <= triangle->circum_radius) {
            triangle->marked_for_delete = true;
            triangle->gen = i;
            mesh_push_triangle(scratch.arena, &bad_triangles, *triangle);
          }
        }
      }

      // @todo: Should I extend Temp_Arenas to be more robust for "pocket array" patterns like these?
      u64 restore;
      ldefer(restore=arena_pos(scratch.arena),arena_pop_to(scratch.arena,restore)) {
        Edge *polygon = arena_pushn(scratch.arena, Edge, 0);

      }
    }
  }

  while (!WindowShouldClose())
  {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    for (u32 i = 0; i < array_count(test_points); ++i) {
      DrawCircle(test_points[i].x, test_points[i].y, 5.f, GRAY);
    }
    EndDrawing();
  }

  CloseWindow();
  return 0;
}