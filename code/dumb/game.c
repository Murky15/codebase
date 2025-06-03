//- @note: Headers
#include <stdio.h>

#include "base/include.h"
#include "os/include.h"
#include "file/json.h"
#include "file/png.h"

#include "game.h"
#include "map.h"
#include "renderer.h"

//- @note: Source
#include "base/include.c"
#include "os/include.c"
#include "file/json.c"
#include "file/png.c"

#include "map.c"
#include "renderer.c"

#define MIN_EXCESS_MEMORY Kilobytes(2)

#define PLAYER_MOVE_SPEED 150.f

/*
@todo
-Bake asset path
-Level editor
-Hot Reloading
-Lighting
-Wall texture mapping
-Collisions
-Optimize / profile render functions
-compile renderer code into ISPC AoS->SoA layout
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
    
    Asset_Group level_textures;
    
    // @todo: Can start to see how we will have a list of "old" entities and new ones...
    Entity player;
    Entity old_player;
    
    Arena *permanent;
    Arena *frame;
    Arena *level;
} Game_State;

function Asset
asset_group_fetch (Asset_Group *assets, String8 name) {
    u64 hash = str8_hash(name) % assets->count;
    b32 found = true;
    for (u64 start_hash = hash, i = 0; !str8_match(assets->table[hash].name, name, 0); ++i) {
        if (i > 0 && start_hash == hash) {
            found = false;
            break;
        }
        hash++;
        hash %= assets->count;
    }
    
    return found == true ? assets->table[hash] : comp_zero(Asset);
}

function Asset_Group
create_asset_group_from_directory (Arena *arena, Asset_Group_Type type, Directory_Search_Results dir) {
    Asset_Group result = {0};
    result.count = dir.count;
    result.type = type;
    result.table = arena_pushn(arena, Asset, result.count);
    
    for (Directory_Search_Result_Node *node = dir.first; node; node = node->next) {
        Directory_Search_Result raw_asset = node->result;
        Asset to_add = {0};
        to_add.name = str8_push_copy(arena, raw_asset.name);
        switch (type) {
            case ASSET_GROUP_IMAGES: {
                to_add.img = png_decode(arena, raw_asset.data);
            } break;
        }
        u64 hash = str8_hash(to_add.name) % result.count;
        while (result.table[hash].name.str != 0) {
            hash++;
            hash %= result.count;
        }
        result.table[hash] = to_add;
    }
    
    return result;
}

function Entity 
entity_lerp (Entity a, Entity b, f32 amount) {
    Entity l = b;
    l.pos.x = lerp(a.pos.x, b.pos.x, amount);
    l.pos.y = lerp(a.pos.y, b.pos.y, amount);
    l.rotation_angle = lerp(a.rotation_angle, a.rotation_angle - b.rotation_diff, amount);
    
    return l;
}

function void
game_init (Game_Memory_Package memory, Range view_bounds) {
    assert(memory.size >= sizeof(Game_State));
    u64 remaining_size = memory.size - sizeof(Game_State);
    assert (remaining_size >= MIN_EXCESS_MEMORY);
    
    Temp_Arena scratch = get_scratch(0,0);
    
    // @todo: Need to revisit and come up with a better memory partitioning scheme
    Game_State *gs = (Game_State*)memory.memory;
    u64 arena_size = remaining_size / 3;
    u8 *perm_addr = (u8*)memory.memory + sizeof(Game_State);
    u8 *frame_addr = perm_addr + arena_size;
    u8 *level_addr = frame_addr + arena_size;
    gs->permanent = arena_alloc_fixed(perm_addr, arena_size);
    gs->frame = arena_alloc_fixed(frame_addr, arena_size);
    gs->level = arena_alloc_fixed(level_addr, arena_size);
    gs->player.height = 15;
    gs->player.radius = 20.f;
    gs->player.rotation_angle = M_PI32/2.f-.0001f;
    gs->old_player = gs->player;
    gs->view_bounds = view_bounds;
    gs->test_level = map_load(gs->level, str8_lit("w:/code/dumb/level.json"));
    
    // Load textures
    Directory_Search_Results raw_textures = os_search_directory_and_read_files(scratch.arena, str8_lit("W:/assets/dumb/art/RETRO_TEXTURE_PACK_V17/TEXTURES"), str8_lit("*.PNG"));
    gs->level_textures = create_asset_group_from_directory(gs->permanent, ASSET_GROUP_IMAGES, raw_textures);
    
    release_scratch(scratch);
}

function void
game_tick (Game_Memory_Package memory, Game_Input_Package input, f32 dt) {
    Game_State *gs = (Game_State*)memory.memory;
    
    arena_clear(gs->frame);
    
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
    
    // @note: Player sector is updated in game_render! (Otherwise we get a nasty flicker which we can't solve through approximation)
    // We just reuse the "lerped player's" current sector to avoid calling the `update_current_sector` function more than we need to
    // This is a good thing to share for my MIT portfolio, how I mastered the debugger to try and find this thing.
}

function void
game_render (Game_Memory_Package memory, f32 lerp_amount) {
    Game_State *gs = (Game_State*)memory.memory;
    Entity lerped_player = entity_lerp(gs->old_player, gs->player, lerp_amount);
    
    update_current_sector(&lerped_player, &gs->test_level);
    Sector *player_sector = &gs->test_level.sectors[lerped_player.curr_sector];
    gs->player.curr_sector = lerped_player.curr_sector;
    
    r_clear();
    r_sector(&gs->test_level, player_sector, gs->level_textures, &lerped_player, -1, 0, gs->view_bounds);
}