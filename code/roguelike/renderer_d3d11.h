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

#endif // RENDERER_D3D11_H