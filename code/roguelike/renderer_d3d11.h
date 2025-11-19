#ifndef RENDERER_D3D11_H
#define RENDERER_D3D11_H

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
  PNG_Bitmap_RGBA raw_texture_data;
  Sprite *sprites;
  u64 num_sprites;
} Texture_Atlas;

typedef struct R_Vertex {
  Vec3 pos;
  Vec2 uv;
} R_Vertex;

typedef struct Uniforms {
  Mat4 view_proj;
} Uniforms;

typedef struct Instance_Data {
  Mat4 world;
  Atlas_Coords atlas_coords;
  Vec4 color;
} Instance_Data;

function Vec2i r_init(HWND hwnd);
function void r_create_and_bind_texture(PNG_Bitmap_RGBA raw_texture_data, b32 generate_mipmaps);
function void r_prep(void);
function void r_update_transform(Mat4 m);
function void r_present(b32 enable_vsync);

typedef struct Push_Quad_Params {
  Vec3 pos;
  Vec2 scale;
  Vec2 rot_offset;
  Vec4 col;
  Quat rot;
  Atlas_Coords atlas_coords;
  Sprite sprite;
} Push_Quad_Params;
#define r_push_quad(...) r_push_quad_(&(Push_Quad_Params){ \
  .scale = v2(1,1), \
  .col = v4(1,1,1,1), \
  .rot = qi(), \
  __VA_ARGS__ \
  })
function void r_push_quad_(Push_Quad_Params *p);

#endif // RENDERER_D3D11_H