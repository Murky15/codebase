#ifndef MAP_H
#define MAP_H

// @todo: Need to think about map mutability for editors and stuff

typedef struct Wall {
    Vec2 p1, p2;
    s32 next_sector;
} Wall;

typedef struct Sector {
    u16 id;
    u16 depth;
    u16 height;
    // @todo: Texture references
    Wall *walls;
    u64 num_walls;
} Sector;

typedef struct Map {
    String8 name;
    Sector *sectors;
    u64 num_sectors;
} Map;

function Map map_load(Arena *arena, String8 path);

#endif //MAP_H
