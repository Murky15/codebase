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
function void game_render(Game_Memory_Package memory);

//- @note: Game specific

typedef struct Entity {
    Vec2 pos;
    f32 height;
    f32 rotation_angle;
    f32 radius;
    s32 curr_sector;
} Entity;

#endif //GAME_H
