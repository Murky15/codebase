// Voodoo is a sick name

//- @note: Headers
#include <stdio.h>

#include "base/include.h"
#include "os/include.h"
#include "file/json.h"

#include "game.h"
#include "map.h"
#include "renderer.h"

//- @note: Source
#include "base/include.c"
#include "os/include.c"
#include "file/json.c"

#include "map.c"
#include "renderer.c"

#define MIN_EXCESS_MEMORY Kilobytes(2)

#define PLAYER_MOVE_SPEED 150.f

/*
@todo
-Microui
-Level editor
-Hot Reloading
-Lighting
-Wall texture mapping
-Collisions
-SIMD???? -> compile renderer code into ISPC
-Optimize / profile render functions
-Parse c file and pass #run directive to stdin of compiler, link with result
-sin/cos/tan table lookup: https://namoseley.wordpress.com/2015/07/26/sincos-generation-using-table-lookup-and-iterpolation/
-Asan / Libfuzzer
-Aspect ratio correction
-OH I FORGOT ABOUT AUDIO
*/

/*
What if instead of header files we just had source files and the metaprogram 
would travel to every source file in a dir/project and generate
forward declarations for structs and functions into one huge "project.inc"
that you would include. That way you don't need to fret about ordering, you could write
all your structs like

struct name {

};

 The file would be comment seperated to indicate the source file boundaries it was generated from.
*/

typedef struct Game_State {
    Map test_level;
    Range view_bounds;
    
    // @todo: Can start to see how we will have a list of "old" entities and new ones...
    Entity player;
    Entity old_player;
    
    Arena *frame_arena;
    Arena *level_arena;
} Game_State;

function Entity 
entity_lerp (Entity a, Entity b, f32 amount) {
    Entity l = b;
    l.pos.x = lerp(a.pos.x, b.pos.x, amount);
    l.pos.y = lerp(a.pos.y, b.pos.y, amount);
    l.height = lerp(a.height, b.height, amount);
    l.rotation_angle = lerp(a.rotation_angle, a.rotation_angle - b.rotation_diff, amount);
    
    return l;
}

function void
game_init (Game_Memory_Package memory, Range view_bounds) {
    assert(memory.size >= sizeof(Game_State));
    u64 remaining_size = memory.size - sizeof(Game_State);
    if (remaining_size >= MIN_EXCESS_MEMORY) {
        // @todo: Need to revisit and come up with a better memory partitioning scheme
        Game_State *gs = (Game_State*)memory.memory;
        u64 arena_size = remaining_size / 2;
        u8 *frame_addr = (u8*)memory.memory + sizeof(Game_State);
        u8 *level_addr = frame_addr + arena_size;
        gs->frame_arena = arena_alloc_fixed(frame_addr, arena_size);
        gs->level_arena = arena_alloc_fixed(level_addr, arena_size);
        gs->player.height = 15;
        gs->player.radius = 20.f;
        gs->view_bounds = view_bounds;
        gs->test_level = map_load(gs->level_arena, str8_lit("w:/code/dumb/level.json"));
    } else {
        fprintf(stderr, "Insufficient memory!\n");
    }
}

function void
game_tick (Game_Memory_Package memory, Game_Input_Package input, f32 dt) {
    assert(memory.memory);
    Game_State *gs = (Game_State*)memory.memory;
    
    arena_clear(gs->frame_arena);
    
    gs->old_player = gs->player;
    Entity *player = &gs->player;
    
    player->rotation_diff = input.turn_amount * dt;
    player->rotation_angle -= player->rotation_diff;
    player->rotation_angle = fmod_cycling(player->rotation_angle, 2 * M_PI32);
    
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
    player->pos = v2add(player->pos, v2muls(dir, PLAYER_MOVE_SPEED * dt));
    
    update_current_sector(player, &gs->test_level);
}

function void
game_render (Game_Memory_Package memory, f32 lerp_amount) {
    Game_State *gs = (Game_State*)memory.memory;
    Sector *player_sector = &gs->test_level.sectors[gs->player.curr_sector];
    
    Entity lerped_player = entity_lerp(gs->old_player, gs->player, lerp_amount);
    r_sector(&gs->test_level, player_sector, &lerped_player, -1, gs->view_bounds);
}