// @todo: Textures should be stored per-wall for more flexibility

typedef struct Wall {
    Vec2 p1, p2;
    u64 next_sector;
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

function Map
map_load (Arena *arena, String8 path) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    Map map = {0};
    
    String8 json = os_read_file(scratch.arena, path, false);
    Json_Value level_data = json_parse(scratch.arena, json);
    if (level_data.type > 0) {
        map.name = json_object_fetch(&level_data.object, str8_lit("name")).value.string;
        Json_Array sectors = json_object_fetch(&level_data.object, str8_lit("sectors")).value.array;
        map.num_sectors = sectors.values.count;
        map.sectors = arena_pushn(arena, Sector, map.num_sectors);
        u64 i = 0;
        for (Json_Value_Node *sector_node = sectors.values.first; sector_node; sector_node = sector_node->next, ++i) {
            Json_Object sector_data = sector_node->value.object;
            Sector *sector = map.sectors + i;
            sector->id = (u64)json_object_fetch(&sector_data, str8_lit("id")).value.number;
            sector->depth = (u64)json_object_fetch(&sector_data, str8_lit("depth")).value.number;
            sector->height = (u64)json_object_fetch(&sector_data, str8_lit("height")).value.number;
            Json_Array walls = json_object_fetch(&sector_data, str8_lit("walls")).value.array;
            sector->num_walls = walls.values.count;
            sector->walls = arena_pushn(arena, Wall, sector->num_walls);
            u64 j = 0;
            for (Json_Value_Node *wall_node = walls.values.first; wall_node; wall_node = wall_node->next, ++j) {
                sector->walls[j].p1 = ;
            }
        }
    } else {
        fprintf(stderr, "Error parsing level file!");
    }
    
    release_scratch(scratch);
    return map;
}