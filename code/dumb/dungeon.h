#ifndef DUNGEON_H
#define DUNGEON_H

// https://www.roguebasin.com/index.php?title=Basic_BSP_Dungeon_generation

typedef struct Dungeon_Params {
    Vec2 size;
    u64 depth; // # of rooms = 2^depth (this API kinda sucks)
} Dungeon_Params;

typedef struct Border {
    Vec2 p0, p1;
    Color color;
} Border;

typedef struct BSP_Node {
    // BSP algorithm enforces dungeon rooms to be rectangles
    union {
        struct {
            Border t;
            Border b;
            Border l;
            Border r;
        };
        Border borders[4];
    };
} BSP_Node;

#endif //DUNGEON_H
