#ifndef ROGUELIKE_H
#define ROGUELIKE_H

// NOTE: Game<-->platform interface

typedef R_Texture_2D (*r_create_texture_type)(PNG_Bitmap_RGBA,b32);
typedef void         (*r_bind_texture_type)(R_Texture_2D);
typedef void         (*r_prep_type)(void);
typedef void         (*r_update_transform_type)(Mat4);
typedef void         (*r_push_quad_type)(Push_Quad_Params*);
typedef void         (*r_draw_quads_type)(void);
typedef void         (*r_present_type)(b32);

#define r_push_quad(...) r->push_quad(&(Push_Quad_Params){ \
  .scale = v2(1,1), \
  .col = v4(1,1,1,1), \
  .rot = qi(), \
  __VA_ARGS__ \
  })

typedef struct Renderer_VTable {
  r_create_texture_type create_texture;
  r_bind_texture_type bind_texture;
  r_prep_type prep;
  r_update_transform_type update_transform;
  r_push_quad_type push_quad;
  r_draw_quads_type draw_quads;
  r_present_type present;
} Renderer_VTable;

typedef struct Game_Init_Package {
  Arena *perm;
  Arena *frame;
  String8 source_dir;
  String8 asset_dir;
  f32 display_width;
  f32 display_height;
  Renderer_VTable rvtbl;
} Game_Init_Package;

typedef struct Game_Input_Package {
  b32 move_forward;
  b32 move_back;
  b32 strafe_left;
  b32 strafe_right;

  b32 action_primary;
  Vec2 cursor;
} Game_Input_Package;

typedef void *(*roguelike_init_type)(Thread_Context*,Game_Init_Package);
typedef void  (*roguelike_tick_type)(Thread_Context*,void*,f32,Game_Input_Package,Game_Input_Package);
typedef void  (*roguelike_draw_type)(Thread_Context*,void*);

typedef struct Game_VTable {
  roguelike_init_type init; /* NOTE: Always single threaded */
  roguelike_tick_type tick;
  roguelike_draw_type draw;
} Game_VTable;

// NOTE: Game data

typedef u64 Entity_Flags;
enum {
  ENTITY_FLAG_INPUT_SENSITIVE = (1 << 0),
  ENTITY_FLAG_ANIMATE_SPRITES = (1 << 1),
  ENTITY_FLAG_ANIMATE_ROTATIONS = (1 << 2),
  ENTITY_FLAG_DRAWABLE = (1 << 3),
  ENTITY_FLAG_COLLISION = (1 << 4),
  ENTITY_FLAG_DRAW_HEALTH = (1 << 5),
  ENTITY_FLAG_VULNERABLE = (1 << 6),
  ENTITY_FLAG_HARMFUL = (1 << 7),
};

typedef u32 Entity_Class;
enum {
  ENTITY_CLASS_NEUTRAL = 0,
  ENTITY_CLASS_HERO,
  ENTITY_CLASS_MONSTER,
  ENTITY_CLASS_MONSTER_TINY,
  ENTITY_CLASS_MONSTER_BIG,
  ENTITY_CLASS_WEAPON,

  ENTITY_CLASS_COUNT
};

// NOTE: These can be used to index into a metatable of entity templates
typedef u32 Entity_Type;
enum {
  ENTITY_TYPE_NULL = 0,
  ENTITY_TYPE_DEMON_BIG,
  ENTITY_TYPE_ZOMBIE_BIG,
  ENTITY_TYPE_WIZZARD,
  ENTITY_TYPE_IMP,
  ENTITY_TYPE_LIZARD,
  ENTITY_TYPE_DWARD,
  ENTITY_TYPE_KNIGHT,
  ENTITY_TYPE_WOGOL,
  ENTITY_TYPE_ZOMBIE,
  ENTITY_TYPE_ZOMBIE_TINY,
  ENTITY_TYPE_GOBLIN,
  ENTITY_TYPE_ICE_ZOMBIE,
  ENTITY_TYPE_ORC_SHAMAN,
  ENTITY_TYPE_SWAMPY,
  ENTITY_TYPE_MUDDY,
  ENTITY_TYPE_NECROMANCER,
  ENTITY_TYPE_MASKED_ORC,
  ENTITY_TYPE_ORC_WARRIOR,
  ENTITY_TYPE_SKELET,
  ENTITY_TYPE_OGRE,
  ENTITY_TYPE_DOC,
  ENTITY_TYPE_PUMPKIN_MAN,
  ENTITY_TYPE_ANGEL, // Could this be a hero? Monsters would attack the angel, that's interesting.
  ENTITY_TYPE_CHORT,
  ENTITY_TYPE_ELF,
  ENTITY_TYPE_SLUG,
  ENTITY_TYPE_SLUG_TINY,

  ENTITY_TYPE_COUNT
};

typedef u32 Attack_Phase;
enum {
  ATTACK_PHASE_NULL = 0,
  ATTACK_PHASE_ANTICIPATION,
  ATTACK_PHASE_ACTION,
  ATTACK_PHASE_RECOVERY,

  ATTACK_PHASE_COUNT
};

typedef struct Entity Entity;

typedef struct Entity_Ref {
  u64 gen;
  Entity *e;
} Entity_Ref;

struct Entity {
  // Header
  u64 gen;
  Entity_Flags flags;
  Entity_Class class;
  Entity_Type  type;

  // Position / Orientation
  Vec3 pos;
  Vec2 dir;
  Vec2 old_dir;
  Vec2 bbox;
  Quat rot;

  // Stats
  f32 hp;
  f32 hp_max;
  u64 num_heart_containers;
  f32 speed;

  f32 damage;
  f32 knockback;
  f32 durability;
  f32 max_durability;

  // TODO: Some way to classify what kinds of entities can hold this weapon.

  // Flip animation
  f32 start_flip_angle;
  f32 end_flip_angle;
  f32 seconds_to_flip;
  f32 started_flipping_at;
  f32 flip_angle;

  // Sword swing animation
  Attack_Phase slash_phase;
  f32 swing_angle;
  f32 seconds_to_swing;
  f32 seconds_for_anticipation;
  f32 seconds_for_recovery;
  f32 started_swing_at;
  Quat start_point_rot;
  Quat end_point_rot;
  Quat start_pos_rot;
  Quat end_pos_rot;

  // Sprites
  Sprite idle;
  Sprite run;
  f32 scale_mul;
  Vec3 rot_offset;

  // Misc
  Entity_Ref parent;

  // For Monsters
  Entity_Ref target_hero;
};

typedef u32 Camera_Track_Mode;
enum {
  CAMERA_TRACK_MODE_FIXED = 0,
  CAMERA_TRACK_MODE_LERP,

  CAMERA_TRACK_MODE_COUNT
};
typedef struct Camera {
  Vec3 pos;
  Vec3 focus;
  Vec3 follow_dist;
  Vec3 offset;
  f32  zoom;

  f32  fov_h;
  f32  aspect_ratio;
  f32  znear;
  f32  zfar;
  Rect visible_range;

  Entity_Ref tracking;
  Camera_Track_Mode track_mode;
} Camera;

function Sprite*       get_atlas_slot(Texture_Atlas atlas, String8 key);
function Sprite        get_sprite(Texture_Atlas atlas, String8 key);
function Atlas_Coords  make_atlas_coords_from_string(String8 coords);
function Texture_Atlas load_textures(Arena *arena, String8 absolute_path_to_asset_dir, r_create_texture_type r_create_texture);
function Texture_Atlas load_font (Arena* arena, String8 absolute_path_to_bitmap, r_create_texture_type r_create_texture);

function Entity_Ref make_ref(Entity *e);
function Entity*    get_entity(Entity_Ref ref);

function Rect cam_calculate_visible_range(Camera cam, f32 fov_h, f32 aspect_ratio, f32 znear);
function void cam_set_target(Camera *cam, Entity *e, Camera_Track_Mode track_mode);
function void cam_update_tracking(Camera *cam, f32 dt);

function void draw_entity(Entity *e);
function void draw_string(Texture_Atlas font_atlas, Vec2 pos, f32 scale, String8 string);

#endif // ROGUELIKE_H