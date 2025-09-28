#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "raylib.h"

#define BASE_MEMORY_MINIMAL
#include "base/include.h"
#include "base/include.c"

/*
  Simple workshop for building a dungeon generation algorithm b/c my D3D11 renderer is still in its infancy and
  too specialized to conviniently display this. Oops.

  Next up:
  https://en.wikipedia.org/wiki/Prim%27s_algorithm

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

global u64 window_width = 1280, window_height = 720;

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

function Triangle_Mesh
bowyer_watson_triangulate (Arena *arena, Vec2 *points, u64 num_points, Triangle super) { // I don't want the user to have to calculate the super triangle
  Triangle_Mesh result = {0};

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
        mesh_push_triangle(arena, &result, *triangle);
      }
    }
  }

  return result;
}

function Vector2
v2raylib (Vec2 v) {
  return (Vector2){v.x, v.y};
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

function void
push_edge (Arena *arena, Edge_List *edges, Edge e) {
  Edge *node = arena_pushn(arena, Edge, 1);
  *node = e;
  sll_queue_push(edges->first, edges->last, node);
  edges->count++;
}

int
main (void) {
  InitWindow(window_width, window_height, "Procedural dungeon generation workshop");
  srand(time(0));

  Arena *arena = arena_alloc();

  Vec2 test_points[10];
  u64 num_points = array_count(test_points);
  for (u64 i = 0; i < num_points; ++i) {
    test_points[i].x = rand() % window_width;
    test_points[i].y = rand() % window_height;
  }

  Triangle super = make_triangle(v2(-10000, -10000), v2(0, 10000), v2(10000, -10000));
  Triangle_Mesh bw_result = bowyer_watson_triangulate(arena, test_points, num_points, super);
  // Prim's algorithm
  Edge_List mst = {0};
  Vertex *vertices; // @todo: For debugging convenience, move this back.
  Temp_Arena scratch;
  ldefer (scratch=get_scratch(&arena,1),release_scratch(scratch)) {
    // Transform Delaunay triangulation data to suit our needs
    vertices = arena_pushn(scratch.arena, Vertex, num_points);
    for (u64 i = 0; i < num_points; ++i) {
      Vec2 p = test_points[i];
      Vertex *v = get_vertex(vertices, num_points, p);
      v->slot_filled = true;
      v->p = p;
      v->cheapest_cost = INFINITY;
    }
    foreach (triangle, &bw_result) {
      Vertex *v0 = get_vertex(vertices, num_points, triangle->p[0]);
      Vertex *v1 = get_vertex(vertices, num_points, triangle->p[1]);
      Vertex *v2 = get_vertex(vertices, num_points, triangle->p[2]);
      Vertex *cycle[3] = {v0, v1, v2};
      for (u64 i = 0; i < 3; ++i) {
        push_vertex_if_unique(scratch.arena, &cycle[i]->neighbors, cycle[(i+1)%3]);
        push_vertex_if_unique(scratch.arena, &cycle[i]->neighbors, cycle[(i+2)%3]);
      }
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
        push_edge(arena, &mst, (Edge){.p0=vertex->p, .p1=closest->p});
      }
    }
  }

  while (!WindowShouldClose())
  {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    foreach (triangle, &bw_result) {
      DrawTriangleLines(v2raylib(triangle->p[0]), v2raylib(triangle->p[1]), v2raylib(triangle->p[2]), GREEN);
    }
    foreach (edge, &mst) {
      DrawLineV(v2raylib(edge->p0), v2raylib(edge->p1), RED);
    }
    for (u64 i = 0; i < num_points; ++i) {
      DrawCircle(test_points[i].x, test_points[i].y, 5.f, GRAY);
    }
    EndDrawing();
  }

  CloseWindow();
  return 0;
}