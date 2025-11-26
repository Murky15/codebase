/* TODO
  X -> complete
  O -> omitted

  - [X] Linux cross-compile & run with wine
  - [O] Linux multithreading
    Here's the problem. mingw-w64 gcc uses the posix thread model instead of win32.
    This emulates win32 threading API calls like CreateThread, EnterCriticalSection, etc.
    But it also implements the backend for all non win32 APIs like the std thread library.
    If we only pick one and stick with it we would be fine, easy right?
    Nope! Thread local storage is currently implemented with __declspec(thread) on MSVC, but this is
    *unsupported* on mingw-w64 gcc! So, we can't use gcc's __thread, because that's mixing the two APIs,
    and I can't find a binary of mingw-w64 gcc that uses the win32 threading model to make everything uniform.
    So the only solution now would be to rewrite all tls code to use the win32 runtime tls API. However,
    because I have already wasted half of what should've been a very productive week on this, and because this will
    probably take some time to build and debug, I have decided that Linux will just have to wait.

  - [ ] Linux/Web platform layer (emscripten supports Xlib and EGL)

  - [X] Separate game & platform
  - [X] Hot Reloading
  - [ ] Profiling (probably a codebase addition)

  - [X] Deprecate vector construction functions in favor of compound literals
      and also typedef all vectors to be their construction name
      (e.g. Vec2 -> v2). This will make writing compound literals easier
      OR BETTER YET #define v2 as a macro over (Vec2) compound lit!
      I should also remove `pv2` and `dv3`
  - [X] Clean up build script (https://steve-jansen.github.io/guides/windows-batch-scripting/)
  - [X] It looks like the game is most performant with spin count = 0 for barriers?
    Verify this. Also, what is a good spin count for Critical sections?
  - [X] Vector swizzle *macros*: xz(Vec3) -> Vec2

  - [ ] Instead of a simple AABB check for determining the visible range, I should
    instead use a point-in-polygon function to support angles rotated around y-axis.
  - [ ] Make wall hight a property per room / hallway for more interesting visuals
  - [ ] For inward map corners, use the actual cornered ceiling sprite to patch the hole.
    This means that each inward corner should only be added to the list of perimeters once
    to prevent z-flimmering.
  - [ ] Audio
*/

#define OS_NO_ENTRY 1
#define ENABLE_ASSERT 1
#define DEBUG 1
#include <base/include.h>
#include <os/include.h>
#include <file/png.h>
#include "graphics.h"
#include "dungeon.h"
#include "roguelike.h"

#include <base/include.c>
#include <os/include.c>
#include <file/png.c>
#include "dungeon.c"

#define PLAYER_MOVE_SPEED 0.15f
#define MONSTER_MOVE_SPEED PLAYER_MOVE_SPEED/1.3f
#define MAX_ENTITIES 256

typedef struct Game_State {
  Arena *perm;
  Arena *frame;
  Renderer_VTable rvtbl;

  Texture_Atlas sprites;
  Sprite spr_wall_mid;
  Sprite spr_ceil;
  Vec4 ceil_color;

  Dungeon dungeon;
  Mat4 proj;
  Quat floor_rot;
  Quat forward_wall_rot;

  //Entity player;
  Camera cam;
  u64 num_entities;
  Entity entities[MAX_ENTITIES];
} Game_State;

global threadvar b32 dll_is_loaded = false;

function Cardinal_Dir
to_cardinal (Vec2 dir) {
  Cardinal_Dir result = 0;
  if (dir.x != 0) {
    result |= dir.x > 0 ? EAST : WEST;
  }
  if (dir.y != 0) {
    result |= dir.y > 0 ? NORTH : SOUTH;
  }

  return result;
}

// These could probably be named better
function Sprite*
get_atlas_slot (Texture_Atlas atlas, String8 key) {
  Sprite *result = 0;
  u64 hash = str8_hash(key) % atlas.num_sprites;
  while (true) {
    Sprite *selected = &atlas.sprites[hash];
    if (str8_match(selected->name, key, 0) || selected->name.len == 0) {
      result = selected;
      break;
    }
    hash = (hash + 1) % atlas.num_sprites;
  }

  return result;
}

function Sprite
get_sprite (Texture_Atlas atlas, String8 key) {
  return *get_atlas_slot(atlas, key);
}

function Atlas_Coords
make_atlas_coords_from_string (String8 coords) {
  Atlas_Coords result = {0};
  Temp_Arena scratch;
  ldefer (scratch=get_scratch(0,0), release_scratch(scratch)) {
    String8List numbers = str8_split(scratch.arena, coords, 1, " ");
    Vec4 coords = {0};
    u64 i = 0;
    for each_in_list (num, &numbers) {
      coords.e[i++] = f64_from_str8(num->string);
    }
    result.scale = coords.zw;
    result.offset = coords.xy;
  }

  return result;
}

function Texture_Atlas
load_textures (Arena *arena, String8 absolute_path_to_asset_dir) {
  Texture_Atlas result = {0};
  Temp_Arena scratch;
  ldefer(scratch=get_scratch(&arena, 1),release_scratch(scratch)) {
    String8 path_to_atlas_data = str8_pushf(scratch.arena,
      "%.*s/tile_list_v1.7", str8_expand(absolute_path_to_asset_dir));
    String8 atlas_txt = os_read_file(arena, path_to_atlas_data, false);
    if (atlas_txt.len == 0) {
      printf("Unable to find textures!\n");
      exit(1);
    }

    String8List atlas_lines = str8_split(scratch.arena, atlas_txt, 1, "\n");
    result.num_sprites = atlas_lines.num_nodes;
    result.sprites = arena_pushn(arena, Sprite, result.num_sprites);
    for each_in_list(line, &atlas_lines) {
      u64 sep_pos = str8_find(line->string, str8_lit(" "), 0, 0);
      String8 name = str8_prefix(line->string, sep_pos);
      String8 coords = str8_skip(line->string, sep_pos+1);
      String8 frame = str8_sub(name, name.len-3, name.len-1);

      if (str8_match(frame, str8_lit("_f"), 0)) {
        name = str8_chop(name, 3);
      }
      Sprite *sprite = get_atlas_slot(result, name);
      if (sprite->name.len == 0) {
        sprite->name = name;
      }
      sprite->coords[sprite->num_frames++] = make_atlas_coords_from_string(coords);
    }

    String8 path_to_texture_data = str8_pushf(scratch.arena,
      "%.*s/0x72_DungeonTilesetII_v1.7.png", str8_expand(absolute_path_to_asset_dir));
    String8 texture_png_data = os_read_file(scratch.arena, path_to_texture_data, false);
    result.raw_texture_data = png_decode(arena, texture_png_data);
  }
  return result;
}

function Rect
cam_calculate_visible_range (Camera cam, f32 fov_h, f32 aspect_ratio, f32 znear) {
  // Calculate corners of the near plane
  f32 near_height = 2.f * tanf(fov_h*0.5f) * znear;
  f32 near_width = near_height * aspect_ratio;

  // Man I wish we had operator overloading...
  Vec3 camera_dir = v3norm(v3sub(cam.focus, cam.pos));
  Vec3 camera_right = v3norm(v3cross(v3(0,1,0), camera_dir));
  Vec3 camera_up = v3norm(v3cross(camera_dir, camera_right));

  Vec2 half_near = v2muls(v2(near_width, near_height), 0.5f);
  Vec3 near_center = v3add(cam.pos, v3muls(camera_dir, znear));

  Vec3 ntl = v3sub(v3add(near_center, v3muls(camera_up, half_near.height)), v3muls(camera_right, half_near.width));
  Vec3 ntr = v3add(v3add(near_center, v3muls(camera_up, half_near.height)), v3muls(camera_right, half_near.width));
  Vec3 nbl = v3sub(v3sub(near_center, v3muls(camera_up, half_near.height)), v3muls(camera_right, half_near.width));
  Vec3 nbr = v3add(v3sub(near_center, v3muls(camera_up, half_near.height)), v3muls(camera_right, half_near.width));

  Vec3 line_top_left = v3sub(ntl, cam.pos);
  Vec3 line_top_right = v3sub(ntr, cam.pos);
  Vec3 line_bottom_left = v3sub(nbl, cam.pos);
  Vec3 line_bottom_right = v3sub(nbr, cam.pos);

  f32 t0 = -cam.pos.y / line_top_left.y;
  f32 t1 = -cam.pos.y / line_top_right.y;
  f32 t2 = -cam.pos.y / line_bottom_left.y;
  f32 t3 = -cam.pos.y / line_bottom_right.y;

  Vec2 top_left = v2(cam.pos.x + line_top_left.x * t0, cam.pos.z + line_top_left.z * t0);
  Vec2 top_right = v2(cam.pos.x + line_top_right.x * t1, cam.pos.z + line_top_right.z * t1);
  Vec2 bottom_left = v2(cam.pos.x + line_bottom_left.x * t2, cam.pos.z + line_bottom_left.z * t2);
  Vec2 bottom_right = v2(cam.pos.x + line_bottom_right.x * t3, cam.pos.z + line_bottom_right.z * t3);

  f32 min_x = min(top_left.x, bottom_left.x);
  f32 max_x = max(top_right.x, bottom_right.x);
  f32 x_diff = max_x - min_x;
  f32 y_diff = top_left.y - bottom_left.y;

  return (Rect){.xy=v2(min_x, bottom_left.y), .zw=v2(x_diff, y_diff)};
}

function void
cam_set_target (Camera *cam, Entity *e, Camera_Track_Mode track_mode) {
  Entity_Ref ref = {e->gen, e};
  cam->tracking = ref;
  cam->track_mode = track_mode;

  cam->offset = v3(e->idle.coords[0].scale.x/2.f);
  cam->pos = v3(cam->offset.x, cam->zoom, -cam->zoom);
  cam->focus = v3(cam->offset.x, e->pos.y, 0);
  cam->follow_dist = v3sub(cam->pos,cam->focus);
  cam->visible_range = cam_calculate_visible_range(*cam, cam->fov_h, cam->aspect_ratio, cam->znear);
}

function void
cam_update_tracking (Camera *cam, f32 dt) {
  if (cam->tracking.gen != cam->tracking.e->gen) {
    return;
  }

  Entity *tracking = cam->tracking.e;

  if (cam->track_mode == CAMERA_TRACK_MODE_FIXED) { // Default "snap"
    Vec3 focus_diff = v3sub(tracking->pos, cam->focus);
    focus_diff = v3add(focus_diff, cam->offset);
    focus_diff.y = 0;
    cam->pos   = v3add(cam->pos, focus_diff);
    cam->focus = v3add(cam->focus, focus_diff);
    cam->visible_range.xy = v2add(cam->visible_range.xy, xz(focus_diff));
  } else if (cam->track_mode == CAMERA_TRACK_MODE_LERP) {
    f32 t = clamp(0.0045f * dt, 0, 1);
    Vec2 prev_focus = xz(cam->focus);
    cam->pos   = v3lerp(cam->pos, v3add(tracking->pos,v3(.x1=cam->offset.x,.yz=cam->follow_dist.yz)), t);
    cam->focus = v3lerp(cam->focus, v3add(tracking->pos, v3(cam->offset.x)), t);
    cam->visible_range.xy = v2add(cam->visible_range.xy, v2sub(xz(cam->focus), prev_focus));
  }
}

function void
draw_entity (Entity *e, Renderer_VTable *r) {
  Sprite *anim = &e->run;
  Sprite *prev_anim = &e->idle;
  if (e->flags & ENTITY_FLAG_ANIMATE_SPRITES) {
    if (e->dir == 0) {
      swap(anim, prev_anim);
    }
    if (prev_anim->started_at || anim->started_at == 0) {
      anim->started_at = os_clock_seconds();
      anim->current_frame = 0;

      prev_anim->started_at = 0;
    }

    f32 seconds_per_frame = anim->seconds_to_complete / anim->num_frames;
    f32 current_step = anim->started_at + seconds_per_frame * anim->current_frame;
    f32 now = os_clock_seconds();
    if (now - current_step >= seconds_per_frame) {
      anim->current_frame++;
      if (anim->current_frame == anim->num_frames) {
        anim->started_at += seconds_per_frame * anim->current_frame;
      }
      anim->current_frame %= anim->num_frames;
    }
  } else {
    anim = &e->idle;
    anim->current_frame = 0;
  }

  Quat rot = axis_angle(v3(0,1,0), e->rotation_angle);
  Atlas_Coords texcoord = anim->coords[anim->current_frame];
  r_push_quad(.pos = e->pos, .scale = texcoord.scale, .rot = rot, .rot_offset = v2(texcoord.scale.x/2.f, 0), .atlas_coords = texcoord);
}

extern void*
roguelike_init (Thread_Context *tctx, Game_Init_Package init) { /* NOTE: Always single threaded */
  os_set_thread_context(*tctx);
  u64 seed = os_query_clock();
  srand(seed);
  printf("Seed: %llu\n", seed);
  Game_State *gs = arena_pushn(init.perm, Game_State, 1);

  String8 asset_path = str8_pushf(init.frame, "%.*s0x72_DungeonTilesetII_v1.7", str8_expand(init.asset_dir));
  Texture_Atlas sprites = load_textures(init.perm, asset_path);
  init.rvtbl.create_and_bind_texture(sprites.raw_texture_data, true);

  Dungeon dungeon = d_create(init.perm, sprites,
    .target_room_count = 500,
    .grid_dim   = 16,
    .map_width  = 512,
    .map_height = 512,
    .room_width_mean = 15,
    .room_width_deviation = 5,
    .room_height_mean = 15,
    .room_height_deviation = 5,
    .hallway_width = 3,
    .percent_edges_included = 12,
    .percent_tiles_cracked = 5,
    .max_tiles_per_map_slice = 512);

  Entity player = {0};
  player.class = ENTITY_CLASS_HERO;
  player.flags = ENTITY_FLAG_INPUT_SENSITIVE
    | ENTITY_FLAG_ANIMATE_SPRITES
    | ENTITY_FLAG_ANIMATE_ROTATIONS
    | ENTITY_FLAG_DRAWABLE
    | ENTITY_FLAG_COLLISION;

  player.pos = v3(0,1,0);
  while (d_index_tile_from_world(&dungeon, xz(player.pos))->flags == DUNGEON_TILE_EMPTY) {
    player.pos.x = (rand() % dungeon.width) - dungeon.width/2;
    player.pos.z = (rand() % dungeon.height) - dungeon.height/2;
  }
  player.seconds_to_rotate = 0.12f;
  player.idle = get_sprite(sprites, str8_lit("doc_idle_anim"));
  player.idle.seconds_to_complete = 0.5f;
  player.run  = get_sprite(sprites, str8_lit("doc_run_anim"));
  player.run.seconds_to_complete = 0.5f;
  player.bbox = player.idle.coords[0].scale;
  player.speed = PLAYER_MOVE_SPEED;
  gs->entities[gs->num_entities++] = player;

  f32 fov_h = M_PI32/4.f;
  f32 aspect_ratio = init.display_width/init.display_height;
  f32 znear = 20.f;
  f32 zfar = 500.f;

  Mat4 proj = m4perspective(fov_h, aspect_ratio, znear, zfar);
  Camera cam = {0};
  cam.zoom = 200.f;
  cam.fov_h = fov_h;
  cam.aspect_ratio = aspect_ratio;
  cam.znear = znear;
  cam.zfar = zfar;
  cam_set_target(&cam, &gs->entities[0], CAMERA_TRACK_MODE_LERP);

  Quat floor_rot = axis_angle(v3(1,0,0), M_PI32/2.f);
  Quat forward_wall_rot = axis_angle(v3(0,1,0), M_PI32/2.f);

  // NOTE: This sprite pack comes with a variety of sprites, but we can only use a few of them because of the 3D perspective
  Sprite spr_wall_mid = get_sprite(sprites, str8_lit("wall_mid"));
  Sprite spr_ceil = get_sprite(sprites, str8_lit("wall_top_mid"));
  Vec4 ceil_color = v4(0.13f,0.13f,0.13f,1);

  gs->perm = init.perm;
  gs->frame = init.frame;
  gs->sprites = sprites;
  gs->dungeon = dungeon;
  gs->proj = proj;
  gs->cam = cam;
  gs->floor_rot = floor_rot;
  gs->forward_wall_rot = forward_wall_rot;
  gs->spr_wall_mid = spr_wall_mid;
  gs->spr_ceil = spr_ceil;
  gs->ceil_color = ceil_color;
  gs->rvtbl = init.rvtbl;

  return (void*)gs;
}

function Entity_Ref
create_entity_reference (Entity *e) {
  return (Entity_Ref){e->gen, e};
}

function Entity*
get_entity (Entity_Ref ref) {
  Entity *result = 0;
  if (ref.e && ref.e->gen == ref.gen) {
    result = ref.e;
  }

  return result;
}

extern void
roguelike_tick (Thread_Context *tctx, void *game_state, f32 dt, Game_Input_Package input) {
  Game_State *gs = (Game_State*)game_state;
  if (!dll_is_loaded) {
    dll_is_loaded = true;
    os_set_thread_context(*tctx);
    if (runner_id() == 0) {
      srand(os_query_clock());

      Entity enemy = {0};
      enemy.flags = ENTITY_FLAG_ANIMATE_SPRITES
        | ENTITY_FLAG_ANIMATE_ROTATIONS
        | ENTITY_FLAG_DRAWABLE
        | ENTITY_FLAG_COLLISION;
      enemy.pos = gs->entities[0].pos;
      enemy.seconds_to_rotate = 0.12f;
      enemy.idle = get_sprite(gs->sprites, str8_lit("skelet_idle_anim"));
      enemy.idle.seconds_to_complete = 0.5f;
      enemy.run = get_sprite(gs->sprites, str8_lit("skelet_run_anim"));
      enemy.run.seconds_to_complete = 0.5f;
      enemy.speed = MONSTER_MOVE_SPEED;
      enemy.bbox = enemy.idle.coords[0].scale;
      gs->entities[1] = enemy;
      gs->num_entities = 2;
    }
  }

  // NOTE: Process received input
  Vec2 move_dir = {0};
  if (input.move_forward) move_dir.y += 1;
  if (input.strafe_left)  move_dir.x -= 1;
  if (input.move_back)    move_dir.y -= 1;
  if (input.strafe_right) move_dir.x += 1;
  if (v2len(move_dir) > 1) move_dir = v2norm(move_dir);

  // NOTE: Update all entities

  Rangei entity_snippet = os_heat_distribute(gs->num_entities);
  for each_in_range (e, gs->entities, entity_snippet) {
    // Get some spatial context
    Dungeon_Tile *current_tile = d_index_tile_from_world(&gs->dungeon, xz(e->pos));
    Dungeon_Tile *potential_overlap = d_index_tile_from_world(&gs->dungeon, v2add(xz(e->pos), v2(e->bbox.width-1,1)));

    Dungeon_Tile *right      = d_index_tile(&gs->dungeon, v2add(current_tile->grid_pos, v2(1)));
    Dungeon_Tile *left       = d_index_tile(&gs->dungeon, v2sub(current_tile->grid_pos, v2(1)));
    Dungeon_Tile *up_left    = d_index_tile(&gs->dungeon, v2add(current_tile->grid_pos, v2(.y=1)));
    Dungeon_Tile *down_left  = d_index_tile(&gs->dungeon, v2sub(current_tile->grid_pos, v2(.y=1)));
    Dungeon_Tile *up_right   = d_index_tile(&gs->dungeon, v2add(potential_overlap->grid_pos, v2(.y=1)));
    Dungeon_Tile *down_right = d_index_tile(&gs->dungeon, v2sub(potential_overlap->grid_pos, v2(.y=1)));

    if (e->flags & ENTITY_FLAG_INPUT_SENSITIVE) {
      e->pos = v3add(e->pos, v3muls(v3(move_dir.x, 0, move_dir.y), dt * e->speed));
      e->dir = to_cardinal(move_dir);
    } else {
      // NOTE: Find Hero
      Entity *target_hero = get_entity(e->target_hero);
      if (target_hero == 0) {
        for each_in_arrayc (it, gs->entities, (s64)gs->num_entities) {
          if (it->class == ENTITY_CLASS_HERO) {
            e->target_hero = create_entity_reference(it);
            target_hero = it;
            break;
          }
        }
      }
      Dungeon_Tile *hero_tile = d_index_tile_from_world(&gs->dungeon, xz(target_hero->pos));
      Dungeon_Tile_List path_to_hero = d_astar_calculate_path(gs->frame, &gs->dungeon, current_tile, hero_tile);
      if (e->path_end == 0 || e->path_end != hero_tile) {
        e->path_start = current_tile;
        e->path_end = hero_tile;
        e->path_pos = path_to_hero.first;
      }
      if ((current_tile == e->path_pos->tile || potential_overlap == e->path_pos->tile) && e->path_pos->next) {
        e->path_pos = e->path_pos->next;
      }
      Vec2 next_pos = d_grid_to_world(&gs->dungeon, e->path_pos->tile->grid_pos);
      Vec2 move_dir = v2sub(next_pos, xz(e->pos));
      if (v2len(move_dir) > 1) move_dir = v2norm(move_dir);
      e->pos = v3add(e->pos, v3muls(v3(move_dir.x, 0, move_dir.y), dt * e->speed));
      e->dir = to_cardinal(move_dir);
    }

    if (e->flags & ENTITY_FLAG_COLLISION) {
      // NOTE: Handle wall collisions
      if (up_left->flags == DUNGEON_TILE_EMPTY) {
        e->pos.z = min(e->pos.z, d_grid_to_world(&gs->dungeon, up_left->grid_pos).y-1);
      }
      if (down_left->flags == DUNGEON_TILE_EMPTY) {
        e->pos.z = max(e->pos.z, d_grid_to_world(&gs->dungeon, v2add(down_left->grid_pos, v2(.y=1))).y+1);
      }
      if (current_tile != potential_overlap && up_right->flags == DUNGEON_TILE_EMPTY) {
        e->pos.z = min(e->pos.z, d_grid_to_world(&gs->dungeon, up_right->grid_pos).y-1);
      }
      if (current_tile != potential_overlap && down_right->flags == DUNGEON_TILE_EMPTY) {
        e->pos.z = max(e->pos.z, d_grid_to_world(&gs->dungeon, v2add(down_right->grid_pos, v2(.y=1))).y+1);
      }
      if (left->flags == DUNGEON_TILE_EMPTY) {
        e->pos.x = max(e->pos.x, d_grid_to_world(&gs->dungeon, v2add(left->grid_pos, v2(1))).x+1);
      }
      if (right->flags == DUNGEON_TILE_EMPTY) {
        e->pos.x = min(e->pos.x + e->bbox.width, d_grid_to_world(&gs->dungeon, right->grid_pos).x) - e->bbox.width;
      }
    }

    if (e->flags & ENTITY_FLAG_ANIMATE_ROTATIONS) {
      if (e->dir & EAST) {
        e->end_angle = 0;
      } else if (e->dir & WEST) {
        e->end_angle = M_PI32;
      }

      if (!almost_equal(e->rotation_angle, e->end_angle)) {
        if (!e->started_rotating_at) {
          e->started_rotating_at = os_clock_seconds();
        }
        f64 current_time = os_clock_seconds();
        f64 rot_amt = cnorm(current_time, e->started_rotating_at, e->started_rotating_at + e->seconds_to_rotate);
        e->rotation_angle = lerp(e->start_angle, e->end_angle, rot_amt);
      } else {
        e->start_angle = e->end_angle;
        e->started_rotating_at = 0;
      }
    }
  }

  if (runner_id() == 0) {
    cam_update_tracking(&gs->cam, dt);
  }
}

extern void
roguelike_draw (Thread_Context *tctx, void *game_state) {
  Game_State *gs = (Game_State*)game_state;
  Renderer_VTable *r = &gs->rvtbl;
  if (!dll_is_loaded) {
    dll_is_loaded = true;
    os_set_thread_context(*tctx);
  }

  r->prep();

  Mat4 view = m4lookat(gs->cam.pos, gs->cam.focus, v3(0,1,0));
  Mat4 VP = m4mul(gs->proj, view);

  Rect player_visible_range;
  player_visible_range.xy = d_world_to_grid(&gs->dungeon, gs->cam.visible_range.xy);
  player_visible_range.zw = d_world_to_grid(&gs->dungeon, gs->cam.visible_range.zw);
  // Apply buffer
  f32 buff_amt_tiles = 3;
  player_visible_range.xy = v2sub(player_visible_range.xy, v2(buff_amt_tiles,buff_amt_tiles));
  player_visible_range.zw = v2add(player_visible_range.zw, v2(buff_amt_tiles*2,buff_amt_tiles*2));
  Dungeon_Tile_List visible_tile_list = d_query_range(gs->frame, gs->dungeon.map, player_visible_range, true);

  Dungeon_Tile *visible_tiles = 0;
  Dungeon_Perimeter_Tile *perimeter = 0;
  if (runner_id() == 0) {
    visible_tiles = arena_pushn(gs->frame, Dungeon_Tile, visible_tile_list.count);
    perimeter = arena_pushn(gs->frame, Dungeon_Perimeter_Tile, visible_tile_list.num_perimeter);

    u64 pidx = 0;
    for each_in_list (tile_node, &visible_tile_list) {
      u64 i = (tile_node - visible_tile_list.first);
      Dungeon_Tile tile = *tile_node->tile;
      visible_tiles[i] = tile;

      for (u64 p = 0; p < tile.on_perimeter; ++p) {
        Dungeon_Perimeter_Tile *perim = &perimeter[pidx++];
        perim->sprite = gs->spr_wall_mid;
        perim->grid_pos = v2add(tile.grid_pos, tile.perim[p].offset);
        perim->lateral = tile.perim[p].lateral;
        perim->requires_ceil_adjustment = !tile.perim[p].side;
      }
    }
  }
  os_heat_sync_ptr(visible_tiles, 0);
  os_heat_sync_ptr(perimeter, 0);

  u64 wall_height = 2;
  f32 ceil_height = wall_height * gs->dungeon.grid_dim;

  Rangei visible_snippet = os_heat_distribute(visible_tile_list.count);
  for each_in_range (tile, visible_tiles, visible_snippet) {
    Vec2 world = d_grid_to_world(&gs->dungeon, tile->grid_pos);
    Vec3 pos = v3(world.x, 1, world.y);
    Sprite sprite = tile->sprite;
    if (tile->flags == DUNGEON_TILE_EMPTY) {
      pos = v3(pos.x, ceil_height, pos.z);
      Vec2 scale = v2(gs->dungeon.grid_dim,gs->dungeon.grid_dim);
      r_push_quad(.pos = pos, .col = gs->ceil_color, .scale = scale, .rot = gs->floor_rot);
    } else {
      r_push_quad(.pos = pos, .sprite = sprite, .rot = gs->floor_rot);
    }
  }

  Rangei perimeter_snippet = os_heat_distribute(visible_tile_list.num_perimeter);
  for each_in_range (tile, perimeter, perimeter_snippet) {
    Vec2 p0 = d_grid_to_world(&gs->dungeon, tile->grid_pos);
    Quat rot;
    Quat ceil_rot;
    Vec3 ceil_pos = v3(p0.x, ceil_height+0.006f, p0.y);
    if (tile->lateral) {
      rot = qi();
      ceil_rot = rot;
      ceil_pos.y += 0.005f;
      if (tile->requires_ceil_adjustment) {
        ceil_rot = axis_angle(v3(0,1,0), M_PI32);
        ceil_pos.x += gs->dungeon.grid_dim;
      }
    } else {
      rot = gs->forward_wall_rot;
      ceil_rot = rot;
      if (tile->requires_ceil_adjustment) {
        ceil_rot = qinv(rot);
        ceil_pos.z -= gs->dungeon.grid_dim;
      }
    }
    for (u64 i = 0; i < wall_height; ++i) {
      f32 y = i * gs->dungeon.grid_dim;
      Vec3 world_pos = v3(p0.x, y, p0.y);
      r_push_quad(.pos = world_pos, .sprite = tile->sprite, .rot = rot);
    }
    r_push_quad(.pos = ceil_pos, .sprite = gs->spr_ceil, .rot = qmul(ceil_rot, gs->floor_rot));
  }

  os_heat_sync();

  Rangei entity_snippet = os_heat_distribute(gs->num_entities);
  for each_in_range (e, gs->entities, entity_snippet) {
    if (e->flags & ENTITY_FLAG_DRAWABLE) {
      draw_entity(e, r);
    }
  }

  os_heat_sync();

  r->update_transform(VP);
  r->present(true);
}