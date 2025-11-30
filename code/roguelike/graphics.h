#ifndef GRAPHICS_H
#define GRAPHICS_H

typedef void* R_Texture_2D;

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

#endif // GRAPHICS_H