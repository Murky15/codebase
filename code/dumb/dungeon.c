// Use dungeon create params like how fleury uses rectparams
function void
generate_dungeon (Arena *arena) {
    Temp_Arena *scratch = get_scratch(&arena, 1);
    BSP_Node *root = arena_pushn(scratch, BSP_Node, 1);
    BSP_Node *current = root;
    while (??) {
        BSP_Node *additions = arena_pushn(scratch, BSP_Node, 2);
        current->left = additions[0];
        current->right = additions[1];
    }
    release_scratch(scratch);
}