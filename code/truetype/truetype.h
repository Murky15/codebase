#ifndef TRUETYPE_H
#define TRUETYPE_H

//- @note: Font directory handling 

typedef struct Offset_Subtable {
    u32 scaler;
    u16 num_tables;
    u16 search_range;
    u16 entry_selector;
    u16 range_shift;
} Offset_Subtable;

typedef struct Table_Directory {
    union {
        u8 tag_c[4];
        u32 tag;
    };
    u32 checksum;
    u32 offset;
    u32 len;
} Table_Directory;

typedef struct Font_Directory {
    Offset_Subtable off_sub;
    Table_Directory *table_dir;
} Font_Directory;

//- @note: Table structures

typedef struct Cmap_Subtable {
    // id & extension are usually only (0, 3) so that is all I am focusing on supporting
    u16 id;
    u16 extension;
    u32 offset;
} Cmap_Subtable;

typedef struct Cmap {
    u16 version; 
    u16 num_subtables; 
    Cmap_Subtable *subtables;
} Cmap;

#endif //TRUETYPE_H
