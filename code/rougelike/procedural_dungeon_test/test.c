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

/*
  Simple workshop for building a dungeon generation algorithm b/c my D3D11 renderer is still in its infancy and
  too specialized to conviniently display this. Oops.

  A* pathfinding links:
  https://en.wikipedia.org/wiki/A*_search_algorithm
  https://en.wikipedia.org/wiki/Priority_queue
  https://en.wikipedia.org/wiki/Taxicab_geometry

  For when I go 3D, here are some links relating to extending the bowyer-watson algorithm:
  https://en.wikipedia.org/wiki/Tetrahedron# (Circumradius & circumcenter for the circumsphere are defined in this article)
*/

typedef struct Edge {
  struct Edge *next;
  Vec2 p0, p1;
} Edge;

typedef struct Edge_List {
  Edge *first, *last;
  u64 count;
} Edge_List, Polygon;

typedef struct Triangle {
  struct Triangle *next;
  Edge e[3];
  Vec2 p[3];

  Vec2 circum_center;
  f32  circum_radius;

  b32 marked_for_delete;
} Triangle;

typedef struct Triangle_Mesh {
  Triangle *first, *last;
  u64 count;
} Triangle_Mesh;

typedef struct Vertex Vertex;

typedef struct Vertex_Node {
  struct Vertex_Node *next;
  Vertex *v;
} Vertex_Node;

typedef struct Vertex_Neighborhood {
  Vertex_Node *first, *last;
  Vertex *cheapest_connection;
  u64 count;
} Vertex_Neighborhood;

struct Vertex {
  b32 slot_filled;
  b32 explored;
  Vec2 p;
  f32 cheapest_cost;
  Vertex_Neighborhood neighbors;
};

typedef struct Dungeon_Room {
  struct Dungeon_Room *next;
  Vec2 grid_pos;
  Vec2 grid_size;

  Vec2 world_pos;
  Vec2 world_size;
} Dungeon_Room;

typedef struct Dungeon {
  Dungeon_Room *first, *last;
  u64 num_rooms;
} Dungeon;

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

function b32
edges_are_equal (Edge a, Edge b) {
  return ((a.p0.x == b.p0.x) && (a.p0.y == b.p0.y) && (a.p1.x == b.p1.x) && (a.p1.y == b.p1.y)) ||
    ((a.p0.x == b.p1.x) && (a.p0.y == b.p1.y) && (a.p1.x == b.p0.x) && (a.p1.y == b.p0.y));
}

function void
polygon_push_triangle_edges (Arena *arena, Polygon *p, Triangle triangle) {
  Edge *newly_added_edges = arena_pushn(arena, Edge, 3);
  memory_copy(newly_added_edges, triangle.e, sizeof(triangle.e));
  sll_queue_push(p->first, p->last, &newly_added_edges[0]);
  sll_queue_push(p->first, p->last, &newly_added_edges[1]);
  sll_queue_push(p->first, p->last, &newly_added_edges[2]);
  p->count += 3;
}

function void
push_edge (Arena *arena, Edge_List *edges, Edge e) {
  Edge *node = arena_pushn(arena, Edge, 1);
  *node = e;
  sll_queue_push(edges->first, edges->last, node);
  edges->count++;
}

function void
push_edge_if_unique (Arena *arena, Edge_List *edges, Edge e) {
  b32 unique = true;
  foreach (edge, edges) {
    if (edges_are_equal(e, *edge)) {
      unique = false;
      break;
    }
  }
  if (unique) {
    push_edge(arena, edges, e);
  }
}

function void
mesh_push_triangle (Arena *arena, Triangle_Mesh *mesh, Triangle triangle) {
  Triangle *node = arena_pushn(arena, Triangle, 1);
  *node = triangle;
  sll_queue_push(mesh->first, mesh->last, node);
  mesh->count++;
}

function b32
shared_vertex (Triangle a, Triangle b) {
  for (u64 i = 0; i < 3; ++i) {
    for (u64 j = 0; j < 3; ++j) {
      if (a.p[i].x == b.p[j].x && a.p[i].y == b.p[j].y) {
        return true;
      }
    }
  }

  return false;
}

function Triangle
make_triangle (Vec2 p0, Vec2 p1, Vec2 p2) {
  Triangle result = {0};
  result.p[0] = p0;
  result.p[1] = p1;
  result.p[2] = p2;
  result.e[0] = (Edge){.p0=p0, .p1=p1};
  result.e[1] = (Edge){.p0=p1, .p1=p2};
  result.e[2] = (Edge){.p0=p2, .p1=p0};

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

function Edge_List
bowyer_watson_triangulate (Arena *arena, Vec2 *points, u64 num_points, Triangle super) { // I don't want the user to have to calculate the super triangle
  Edge_List result = {0};

  Temp_Arena scratch;
  ldefer(scratch=get_scratch(&arena,1),release_scratch(scratch)) {
    Triangle_Mesh delaunay = {0};
    mesh_push_triangle(scratch.arena, &delaunay, super);
    for (u64 i = 0; i < num_points; ++i) {
      Vec2 p = points[i];
      Polygon edges = {0};
      foreach (triangle, &delaunay) {
        if (!triangle->marked_for_delete) {
          f32 dist = v2dist(p, triangle->circum_center);
          if (dist < triangle->circum_radius) {
            triangle->marked_for_delete = true;
            polygon_push_triangle_edges(scratch.arena, &edges, *triangle);
          }
        }
      }
      foreach (e1, &edges) {
        b32 is_unique = true;
        foreach (e2, &edges) {
          if (e1 != e2 && edges_are_equal(*e1, *e2)) {
            is_unique = false;
            break;
          }
        }
        if (is_unique) {
          Triangle new_triangle = make_triangle(p, e1->p0, e1->p1);
          mesh_push_triangle(scratch.arena, &delaunay, new_triangle);
        }
      }
    }

    foreach (triangle, &delaunay) {
      if (!triangle->marked_for_delete && !shared_vertex(*triangle, super)) {
        push_edge_if_unique(arena, &result, triangle->e[0]);
        push_edge_if_unique(arena, &result, triangle->e[1]);
        push_edge_if_unique(arena, &result, triangle->e[2]);
      }
    }
  }

  return result;
}

function u64
v2hash (Vec2 v) {
  u64 l = (u32)v.x;
  u64 h = (u32)v.y;
  return (u64)((h << 32) | l);
}

function Vertex*
get_vertex (Vertex *vertices, u64 num_vertices, Vec2 v) {
  Vertex *result = 0;
  u64 hash = v2hash(v) % num_vertices;
  while (true) {
    result = &vertices[hash];
    if ((result->p.x == v.x && result->p.y == v.y) || (result->slot_filled == false)) {
      break;
    }
    hash = (hash + 1) % num_vertices;
  }

  return result;
}

function void
push_vertex_if_unique (Arena *arena, Vertex_Neighborhood *n, Vertex *v) {
  b32 unique = true;
  foreach (neighbor, n) {
    if (neighbor->v == v) {
      unique = false;
      break;
    }
  }
  if (unique) {
    Vertex_Node *node = arena_pushn(arena, Vertex_Node, 1);
    node->v = v;
    sll_queue_push(n->first, n->last, node);
    n->count++;
  }
}

function Edge_List
prim_mst (Arena *arena, Edge_List bw_result, u64 num_points) {
  Edge_List result = {0};

  Temp_Arena scratch;
  ldefer (scratch=get_scratch(&arena,1),release_scratch(scratch)) {
    Vertex *vertices = arena_pushn(scratch.arena, Vertex, num_points);
    foreach (edge, &bw_result) {
      Vertex *v0 = get_vertex(vertices, num_points, edge->p0);
      Vertex *v1 = get_vertex(vertices, num_points, edge->p1);
      if (!v0->slot_filled) {
        v0->slot_filled = true;
        v0->p = edge->p0;
        v0->cheapest_cost = INFINITY;
      }
      if (!v1->slot_filled) {
        v1->slot_filled = true;
        v1->p = edge->p1;
        v1->cheapest_cost = INFINITY;
      }

      push_vertex_if_unique(scratch.arena, &v0->neighbors, v1);
      push_vertex_if_unique(scratch.arena, &v1->neighbors, v0);
    }

    // Begin algorithm
    vertices[0].cheapest_cost = 0;
    u64 processed_vertices = 0;
    while (processed_vertices < num_points) {
      u64 next_vertex = 0;
      f32 cheapest_cost = INFINITY;
      for (u64 i = 0; i < num_points; ++i) {
        if (!vertices[i].explored && vertices[i].cheapest_cost < cheapest_cost) {
          next_vertex = i;
          cheapest_cost = vertices[i].cheapest_cost;
        }
      }

      Vertex *v = &vertices[next_vertex];
      v->explored = true;
      processed_vertices++;

      foreach (neighbor, &v->neighbors) {
        Vertex *n = neighbor->v;
        if (!n->explored) {
          f32 cost = v2dist(v->p, n->p);
          if (cost < n->cheapest_cost) {
            n->cheapest_cost = cost;
            n->neighbors.cheapest_connection = v;
          }
        }
      }
    }

    for (u64 i = 0; i < num_points; ++i) {
      Vertex *vertex = &vertices[i];
      Vertex *closest = vertex->neighbors.cheapest_connection;
      if (closest != 0) {
        push_edge(arena, &result, (Edge){.p0=vertex->p, .p1=closest->p});
      }
    }
  }

  return result;
}

// @todo: Rand doesn't work very well, I should add my own rng to the codebase.
function f64
gaussian_next (f64 mu, f64 sigma) {
  f64 U;
  while (true) {
    U = (f64)rand() / RAND_MAX;
    if (U > 0) break;
  }
  f64 V = (f64)rand() / RAND_MAX;
  f64 R = sqrt(-2 * log(U));
  f64 Z = R * cos(2*M_PI*V);

  return mu + sigma*Z;
}

struct Dungeon_Creation_Params {
  u64 target_num_rooms;
  u64 grid_dim;

  u64 map_width;
  u64 map_height;

  u64 room_width_mean;
  u64 room_width_deviation;
  u64 room_height_mean;
  u64 room_height_deviation;

  u64 room_width_border;
  u64 room_height_border;

#if 0 // Not sure if I want these yet
  u64 room_width_floor;
  u64 room_width_ceil;
  u64 room_height_floor;
  u64 room_height_ceil;
#endif
};

function void
dungeon_push_room (Arena *arena, Dungeon *dungeon, Dungeon_Room room) {
  Dungeon_Room *node = arena_pushn(arena, Dungeon_Room, 1);
  *node = room;
  sll_queue_push(dungeon->first, dungeon->last, node);
  dungeon->num_rooms++;
}

function b32
rects_intersect (Vec2 p0, Vec2 s0, Vec2 p1, Vec2 s1) {
  return  (p0.x + s0.x > p1.x) &&
          (p1.x + s1.x > p0.x) &&
          (p0.y + s0.y > p1.y) &&
          (p1.y + s1.y > p0.y);
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

  Vec2 test_points[50];
  u64 num_points = array_count(test_points);
  for (u64 i = 0; i < num_points; ++i) {
    test_points[i].x = rand() % window_width;
    test_points[i].y = rand() % window_height;
  }

  Triangle super = make_triangle(v2(-10000, -10000), v2(0, 10000), v2(10000, -10000));
  Edge_List bw_result = bowyer_watson_triangulate(arena, test_points, num_points, super);
  Edge_List mst = prim_mst(arena, bw_result, num_points);

  u64 target_num_rooms = 500;
  u64 grid_dim = 16;
  u64 map_width = 256;
  u64 map_height = 256;
  u64 room_width_mean = 16;
  u64 room_width_deviation = 4;
  u64 room_height_mean = 8;
  u64 room_height_deviation = 2;
  u64 room_width_border = 1;
  u64 room_height_border = 1;

  Dungeon dungeon = {0};
  f32 half_width = (f32)map_width / 2.f;
  f32 half_height = (f32)map_height / 2.f;
  for (u64 i = 0; i < target_num_rooms; ++i) {
    Dungeon_Room new_room = {0};
    f32 width  = roundf(gaussian_next(room_width_mean,  room_width_deviation));
    f32 height = roundf(gaussian_next(room_height_mean, room_height_deviation));
    Vec2 grid_size = v2(width, height);
    u64 max_tries = 15;
    u64 attempt = 0;
    while (true) {
      if (attempt > max_tries) {
        break;
      }

      f32 x = (f32)(rand() % map_width)  - half_width;
      f32 y = (f32)(rand() % map_height) - half_height;
      Vec2 grid_pos = v2(x,y);

      if (grid_pos.x + grid_size.x > half_width || grid_pos.y + grid_size.y > half_height) continue;

      b32 overlap = false;
      foreach (room, &dungeon) {
        Vec2 border = v2(room_width_border, room_height_border);
        Vec2 new_pos = v2sub(grid_pos, border);
        Vec2 new_size = v2add(grid_size, border);
        Vec2 room_pos = v2sub(room->grid_pos, border);
        Vec2 room_size = v2add(room->grid_size, border);
        if (rects_intersect(new_pos, new_size, room_pos, room_size)) {
          overlap = true;
          break;
        }
        ++attempt;
      }
      if (overlap) continue;

      new_room.grid_pos   = grid_pos;
      new_room.grid_size  = grid_size;
      new_room.world_pos  = v2muls(grid_pos, grid_dim);
      new_room.world_size = v2muls(grid_size, grid_dim);
      dungeon_push_room(arena, &dungeon, new_room);
      break;
    }
  }


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
    Vector2 cursor = GetScreenToWorld2D(GetMousePosition(), cam);
    DrawText(TextFormat("[%f, %f]", cursor.x, cursor.y), GetMouseX() + 15, GetMouseY(), 20, BLACK);

    BeginMode2D(cam);

    /*
    foreach (edge, &bw_result) {
      DrawLineV(v2raylib(edge->p0), v2raylib(edge->p1), GREEN);
    }

    foreach (edge, &mst) {
      DrawLineV(v2raylib(edge->p0), v2raylib(edge->p1), RED);
    }

    for (u64 i = 0; i < num_points; ++i) {
      DrawCircle(test_points[i].x, test_points[i].y, 5.f/cam.zoom, GRAY);
    }
    */

    foreach (room, &dungeon) {
      DrawRectangleV(v2raylib(room->world_pos), v2raylib(room->world_size), RED);
    }
    raylib_draw_grid(cam, v2muls(v2(map_width,map_height), 0.5f * grid_dim), grid_dim);

    EndMode2D();
    EndDrawing();
  }

  CloseWindow();
  return 0;
}