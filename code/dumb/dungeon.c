// Use dungeon create params like how fleury uses rectparams
function void
generate_dungeon (Arena *arena, Dungeon_Params *params) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    u64 node_count = 0;
    for (u64 i = 0; i <= params->depth; ++i) { node_count += pow(2, i); } 
    BSP_Node *tree = arena_pushn(scratch.arena, BSP_Node, node_count);
    BSP_Node *root = &tree[0];
    // Setup world borders (@todo can we clean this up?)
    f32 half_width  = params->size.x / 2.f;
    f32 half_height = params->size.y / 2.f;
    Border *map_boundaries = tree[0].borders;
    map_boundaries[0].p0 = v2(-half_width,  half_height);
    map_boundaries[0].p1 = v2( half_width,  half_height);
    map_boundaries[1].p0 = v2( half_width,  half_height);
    map_boundaries[1].p1 = v2( half_width, -half_height);
    map_boundaries[2].p0 = v2( half_width, -half_height);
    map_boundaries[2].p1 = v2(-half_width, -half_height);
    map_boundaries[3].p0 = v2(-half_width, -half_height);
    map_boundaries[3].p1 = v2(-half_width,  half_height);
    
    // Run BSP algorithm
    for (u64 i = 1; i < node_count; ++i) {
        u64 lidx = 2*i + 1;
        u64 ridx = 2*i + 2;
        BSP_Node *current =  &tree[i];
        BSP_Node *left_child = &tree[lidx];
        BSP_Node *right_child = &tree[ridx];
        
        b32 split_horizontal = lcg_next(0) % 2;
        if (split_horizontal == true) {
            f32 height = current->l.p0.y - current->l.p1.y;
            f32 split_y = fmod_cycling(lcg_next(0), height);
            if (lidx < node_count) {
                
            }
        } else {
            
        }
    }
    
    release_scratch(scratch);
}