#ifndef DUNGEON_H
#define DUNGEON_H

// https://www.roguebasin.com/index.php?title=Basic_BSP_Dungeon_generation

typedef struct Dungeon_Create_Params {
    u64 map_width;
    u64 map_height;
} Dungeon_Create_Params;

typedef struct Border {
    Vec2 p0, p1;
    Color color;
} Border;

typedef struct BSP_Node {
    struct BSP_Node *left;
    struct BSP_Node *right;
    
    // BSP algorithm enforces dungeon rooms to be rectangles
    union {
        struct {
            Border *top;
            Border *bottom;
            Border *left;
            Border *right;
        }
        Border *borders[4];
    }
} BSP_Node;

#endif //DUNGEON_H
