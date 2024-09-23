// Following this tutorial: https://handmade.network/forums/articles/t/7330-implementing_a_font_reader_and_rasterizer_from_scratch%252C_part_1__ttf_font_reader.
// might wanna refactor this eventually cause this tutorial kinda sucks

#define ttf_read_be16(buff) ((((u8*)(buff))[0] << 8) | (((u8*)(buff))[1]))
#define ttf_read_be32(buff) ((((u8*)(buff))[0] << 24) | (((u8*)(buff))[1] << 16) | (((u8*)(buff))[2] << 8) | (((u8*)(buff))[3]))
#define ttf_p_move(buff, a) ((buff) += (a))
#define ttf_read_be16_move(buff) (ttf_read_be16((buff))); (ttf_p_move((buff), 2))
#define ttf_read_be32_move(buff) (ttf_read_be32((buff))); (ttf_p_move((buff), 4))

core_function void
ttf_read_offset_subtable (u8 **buff, Offset_Subtable *off_sub) {
    u8 *c = *buff;
    off_sub->scaler         = ttf_read_be32_move(c);
    off_sub->num_tables     = ttf_read_be16_move(c);
    off_sub->search_range   = ttf_read_be16_move(c);
    off_sub->entry_selector = ttf_read_be16_move(c);
    off_sub->range_shift    = ttf_read_be16_move(c);
    *buff = c;
}

core_function void
ttf_read_table_directory (Arena *arena, u8 **buff, Table_Directory **table_dir, u16 num_tables) {
    u8 *c = *buff;
    *table_dir = arena_pushn(arena, Table_Directory, num_tables);
    for (u16 i = 0; i < num_tables; ++i) {
        Table_Directory *t = *table_dir + i;
        t->tag      = ttf_read_be32_move(c);
        t->checksum = ttf_read_be32_move(c);
        t->offset   = ttf_read_be32_move(c);
        t->len      = ttf_read_be32_move(c);
    }
    *buff = c;
}

core_function void
ttf_print_table_directory (Table_Directory *table_dir, u16 num_tables) {
    printf("#)\ttag\tlen\toffset\n");
    for (u16 i = 0; i < num_tables; ++i) {
        Table_Directory *t = table_dir + i;
        printf("%d)\t%c%c%c%c\t%d\t%d\n",
               i+1,
               t->tag_c[3], t->tag_c[2], t->tag_c[1], t->tag_c[0],
               t->len,
               t->offset);
    }
}

core_function void
ttf_read_font_directory (Arena *arena, u8 **buff, Font_Directory *font_dir) {
    u8 *restore = *buff;
    ttf_read_offset_subtable(buff, &font_dir->off_sub);
    ttf_read_table_directory(arena, buff, &font_dir->table_dir, font_dir->off_sub.num_tables);
    *buff = restore;
}

core_function void
ttf_read_cmap (Arena *arena, u8 *buff, Cmap *cmap) {
    u8 *c = buff;
    cmap->version = ttf_read_be16_move(c);
    cmap->num_subtables = ttf_read_be16_move(c);
    
    cmap->subtables = arena_pushn(arena, Cmap_Subtable, cmap->num_subtables);
    for (u16 i = 0; i < cmap->num_subtables; ++i) {
        Cmap_Subtable *est = cmap->subtables + i;
        est->id = ttf_read_be16_move(c);
        est->extension = ttf_read_be16_move(c);
        est->offset = ttf_read_be32_move(c);
    }
} 

core_function void
ttf_print_cmap (Cmap *cmap) {
    printf("#)\tid\text\toff\ttype\n");
    for (u16 i = 0; i < cmap->num_subtables; ++i) {
        Cmap_Subtable *est = cmap->subtables + i;
        printf("%d)\t%d\t%d\t%d\t", i+1, est->id, est->extension, est->offset);
        switch (est->id) {
            case 0: printf("Unicode"); break;
            case 1: printf("Mac"); break;
            case 2: printf("--Reserved--"); break;
            case 3: printf("Microsoft"); break;
        }
        printf("\n");
    }
}