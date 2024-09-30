function void
generate_dungeon (Arena *arena, Dungeon_Create_Params *params) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    BSP_Node *root = arena_pushn(scratch.arena, BSP_Node, 1);
    unused(root);
    release_scratch(scratch);
}