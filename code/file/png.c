#include "base/include.h"
#include "os/include.h"
#include "base/include.c"
#include "os/include.c"

// PNG Bitmap *always* outputs to RGBA format
typedef struct PNG_Bitmap_RGBA {
    u64 width;
    u64 height;
    u32 *pixels;
} PNG_Bitmap_RGBA;

typedef struct PNG_Chunk {
    u32 length;
    union {
        u32 code;
        u8 c[4];
    } type;
    b32 ancillary;
    u8* data;
} PNG_Chunk;

typedef struct PNG_Chunk_Node {
    struct PNG_Chunk_Node *next;
    PNG_Chunk chunk;
} PNG_Chunk_Node;

typedef struct PNG_Chunk_List {
    PNG_Chunk_Node *first;
    PNG_Chunk_Node *last;
    u64 count;
} PNG_Chunk_List;

typedef union PNG_Palette_Entry {
    struct {
        u8 r,g,b;
    };
    u8 e[3];
} PNG_Palette_Entry;

enum {
    PNG_Grayscale = 0,
    PNG_RGB = 2,
    PNG_Palette = 3,
    PNG_Grayscale_Alpha = 4,
    PNG_RGBA = 6
};

enum {
    PNG_Interlace_Null = 0,
    PNG_Interlace_Adam7 = 1
};

typedef struct PNG_Critical_Data {
    u32 width;
    u32 height;
    u8 color_type;
    u8 bit_depth;
    u8 compression;
    u8 filter;
    u8 interlace;
    
    b8 palette_present;
    u32 palette_entry_count;
    PNG_Palette_Entry *palette_entries;
    
    u8 *raw_processed_data;
} PNG_Critical_Data;

// [color type][bit depth] @todo: Can we make this more efficient?
global read_only png_valid_color_config[7][17] = {
    [0,1,1,0,1,0,0,0,1,0,0,0,0,0,0,0,1],
    [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1],
    [0,1,1,0,1,0,0,0,1,0,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1],
    [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1]
};

core_function void
png_chunk_list_push (Arena *arena, PNG_Chunk_List *list, PNG_Chunk chunk) {
    PNG_Chunk_Node *node = arena_pushn(arena, PNG_Chunk_Node, 1);
    node->chunk = chunk;
    sll_queue_push(list->first, list->last, node);
    list->count++;
}

core_function PNG_Bitmap_RGBA
png_decode (Arena *arena, String8 png_data) {
    PNG_Bitmap_RGBA result = zero_struct;
    Temp_Arena scratch = get_scratch(&arena, 1);
    u8 *c = png_data.str;
    
    // Parse file
    local_persist read_only u8 png_header[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (memory_match(png_header, png_data.str, 8)) {
        PNG_Chunk_List chunks = zero_struct;
        c += array_count(png_header);
        for (;c != png_data.str + png_data.len;) {
            PNG_Chunk chunk = zero_struct;
            chunk.length = be_to_le32(c); c += sizeof(u32);
            chunk.type.code = *(u32*)c; c += sizeof(u32);
            chunk.ancillary = check_bit(chunk.type.c[0], 5);
            chunk.data = c;
            png_chunk_list_push(scratch.arena, &chunks, chunk);
            
            c += chunk.length;
            c += sizeof(u32); // Because this decoder is local, we can skip the CRC
        }
        
#if 0
        // Chunk dump
        for (PNG_Chunk_Node *n = chunks.first; n; n = n->next) {
            PNG_Chunk chunk = n->chunk;
            printf("Type: %.*s\tSize: %d\tAncillary: %d\n", 4, chunk.type.c, chunk.length, chunk.ancillary);
        }
#endif
        
        // Read chunks
        PNG_Critical_Data critical_data = zero_struct;
        critical_data.raw_processed_data = arena_pushn(scratch.arena, u8, 0);
        for (PNG_Chunk_Node *n = chunks.first; n; n = n->next) {
            PNG_Chunk chunk = n->chunk;
            if (!chunk.ancillary) {
                if (memory_match(chunk.type.c, "IHDR")) { // Always comes first
                    if (n != chunks.first) {
                        fprintf(stderr, "PNG decode error! IHDR is not first!\n");
                        goto exit;
                    }
                    
                    u8 *c = chunk.data;
                    critical_data.width = be_to_le32(c);  c += sizeof(u32);
                    critical_data.height = be_to_le32(c); c += sizeof(u32);
                    critical_data.bit_depth   = *c++;
                    critical_data.color_type  = *c++;
                    critical_data.compression = *c++;
                    critical_data.filter      = *c++;
                    critical_data.interlace   = *c;
                    
                    if (!png_valid_color_config[ctritical_data.color_type][critical_data.bit_depth]) {
                        fprintf(stderr, "PNG decode error! Invalid color configuration!\n");
                        goto exit;
                    }
                    if (critical_data.compression != 0 || critical_data.filter != 0) {
                        fprintf(stderr, "PNG decode error! Unrecognized compression/filter mode\n");
                        goto exit;
                    }
                    
                } else if (memory_match(chunk.type.c, "PLTE")) {
                    critical_data.palette_entry_count = chunk.length / 3;
                    critical_data.palette_entries = (PNG_Palette_Entry*)chunk.data;
                    
                    if (critical_data.bit_depth == 0 || *critical_data.raw_processed_data != 0) {
                        fprintf(stderr, "PNG decode error! Incorrect ordering of palette chunk!\n");
                        goto exit;
                    }
                    
                    if (critical_data.color_type == PNG_Grayscale || critical_data.color_type == PNG_Grayscale_Alpha || critical_data.palette_present) {
                        fprintf(stderr, "PNG decode error! Unnecessary palette chunk!\n");
                        goto exit;
                    }
                    if (chunk.length % 3 != 0) {
                        fprintf(stderr, "PNG decode error! Invalid palette depth!\n");
                        goto exit;
                    }
                    if (critical_data.palette_entry_count > (1 << critical_data.bit_depth)) {
                        fprintf(stderr, "PNG decode error! palette entries exceed bit depth!\n");
                        goto exit;
                    }
                    critical_data.palette_present = true;
                    
                } else if (memory_match(chunk.type.c, "IDAT")) {
                    
                    
                    
                } else if (memory_match(chunk.type.c, "IEND")) {
                    
                } else {
                    fprintf(stderr, "PNG decode error! Unrecognized critical chunk!\n");
                    goto exit;
                }
            } else {
                
            }
        }
        
    } else {
        fprintf(stderr, "Supplied data is not a valid PNG!\n");
    }
    
    exit:
    release_scratch(scratch);
    return result;
}

int
main (void) {
    Temp_Arena scratch = get_scratch(0,0);
    String8 png_data = os_read_file(scratch.arena, str8_lit("W:/assets/dumb/art/tileset/0x72_DungeonTilesetII_v1.7.png"), false);
    PNG_Bitmap parsed_data = png_decode(scratch.arena, png_data);
    
    release_scratch(scratch);
    return 0;
}