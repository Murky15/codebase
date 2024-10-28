/* @note:
http://rdcu.be/tIHq
First part of the dungeon generation algorithm. Runs a evolutionary algorithm (EA) to determine adequate Cellular Automata (CA) rules for
quickly generating game dungeons at runtime. This way we can have every dungeon have a distinct "feel", without suffering the penalty
of waiting 20+ min for the EA to finish, or spend hours trying to find the perfect CA rules.

This file is meant to be compiled standalone and run before the CA process begins.
*/

#include <stdio.h>
#include <stdint.h>

#include "../base/include.h"
#include "../base/include.c"


typedef struct {
    u64 traversable_areas;
    Vec2 max_traversable_size;
    Vec2 avg_traversable_size;
    u64 passages;
    u64 avg_passage_len;
    u64 rooms;
    Vec2 avg_room_size;
    u64 culdesacs;
    u64 dead_ends;
} Target_Attributes;

// popcnt to get neighborhood size
#define MOORE   0b11111111
#define NEUMANN 0b11110000
typedef struct {
    u8 neighborhood;
    u8 states;
} Chromosome_Data;

void
ea_tick () {
    
}

int
main (void) {
    Arena *sim_arena = arena_alloc();
    
    
    return 0;
}