// Use dungeon create params like how fleury uses rectparams
function Border_Array
generate_dungeon (Arena *arena, Dungeon_Params *params) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    
    Border *borders = (Border*)((u8*)arena + round_up_pow2(arena->pos, align_of(Border)));
    Border_Array result = {borders};
    
    u64 node_count = 0;
    for (u64 i = 0; i <= params->depth; ++i) { node_count += pow(2, i); } 
    BSP_Node *tree = arena_pushn(scratch.arena, BSP_Node, node_count);
    BSP_Node *root = &tree[0];
    
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
            b32 split_horizontal = rand() % 2;
            if (split_horizontal == true) {
                f32 split_y = lerp(current->p3.y, current->p0.y, (f32)rand()/(f32)RAND_MAX);
                left_child->p0 = current->p0;
                left_child->p1 = current->p1;
                left_child->p2 = v2(current->p2.x, split_y);
                left_child->p3 = v2(current->p3.x, split_y);
                
                right_child->p0 = left_child->p3;
                right_child->p1 = left_child->p2;
                right_child->p2 = current->p2;
                right_child->p3 = current->p3;
            } else {
                f32 split_x = lerp(current->p0.x, current->p1.x, (f32)rand()/(f32)RAND_MAX);
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
            Border *new_borders = arena_pushn(arena, Border, 4);
            for (int i = 0; i < 4; ++i) {
                new_borders[i].p0 = current->p[i];
                new_borders[i].p1 = current->p[(i+1)%4];
                new_borders[i].color = Color_Red;
                result.count++;
            }
        }
    }
    release_scratch(scratch);
    
    return result;
}