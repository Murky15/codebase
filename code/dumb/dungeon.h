#ifndef DUNGEON_H
#define DUNGEON_H

// https://www.roguebasin.com/index.php?title=Basic_BSP_Dungeon_generation

typedef struct Dungeon_Params {
    Vec2 size; // Size of bounding box
    Range cell_split_bounds;
    Vec2 min_cell;
    Vec2 min_room;
    u64 depth; // # of rooms = 2^depth (this API kinda sucks)
} Dungeon_Params;

typedef struct Border {
    Vec2 p0, p1;
    Color color;
} Border;

typedef struct Border_Array {
    Border *borders;
    u64 count;
} Border_Array;

// Basically a room
typedef struct Sector {
    union {
        struct { Border top, bottom, left, right; };
        Border borders[4];
    };
} Sector;

typedef struct Sector_Array {
    Sector *sectors;
    u64 count;
} Sector_Array;

typedef struct BSP_Node {
    Sector *child_sector;
    // BSP algorithm enforces dungeon rooms to be rectangles
    union {
        struct { Vec2 p0, p1, p2, p3; }; // Starting with top left - goes in clockwise order
        Vec2 p[4];
    };
} BSP_Node;

#endif //DUNGEON_H
