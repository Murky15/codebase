// Use dungeon create params like how fleury uses rectparams
function void
generate_dungeon (Arena *arena, Dungeon_Params *params, Border_Array *debug_borders, Sector_Array *sectors_out) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    
    u64 node_count = 0;
    for (u64 i = 0; i <= params->depth; ++i) { node_count += pow(2, i); } 
    BSP_Node *tree = arena_pushn(scratch.arena, BSP_Node, node_count);
    
    u64 num_leaves = (1 << params->depth);
    Sector *sectors = arena_pushn(arena, Sector, num_leaves);
    sectors_out->sectors = sectors;
    
    // @debug
    Border *borders = arena_pushn(arena, Border, num_leaves * 4);
    debug_borders->borders = borders;
    
    BSP_Node *root = &tree[0];
    
    // Build dungeon grid
    root->p0 = v2(0, params->size.y);
    root->p1 = v2(params->size.x, params->size.y);
    root->p2 = v2(params->size.x, 0);
    root->p3 = v2(0, 0);
    for (u64 i = 0; i < node_count; ++i) {
        u64 lidx = 2*i + 1;
        u64 ridx = 2*i + 2;
        b32 leaf = ((lidx >= node_count) || (lidx >= node_count));
        BSP_Node *current =  &tree[i];
        BSP_Node *left_child = leaf ? 0 : &tree[lidx];
        BSP_Node *right_child = leaf ? 0 : &tree[ridx];
        
        if (!leaf) {
            f32 split = (f32)rand()/(f32)RAND_MAX;
            split = clamp(split, 0.45f, 0.55f);
            
            b32 split_horizontal = rand() % 2;
            if (split_horizontal == true) {
                f32 split_y = lerp(current->p3.y, current->p0.y, split);
                left_child->p0 = current->p0;
                left_child->p1 = current->p1;
                left_child->p2 = v2(current->p2.x, split_y);
                left_child->p3 = v2(current->p3.x, split_y);
                
                right_child->p0 = left_child->p3;
                right_child->p1 = left_child->p2;
                right_child->p2 = current->p2;
                right_child->p3 = current->p3;
            } else {
                f32 split_x = lerp(current->p0.x, current->p1.x, split);
                left_child->p0 = current->p0;
                left_child->p1 = v2(split_x, current->p1.y);
                left_child->p2 = v2(split_x, current->p2.y);
                left_child->p3 = current->p3;
                
                right_child->p0 = left_child->p1;
                right_child->p1 = current->p1;
                right_child->p2 = current->p2;
                right_child->p3 = left_child->p2;
            }
        } else {
            // @debug
            for (u64 j = 0; j < 4; ++j) {
                u64 idx = ((i - (num_leaves - 1)) * 4) + j; // Someone did not cook here
                borders[idx].p0 = current->p[j];
                borders[idx].p1 = current->p[(j+1)%4];
                borders[idx].color = Color_Red;
                debug_borders->count++;
            }
            
            u64 j = i - (num_leaves - 1);
            
            /*
                        f32 width = current->p1.x - current->p0.x;
                        f32 height = current->p0.y - current->p3.y;
                        f32 rand_width  = ((f32)rand() / (f32)RAND_MAX) * width;
                        f32 rand_height = ((f32)rand() / (f32)RAND_MAX) * height;
                        f32 offx = (width - rand_width) / 2.f;
                        f32 offy = (height - rand_height) / 2.f;
                        
                        sectors[j].top.p0 = v2(current->p0.x + offx, current->p0.y - offy);
                        sectors[j].top.p1 = v2(current->p1.x - offx, current->p1.y - offy);
                        sectors[j].bottom.p0 = v2(current->p3.x + offx, current->p3.y + offy);
                        sectors[j].bottom.p1 = v2(current->p2.x - offx, current->p2.y + offy);
                        sectors[j].left.p0 = sectors[j].top.p0;
                        sectors[j].left.p1 = sectors[j].bottom.p0;
                        sectors[j].right.p0 = sectors[j].top.p1;
                        sectors[j].right.p1 = sectors[j].bottom.p1;
                        */
            
            for (u64 b = 0; b < 4; ++b) {
                sectors[j].borders[b].color = Color_Cyan;
            }
            
            current->child_sector = sectors + j;
            sectors_out->count++;
        }
    }
    
    release_scratch(scratch);
}