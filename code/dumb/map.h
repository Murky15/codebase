#ifndef MAP_H
#define MAP_H

// @todo: Need to think about map mutability for editors and stuff

typedef struct Wall {
    Vec2 p0, p1;
    s32 next_sector;
    // @todo: Texture references
} Wall;

typedef struct Sector Sector;
typedef struct Sector_Ref {
    Sector *sector;
    struct Sector_Ref *next;
} Sector_Ref;

typedef struct Sector_List {
    Sector_Ref *first;
    Sector_Ref *last;
    u64 count;
} Sector_List;

struct Sector {
    s32 id;
    s32 floor;
    s32 ceiling;
    Wall *walls;
    u64 num_walls;
    Sector_List adjacent;
};

typedef struct Map {
    String8 name;
    Sector *sectors;
    u64 num_sectors;
} Map;

function void sector_list_push_ref(Arena *arena, Sector_List *list,  Sector *ref);
function Map map_load(Arena *arena, String8 path);
function b32 wall_intersect(Vec2 p, Vec2 dir, Wall w);
function b32 point_in_sector(Vec2 p, Sector *sector);

#endif //MAP_H
