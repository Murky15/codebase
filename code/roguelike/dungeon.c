function b32
edges_are_equal (Edge a, Edge b) {
  return ((a.p0.x == b.p0.x) && (a.p0.y == b.p0.y) && (a.p1.x == b.p1.x) && (a.p1.y == b.p1.y)) ||
    ((a.p0.x == b.p1.x) && (a.p0.y == b.p1.y) && (a.p1.x == b.p0.x) && (a.p1.y == b.p0.y));
}

function void
polygon_push_triangle_edges (Arena *arena, Edge_List *p, Triangle triangle) {
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
  for each_in_list (edge, edges) {
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
bowyer_watson_triangulate (Arena *arena, Vec2 *points, u64 num_points, Triangle super) {
  Edge_List result = {0};

  Temp_Arena scratch;
  ldefer(scratch=get_scratch(&arena,1),release_scratch(scratch)) {
    Triangle_Mesh delaunay = {0};
    mesh_push_triangle(scratch.arena, &delaunay, super);
    for (u64 i = 0; i < num_points; ++i) {
      Vec2 p = points[i];
      Edge_List edges = {0};
      for each_in_list (triangle, &delaunay) {
        if (!triangle->marked_for_delete) {
          f32 dist = v2dist(p, triangle->circum_center);
          if (dist < triangle->circum_radius) {
            triangle->marked_for_delete = true;
            polygon_push_triangle_edges(scratch.arena, &edges, *triangle);
          }
        }
      }
      for each_in_list (e1, &edges) {
        b32 is_unique = true;
        for each_in_list (e2, &edges) {
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

    for each_in_list (triangle, &delaunay) {
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
  for each_in_list (neighbor, n) {
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
    for each_in_list (edge, &bw_result) {
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

      for each_in_list (neighbor, &v->neighbors) {
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
d_push_room (Arena *arena, Dungeon *dungeon, Dungeon_Room room) {
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

function Dungeon_Tile*
d_index_tile (Dungeon *dungeon, Vec2 index) {
  u64 x = index.x + dungeon->width/2;
  u64 y = index.y + dungeon->height/2;

  return &dungeon->tiles[y * dungeon->width + x];
}

function Dungeon_Tile*
d_index_tile_from_world (Dungeon *dungeon, Vec2 p) {
  p = v2muls(p, 1.f/dungeon->grid_dim);
  u64 x = p.x + dungeon->width/2;
  u64 y = p.y + dungeon->height/2;

  return &dungeon->tiles[y * dungeon->width + x];
}

function Vec2
d_grid_to_world (Dungeon *dungeon, Vec2 index) {
  index = v2muls(index, dungeon->grid_dim);

  return index;
}

function Vec2
d_world_to_grid (Dungeon *dungeon, Vec2 p) {
  p = v2muls(p, 1.f/dungeon->grid_dim);
  p.x = roundf(p.x);
  p.y = roundf(p.y);

  return p;
}

function Dungeon_Room*
d_get_room_at_pos (Dungeon *dungeon, Vec2 p) {
  Dungeon_Tile *tile = d_index_tile_from_world(dungeon, p);

  return tile->room;
}

function Dungeon
d_create_ (Arena *arena, Dungeon_Create_Params *p) {
  Dungeon result = {0};
  result.width = p->map_width;
  result.height = p->map_height;
  result.grid_dim = p->grid_dim;
  result.tiles = arena_pushn(arena, Dungeon_Tile, result.width * result.height);
  Temp_Arena scratch;
  ldefer (scratch=get_scratch(&arena,1),release_scratch(scratch)) {
    // NOTE: Step 1: Place rooms.
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
          for each_in_list (room, &result) {
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
          new_room.grid_pos   = grid_pos;
          new_room.grid_size  = grid_size;
          Dungeon_Room *new_room_ptr = d_push_room(arena, &result, new_room);

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

    // NOTE: Step two: Triangulate and obtain mst.
    Vec2 *room_midpoints = arena_pushn(scratch.arena, Vec2, result.num_rooms);
    for each_in_list (room, &result) {
      u64 i = room - result.first;
      room_midpoints[i] = v2add(room->world_pos, v2muls(room->world_size, 0.5f));
    }

    Triangle super = make_triangle(v2(-100000, -100000), v2(0, 100000), v2(100000, -100000));
    Edge_List bw_result = bowyer_watson_triangulate(scratch.arena, room_midpoints, result.num_rooms, super);
    Edge_List pathway = prim_mst(scratch.arena, bw_result, result.num_rooms);

    // Add some edges back to improve dungeon quality
    for each_in_list (edge, &bw_result) {
      f32 val = (f32)((rand() % 100) + 1);
      if (val < p->percent_edges_included || almost_equal(val, p->percent_edges_included)) {
        push_edge_if_unique(scratch.arena, &pathway, *edge);
      }
    }

    // NOTE: Step three: Connect rooms based on MST.
    s64 onside_width = p->hallway_width / 2;
    for each_in_list (path, &pathway) {
      Dungeon_Room *r1 = d_get_room_at_pos(&result, path->p0);
      Dungeon_Room *r2 = d_get_room_at_pos(&result, path->p1);
      // We don't need this yet
      //r1->connections[r1->num_connections++] = r2;
      //r2->connections[r2->num_connections++] = r1;

      Vec2 mp = d_world_to_grid(&result, v2muls(v2add(path->p0, path->p1), 0.5f));
      Vec2 r1_p0 = r1->grid_pos;
      Vec2 r1_p1 = v2add(r1->grid_pos, r1->grid_size);
      Vec2 r2_p0 = r2->grid_pos;
      Vec2 r2_p1 = v2add(r2->grid_pos, r2->grid_size);
      b32 x_is_close = mp.x - onside_width > r1_p0.x && mp.x + onside_width < r1_p1.x && mp.x - onside_width > r2_p0.x && mp.x + onside_width < r2_p1.x;
      b32 y_is_close = mp.y - onside_width > r1_p0.y && mp.y + onside_width < r1_p1.y && mp.y - onside_width > r2_p0.y && mp.y + onside_width < r2_p1.y;

      // First we check if we can reach the room through the midpoint, and connect with a straight line.
      if (x_is_close) {
        if (r1_p0.y > r2_p0.y) {
          swap(r1_p0, r2_p0);
        }
        for (s64 y = r1_p0.y; y < r2_p0.y; ++y) {
          for (s64 x = mp.x - onside_width; x <= mp.x + onside_width; ++x) {
            Dungeon_Tile *tile = d_index_tile(&result, v2(x, y));
            if ((tile->flags & DUNGEON_TILE_ROOM) == 0) {
              tile->flags |= DUNGEON_TILE_HALLWAY;
            }
          }
        }
      } else if (y_is_close) {
        if (r1_p0.x > r2_p0.x) {
          swap(r1_p0, r2_p0);
        }
        for (s64 x = r1_p0.x; x < r2_p0.x; ++x) {
          for (s64 y = mp.y - onside_width; y <= mp.y + onside_width; ++y) {
            Dungeon_Tile *tile = d_index_tile(&result, v2(x, y));
            if ((tile->flags & DUNGEON_TILE_ROOM) == 0) {
              tile->flags |= DUNGEON_TILE_HALLWAY;
            }
          }
        }
      } else {
        // Otherwise, we need to create an L-shaped path connecting the room midpoints
        Vec2 p0 = d_world_to_grid(&result, path->p0);
        Vec2 p1 = d_world_to_grid(&result, path->p1);

        s64 minx = min(p0.x, p1.x);
        s64 maxx = max(p0.x, p1.x);
        s64 miny = min(p0.y, p1.y);
        s64 maxy = max(p0.y, p1.y);

        // TODO: We should determine which L-path is the shortest and use that (based on taxicab distance).
        s64 alternate_start_y = rand() % 2;
        s64 hy, hx;
        if (alternate_start_y) {
          hy = p0.x == minx ? p0.y : p1.y;
          hx = maxx;
        } else {
          hx = p0.y == miny ? p0.x : p1.x;
          hy = maxy;
        }

        for (s64 x = minx; x <= maxx + onside_width; ++x) {
          for (s64 y = hy - onside_width; y <= hy + onside_width; ++y) {
            Dungeon_Tile *tile = d_index_tile(&result, v2(x, y));
            if ((tile->flags & DUNGEON_TILE_ROOM) == 0) {
              tile->flags |= DUNGEON_TILE_HALLWAY;
            }
          }
        }

        for (s64 y = miny; y <= maxy + onside_width; ++y) {
          for (s64 x = hx - onside_width; x <= hx + onside_width; ++x) {
            Dungeon_Tile *tile = d_index_tile(&result, v2(x, y));
            if ((tile->flags & DUNGEON_TILE_ROOM) == 0) {
              tile->flags |= DUNGEON_TILE_HALLWAY;
            }
          }
        }
      }
    }
  }

  return result;
}
