#include <base/include.h>
#include <os/include.h>
#include <file/png.h>
#include "renderer_d3d11.h"
#include "dungeon.h"
#include "roguelike.h"

#include <base/include.c>
#include <os/include.c>
#include <file/png.c>
#include "dungeon.c"

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
draw_entity (Entity *e) {
  Sprite *anim = &e->run;
  Sprite *prev_anim = &e->idle;
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

  Quat rot = axis_angle(v3(0,1,0), e->rotation_angle);
  Atlas_Coords texcoord = anim->coords[anim->current_frame];
  r_push_quad(.pos = e->pos, .scale = texcoord.scale, .rot = rot, .rot_offset = v2(texcoord.scale.x/2.f, 0), .atlas_coords = texcoord);
}

function void*
roguelike_init (Game_Init_Package init) { /* NOTE: Always single threaded */
  Game_State *gs = arena_pushn(init.perm, Game_State, 1);

  String8 asset_path = str8_pushf(init.frame, "%.*s0x72_DungeonTilesetII_v1.7", str8_expand(init.asset_dir));
  Texture_Atlas sprites = load_textures(init.perm, asset_path);
  r_create_and_bind_texture(sprites.raw_texture_data, true);

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
    .percent_tiles_cracked = 5);

  Entity player = {0};
  player.pos = v3(0,1,0);
  player.seconds_to_rotate = 0.12f;
  player.idle = get_sprite(sprites, str8_lit("doc_idle_anim"));
  player.idle.seconds_to_complete = 0.5f;
  player.run  = get_sprite(sprites, str8_lit("doc_run_anim"));
  player.run.seconds_to_complete = 0.5f;

  f32 fov_h = M_PI32/4.f;
  f32 aspect_ratio = init.display_width/init.display_height;
  f32 znear = 20.f;
  f32 zfar = 500.f;

  Mat4 proj = m4perspective(fov_h, aspect_ratio, znear, zfar);
  Camera cam = {0};
  f32 cam_zoom = 150.f;
  cam.pos = v3(player.idle.coords[0].scale.x/2.f, cam_zoom, -cam_zoom);
  cam.focus = v3(cam.pos.x, player.pos.y, 0);
  cam.follow_dist = v3sub(cam.pos,cam.focus);
  cam.visible_range = cam_calculate_visible_range(cam, fov_h, aspect_ratio, znear);

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
  gs->player = player;
  gs->cam = cam;
  gs->floor_rot = floor_rot;
  gs->forward_wall_rot = forward_wall_rot;
  gs->spr_wall_mid = spr_wall_mid;
  gs->spr_ceil = spr_ceil;
  gs->ceil_color = ceil_color;

  return (void*)gs;
}

function void
roguelike_tick (void *game_state, f32 dt, Game_Input_Package input) {
  Game_State *gs = (Game_State*)game_state;

  Vec2 move_dir = {0};
  if (input.move_forward) move_dir.y += 1;
  if (input.strafe_left)  move_dir.x -= 1;
  if (input.move_back)    move_dir.y -= 1;
  if (input.strafe_right) move_dir.x += 1;
  if (v2len(move_dir) > 1) move_dir = v2norm(move_dir);
  gs->cam.pos = v3add(gs->cam.pos, v3muls(v3(move_dir.x, 0, move_dir.y), dt * PLAYER_MOVE_SPEED));
  gs->cam.focus = v3add(gs->cam.focus, v3muls(v3(move_dir.x, 0, move_dir.y), dt * PLAYER_MOVE_SPEED));
  gs->cam.visible_range.xy = v2add(gs->cam.visible_range.xy, v2muls(move_dir, dt * PLAYER_MOVE_SPEED));

  gs->player.pos = v3add(gs->player.pos, v3muls(v3(move_dir.x, 0, move_dir.y), dt * PLAYER_MOVE_SPEED));

  gs->player.dir = to_cardinal(move_dir);
  if (gs->player.dir & EAST) {
    gs->player.end_angle = 0;
  } else if (gs->player.dir & WEST) {
    gs->player.end_angle = M_PI32;
  }

  if (!almost_equal(gs->player.rotation_angle, gs->player.end_angle)) {
    if (!gs->player.started_rotating_at) {
      gs->player.started_rotating_at = os_clock_seconds();
    }
    f64 current_time = os_clock_seconds();
    f64 rot_amt = cnorm(current_time, gs->player.started_rotating_at, gs->player.started_rotating_at + gs->player.seconds_to_rotate);
    gs->player.rotation_angle = lerp(gs->player.start_angle, gs->player.end_angle, rot_amt);
  } else {
    gs->player.start_angle = gs->player.end_angle;
    gs->player.started_rotating_at = 0;
  }
}

function void
roguelike_draw (void *game_state) {
  Game_State *gs = (Game_State*)game_state;

  r_prep();

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

  Dungeon_Tile *visible_tiles;
  Dungeon_Perimeter_Tile *perimeter;
  if (runner_id() == 0) {
    visible_tiles = arena_pushn(gs->frame, Dungeon_Tile, visible_tile_list.count);
    perimeter = arena_pushn(gs->frame, Dungeon_Perimeter_Tile, visible_tile_list.num_perimeter);

    u64 pidx = 0;
    for each_in_list (tile_node, &visible_tile_list) {
      u64 i = (tile_node - visible_tile_list.first);
      Dungeon_Tile tile = tile_node->tile;
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
  os_heat_sync_u64((u64*)&visible_tiles, 0);
  os_heat_sync_u64((u64*)&perimeter, 0);

  u64 wall_height = 3;
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
      // TODO: Can we just use backface culling for this?
      r_push_quad(.pos = pos, .sprite = sprite, .rot = gs->floor_rot);
    }
  }

  Rangei perimeter_snippet = os_heat_distribute(visible_tile_list.num_perimeter);
  for each_in_range (tile, perimeter, perimeter_snippet) {
    Vec2 p0 = d_grid_to_world(&gs->dungeon, tile->grid_pos);
    Quat rot;
    Quat ceil_rot;
    Vec3 ceil_pos = v3(p0.x, ceil_height+0.002f, p0.y);
    if (tile->lateral) {
      rot = qi();
      ceil_rot = rot;
      ceil_pos.y += 0.001f;
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

  if (runner_id() == 0) {
    draw_entity(&gs->player);
  }

  r_update_transform(VP);
  r_present(true);
}