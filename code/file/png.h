#ifndef PNG_H
#define PNG_H

#define COLOR_TYPE_COUNT 6
#define BIT_DEPTH_COUNT 16

#define HUFFMAN_CODE_LENGTH_MAX_CODE_SIZE  7
#define HUFFMAN_MAX_CODE_SIZE 15
#define HUFFMAN_PRIMARY_TABLE_SIZE_THRESHOLD 9
#define HUFFMAN_SECONDARY_TABLE_SIZE_THRESHOLD 6
#define LZ77_NUM_LEN_CODES  29
#define LZ77_NUM_DIST_CODES 30
#define ADLER32_BASE 65521 // Largest prime smaller than 65536

//- @note: Output types 
typedef struct PNG_Bitmap_RGBA {
    u64 width;
    u64 height;
    u32 *pixels;
} PNG_Bitmap_RGBA;

//- @note: Internal data
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

enum {
    Zlib_No_Compression = 0,
    Zlib_Static_Compression = 1,
    Zlib_Dynamic_Compression = 2,
};

typedef struct Zlib_Header {
    u8 cmf;
    u8 flg;
} Zlib_Header;

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
    
    u8 *compressed_data;
    u32 compressed_data_size;
    
    b8 end_chunk_processed;
} PNG_Critical_Data;

typedef struct Bit_Stream {
    u64 count;
    u64 buffer;
    u8 *cursor;
} Bit_Stream;

typedef struct Huffman_Dict {
    u32Array sorted_symbols;
    u32Array lengths;
    s32Array table;
    
    u32 max_code_length;
    u32 *codes_per_length;
} Huffman_Dict;

//- @note: Public API
core_function PNG_Bitmap_RGBA png_decode(Arena *arena, String8 png_data);

//- @note: Bitstream helpers
core_function u32 peek_bits(Bit_Stream *stream, u32 count);
core_function u32 consume_bits(Bit_Stream *stream, u32 count);
core_function u32 reverse_bits(u32 val, u32 count);

//- @note: Zlib decompression
core_function Huffman_Dict huffman_make(Arena *arena, u32Array lengths, u32 max_code_length);
core_function s32 huffman_decode_next(Bit_Stream *stream, Huffman_Dict huff);
core_function s32 huffman_decode_next(Bit_Stream *stream, Huffman_Dict huff);
core_function b32 validate_adler32(u32 adler, String8 inflated);
core_function String8 png_zlib_inflate(Arena *arena, String8 deflated);

//- @note: PNG filter functions
core_function s32 png_paeth_predictor(s32 a, s32 b, s32 c);
core_function String8 png_reverse_filters(Arena *arena, PNG_Critical_Data *png, String8 filtered);

//- @note: Misc
core_function void png_chunk_list_push(Arena *arena, PNG_Chunk_List *list, PNG_Chunk chunk);

#endif //PNG_H