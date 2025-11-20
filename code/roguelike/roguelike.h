#ifndef ROGUELIKE_H
#define ROGUELIKE_H

// NOTE: Game<-->platform interface

typedef void (*r_create_and_bind_texture_type)(PNG_Bitmap_RGBA,b32);
typedef void (*r_prep_type)(void);
typedef void (*r_update_transform_type)(Mat4);
typedef void (*r_push_quad_type)(Push_Quad_Params);
typedef void (*r_present_type)(Push_Quad_Params);

typedef struct Game_Init_Package {
  Arena *perm;
  Arena *frame;
  String8 source_dir;
  String8 asset_dir;
  f32 display_width;
  f32 display_height;
  r_create_and_bind_texture_type create_and_bind_texture;
} Game_Init_Package;

typedef struct Game_Input_Package {
  b32 move_forward;
  b32 move_back;
  b32 strafe_left;
  b32 strafe_right;
} Game_Input_Package;

typedef struct Push_Quad_Params {
  Vec3 pos;
  Vec2 scale;
  Vec2 rot_offset;
  Vec4 col;
  Quat rot;
  Atlas_Coords atlas_coords;
  Sprite sprite;
} Push_Quad_Params;
#define r_push_quad(...) r->push_quad(&(Push_Quad_Params){ \
  .scale = v2(1,1), \
  .col = v4(1,1,1,1), \
  .rot = qi(), \
  __VA_ARGS__ \
  })

typedef struct Renderer_VTable {
  r_create_and_bind_texture_type create_and_bind_texture;
  r_prep_type prep;
  r_update_transform_type update_transform;
  r_push_quad_type push_quad;
  r_present_type present;
} Renderer_VTable;


typedef void *(*roguelike_init_type)(Thread_Context*,Game_Init_Package);
typedef void  (*roguelike_tick_type)(Thread_Context*,void*,f32,Game_Input_Package);
typedef void  (*roguelike_draw_type)(Thread_Context*,void*,Renderer_VTable*);

typedef struct Game_VTable {
  roguelike_init_type init; /* NOTE: Always single threaded */
  roguelike_tick_type tick;
  roguelike_draw_type draw;
} Game_VTable;

// NOTE: Game data

#define PLAYER_MOVE_SPEED 0.15f

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

typedef struct Game_State {
  Arena *perm;
  Arena *frame;

  Texture_Atlas sprites;
  Sprite spr_wall_mid;
  Sprite spr_ceil;
  Vec4 ceil_color;

  Dungeon dungeon;
  Mat4 proj;
  Quat floor_rot;
  Quat forward_wall_rot;

  Entity player;
  Camera cam;
} Game_State;


function Cardinal_Dir to_cardinal(Vec2 dir);
function Sprite* get_atlas_slot(Texture_Atlas atlas, String8 key);
function Sprite  get_sprite(Texture_Atlas atlas, String8 key);
function Atlas_Coords make_atlas_coords_from_string(String8 coords);
function Texture_Atlas load_textures(Arena *arena, String8 absolute_path_to_asset_dir);
function Rect cam_calculate_visible_range(Camera cam, f32 fov_h, f32 aspect_ratio, f32 znear);
function void draw_entity(Entity *e, Renderer_VTable *r);

#endif // ROGUELIKE_H