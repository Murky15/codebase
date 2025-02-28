#ifndef GAME_H
#define GAME_H

//- @note: Platform - game interop

typedef struct Game_Memory_Package {
    void *memory;
    u64 size;
} Game_Memory_Package;

typedef struct Game_Input_Package {
    // @note: Do we put "camera controlls" here? 
    // If it is a dev tool it should probably be handled by the
    // platform layer no?
    b32 move_forward;
    b32 move_back;
    b32 strafe_left;
    b32 strafe_right;
    
    f32 turn_amount;
} Game_Input_Package;

function void game_init(Game_Memory_Package memory, Range view_bounds);
function void game_tick(Game_Memory_Package memory, Game_Input_Package input, f32 dt);
function void game_render(Game_Memory_Package memory, f32 lerp_amount);

//- @note: Game specific

typedef struct Entity {
    Vec2 pos;
    f32 height;
    f32 rotation_angle;
    f32 rotation_diff;
    f32 radius;
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
        struct {
            PNG_Bitmap_RGBA img;
        };
    };
} Asset;

typedef struct Asset_Group {
    Asset_Group_Type type;
    Asset *table;
    u64 count;
} Asset_Group;

function Asset asset_group_fetch(Asset_Group *assets, String8 name);
function Asset_Group create_asset_group_from_directory(Arena *arena, Asset_Group_Type type, Directory_Search_Results dir);

function Entity entity_lerp(Entity a, Entity b, f32 amount);

#endif //GAME_H
