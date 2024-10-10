// Use dungeon create params like how fleury uses rectparams
#define rand_float() ((f32)rand()/(f32)RAND_MAX)

function void
generate_dungeon (Arena *arena, Dungeon_Params *params, Border_Array *debug_borders, Sector_Array *sectors_out) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    
    u64 node_count = 0;
    for (u64 i = 0; i <= params->depth; ++i) { node_count += (1 << i); } 
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
        
        /*
        So the way we handle the `min` sizes is kind of a lie, it's more of a "suggestion".
The algorithm would balloon in complexity and time if we had to strictly follow
the minimum size and then modify the new cell and loop over all cells and make sure they still
fit the min size. See how this could get very messy very quickly? Instead if the size doesn't hit
the requirements we flip the split direction and pray it comes out better. So far this looks
good so I'm gonna keep it (for now).
        */
        
        if (!leaf) {
            f32 split = rand_float();
            split = clamp(split, params->cell_split_bounds.first, params->cell_split_bounds.last);
            b32 split_horizontal = rand() % 2;
            
            // This is so we don't lock ourselves in an infinite loop by accident
            b32 resplit = false;
            new_cell:
            if (split_horizontal == true) {
                f32 split_y = lerp(current->p3.y, current->p0.y, split);
                if (!resplit && (current->p0.y - split_y < params->min_cell.height || 
                                 split_y - current->p3.y < params->min_cell.height)) {
                    split_horizontal = !(split_horizontal & 1);
                    resplit = true;
                    goto new_cell;
                }
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
                if (!resplit && (current->p1.x - split_x < params->min_cell.width || 
                                 split_x - current->p0.x < params->min_cell.width)) {
                    split_horizontal = !(split_horizontal & 1);
                    resplit = true;
                    goto new_cell;
                }
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
            
            // Calculate bottom-left point and adjust x & y based on min room size
            f32 width  = current->p1.x - current->p0.x;
            f32 height = current->p0.y - current->p3.y;
            f32 min_width = width < params->min_room.width ? width : params->min_room.width;
            f32 min_height = height < params->min_room.height ? height : params->min_room.height;
            Vec2 anchor = v2(width, height);
            Vec2 remaining = v2sub(v2(width, height), anchor);
            while (remaining.width < min_width || remaining.height < min_height) {
                anchor = v2(rand_float() * width, rand_float() * height);
                remaining = v2sub(v2(width, height), anchor);
            }
            anchor  = v2add(anchor, current->p3);
            remaining = v2sub(remaining, v2(min_width, min_height));
            f32 rand_height = rand_float() * remaining.height;
            f32 rand_width  = rand_float() * remaining.width;
            Vec2 p0 = v2add(anchor, v2(0, min_height + rand_height));
            Vec2 p1 = v2add(p0, v2(min_width + rand_width, 0));
            Vec2 p2 = v2(p1.x, anchor.y);
            Vec2 p3 = anchor;
            sectors[j].top    = (Border){p0, p1};
            sectors[j].bottom = (Border){p3, p2};
            sectors[j].left   = (Border){p0, p3};
            sectors[j].right  = (Border){p1, p2};
            
            // @todo: Color should be stored per-sector instead of per-border so we don't need to do this
            for (u64 b = 0; b < 4; ++b) {
                sectors[j].borders[b].color = Color_Cyan;
            }
            
            current->child_sector = sectors + j;
            sectors_out->count++;
        }
    }
    
    release_scratch(scratch);
}