#ifndef DUNGEON_H
#define DUNGEON_H

// https://www.roguebasin.com/index.php?title=Basic_BSP_Dungeon_generation

typedef struct Dungeon_Params {
    Vec2 size;
    u64 depth; // # of rooms = 2^depth (this API kinda sucks)
} Dungeon_Params;

typedef union BSP_Node {
    // BSP algorithm enforces dungeon rooms to be rectangles
    struct { Vec2 p0, p1, p2, p3; }; // Starting with top left - goes in clockwise order
    Vec2 p[4];
} BSP_Node;

typedef struct Border {
    Vec2 p0, p1;
    Color color;
} Border;

typedef struct Border_Array {
    Border *borders;
    u64 count;
} Border_Array;

#endif //DUNGEON_H
