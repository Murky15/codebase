#ifndef ROGUELIKE_H
#define ROGUELIKE_H

typedef u32 Cardinal_Dir;
enum {
  NORTH = (1 << 0),
  SOUTH = (1 << 1),
  EAST  = (1 << 2),
  WEST  = (1 << 3),

  NORTHEAST = NORTH | EAST,
  NORTHWEST = NORTH | WEST,
  SOUTHEAST = SOUTH | EAST,
  SOUTHWEST = SOUTH | WEST,
};

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

/*
  We can have a 'tween' function type here that takes two Vector3s
  which is then packed in the Camera struct for camera_update_tracking
  to modify how the camera changes position
*/
typedef struct Camera {
  Vec3 pos;
  Vec3 focus;
  Vec3 follow_dist;
  Rect visible_range;
} Camera;

typedef struct Entity {
  // General info
  Vec3 pos;
  f32 rotation_angle;

  // Rotation animation
  Cardinal_Dir dir;
  f32 start_angle;
  f32 end_angle;
  f32 seconds_to_rotate;
  f32 started_rotating_at;

  // Sprites
  Sprite idle;
  Sprite run;
} Entity;

function Cardinal_Dir to_cardinal(Vec2 dir);
function Sprite* get_atlas_slot(Texture_Atlas atlas, String8 key);
function Sprite  get_sprite(Texture_Atlas atlas, String8 key);
function Atlas_Coords make_atlas_coords_from_string(String8 coords);
function Texture_Atlas load_textures(Arena *arena, String8 absolute_path_to_asset_dir);
function Rect cam_calculate_visible_range(Camera cam, f32 fov_h, f32 aspect_ratio, f32 znear);

#endif // ROGUELIKE_H