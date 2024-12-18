// Voodoo is a sick name

//- @note: Unity build

//- @note: Headers
//#include "third_party/microui/microui.h"

#include "base/include.h"
#include "os/include.h"
#include "json/json.h"

#include "game.h"
#include "map.h"
#include "renderer.h"

//- @note: Source
#undef RELATIVE
#undef ABSOLUTE
//#include "third_party/microui/microui.c"

#include "base/include.c"
#include "os/include.c"
#include "json/json.c"

#include "map.c"
#include "renderer.c"

/*
@todo
-Seperate game / platform
-Make another window using win ui for like dev tweaking n stuff
-Microui
-Level editor
-Hot Reloading
-Lighting
-Wall texture mapping
-SIMD???? -> compile renderer code into ISPC
-Multithreading??
-Optimize / profile render functions
-Parse c file and pass #run directive to stdin of compiler, link with result
-sin/cos/tan table lookup: https://namoseley.wordpress.com/2015/07/26/sincos-generation-using-table-lookup-and-iterpolation/
-Asan / Libfuzzer
*/

typedef struct Permanent_Data {
    Entity player;
    Map test_level;
    Vec3 map_cam;
} Permanent_Data;

typedef struct Game_Memory_Package {
    Arena *forever;
    Arena *frame;
    Arena *level;
    Permanent_Data *permanent_data;
} Game_Memory_Package;

typedef struct Game_Tick_Package {
    f32 dt;
    Game_Input input;
} Game_Tick_Package;

function void
game_init (Game_Memory_Package *memory) {
    Permanent_Data *data = arena_pushn(memory.forever, Permanent_Data, 1);
    memory->permanent_data = data;
    Entity *player = &data->player;
    player->height = 15;
    player->radius = 20.f;
    
    data->test_level = map_load(perm_arena, str8_lit("w:/code/dumb/level.json"));
    data->map_cam = v3(0, 0, 50);
}

function void
game_tick (Game_Memory_Package memory, Game_Tick_Package tick) {
    Entity *player = &memory.permanent_data->player;
    Vec3 *map_cam = &memory.permanent_data->map_cam;
    
    Sector *player_sector = &test_level.sectors[player.curr_sector];
    player->rotation_angle -= turn_amount;
    player->rotation_angle = fmod_cycling(player.rotation_angle, 2 * M_PI32);
    turn_amount = 0;
    
    // @todo: There has got to be a better/cleaner/faster way to calculate movement + dir for both of these things
    Vec2 dir;
    f32 x = 0, y = 0;
    if (move_forward)   x +=  1;
    if (move_back)      x += -1;
    if (strafe_left)    y +=  1;
    if (strafe_right)   y += -1;
    dir.x = x * cosf(player.rotation_angle) - y * sinf(player.rotation_angle);
    dir.y = x * sinf(player.rotation_angle) + y * cosf(player.rotation_angle);
    if (v2len(dir) > 1)
        dir = v2norm(dir);
    player->pos = v2add(player->pos, v2muls(dir, PLAYER_MOVE_SPEED * dt));
    
    Vec2 map_cam_dir = {0};
    if (cam_up)    map_cam_dir.y += 1;
    if (cam_down)  map_cam_dir.y -= 1;
    if (cam_left)  map_cam_dir.x -= 1;
    if (cam_right) map_cam_dir.x += 1;
    if (v2len(map_cam_dir) > 1)
        map_cam_dir = v2norm(map_cam_dir);
    Vec2 map_cam_v2 = v2add(dv3(map_cam), v2muls(map_cam_dir, CAM_MOVE_SPEED * dt));
    map_cam->x = map_cam_v2.x;
    map_cam->y = map_cam_v2.y;
    
    // @todo: Can we avoid polling every frame?
    update_current_sector(&player, &test_level);
}

function void
game_render (Game_Memory_Package memory) {
#if 1
    r_sector(&test_level, player_sector, &player, -1, initial_bounds);
#else
    // @todo: This causes the screen to flicker when changing rooms in 3D. Keep an eye on this one...
    r_clear_color(Color_Black);
    r_map(test_level, map_cam, player, true);
#endif
}