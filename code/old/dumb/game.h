#ifndef GAME_H
#define GAME_H

//- @note: Platform - game interop

typedef struct Game_Memory_Package {
    void *memory;
    u64 size;
} Game_Memory_Package;

typedef struct Game_Input_Package {
    b32 move_DIR_FORWARD;
    b32 move_back;
    b32 strafe_left;
    b32 strafe_right;

    f32 turn_amount;
} Game_Input_Package;

function void game_init(Game_Memory_Package memory, Range view_bounds);
function void game_tick(Game_Memory_Package memory, Game_Input_Package input, f32 dt);
function void game_render(Game_Memory_Package memory, f32 lerp_amount);

//- @note: Game specific

typedef u32 Entity_Flags;

typedef struct Entity {
    Vec2 pos;
    f32 height;
    f32 radius;

    f32 rotation_angle;
    f32 rotation_diff; // For movement calculation purposes only

    s32 curr_sector;
} Entity;

typedef u32 Asset_Group_Type;
enum {
    ASSET_GROUP_NULL,

    ASSET_GROUP_IMAGES,
    ASSET_GROUP_MUSIC,
    ASSET_GROUP_FONTS,
    ASSET_GROUP_DATA,
};

// @todo: Where to store this? (With texture or with wall data?)
typedef u32 Texture_Map_Type;
enum {
    TEXTURE_MAP_NULL,

    TEXTURE_MAP_FIT,
    TEXTURE_MAP_REPEAT,
    TEXTURE_MAP_MIRROR,
};

typedef struct Asset {
    String8 name;
    union { // Put other asset types here
        PNG_Bitmap_RGBA img;
    };
} Asset;

typedef struct Asset_Group {
    Asset_Group_Type type;
    Asset *table;
    u64 count;
} Asset_Group;


//- @note: Tangible types
typedef union Color {
  struct {u8 r, g, b, a;};
  u32 packed;
} Color;

#define colors \
color(Black,   0,   0,   0)   \
color(White,   255, 255, 255) \
color(Red,     255, 0,   0)   \
color(Lime,    0,   255, 0)   \
color(Blue,    0,   0,   255) \
color(Yellow,  255, 255, 0)   \
color(Cyan,    0,   255, 255) \
color(Magenta, 255, 0,   255) \
color(Silver,  192, 192, 192) \
color(Gray,    128, 128, 128) \
color(Maroon,  128, 0,   0)   \
color(Olive,   128, 128, 0)   \
color(Green,   0,   128, 0)   \
color(Purple,  128, 0,   128) \
color(Teal,    0,   128, 128) \
color(Navy,    0,   0,   128) \

#define color(name, r, g, b) read_only Color glue(Color_,name) = {r,g,b,255};
colors
#undef color
#undef colors

function Asset asset_group_fetch(Asset_Group *assets, String8 name);
function Asset_Group create_asset_group_from_directory(Arena *arena, Asset_Group_Type type, Directory_Search_Results dir);

function Entity entity_lerp(Entity a, Entity b, f32 amount);

#endif //GAME_H
