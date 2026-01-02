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
  - [X] Ditch CRT rand functions for CPU intrinsic

  - [X] Deprecate vector construction functions in favor of compound literals
      and also typedef all vectors to be their construction name
      (e.g. Vec2 -> v2). This will make writing compound literals easier
      OR BETTER YET #define v2 as a macro over (Vec2) compound lit!
      I should also remove `pv2` and `dv3`
  - [X] Clean up build script (https://steve-jansen.github.io/guides/windows-batch-scripting/)
  - [X] It looks like the game is most performant with spin count = 0 for barriers?
    Verify this. Also, what is a good spin count for Critical sections?
  - [X] Vector swizzle *macros*: xz(Vec3) -> Vec2
  - [ ] Conversion routines between strings and wide strings

  - [O] Instead of a simple AABB check for determining the visible range, I should
    instead use a point-in-polygon function to support angles rotated around y-axis.
  - [ ] Make wall hight a property per room / hallway for more interesting visuals
  - [ ] For inward map corners, use the actual cornered ceiling sprite to patch the hole.
    This means that each inward corner should only be added to the list of perimeters once
    to prevent z-flimmering.
  - [O] Debug wireframe renderer for hitboxes
  - [X] HUD
  - [ ] Audio
  - [ ] Lighting & Shadows
  - [X] Mouse Input
  - [X] Old input & new input
  - [ ] Clean up entity pathfinding
  - [ ] More robust physics system for movement.
    This would make properties like knockback way more interesting
  - [X] BASIC sword animation

  For bosses, instead of a health bar, we can still just use the hearts.
  We should display them at the top-middle portion of the screen but every time you hit the boss it doesn't
  immediately split the heart it will have like a little shake effect for a few hits before breaking.
*/

/* NOTE
  What if nobody starts with any weapons and the player just has to
  punch their way to the nearest chest. Can heroes pick up weapons from monsters?

  What if there was a weapon that you could throw and retreive like Thor's hammer?
*/

#define OS_NO_ENTRY 1
#define DEBUG 1
#include <base/include.h>
#include <os/include.h>
#include <file/png.h>
#include <file/wav.h>
#include "media.h"
#include "dungeon.h"
#include "roguelike.h"

#include <base/include.c>
#include <os/include.c>
#include <file/png.c>
#include <file/wav.c>
#include "dungeon.c"

#define PLAYER_MOVE_SPEED 0.15f
#define MONSTER_MOVE_SPEED PLAYER_MOVE_SPEED/1.2f
#define MAX_ENTITIES 256

typedef struct Game_State {
  // NOTE: Header
  Arena *perm;
  Arena *frame;
  Renderer_VTable rvtbl;
  Vec2 render_dim;

  // NOTE: Asset cache
  Texture_Atlas sprites;
  Texture_Atlas font;
  Playlist bg_music;
  Playlist sfx;
  Sprite spr_wall_mid;
  Sprite spr_ceil;
  Sprite spr_heart_full;
  Sprite spr_heart_half;
  Sprite spr_heart_empty;
  Vec4 ceil_color;

  Mat4 proj;
  Mat4 ortho;
  Quat floor_rot;
  Quat forward_wall_rot;

  // NOTE: Audio engine
  // TODO: Can easily be made into a queue of playlists.
  Playlist active_playlist;
  u64 active_sound_idx;
  Sound_List sounds_to_play;
  Sound *first_free_sound;

  // NOTE: Game objects
  Dungeon *dungeon;
  Camera cam;
  u64 num_entities;
  Entity entities[MAX_ENTITIES];
} Game_State;

// NOTE: Actual useful globals
global threadvar b32 dll_is_loaded = false;
global Game_State *gs;
global Mat4 vp;

// NOTE: Debug globals (must be cleaned up)
global f32 g_delta_time;

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
load_textures (Arena *arena, String8 absolute_path_to_asset_dir, r_create_texture_type r_create_texture) {
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
    PNG_Bitmap_RGBA bitmap = png_decode(arena, texture_png_data);
    result.texture = r_create_texture(bitmap, true);
  }

  return result;
}

function Texture_Atlas
load_font (Arena* arena, String8 absolute_path_to_bitmap, r_create_texture_type r_create_texture) {
  Texture_Atlas result = {0};

  // NOTE: Hard-coded monospace font dimensions in pixels
  // and characters *in-order*
  local_persist read_only u64 glyph_width  = 6;
  local_persist read_only u64 glyph_height = 12;
  local_persist read_only u8 glyph_lookup[] = {
    ' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
    '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?',
    '@','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
    'P','Q','R','S','T','U','V','W','X','Y','Z','[','\\',']','^','_',
    '`','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',
    'p','q','r','s','t','u','v','w','x','y','z','{','|','}','~','?',
    '?','?','?','?','?','?','?','?','?','?','?','?','?','?','?','?',
    '?','?','?','?','?','?','?','?','?','?','?','?','?','?','?','?',
  };

  Temp_Arena scratch;
  ldefer (scratch=get_scratch(&arena,1),release_scratch(scratch)) {
    String8 bitmap_data = os_read_file(scratch.arena, absolute_path_to_bitmap, false);
    if (bitmap_data.len == 0) {
      printf("Unable to find font!\n");
      exit(1);
    }
    PNG_Bitmap_RGBA bitmap = png_decode(arena, bitmap_data);
    result.texture = r_create_texture(bitmap, false);
    result.num_sprites = (bitmap.width / glyph_width) * (bitmap.height / glyph_height);
    result.sprites = arena_pushn(arena, Sprite, result.num_sprites);
    u64 i = 0;
    for (u64 y = 0; y < bitmap.height; y += glyph_height) {
      for (u64 x = 0; x < bitmap.width; x += glyph_width) {
        // NOTE: Get rid of these unhandled non-ascii characters
        if (i > 94) {
          break;
        }
        Atlas_Coords coords = {.offset = v2(x,y), .scale = v2(glyph_width, glyph_height)};
        u8 c = glyph_lookup[i];
        Sprite *sprite = &result.sprites[c];
        sprite->name = str8_cstring((const char*)&c);
        sprite->coords[0] = coords;
        ++i;
      }
    }
  }

  return result;
}

function Sound
sound_from_wave (String8 name, Wave_Data raw_sound_data) {
  local_persist u64 id_counter;

  Sound result = {0};
  assert (
    raw_sound_data.format == 1 &&
    raw_sound_data.channels == 2 &&
    raw_sound_data.frequency == 44100
  );
  result.audio_data = raw_sound_data;
  result.id = id_counter++;
  result.name = name;

  return result;
}

function Sound*
get_playlist_slot (Playlist pl, String8 sound) {
  Sound *result = 0;
  u64 hash = str8_hash(sound) % pl.count;
  while (true) {
    Sound *found = &pl.sounds[hash];
    if (str8_match(sound, found->name, 0) || found->name.len == 0) {
      result = found;
      break;
    }
    hash = (hash + 1) % pl.count;
  }

  return result;
}

function Sound
find_sound (Playlist pl, String8 sound) {
  return *get_playlist_slot(pl, sound);
}

function Playlist
make_playlist_from_dir (Arena *arena, String8 absolute_path_to_audio) {
  Playlist result = {0};
  Temp_Arena scratch;
  ldefer (scratch=get_scratch(&arena,1),release_scratch(scratch)) {
    Directory_Search_Results audio_files = os_search_directory_and_read_files(scratch.arena, absolute_path_to_audio, str8_lit("*.wav"));
    result.sounds = arena_pushn(arena, Sound, audio_files.count);
    result.played = arena_pushn(arena, b32, audio_files.count);
    result.count = audio_files.count;

    for each_in_list (wav_file, &audio_files) {
      Wave_Data sound_data = wav_load(arena, wav_file->result.data);
      Sound sound = sound_from_wave(str8_push_copy(arena, str8_chop(wav_file->result.name, 4)), sound_data);
      Sound *slot = get_playlist_slot(result, sound.name);
      *slot = sound;
    }
  }

  return result;
}

function Entity_Ref
make_ref (Entity *e) {
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
  Entity_Ref ref = make_ref(e);
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

function Vec3
cam_raycast_to_floor (Camera cam, Mat4 vp, Vec2 screen_pos) {
  Vec2 ndc = v2div(screen_pos, v2muls(gs->render_dim, 0.5f));
  ndc = v2sub(ndc, v2(1,1));
  Mat4 vp_inverse = m4invert(vp);
  Vec4 screen_pos_transformed = m4mulv(vp_inverse, v4(.xy = ndc, .zw=v2(0.f,1.f))); // NOTE: Here 0.f is the min-depth for d3d11
  Vec3 world_pos = v3muls(screen_pos_transformed.xyz, 1.f/screen_pos_transformed.w);
  Vec3 ray_dir = v3sub(world_pos, cam.pos);
  f32 t = -cam.pos.y / ray_dir.y;

  return v3add(cam.pos, v3muls(ray_dir,t));
}

function void
draw_entity (Entity *e) {
  Renderer_VTable *r = &gs->rvtbl;
  Sprite *anim = &e->run;
  Sprite *prev_anim = &e->idle;
  f64 clock = os_clock_seconds();

  if (e->flags & ENTITY_FLAG_ANIMATE_SPRITES) {
    if (e->dir.x == 0 && e->dir.y == 0) {
      swap(anim, prev_anim);
    }
    if (prev_anim->started_at || anim->started_at == 0) {
      anim->started_at = clock;
      anim->current_frame = 0;

      prev_anim->started_at = 0;
    }

    f32 seconds_per_frame = anim->seconds_to_complete / anim->num_frames;
    f32 current_step = anim->started_at + seconds_per_frame * anim->current_frame;
    f32 now = clock;
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

  if (e->flags & ENTITY_FLAG_ANIMATE_ROTATIONS) {
    if (e->dir.x > 0) {
      e->end_flip_angle = 0;
    } else if (e->dir.x < 0) {
      e->end_flip_angle = M_PI32;
    }

    if (!almost_equal(e->flip_angle, e->end_flip_angle)) {
      if (!e->started_flipping_at) {
        e->started_flipping_at = clock;
      }
      f64 current_time = clock;
      f64 rot_amt = cnorm(current_time, e->started_flipping_at, e->started_flipping_at + e->seconds_to_flip);
      e->flip_angle = lerp(e->start_flip_angle, e->end_flip_angle, rot_amt);
    } else {
      e->start_flip_angle = e->end_flip_angle;
      e->started_flipping_at = 0;
    }
  }

  Quat rot = e->rot;
  if (e->flags & ENTITY_FLAG_ANIMATE_ROTATIONS) {
    rot = axis_angle(v3(0,1,0), e->flip_angle);
  }
  Atlas_Coords texcoord = anim->coords[anim->current_frame];
  r_push_quad(.pos = e->pos, .scale = v2muls(texcoord.scale, e->scale_mul), .rot = rot, .rot_offset = e->rot_offset, .atlas_coords = texcoord);
}

function void
draw_string (Texture_Atlas font_atlas, Vec2 pos, f32 scale, String8 string) {
  Renderer_VTable *r = &gs->rvtbl;
  f32 glyph_scale = scale * gs->render_dim.width;
  for (u64 i = 0; i < string.len; ++i) {
    u8 c = string.str[i];
    Atlas_Coords coords = font_atlas.sprites[c].coords[0];
    f32 glyph_height_scale = glyph_scale * (coords.scale.height / coords.scale.width);
    r_push_quad(.pos = v3(pos.x+i*glyph_scale,pos.y), .atlas_coords=coords, .scale=v2(glyph_scale,glyph_height_scale));
  }
}

function void
play_sound (Arena *arena, Sound sound, b32 loop) {
  Sound *sound_slot = gs->first_free_sound;
  if (sound_slot) {
    gs->first_free_sound = sound_slot->next;
    memory_zero(sound_slot, sizeof(Sound));
  } else {
    sound_slot = arena_pushn(arena, Sound, 1);
  }
  *sound_slot = sound;
  sound_slot->loop = loop;

  sll_queue_push(gs->sounds_to_play.first, gs->sounds_to_play.last, sound_slot);
  gs->sounds_to_play.count += 1;
}

// TODO: Add transitions
function void
run_playlist (Arena *arena, Playlist pl, b32 loop, b32 shuffle) {
  gs->active_playlist = pl;
  gs->active_playlist.loop = loop;
  gs->active_playlist.shuffle = shuffle;
  gs->active_sound_idx = shuffle ? rand_next() % pl.count : 0;
  gs->active_playlist.sounds_played = 0;
  memory_zero(gs->active_playlist.played, sizeof(b32) * gs->active_playlist.count);

  play_sound(arena, pl.sounds[gs->active_sound_idx], false);
}

function s32
sign_extend(s32 v, s32 b) {
  s32 shift = 32 - b;
  return (v << shift) >> shift;
}

extern void
roguelike_audio_callback (f32 *sample_buffer, u32 samples_to_write) {
  f32 *cursor = sample_buffer;
  for (u32 frame = 0; frame < samples_to_write; ++frame) {
    f32 mixed_sample_left = 0, mixed_sample_right = 0;
    Sound temp_sound = {0};
    temp_sound.next = gs->sounds_to_play.first;
    Sound *prev = &temp_sound;
    // TODO: Need mutex here?
    for each_in_list (sound, &gs->sounds_to_play) {
      u16 bits_per_sample  = sound->audio_data.bits_per_sample;
      u16 bytes_per_sample = bits_per_sample/8;
      u64 buffer_size = sound->audio_data.sample_buffer_size;
      s32 sample_left = 0, sample_right = 0;
      assert (sound->local_cursor != buffer_size);
      for (u16 b = 0; b < bytes_per_sample; ++b) {
        sample_left  |= (sound->audio_data.sample_buffer[sound->local_cursor + b]) << (b*8);
        sample_right |= (sound->audio_data.sample_buffer[sound->local_cursor + b + bytes_per_sample]) << (b*8);
      }
      sound->local_cursor += 2 * bytes_per_sample;

      f32 divisor = bits_per_sample == 16 ? s16_max : bits_per_sample == 24 ? s24_max : 0;
      assert (divisor != 0);
      sample_left  = sign_extend(sample_left,  bits_per_sample);
      sample_right = sign_extend(sample_right, bits_per_sample);
      // TODO: relative audio can be achieved by multiplying this float value by a scalar [0,1]
      mixed_sample_left  += ((f32)sample_left  / divisor);
      mixed_sample_right += ((f32)sample_right / divisor);

      if (sound->local_cursor == buffer_size) {
        if (sound->loop) {
          sound->local_cursor %= buffer_size;
        } else {
          u64 sound_id = sound->id;
          Sound *next = sound->next;
          // TODO: Pull this out into a macro
          if (gs->sounds_to_play.count == 1) {
            gs->sounds_to_play.first = gs->sounds_to_play.last = 0;
          } else if (gs->sounds_to_play.first == sound) {
            gs->sounds_to_play.first = sound->next;
          } else if (gs->sounds_to_play.last == sound) {
            gs->sounds_to_play.last = prev;
            gs->sounds_to_play.last->next = 0;
          }
          prev->next = next;
          sound->next = gs->first_free_sound;
          gs->first_free_sound = sound;
          gs->sounds_to_play.count -= 1;

          if (gs->active_playlist.count > 0 && sound_id == gs->active_playlist.sounds[gs->active_sound_idx].id) {
            gs->active_playlist.played[gs->active_sound_idx] = true;
            if (++gs->active_playlist.sounds_played == gs->active_playlist.count) {
              if (gs->active_playlist.loop) {
                run_playlist(gs->perm, gs->active_playlist, true, gs->active_playlist.shuffle);
              } else {
                gs->active_playlist = (Playlist){0};
                gs->active_sound_idx = 0;
              }
            } else {
              u64 bounds = gs->active_playlist.count - gs->active_playlist.sounds_played;
              u64 new_song_idx = gs->active_playlist.shuffle ? rand_next() % bounds : gs->active_sound_idx + 1;
              for (;gs->active_playlist.played[new_song_idx]; ++new_song_idx);
              gs->active_sound_idx = new_song_idx;
              play_sound(gs->perm, gs->active_playlist.sounds[gs->active_sound_idx], false);
            }
          }

          sound = prev;
          continue;
        }
      }

      prev = sound;
    }

    *(cursor++) = clamp(mixed_sample_left, -1.f, 1.f);
    *(cursor++) = clamp(mixed_sample_right, -1.f, 1.f);
  }
}

extern void*
roguelike_init (Thread_Context *tctx, Game_Init_Package init) { /* NOTE: Always single threaded */
  os_set_thread_context(*tctx);
  Game_State *new_game_state = arena_pushn(init.perm, Game_State, 1);
  gs = new_game_state;

  String8 asset_path = str8_pushf(init.frame, "%.*sArt/Sprites", str8_expand(init.asset_dir));
  String8 font_path = str8_pushf(init.frame, "%.*sArt/Fonts/monogram-bitmap.png", str8_expand(init.asset_dir));
  Texture_Atlas sprites = load_textures(init.perm, asset_path, init.rvtbl.create_texture);
  Texture_Atlas font = load_font(init.perm, font_path, init.rvtbl.create_texture);
  init.rvtbl.bind_texture(sprites.texture);

  String8 sfx_path = str8_pushf(init.frame, "%.*sMusic/SFX", str8_expand(init.asset_dir));
  String8 music_path = str8_pushf(init.frame, "%.*sMusic/Background", str8_expand(init.asset_dir));
  Playlist sound_effects = make_playlist_from_dir(init.perm, sfx_path);
  Playlist bg_music = make_playlist_from_dir(init.perm, music_path);
  //run_playlist(init.perm, bg_music, true, true);

  Dungeon *dungeon = d_create(init.perm, sprites,
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
  player.eclass = ENTITY_CLASS_HERO;
  player.flags = ENTITY_FLAG_INPUT_SENSITIVE
    | ENTITY_FLAG_ANIMATE_SPRITES
    | ENTITY_FLAG_ANIMATE_ROTATIONS
    | ENTITY_FLAG_DRAWABLE
    | ENTITY_FLAG_COLLISION
    | ENTITY_FLAG_DRAW_HEALTH;

  player.pos = v3(0,1,0);
  while (d_index_tile_from_world(xz(player.pos))->flags == DUNGEON_TILE_EMPTY) {
    player.pos.x = (f32)(rand_next() % dungeon->width) - dungeon->width/2;
    player.pos.z = (f32)(rand_next() % dungeon->height) - dungeon->height/2;
  }

  player.seconds_to_flip = 0.12f;
  player.idle = get_sprite(sprites, str8_lit("knight_m_idle_anim"));
  player.idle.seconds_to_complete = 0.5f;
  player.run  = get_sprite(sprites, str8_lit("knight_m_run_anim"));
  player.run.seconds_to_complete = 0.5f;
  player.bbox = v2(player.idle.coords[0].scale.x, 6.f);
  player.speed = PLAYER_MOVE_SPEED;
  player.hp = 75.f;
  player.hp_max = 75.f;
  player.num_heart_containers = 3;
  player.scale_mul = 1;
  player.rot_offset = v3(player.idle.coords[0].scale.x/2.f);
  gs->entities[gs->num_entities++] = player;

  f32 fov_h = M_PI32/4.f;
  f32 aspect_ratio = init.display_width/init.display_height;
  f32 znear = 20.f;
  f32 zfar = 500.f;

  Mat4 proj = m4perspective(fov_h, aspect_ratio, znear, zfar);
  Mat4 ortho = m4orthographic(init.display_width, init.display_height, -1, 1);
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
  Sprite spr_heart_full = get_sprite(sprites, str8_lit("ui_heart_full"));
  Sprite spr_heart_half = get_sprite(sprites, str8_lit("ui_heart_half"));
  Sprite spr_heart_empty = get_sprite(sprites, str8_lit("ui_heart_empty"));
  Vec4 ceil_color = v4(0.13f,0.13f,0.13f,1);

  gs->perm = init.perm;
  gs->frame = init.frame;
  gs->sprites = sprites;
  gs->font = font;
  gs->dungeon = dungeon;
  gs->proj = proj;
  gs->ortho = ortho;
  gs->cam = cam;
  gs->floor_rot = floor_rot;
  gs->forward_wall_rot = forward_wall_rot;
  gs->spr_wall_mid = spr_wall_mid;
  gs->spr_ceil = spr_ceil;
  gs->spr_heart_full = spr_heart_full;
  gs->spr_heart_half = spr_heart_half;
  gs->spr_heart_empty = spr_heart_empty;
  gs->ceil_color = ceil_color;
  gs->rvtbl = init.rvtbl;
  gs->render_dim = v2(init.display_width, init.display_height);
  gs->bg_music = bg_music;
  gs->sfx = sound_effects;

  return (void*)gs;
}

extern void
roguelike_tick (Thread_Context *tctx, void *game_state, f32 dt, Game_Input_Package old_input, Game_Input_Package new_input) {
  #define down(k)     (new_input.k)
  #define up(k)       (!down(k))
  #define pressed(k)  (old_input.k == 0 && new_input.k)
  #define released(k) (old_input.k && new_input.k == 0)

  if (!dll_is_loaded) {
    dll_is_loaded = true;
    os_set_thread_context(*tctx);
    if (runner_id() == 0) {
      Game_State *new_game_state = (Game_State*)game_state;
      gs = new_game_state;
      d_select(gs->dungeon);

      gs->num_entities = 1; // Player
      Entity enemy = {0};
      enemy.flags = ENTITY_FLAG_ANIMATE_SPRITES
        | ENTITY_FLAG_ANIMATE_ROTATIONS
        | ENTITY_FLAG_COLLISION;
      //enemy.class = ENTITY_CLASS_MONSTER;
      enemy.pos = gs->entities[0].pos;
      enemy.seconds_to_flip = 0.12f;
      enemy.idle = get_sprite(gs->sprites, str8_lit("big_zombie_idle_anim"));
      enemy.idle.seconds_to_complete = 0.5f;
      enemy.run = get_sprite(gs->sprites, str8_lit("big_zombie_run_anim"));
      enemy.run.seconds_to_complete = 0.5f;
      enemy.speed = MONSTER_MOVE_SPEED;
      enemy.bbox.width = enemy.idle.coords[0].scale.width;
      enemy.bbox.height = gs->dungeon->grid_dim/2.f;
      enemy.scale_mul = 1;
      enemy.rot_offset = v3(enemy.idle.coords[0].scale.width/2.f);
      gs->entities[gs->num_entities++] = enemy;

      Entity sword = {0};
      sword.flags = ENTITY_FLAG_DRAWABLE | ENTITY_FLAG_HARMFUL;
      sword.eclass = ENTITY_CLASS_WEAPON;
      sword.pos = gs->entities[0].pos;
      sword.idle = get_sprite(gs->sprites, str8_lit("weapon_katana"));
      sword.bbox = sword.idle.coords[0].scale;
      sword.parent = make_ref(&gs->entities[0]);
      sword.scale_mul = 0.75f;
      sword.swing_angle = M_PI32;
      sword.seconds_to_swing = 0.1f;
      sword.seconds_for_anticipation = 0.175f;
      sword.seconds_for_recovery = 0.314f;
      gs->entities[gs->num_entities++] = sword;

    }
    os_heat_sync();
  }

  // NOTE: Process received input
  Vec2 move_dir = {0};
  if (down(move_forward)) move_dir.y += 1;
  if (down(strafe_left))  move_dir.x -= 1;
  if (down(move_back))    move_dir.y -= 1;
  if (down(strafe_right)) move_dir.x += 1;
  if (v2len(move_dir) > 1) move_dir = v2norm(move_dir);

  // NOTE: Update all entities
  b32 *done_updating = 0;
  if (runner_id() == 0) {
    done_updating = arena_pushn(gs->frame, b32, gs->num_entities);
  }
  os_heat_sync_ptr(done_updating, 0);

  Rangei entity_snippet = os_heat_distribute(gs->num_entities);
  for each_in_range (e, gs->entities, entity_snippet) {
    Entity old_state = *e;
    Entity new_state = old_state;

    switch (new_state.eclass) {
      case ENTITY_CLASS_HERO: {
        if (new_state.flags & ENTITY_FLAG_INPUT_SENSITIVE) {
          new_state.pos = v3add(new_state.pos, v3muls(v3(move_dir.x, 0, move_dir.y), dt * new_state.speed));
          new_state.dir = move_dir;
        }
      } break;

      case ENTITY_CLASS_MONSTER: {
        Dungeon_Tile *current_tile = d_index_tile_from_world(xz(new_state.pos));
        // NOTE: Find Hero
        Entity *target_hero = get_entity(new_state.target_hero);
        if (target_hero == 0) {
          for each_in_arrayc (it, gs->entities, gs->num_entities) {
            if (it->eclass == ENTITY_CLASS_HERO) {
              new_state.target_hero = make_ref(it);
              target_hero = it;
              break;
            }
          }
        }
        Vec2 next_pos = v2(0);
        Dungeon_Tile *hero_tile = d_index_tile_from_world(xz(target_hero->pos));
        if ((hero_tile->room_or_hallway != current_tile->room_or_hallway) ||
            (hero_tile->hallway_section != current_tile->hallway_section)) {
          Dungeon_Tile_List path_to_hero = d_astar_calculate_path(gs->frame, current_tile, hero_tile);
          if (path_to_hero.count > 1) {
            Dungeon_Tile_Node *path_pos = path_to_hero.first->next;

            f32 dist_to_next_tile = v2dist(xz(new_state.pos), d_grid_to_world(path_pos->tile->grid_pos));
            // TODO: This should be a configurable quantity
            if (dist_to_next_tile <= gs->dungeon->grid_dim && path_pos->next) {
              path_pos = path_pos->next;
            }
            next_pos = d_grid_to_world(path_pos->tile->grid_pos);
          }
        } else {
          next_pos = xz(target_hero->pos);
        }
        Vec2 move_dir = v2sub(next_pos, xz(new_state.pos));
        if (v2len(move_dir) > 1) move_dir = v2norm(move_dir);
        new_state.pos = v3add(new_state.pos, v3muls(v3(move_dir.x, 0, move_dir.y), dt * new_state.speed));
        new_state.dir = move_dir;
      } break;

      case ENTITY_CLASS_WEAPON: {
        // TODO: This is very naive. We first obtain the parent handle, then spin until the parent is done updating.
        // This is drastically inefficient long-term and can really slow down our update cycle. Ideally this thread would
        // find a way to come back to this task later and try again to maximize work efficiency.
        // Or instead of distributing the indices evenly we switch to the uneven-time task model of grabbing more work as it is available.
        // I like the latter more as it is much cleaner and allows us to keep this code snippet the same while still giving us the performance benefit.
        // I would also like to profile this.
        Entity *parent = get_entity(new_state.parent);
        while (parent && !done_updating[parent-gs->entities]) {
          parent = get_entity(new_state.parent);
        }
        if (parent && parent->flags & ENTITY_FLAG_INPUT_SENSITIVE) {
          f32 parent_width = parent->idle.coords[0].scale.width;
          f32 parent_height = parent->idle.coords[0].scale.height;
          Vec3 parent_center = v3(.x1 = parent->pos.x + parent_width/6.f, .yz = parent->pos.yz);
          // TODO: I want this tied to parent velocity
          f32 bob = 2 * sin(15 * os_clock_seconds());
          Vec3 pos_offset = v3(2*parent_width/3.f, parent_height/6.f + bob, -0.1f);
          new_state.pos = v3add(parent_center, pos_offset);
          new_state.rot = qi();
          if (new_state.slash_phase) {
            f64 clock = os_clock_seconds();
            // TODO: Should this be in the draw entity function?
            // and can we condense this?
            switch (new_state.slash_phase) {
              case ATTACK_PHASE_ANTICIPATION: {
                f64 rot_amt = cnorm(clock, new_state.started_swing_at, new_state.started_swing_at + new_state.seconds_for_anticipation);
                Quat pos_rot = slerp(qi(), new_state.start_pos_rot, rot_amt);
                Quat point_rot = slerp(qi(), new_state.start_point_rot, pow(rot_amt,3));
                Mat4 rot = m4rotate_around(pos_rot, parent_center);
                new_state.pos = m4mulv(rot, v4(.xyz=new_state.pos,.w1=1)).xyz;
                new_state.pos.y -= bob;
                new_state.rot = point_rot;
                new_state.rot_offset = v3(new_state.idle.coords[0].scale.width/2.f);
                if (rot_amt >= 1.f) {
                  new_state.slash_phase = ATTACK_PHASE_ACTION;
                  new_state.started_swing_at = clock;
                }
              } break;

              case ATTACK_PHASE_ACTION: {
                f64 rot_amt = cnorm(clock, new_state.started_swing_at, new_state.started_swing_at + new_state.seconds_to_swing);
                Quat pos_rot = slerp(new_state.start_pos_rot, new_state.end_pos_rot, rot_amt);
                Quat point_rot = slerp(new_state.start_point_rot, new_state.end_point_rot, pow(rot_amt,3));
                Mat4 rot = m4rotate_around(pos_rot, parent_center);
                new_state.pos = m4mulv(rot, v4(.xyz=new_state.pos,.w1=1)).xyz;
                new_state.pos.y -= bob;
                new_state.rot = point_rot;
                new_state.rot_offset = v3(new_state.idle.coords[0].scale.width/2.f);
                if (rot_amt >= 1.f) {
                  new_state.slash_phase = ATTACK_PHASE_RECOVERY;
                  new_state.started_swing_at = clock;
                }
              } break;

              case ATTACK_PHASE_RECOVERY: {
                f64 rot_amt = cnorm(clock, new_state.started_swing_at, new_state.started_swing_at + new_state.seconds_for_recovery);
                Quat pos_rot = slerp(new_state.end_pos_rot, new_state.end_pos_rot.w > 0 ? qi() : qneg(qi()), rot_amt);
                Quat point_rot = slerp(new_state.end_point_rot, new_state.end_point_rot.w > 0 ? qi() : qneg(qi()), pow(rot_amt,3));
                Mat4 rot = m4rotate_around(pos_rot, parent_center);
                new_state.pos = m4mulv(rot, v4(.xyz=new_state.pos,.w1=1)).xyz;
                new_state.pos.y -= bob;
                new_state.rot = point_rot;
                new_state.rot_offset = v3(new_state.idle.coords[0].scale.width/2.f);
                if (rot_amt >= 1.f) {
                  new_state.slash_phase = ATTACK_PHASE_NULL;
                }
              }
            }
          } else if (pressed(action_primary)) {
            // NOTE: We can make this more accurate by taking the angle, setting the position rotation,
            // retaking the angle based on the adjusted sprite origin, and then rotating based on that.
            // However, this will still not be a perfect lock to cursor because of inaccuracies in the sprites,
            // so what we have now is good enough.
            new_state.slash_phase = ATTACK_PHASE_ANTICIPATION;
            Vec3 floor_pos = cam_raycast_to_floor(gs->cam, vp, new_input.cursor);
            Vec3 pos_dir = v3norm(v3sub(floor_pos, new_state.pos));
            new_state.pos.y = pos_offset.y - bob;
            f32 point_angle = atan2(-pos_dir.z, pos_dir.x);
            f32 half_swing = new_state.swing_angle/2.f;
            f32 start_angle = point_angle - half_swing;
            f32 end_angle = point_angle + half_swing;
            /*if (point_angle < 0.f) {
              swap(start_angle, end_angle);
            }*/
            new_state.start_pos_rot = axis_angle(v3(.y=1), start_angle);
            new_state.end_pos_rot = axis_angle(v3(.y=1), end_angle);
            new_state.start_point_rot = qmul(axis_angle(v3(.y=1), start_angle + M_PI32/2.f), gs->floor_rot);
            new_state.end_point_rot = qmul(axis_angle(v3(.y=1), end_angle + M_PI32/2.f), gs->floor_rot);
            new_state.started_swing_at = os_clock_seconds();

            play_sound(gs->perm, find_sound(gs->sfx, str8_lit("07_human_atk_sword_1")), false);
          }
        }
      } break;
    }

    if (new_state.flags & ENTITY_FLAG_COLLISION) {
      Vec2 grid_pos = d_world_to_grid(xz(new_state.pos));
      for (s64 y = -1; y <= 1; ++y) {
        for (s64 x = -1; x <= 1; ++x) {
          Vec2 pos_to_check = v2add(grid_pos, v2(x,y));
          pos_to_check.x = clamp(pos_to_check.x, -gs->dungeon->width/2.f, (gs->dungeon->width/2.f)-1);
          pos_to_check.y = clamp(pos_to_check.y, -gs->dungeon->height/2.f, (gs->dungeon->height/2.f)-1);
          Dungeon_Tile *tile_to_check = d_index_tile(pos_to_check);
          if (tile_to_check->flags == DUNGEON_TILE_EMPTY) {
            Vec2 tile_pos = d_grid_to_world(tile_to_check->grid_pos);
            Rect tile_rect = v4(.xy=tile_pos,.zw=v2(gs->dungeon->grid_dim,gs->dungeon->grid_dim));
            Rect collision_rect = {0};
            // Ugly while loop, but it is necessary in cases where the tile rect engulfs the entity's bounding box
            while (d_rects_intersect(tile_rect, v4(.xy=v2(new_state.pos.x, new_state.pos.z - new_state.bbox.height/2.f), .zw=new_state.bbox), &collision_rect)) {
              Vec3 move_amt   = v3sub(new_state.pos, old_state.pos);
              Vec3 test_hori  = v3add(old_state.pos, v3(.x=move_amt.x));
              Vec3 test_vert  = v3add(old_state.pos, v3(.z=move_amt.z));
              b32  hintersect = d_rects_intersect(tile_rect, v4(.xy=v2(test_hori.x, test_hori.z - new_state.bbox.height/2.f), .zw=new_state.bbox), 0);
              b32  vintersect = d_rects_intersect(tile_rect, v4(.xy=v2(test_vert.x, test_vert.z - new_state.bbox.height/2.f), .zw=new_state.bbox), 0);
              if (vintersect) {
                f32 flip = new_state.dir.y > 0 ? -1 : 1;
                new_state.pos.z += collision_rect.height * flip;
              } else if (hintersect) {
                f32 flip = new_state.dir.x > 0 ? -1 : 1;
                new_state.pos.x += collision_rect.width * flip;
              } else {
                break;
              }
            }
          }
        }
      }
    }

    // NOTE: Update Entity state
    *e = new_state;
    e->old_dir = old_state.dir;
    done_updating[e-gs->entities] = true;
  }

  os_heat_sync();

  if (runner_id() == 0) {
    cam_update_tracking(&gs->cam, dt);
    g_delta_time = dt;
    Mat4 view = m4lookat(gs->cam.pos, gs->cam.focus, v3(.y=1));
    vp = m4mul(gs->proj, view);
  }

  #undef down
  #undef up
  #undef pressed
  #undef released
}

extern void
roguelike_draw (Thread_Context *tctx, void *game_state) {
  Game_State *gs = (Game_State*)game_state;
  Renderer_VTable *r = &gs->rvtbl;
  assert (dll_is_loaded);
  if (runner_id() == 0) {
    r->prep();
    r->bind_texture(gs->sprites.texture);
  }

  /*
  NOTE: Moved to game tick
  Mat4 view = m4lookat(gs->cam.pos, gs->cam.focus, v3(0,1,0));
  Mat4 VP = m4mul(gs->proj, view);
  */

  Rect player_visible_range;
  player_visible_range.xy = d_world_to_grid(gs->cam.visible_range.xy);
  player_visible_range.zw = d_world_to_grid(gs->cam.visible_range.zw);
  // Apply buffer
  f32 buff_amt_tiles = 3;
  player_visible_range.xy = v2sub(player_visible_range.xy, v2(buff_amt_tiles,buff_amt_tiles));
  player_visible_range.zw = v2add(player_visible_range.zw, v2(buff_amt_tiles*2,buff_amt_tiles*2));
  Dungeon_Tile_List visible_tile_list = d_query_range(gs->frame, gs->dungeon->map, player_visible_range, true);

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
  f32 ceil_height = wall_height * gs->dungeon->grid_dim;

  Rangei visible_snippet = os_heat_distribute(visible_tile_list.count);
  for each_in_range (tile, visible_tiles, visible_snippet) {
    Vec2 world = d_grid_to_world(tile->grid_pos);
    Vec3 pos = v3(world.x, 1, world.y);
    Sprite sprite = tile->sprite;
    if (tile->flags == DUNGEON_TILE_EMPTY) {
      pos = v3(pos.x, ceil_height, pos.z);
      Vec2 scale = v2(gs->dungeon->grid_dim,gs->dungeon->grid_dim);
      r_push_quad(.pos = pos, .col = gs->ceil_color, .scale = scale, .rot = gs->floor_rot);
    } else {
      r_push_quad(.pos = pos, .sprite = sprite, .rot = gs->floor_rot);
    }
  }

  Rangei perimeter_snippet = os_heat_distribute(visible_tile_list.num_perimeter);
  for each_in_range (tile, perimeter, perimeter_snippet) {
    Vec2 p0 = d_grid_to_world(tile->grid_pos);
    Quat rot = {0};
    Quat ceil_rot = {0};
    Vec3 ceil_pos = v3(p0.x, ceil_height+0.006f, p0.y);
    if (tile->lateral) {
      rot = qi();
      ceil_rot = rot;
      ceil_pos.y += 0.005f;
      if (tile->requires_ceil_adjustment) {
        ceil_rot = axis_angle(v3(0,1,0), M_PI32);
        ceil_pos.x += gs->dungeon->grid_dim;
      }
    } else {
      rot = gs->forward_wall_rot;
      ceil_rot = rot;
      if (tile->requires_ceil_adjustment) {
        ceil_rot = qinv(rot);
        ceil_pos.z -= gs->dungeon->grid_dim;
      }
    }
    for (u64 i = 0; i < wall_height; ++i) {
      f32 y = i * gs->dungeon->grid_dim;
      Vec3 world_pos = v3(p0.x, y, p0.y);
      r_push_quad(.pos = world_pos, .sprite = tile->sprite, .rot = rot);
    }
    r_push_quad(.pos = ceil_pos, .sprite = gs->spr_ceil, .rot = qmul(ceil_rot, gs->floor_rot));
  }

  os_heat_sync();

  Rangei entity_snippet = os_heat_distribute(gs->num_entities);
  for each_in_range (e, gs->entities, entity_snippet) {
    if (e->flags & ENTITY_FLAG_DRAWABLE) {
      draw_entity(e);
    }
  }

  os_heat_sync();

  if (runner_id() == 0) {
    r->update_transform(vp);
    r->draw_quads();

    // NOTE: Draw HUD
    r->update_transform(gs->ortho);
  }

  os_heat_sync();

  for each_in_range (e, gs->entities, entity_snippet) {
    if (e->flags & ENTITY_FLAG_DRAW_HEALTH) {
      f32 heart_hud_scale = gs->render_dim.width * 0.04f;
      f32 heart_gap_size = gs->render_dim.width * 0.005f;
      f32 heart_hud_step = (heart_hud_scale + heart_gap_size);
      f32 full_width = e->num_heart_containers * heart_hud_step;
      Vec3 pos = v3((gs->render_dim.width/2.f) - full_width, (gs->render_dim.height/2.f) - heart_hud_step);

      f32 hp_per_container = e->hp_max / e->num_heart_containers;
      f32 current_hp = e->hp;

      for (u64 i = 0; i < e->num_heart_containers; ++i) {
        Sprite heart = gs->spr_heart_full;
        current_hp -= hp_per_container;
        if (current_hp <= -hp_per_container) {
          heart = gs->spr_heart_empty;
        } else if (current_hp < -hp_per_container/2.f) {
          heart = gs->spr_heart_half;
        }
        r_push_quad(.pos = v3add(pos, v3(i*heart_hud_step)), .scale = v2(heart_hud_scale, heart_hud_scale), .sprite = heart);
      }
    }
  }

  os_heat_sync();

  if (runner_id() == 0) {
    r->draw_quads();

    r->bind_texture(gs->font.texture);
    f32 text_scale = 0.02f;
    // TODO: Display string builder to help with UI layout
    draw_string(gs->font, v2(-gs->render_dim.width/2.f, gs->render_dim.height/2.f - text_scale * 2 * gs->render_dim.width), text_scale,
      str8_pushf(gs->frame, "FPS: %f", 1000.f/g_delta_time));
    r->draw_quads();

    r->present(false);
  }
}