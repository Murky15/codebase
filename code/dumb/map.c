// @todo: Textures should be stored per-wall for more flexibility

function Map
map_load (Arena *arena, String8 path) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    Map map = {0};
    
    String8 json = os_read_file(scratch.arena, path, false);
    Json_Value level_data = json_parse(scratch.arena, json);
    if (level_data.type > 0) {
        map.name = json_object_fetch(&level_data.object, str8_lit("name")).string;
        Json_Array sectors = json_object_fetch(&level_data.object, str8_lit("sectors")).array;
        map.num_sectors = sectors.values.count;
        map.sectors = arena_pushn(arena, Sector, map.num_sectors);
        u64 i = 0;
        for (Json_Value_Node *sector_node = sectors.values.first; sector_node; sector_node = sector_node->next, ++i) {
            Json_Object sector_data = sector_node->value.object;
            Sector *sector = map.sectors + i;
            sector->id = (u16)json_object_fetch(&sector_data, str8_lit("id")).number;
            sector->depth = (u16)json_object_fetch(&sector_data, str8_lit("depth")).number;
            sector->height = (u16)json_object_fetch(&sector_data, str8_lit("height")).number;
            Json_Array walls = json_object_fetch(&sector_data, str8_lit("walls")).array;
            sector->num_walls = walls.values.count;
            sector->walls = arena_pushn(arena, Wall, sector->num_walls);
            u64 j = 0;
            for (Json_Value_Node *wall_node = walls.values.first; wall_node; wall_node = wall_node->next, ++j) {
                Json_Object wall_data = wall_node->value.object;
                sector->walls[j].p0.x = (f32)json_object_fetch(&wall_data, str8_lit("x1")).number;
                sector->walls[j].p0.y = (f32)json_object_fetch(&wall_data, str8_lit("y1")).number;
                sector->walls[j].p1.x = (f32)json_object_fetch(&wall_data, str8_lit("x2")).number;
                sector->walls[j].p1.y = (f32)json_object_fetch(&wall_data, str8_lit("y2")).number;
                sector->walls[j].next_sector = (s32)json_object_fetch(&wall_data, str8_lit("next sector")).number;
            }
        }
    } else {
        fprintf(stderr, "Error parsing level file!");
    }
    
    release_scratch(scratch);
    return map;
}