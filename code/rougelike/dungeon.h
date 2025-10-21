#ifndef DUNGEON_H
#define DUNGEON_H

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
} Edge_List;

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

  // This should probably be a linked list
  //struct Dungeon_Room *connections[DUNGEON_ROOM_MAX_CONNECTIONS];
  u64 num_connections;

  Vec2 world_pos;
  Vec2 world_size;

  Vec2 grid_pos;
  Vec2 grid_size;
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

// NOTE: Helpers
function u64 v2hash (Vec2 v);
function b32 rects_intersect (Vec2 p0, Vec2 s0, Vec2 p1, Vec2 s1);
function f64 gaussian_next (f64 mu, f64 sigma);

// NOTE: Edge Manipulation
function b32 edges_are_equal(Edge a, Edge b);
function void push_edge(Arena *arena, Edge_List *edges, Edge e);
function void push_edge_if_unique(Arena *arena, Edge_List *edges, Edge e);
function void polygon_push_triangle_edges(Arena *arena, Edge_List *p, Triangle triangle);

// NOTE: Triangle Manipulation
function Triangle make_triangle(Vec2 p0, Vec2 p1, Vec2 p2);
function void mesh_push_triangle(Arena *arena, Triangle_Mesh *mesh, Triangle triangle);
function b32 shared_vertex(Triangle a, Triangle b);

// NOTE: Vertex Manipulation
function Vertex* get_vertex(Vertex *vertices, u64 num_vertices, Vec2 v);
function void push_vertex_if_unique(Arena *arena, Vertex_Neighborhood *n, Vertex *v);

// NOTE: Spacial Algorithms
function Edge_List bowyer_watson_triangulate(Arena *arena, Vec2 *points, u64 num_points, Triangle super);
// TODO: We could use taxicab distance instead of euclidean for edge weights, it might make the
// hallway generation smarter because diagonal lines will generate L paths.
function Edge_List prim_mst(Arena *arena, Edge_List bw_result, u64 num_points);

// NOTE: Public API
function Dungeon_Room* d_push_room(Arena *arena, Dungeon *dungeon, Dungeon_Room room);
function Dungeon_Tile* d_index_tile_from_world(Dungeon *dungeon, Vec2 p);
function Vec2          d_grid_to_world(Dungeon *dungeon, Vec2 index);
function Vec2          d_world_to_grid(Dungeon *dungeon, Vec2 p);
function Dungeon_Room* d_get_room_at_pos(Dungeon *dungeon, Vec2 p);

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

  u64 hallway_width;
  f32 percent_edges_included;
} Dungeon_Create_Params;

#define d_create(arena, ...) d_create_((arena), &(Dungeon_Create_Params){ \
  .room_width_deviation = 1,   \
  .room_height_deviation = 1,  \
  .room_width_floor = 1,       \
  .room_width_ceil = u64_max,  \
  .room_height_floor = 1,      \
  .room_height_ceil = u64_max, \
  .hallway_width = 1,          \
  __VA_ARGS__                  \
  })

function Dungeon d_create_ (Arena *arena, Dungeon_Create_Params *p);

#endif // DUNGEON_H