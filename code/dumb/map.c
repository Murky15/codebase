function void
sector_list_push_ref (Arena *arena, Sector_List *list,  Sector *ref) {
    Sector_Ref *node = arena_pushn(arena, Sector_Ref, 1);
    node->sector = ref;
    sll_queue_push(list->first, list->last, node);
    list->count++;
}

// @todo: Textures should be stored per-wall for more flexibility
// @todo: Hella annoying editing this file by hand
function Map
map_load (Arena *arena, String8 path) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    Map map = {0};
    
    String8 json = os_read_file(scratch.arena, path, false);
    Json_Value level_data = json_parse(scratch.arena, json);
    if (level_data.type > 0) {
        map.name = json_fetch_str(&level_data.object, str8_lit("name"));
        Json_Array sectors = json_fetch_arr(&level_data.object, str8_lit("sectors"));
        map.num_sectors = sectors.values.count;
        map.sectors = arena_pushn(arena, Sector, map.num_sectors);
        for (Json_Value_Node *sector_node = sectors.values.first; sector_node; sector_node = sector_node->next) {
            Json_Object sector_data = sector_node->value.object;
            s32 id = json_fetch_num(&sector_data, s32, str8_lit("id"));
            Sector *sector = map.sectors + id;
            sector->id = id;
            sector->floor = json_fetch_num(&sector_data, s32, str8_lit("floor"));
            sector->ceiling = json_fetch_num(&sector_data, s32, str8_lit("ceiling"));
            Json_Array walls = json_fetch_arr(&sector_data, str8_lit("walls"));
            sector->num_walls = walls.values.count;
            sector->walls = arena_pushn(arena, Wall, sector->num_walls);
            u64 j = 0;
            for (Json_Value_Node *wall_node = walls.values.first; wall_node; wall_node = wall_node->next, ++j) {
                Json_Object wall_data = wall_node->value.object;
                sector->walls[j].p0.x = json_fetch_num(&wall_data, f32, str8_lit("x1"));
                sector->walls[j].p0.y = json_fetch_num(&wall_data, f32, str8_lit("y1"));
                sector->walls[j].p1.x = json_fetch_num(&wall_data, f32, str8_lit("x2"));
                sector->walls[j].p1.y = json_fetch_num(&wall_data, f32, str8_lit("y2"));
                sector->walls[j].next_sector = json_fetch_num(&wall_data, s32, str8_lit("next sector"));
                
                s32 pot_adj_id = sector->walls[j].next_sector;
                if (pot_adj_id >= 0) {
                    Sector *adj = map.sectors + pot_adj_id;
                    sector_list_push_ref(arena, &sector->adjacent, adj);
                }
            }
        }
    } else {
        fprintf(stderr, "Error parsing level file!\n");
    }
    
    release_scratch(scratch);
    return map;
}

function b32
wall_intersect (Vec2 p, Vec2 dir, Wall w) {
    b32 result = 0;
    Vec2 a = v2sub(p, w.p0);
    Vec2 b = v2sub(w.p1, w.p0);
    Vec2 c = v2(-dir.y, dir.x);
    
    f32 d = v2dot(b, c);
    f32 t1 = v2cross(b, a) / d;
    f32 t2 = v2dot(a, c) / d;
    result = (t1 >= 0 && t2 >= 0 && t2 <= 1);
    
    return result;
}

function b32 
point_in_sector (Vec2 p, Sector *sector) {
    u64 num_intersections = 0;
    for (u64 widx = 0; widx < sector->num_walls; ++widx) {
        Wall *w = &sector->walls[widx];
        if (wall_intersect(p, V2_Up, *w)) num_intersections++;
    }
    
    return ((num_intersections % 2) != 0);
}

function void
update_current_sector (Entity *entity, Map *map) {
    // First check if we haven't moved
    Sector *curr_sector = &map->sectors[entity->curr_sector];
    if (point_in_sector(entity->pos, curr_sector)) return;
    
    // Check connecting sectors
    for (u64 w = 0; w < curr_sector->num_walls; ++w) {
        Wall *wall = &curr_sector->walls[w];
        Sector *sector =  &map->sectors[wall->next_sector];
        if (wall->next_sector >= 0 && point_in_sector(entity->pos, sector)) {
            entity->curr_sector = sector->id;
            return;
        }
    }
    
    // Linearly search (where did our entity go??)
    for (u64 s = 0; s < map->num_sectors; ++s) {
        Sector *sector = &map->sectors[s];
        if (point_in_sector(entity->pos, sector)) {
            entity->curr_sector = sector->id;
            return;
        }
    }
}