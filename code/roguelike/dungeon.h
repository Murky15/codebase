#ifndef DUNGEON_H
#define DUNGEON_H

typedef u32 Dungeon_Tile_Flags;
enum {
  DUNGEON_TILE_EMPTY = 0,
  DUNGEON_TILE_ROOM = (1 << 0),
  DUNGEON_TILE_HALLWAY = (1 << 1),
};

typedef struct D_Edge {
  struct D_Edge *next;
  Vec2 p0, p1;
} D_Edge;

typedef struct D_Edge_List {
  D_Edge *first, *last;
  u64 count;
} D_Edge_List;

typedef struct D_Triangle {
  struct D_Triangle *next;
  D_Edge e[3];
  Vec2 p[3];

  Vec2 circum_center;
  f32  circum_radius;

  b32 marked_for_delete;
} D_Triangle;

typedef struct D_Triangle_Mesh {
  D_Triangle *first, *last;
  u64 count;
} D_Triangle_Mesh;

typedef struct D_Vertex D_Vertex;

typedef struct D_Vertex_Node {
  struct D_Vertex_Node *next;
  D_Vertex *v;
} D_Vertex_Node;

typedef struct D_Vertex_Neighborhood {
  D_Vertex_Node *first, *last;
  D_Vertex *cheapest_connection;
  u64 count;
} D_Vertex_Neighborhood;

struct D_Vertex {
  b32 slot_filled;
  b32 explored;
  Vec2 p;
  f32 cheapest_cost;
  D_Vertex_Neighborhood neighbors;
};

typedef struct Dungeon_Room {
  struct Dungeon_Room *next;

  // This should probably be a linked list
  //struct Dungeon_Room *connections[DUNGEON_ROOM_MAX_CONNECTIONS];
  //u64 num_connections;

  Vec2 world_pos;
  Vec2 world_size;

  Vec2 grid_pos;
  Vec2 grid_size;
} Dungeon_Room;

typedef struct Dungeon_Tile {
  Dungeon_Tile_Flags flags;
  Dungeon_Room *room;
  Sprite sprite;
  Vec2 grid_pos;
} Dungeon_Tile;

typedef struct Dungeon_Tile_Node {
  struct Dungeon_Tile_Node *next;
  Dungeon_Tile tile;
} Dungeon_Tile_Node;

typedef struct Dungeon_Tile_List {
  Dungeon_Tile_Node *first, *last;
  u64 count;
} Dungeon_Tile_List;

typedef struct Dungeon_Slice {
  b32 is_leaf;
  Rect bounds;
  union {
    union {
      struct {
        struct Dungeon_Slice *south_west;
        struct Dungeon_Slice *south_east;
        struct Dungeon_Slice *north_east;
        struct Dungeon_Slice *north_west;
      };
      struct Dungeon_Slice *s[4];
    };

    Dungeon_Tile_List tiles;
  };
} Dungeon_Slice;

typedef struct Dungeon_Map {
  Dungeon_Slice *root;
  u64 num_slices;
  u64 num_leaves;
} Dungeon_Map;

typedef struct Dungeon {
  Dungeon_Tile *tiles;
  s64 width, height;
  s64 grid_dim;

  Dungeon_Room *first, *last;
  u64 num_rooms;

  Dungeon_Map map;
} Dungeon;


// NOTE: Helpers
function u64 d_v2hash(Vec2 v);
function b32 d_rects_intersect_expanded(Vec2 p0, Vec2 s0, Vec2 p1, Vec2 s1);
function b32 d_rects_intersect(Rect r0, Rect r1);
function b32 d_point_in_rect(Vec2 p, Rect r);
function f64 d_gaussian_next(f64 mu, f64 sigma);

// NOTE: Edge Manipulation
function b32  d_edges_are_equal(D_Edge a, D_Edge b);
function void d_push_edge(Arena *arena, D_Edge_List *edges, D_Edge e);
function void d_push_edge_if_unique(Arena *arena, D_Edge_List *edges, D_Edge e);
function void d_polygon_push_triangle_edges(Arena *arena, D_Edge_List *p, D_Triangle triangle);

// NOTE: D_Triangle Manipulation
function D_Triangle d_make_triangle(Vec2 p0, Vec2 p1, Vec2 p2);
function void       d_mesh_push_triangle(Arena *arena, D_Triangle_Mesh *mesh, D_Triangle triangle);
function b32        d_shared_vertex(D_Triangle a, D_Triangle b);

// NOTE: D_Vertex Manipulation
function D_Vertex* d_get_vertex(D_Vertex *vertices, u64 num_vertices, Vec2 v);
function void      d_push_vertex_if_unique(Arena *arena, D_Vertex_Neighborhood *n, D_Vertex *v);

// NOTE: Spacial Algorithms
function D_Edge_List d_bowyer_watson_triangulate(Arena *arena, Vec2 *points, u64 num_points, D_Triangle super);
// TODO: We could use taxicab distance instead of euclidean for edge weights, it might make the
// hallway generation smarter because diagonal lines will generate L paths.
function D_Edge_List d_prim_mst(Arena *arena, D_Edge_List bw_result, u64 num_points);

// NOTE: Public API
function Dungeon_Room*     d_push_room(Arena *arena, Dungeon *dungeon, Dungeon_Room room);
function Dungeon_Tile*     d_index_tile_from_world(Dungeon *dungeon, Vec2 p);
function Vec2              d_grid_to_world(Dungeon *dungeon, Vec2 index);
function Vec2              d_world_to_grid(Dungeon *dungeon, Vec2 p);
function Dungeon_Room*     d_get_room_at_pos(Dungeon *dungeon, Vec2 p);

function Dungeon_Slice*    d_process_slice(Arena *arena, Dungeon_Map *tree, Dungeon *d, u64 max_tiles_per_slice, Rect bounds);
function Dungeon_Map       d_partition_dungeon(Arena *arena, Dungeon *d, u64 max_tiles_per_slice);
function Dungeon_Slice*    d_index_at(Dungeon_Slice *slice, Vec2 grid_pos);
function Dungeon_Tile_List d_query_range(Arena *arena, Dungeon_Map tree, Rect grid_range, b32 include_full_chunk);

typedef struct Dungeon_Create_Params {
  u64 target_room_count;
  u64 grid_dim;
  u64 map_width;
  u64 map_height;

  u64 room_width_mean;
  u64 room_width_deviation;
  u64 room_height_mean;
  u64 room_height_deviation;
  u64 hallway_width;

  u64 room_width_border;
  u64 room_height_border;

  u64 room_width_floor;
  u64 room_width_ceil;
  u64 room_height_floor;
  u64 room_height_ceil;

  u64 max_tiles_per_map_slice;

  u64 percent_edges_included;
  u64 percent_tile_cracked;
} Dungeon_Create_Params;

#define d_create(arena, atlas, ...) d_create_((arena), (atlas), &(Dungeon_Create_Params){ \
  .room_width_deviation = 1,      \
  .room_height_deviation = 1,     \
  .room_width_floor = 1,          \
  .room_width_ceil = u64_max,     \
  .room_height_floor = 1,         \
  .room_height_ceil = u64_max,    \
  .hallway_width = 1,             \
  .max_tiles_per_map_slice = 128, \
  __VA_ARGS__                     \
  })

function Dungeon d_create_ (Arena *arena, Texture_Atlas textures, Dungeon_Create_Params *p);

#endif // DUNGEON_H