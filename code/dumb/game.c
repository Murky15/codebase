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

#define PLAYER_MOVE_SPEED 150.f

/*
@todo
-Make another window using win ui for like dev tweaking n stuff
-Microui
-Level editor
-Hot Reloading
-Lighting
-Wall texture mapping
-SIMD???? -> compile renderer code into ISPC
-Multithreading?? (Like how Ryan Fleury does it)
-Optimize / profile render functions
-Parse c file and pass #run directive to stdin of compiler, link with result
-sin/cos/tan table lookup: https://namoseley.wordpress.com/2015/07/26/sincos-generation-using-table-lookup-and-iterpolation/
-Asan / Libfuzzer
-Aspect ratio correction
*/

typedef struct Permanent_Data {
    Entity player;
    Map test_level;
    Range view_bounds;
} Permanent_Data;

function void
game_init (Game_Memory_Package *memory, Range view_bounds) {
    Permanent_Data *data = arena_pushn(memory->forever, Permanent_Data, 1);
    memory->permanent_data = data;
    Entity *player = &data->player;
    player->height = 15;
    player->radius = 20.f;
    
    data->test_level = map_load(memory->forever, str8_lit("w:/code/dumb/level.json"));
    data->view_bounds = view_bounds;
}

function void
game_tick (Game_Memory_Package memory, Game_Input_Package input, Game_Tick_Package tick) {
    Permanent_Data *data = (Permanent_Data*)memory.permanent_data;
    Entity *player = &data->player;
    player->rotation_angle -= input.turn_amount;
    player->rotation_angle = fmod_cycling(player->rotation_angle, 2 * M_PI32);
    
    // @todo: There has got to be a better/cleaner/faster way to calculate movement + dir for both of these things
    Vec2 dir;
    f32 x = 0, y = 0;
    if (input.move_forward)   x +=  1;
    if (input.move_back)      x += -1;
    if (input.strafe_left)    y +=  1;
    if (input.strafe_right)   y += -1;
    dir.x = x * cosf(player->rotation_angle) - y * sinf(player->rotation_angle);
    dir.y = x * sinf(player->rotation_angle) + y * cosf(player->rotation_angle);
    if (v2len(dir) > 1)
        dir = v2norm(dir);
    player->pos = v2add(player->pos, v2muls(dir, PLAYER_MOVE_SPEED * tick.dt));
    
    // @todo: Can we avoid polling every frame?
    update_current_sector(player, &data->test_level);
}

function void
game_render (Game_Memory_Package memory) {
    Permanent_Data *data = (Permanent_Data*)memory.permanent_data;
    Entity *player = &data->player;
    Map *test_level = &data->test_level;
    Range view_bounds = data->view_bounds;
    Sector *player_sector = &test_level->sectors[player->curr_sector];
    r_sector(test_level, player_sector, player, -1, view_bounds);
}