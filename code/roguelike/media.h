#ifndef GRAPHICS_H
#define GRAPHICS_H

// NOTE: Cross-Platform Helpers

typedef void* R_Texture_2D;
typedef void* R_Window;

// NOTE: Game Structures

typedef struct Atlas_Coords {
  Vec2 scale;
  Vec2 offset;
} Atlas_Coords;

#define MAX_FRAMES 4
typedef struct Sprite {
  String8 name;
  Atlas_Coords coords[MAX_FRAMES];
  u64 num_frames;
  u64 current_frame;
  f32 started_at;
  f32 seconds_to_complete;
} Sprite;

typedef struct Texture_Atlas {
  R_Texture_2D texture;
  Sprite *sprites;
  u64 num_sprites;
} Texture_Atlas;

typedef struct Sound {
  struct Sound *next;
  u64 _id;
  Wave_Data audio_data;
  String8 name;
  u64 local_cursor;
  b32 loop;
} Sound;

typedef struct Sound_List {
  Sound *first, *last;
  u64 count;
} Sound_List;

typedef struct Playlist {
  Sound *sounds;
  b32 *played;
  u64 count;
  u64 sounds_played;
  b32 loop;
  b32 shuffle;
} Playlist;

// NOTE: Graphics

typedef struct R_Vertex {
  Vec3 pos;
  Vec2 uv;
} R_Vertex;

typedef struct R_Uniforms {
  Mat4 view_proj;
} R_Uniforms;

typedef struct R_Instance_Data {
  Mat4 world;
  Atlas_Coords atlas_coords;
  Vec4 color;
} R_Instance_Data;

global read_only R_Vertex r_quad_vertices[4] = {
  {{0, 0, 0}, {0, 1}},
  {{1, 0, 0}, {1, 1}},
  {{1, 1, 0}, {1, 0}},
  {{0, 1, 0}, {0, 0}},
};
global read_only u32 r_quad_indices[6] = {0, 1, 2, 2, 3, 0};

// NOTE: Graphics API

typedef struct Push_Quad_Params {
  Vec3 pos;
  Vec2 scale;
  Vec3 rot_offset;
  Vec4 col;
  Quat rot;

  Atlas_Coords atlas_coords;
  Sprite sprite;
} Push_Quad_Params;

typedef R_Texture_2D (*r_create_texture_type)(PNG_Bitmap_RGBA,b32);
typedef void         (*r_bind_texture_type)(R_Texture_2D);
typedef void         (*r_prep_type)(void);
typedef void         (*r_update_transform_type)(Mat4);
typedef void         (*r_push_quad_type)(Push_Quad_Params*);
typedef void         (*r_draw_quads_type)(void);
typedef void         (*r_present_type)(b32);

typedef struct Renderer_VTable {
  r_create_texture_type create_texture;
  r_bind_texture_type bind_texture;
  r_prep_type prep;
  r_update_transform_type update_transform;
  r_push_quad_type push_quad;
  r_draw_quads_type draw_quads;
  r_present_type present;
} Renderer_VTable;

function Vec2i        r_init(R_Window canvas);
function R_Texture_2D r_create_texture(PNG_Bitmap_RGBA raw_texture_data, b32 generate_mipmaps);
function void         r_bind_texture(R_Texture_2D tex_view);
function void         r_prep(void);
function void         r_update_transform(Mat4 m);
function void         r_draw_quads(void);
function void         r_present(b32 enable_vsync);
function void         r_push_quad_(Push_Quad_Params *p);

#endif // GRAPHICS_H