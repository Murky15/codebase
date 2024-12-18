#ifndef GAME_H
#define GAME_H

//- @note: Game specific data

typedef struct Entity {
    Vec2 pos;
    f32 height;
    f32 rotation_angle;
    f32 radius;
    s32 curr_sector;
} Entity;

typedef struct Border {
    Vec2 p0, p1;
    Color color;
} Border;

//- @note: Platform - game interop

typedef struct Game_Memory_Package {
    Arena *forever;
    Arena *frame;
    Arena *level;
    void *permanent_data;
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

typedef struct Game_Tick_Package {
    // If we don't need anything else is it really worth having a "package" just for this?
    f32 dt;
} Game_Tick_Package;

function void game_init(Game_Memory_Package *memory, Range view_bounds);
function void game_tick(Game_Memory_Package memory, Game_Input_Package input, Game_Tick_Package tick);
function void game_render(Game_Memory_Package memory);

#endif //GAME_H
