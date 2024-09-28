#ifndef DUNGEON_H
#define DUNGEON_H

// https://www.roguebasin.com/index.php?title=Basic_BSP_Dungeon_generation

typedef struct Boundary {
    vec2 p0, p1;
} Boundary; // Equivelant to a walldef in doom

typedef struct Sector_Node {
    struct Sector_Node *left;
    struct Sector_Node *right;
    
    Boundary *borders[4];
} Sector_Node;


#endif //DUNGEON_H
