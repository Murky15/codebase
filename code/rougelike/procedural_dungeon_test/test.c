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

  For when I go 3D, here are some links relating to extending the bowyer-watson algorithm:
  https://en.wikipedia.org/wiki/Tetrahedron# (Circumradius & circumcenter for the circumsphere are defined in this article)
*/

#define DUNGEON_ROOM_MAX_CONNECTIONS 3

typedef u32 Dungeon_Tile_Flags;
enum {
  DUNGEON_TILE_EMPTY = (1 << 0),
  DUNGEON_TILE_ROOM = (1 << 1),
  DUNGEON_TILE_HALLWAY = (1 << 2),
};

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

  struct Dungeon_Room *connections[DUNGEON_ROOM_MAX_CONNECTIONS];
  u64 num_connections;

  Vec2 world_pos;
  Vec2 world_size;
} Dungeon_Room;

typedef struct Dungeon_Tile {
  Dungeon_Tile_Flags flags;
  Dungeon_Room *room; // TODO: Pointer or ID?
} Dungeon_Tile;

typedef struct Dungeon {
  u64 width, height;
  u64 grid_dim;

  Dungeon_Room *first, *last;
  u64 num_rooms;

  Dungeon_Tile *tiles;
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

function Dungeon_Room*
dungeon_push_room (Arena *arena, Dungeon *dungeon, Dungeon_Room room) {
  Dungeon_Room *node = arena_pushn(arena, Dungeon_Room, 1);
  *node = room;
  sll_queue_push(dungeon->first, dungeon->last, node);
  dungeon->num_rooms++;

  return node;
}

function b32
rects_intersect (Vec2 p0, Vec2 s0, Vec2 p1, Vec2 s1) {
  return  (p0.x + s0.x > p1.x) &&
          (p1.x + s1.x > p0.x) &&
          (p0.y + s0.y > p1.y) &&
          (p1.y + s1.y > p0.y);
}

// TODO: These should be prefixed
function Dungeon_Tile*
index_tile_from_world (Dungeon *dungeon, Vec2 p) {
  p = v2muls(p, 1.f/dungeon->grid_dim);
  u64 x = p.x + dungeon->width/2;
  u64 y = p.y + dungeon->height/2;

  return &dungeon->tiles[y * dungeon->width + x];
}

function Vec2
grid_to_world (Dungeon *dungeon, Vec2 index) {
  Vec2 result;
  result.x = index.x - dungeon->width/2;
  result.y = index.y - dungeon->height/2;
  result = v2muls(result, dungeon->grid_dim);

  return result;
}

function Vec2
world_to_grid (Dungeon *dungeon, Vec2 p) {
  Vec2 result;
  p = v2muls(p, 1.f/dungeon->grid_dim);
  result.x = p.x + dungeon->width/2;
  result.y = p.y + dungeon->height/2;

  return result;
}

function Dungeon_Room*
get_room_at_pos (Dungeon *dungeon, Vec2 p) {
  Dungeon_Tile *tile = index_tile_from_world(dungeon, p);

  return tile->room;
}

typedef struct Dungeon_Create_Params {
  u64 target_room_count;
  u64 grid_dim;

  u64 map_width;
  u64 map_height;

  u64 room_width_mean;
  u64 room_width_deviation;
  u64 room_height_mean;
  u64 room_height_deviation;

  u64 room_width_border;
  u64 room_height_border;

  u64 room_width_floor;
  u64 room_width_ceil;
  u64 room_height_floor;
  u64 room_height_ceil;
} Dungeon_Create_Params;

#define dungeon_create(arena, ...) dungeon_create_((arena), &(Dungeon_Create_Params){ \
  .room_width_deviation = 1,   \
  .room_height_deviation = 1,  \
  .room_width_floor = 1,       \
  .room_width_ceil = u64_max,  \
  .room_height_floor = 1,      \
  .room_height_ceil = u64_max, \
  __VA_ARGS__                  \
  })

// This can 100% be improved, it just feels so jank and messy.
function Dungeon
dungeon_create_ (Arena *arena, Dungeon_Create_Params *p) {
  Dungeon result = {0};
  result.width = p->map_width;
  result.height = p->map_height;
  result.grid_dim = p->grid_dim;
  result.tiles = arena_pushn(arena, Dungeon_Tile, result.width * result.height);

  f32 half_width = (f32)p->map_width / 2.f;
  f32 half_height = (f32)p->map_height / 2.f;
  for (u64 i = 0; i < p->target_room_count; ++i) {
    Dungeon_Room new_room = {0};
    f32 width  = roundf(gaussian_next(p->room_width_mean,  p->room_width_deviation));
    f32 height = roundf(gaussian_next(p->room_height_mean, p->room_height_deviation));
    f32 clamped_width  = clamp(width,  p->room_width_floor,  p->room_width_ceil);
    f32 clamped_height = clamp(height, p->room_height_floor, p->room_height_ceil);
    Vec2 grid_size = v2(clamped_width, clamped_height);
    Vec2 world_size = v2muls(grid_size, p->grid_dim);
    u64 max_tries = 2;
    u64 attempt = 0;
    while (attempt < max_tries) {
      f32 x = (f32)(rand() % p->map_width) - half_width;
      f32 y = (f32)(rand() % p->map_height) - half_height;
      Vec2 grid_pos = v2(x,y);
      Vec2 world_pos = v2muls(grid_pos, p->grid_dim);

      b32 clear = true;
      if (grid_pos.x + grid_size.x > half_width || grid_pos.y + grid_size.y > half_height) {
        clear = false;
      } else {
        foreach (room, &result) {
          Vec2 border = v2muls(v2(p->room_width_border, p->room_height_border), p->grid_dim);
          Vec2 new_pos = v2sub(world_pos, border);
          Vec2 new_size = v2add(world_size, border);
          Vec2 room_pos = v2sub(room->world_pos, border);
          Vec2 room_size = v2add(room->world_size, border);
          if (rects_intersect(new_pos, new_size, room_pos, room_size)) {
            clear = false;
            break;
          }
        }
      }
      if (clear) {
        new_room.world_pos  = world_pos;
        new_room.world_size = world_size;
        Dungeon_Room *new_room_ptr = dungeon_push_room(arena, &result, new_room);

        grid_pos = v2add(grid_pos, v2(half_width, half_height));
        for (u64 y = grid_pos.y; y < grid_pos.y + grid_size.y; ++y) {
          for (u64 x = grid_pos.x; x < grid_pos.x + grid_size.x; ++x) {
            Dungeon_Tile *tile = &result.tiles[y * result.width + x];
            tile->flags |= DUNGEON_TILE_ROOM;
            tile->room = new_room_ptr;
          }
        }

        break;
      }

      attempt++;
    }
  }

  return result;
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

  u64 map_width = 512, map_height = 256;
  u64 grid_dim = 16;
  Dungeon dungeon = dungeon_create(arena,
    .target_room_count = 500,
    .grid_dim   = grid_dim,
    .map_width  = map_width,
    .map_height = map_height,
    .room_width_mean = 16,
    .room_width_deviation = 3,
    .room_height_mean = 8,
    .room_height_deviation = 2,
    .room_width_border = 2,
    .room_height_border = 1);

  Vec2 *room_midpoints = arena_pushn(arena, Vec2, dungeon.num_rooms);
  foreach (room, &dungeon) {
    u64 i = room - dungeon.first;
    room_midpoints[i] = v2add(room->world_pos, v2muls(room->world_size, 0.5f));
  }

  Triangle super = make_triangle(v2(-10000, -10000), v2(0, 10000), v2(10000, -10000));
  Edge_List bw_result = bowyer_watson_triangulate(arena, room_midpoints, dungeon.num_rooms, super);
  Edge_List pathway = prim_mst(arena, bw_result, dungeon.num_rooms);

  // TODO: add a few edges from the delaunay back into the pathway

  foreach (path, &pathway) {
    Dungeon_Room *r1 = get_room_at_pos(&dungeon, path->p0);
    Dungeon_Room *r2 = get_room_at_pos(&dungeon, path->p1);
    r1->connections[r1->num_connections++] = r2;
    r2->connections[r2->num_connections++] = r1;

    Vec2 mp = v2muls(v2add(path->p0, path->p1), 0.5f);
    Vec2 mp_grid = world_to_grid(&dungeon, mp);
    Vec2 r1_p0 = r1->world_pos;
    Vec2 r1_p1 = v2add(r1->world_pos, r1->world_size);
    Vec2 r2_p0 = r2->world_pos;
    Vec2 r2_p1 = v2add(r2->world_pos, r2->world_size);
    b32 x_is_close = mp.x > r1_p0.x && mp.x < r1_p1.x && mp.x > r2_p0.x && mp.x < r2_p1.x;
    b32 y_is_close = mp.y > r1_p0.y && mp.y < r1_p1.y && mp.y > r2_p0.y && mp.y < r2_p1.y;

    // TODO: I think there is a way to condense/simplify this process.

    // First we check if we can reach the room through the midpoint, and connect with a straight line.
    if (x_is_close) {
      if (r1->world_pos.y > r2->world_pos.y)
        swap(r1, r2);
      Vec2 min = v2add(r1->world_pos, r1->world_size);
      Vec2 max = r2->world_pos;
      min = world_to_grid(&dungeon, min);
      max = world_to_grid(&dungeon, max);
      for (u64 y = min.y; y < max.y; ++y) {
        Dungeon_Tile *tile = &dungeon.tiles[y * dungeon.width + (u64)mp_grid.x];
        tile->flags |= DUNGEON_TILE_HALLWAY;
      }
    } else if (y_is_close) {
      if (r1->world_pos.x > r2->world_pos.x)
        swap(r1, r2);
      Vec2 min = v2add(r1->world_pos, r1->world_size);
      Vec2 max = r2->world_pos;
      min = world_to_grid(&dungeon, min);
      max = world_to_grid(&dungeon, max);
      for (u64 x = min.x; x < max.x; ++x) {
        Dungeon_Tile *tile = &dungeon.tiles[(u64)mp_grid.y * dungeon.width + x];
        tile->flags |= DUNGEON_TILE_HALLWAY;
      }
    } else {
      // Otherwise, we need to create an L-shaped path connecting the room midpoints
      Vec2 p0 = world_to_grid(&dungeon, path->p0);
      Vec2 p1 = world_to_grid(&dungeon, path->p1);

      u64 minx = min(p0.x, p1.x);
      u64 maxx = max(p0.x, p1.x);
      u64 miny = min(p0.y, p1.y);
      u64 maxy = max(p0.y, p1.y);

      u64 hy = (u64)p0.x == minx ? p0.y : p1.y;
      for (u64 x = minx; x <= maxx; ++x) {
        Dungeon_Tile *tile = &dungeon.tiles[hy * dungeon.width + x];
        if ((tile->flags & DUNGEON_TILE_ROOM) == 0)
          tile->flags |= DUNGEON_TILE_HALLWAY;
      }

      for (u64 y = miny; y <= maxy; ++y) {
        Dungeon_Tile *tile = &dungeon.tiles[y * dungeon.width + maxx];
        if ((tile->flags & DUNGEON_TILE_ROOM) == 0)
          tile->flags |= DUNGEON_TILE_HALLWAY;
      }
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
    BeginMode2D(cam);

    for (u64 y = 0; y < dungeon.height; ++y) {
      for (u64 x = 0; x < dungeon.width; ++x) {
        Dungeon_Tile tile = dungeon.tiles[y * dungeon.width + x];
        Vec2 world_pos = grid_to_world(&dungeon, v2(x,y));
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
    raylib_draw_grid(cam, v2muls(v2(map_width,map_height), 0.5f * grid_dim), grid_dim);
#if 0
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