#include <stdio.h>

#include "base/include.h"
#include "os/include.h"
#include "base/include.c"
#include "os/include.c"

#define COLOR_TYPE_COUNT 6
#define BIT_DEPTH_COUNT 16

// 2 ^ Max code length  
#define HUFFMAN_PRIMARY_TABLE_SIZE 512     // 9 bits
#define HUFFMAN_SECONDARY_TABLE_SIZE 64    // 6 bits

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

#if 0
// https://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art007
typedef struct Huffman_Table Huffman_Table;
typedef struct Huffman_Table_Entry Huffman_Table_Entry;

struct Huffman_Table {
    Huffman_Table_Entry *array;
    u64 count;
};

struct Huffman_Table_Entry {
    u16 code;
    u16 length;
    Huffman_Table *ptr;
};
#endif

// https://www.researchgate.net/publication/4014140_Direct_Huffman_coding_and_decoding_using_the_table_of_code-lengths
typedef struct Compact_Huffman_Table {
    int placeholder;
} Compact_Huffman_Table;

typedef struct Huffman_Data {
    u32 length;
    u32 symbol;
} Huffman_Data;

global read_only u8 png_valid_color_config[COLOR_TYPE_COUNT+1][BIT_DEPTH_COUNT+1] = {
    {0,1,1,0,1,0,0,0,1,0,0,0,0,0,0,0,1},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1},
    {0,1,1,0,1,0,0,0,1,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1}
};

core_function u32
peek_bits (Bit_Stream *stream, u32 count) {
    while (stream->count < count) {
        stream->buffer |= (u32)*stream->cursor++ << stream->count;
        stream->count += 8;
    }
    
    return stream->buffer & ((1u << count)-1);
}

core_function u32
consume_bits (Bit_Stream *stream, u32 count) {
    // @slow
    u32 result = peek_bits(stream, count);
    stream->buffer >>= count;
    stream->count -= count;
    
    return result;
}

// @slow This function is literally loop haven
core_function Compact_Huffman_Table
huffman_make (Arena *arena, u32 max_code_length, u32 num_codes, Huffman_Data *data) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    Compact_Huffman_Table result = zero_struct;
    
    // Generate keys
    u32 *num_symbols = arena_pushn(scratch.arena, u32, max_code_length+1);
    u32 unique_length_count = 0;
    for (u32 i = 0; i < num_codes; ++i) { 
        if (data[i].length != 0) { 
            unique_length_count += num_symbols[data[i].length]++ == 0 ? 1 : 0;
        } 
    }
    u32 *unique_lengths_ordered = arena_pushn(scratch.arena, u32, unique_length_count);
    u32 *tdcl = arena_pushn(scratch.arena, u32, unique_length_count);
    u32 unique_idx = 0;
    u32 code_sum = 0;
    for (u32 i = 1; i <= max_code_length; ++i) { 
        if (num_symbols[i] > 0) {
            unique_lengths_ordered[unique_idx] = i;
            tdcl[unique_idx] = i - code_sum;
            code_sum += tdcl[unique_idx];
            ++unique_idx;
        }
    }
    
    // The tricky thing about sorting here, is not only do we need to sort the alphabet
    // by each symbol's code length, but we also need to ensure that numbers with the same
    // code length are in the same order
    u32 alphabet_size = (max_code_length+1)*num_codes;
    s32 *alphabet = arena_pushn(scratch.arena, s32, alphabet_size);
    u32 *entries_logged = arena_pushn(scratch.arena, u32, max_code_length+1);
    u32 used_codes = num_codes;
    memory_init(alphabet, alphabet_size * sizeof(s32), -1);
    for (u32 i = 0; i < num_codes; ++i) {
        if (data[i].length > 0) {
            u32 j = entries_logged[data[i].length]++;
            alphabet[data[i].length * num_codes + j] = data[i].symbol;
        } else {
            used_codes--;
        }
    }
    u32 *sorted_alphabet = arena_pushn(scratch.arena, u32, used_codes);
    u32 sorted_idx = 0;
    for (u32 i = 0; i < alphabet_size; ++i) {
        if (alphabet[i] > -1) sorted_alphabet[sorted_idx++] = alphabet[i];
    }
    
    u32 *codes = arena_pushn(scratch.arena, u32, used_codes);
    u32 k = 0, j = 0;
    for (u32 i = 0; i < unique_length_count; ++i) {
        k <<= tdcl[i];
        codes[j] = k;
        j++;
        u32 symbol_count = num_symbols[unique_lengths_ordered[i]];
        if (symbol_count > 1) {
            for (u32 m = 1; m < symbol_count; ++m) {
                k++;
                codes[j] = k;
                j++;
            }
        }
        k++;
    }
    
    //-- dump
    printf("I hope this works\n");
    for (u32 i = 0; i < used_codes; ++i) {
        printf("Symbol: %d, mapped to: "BYTE_TO_BINARY_PATTERN"\n", sorted_alphabet[i], byte_to_binary(codes[i]));
    }
    
    // Create huffman structure
    
    release_scratch(scratch);
    return result;
}
// Code dump

// https://www.zlib.net/feldspar.html
// https://datatracker.ietf.org/doc/html/rfc1951
// https://datatracker.ietf.org/doc/html/rfc1950
core_function String8
png_zlib_inflate (Arena *arena, String8 deflated) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    
    String8 inflated = zero_struct;
    inflated.str = arena_pushn(arena, u8, 0);
    
    Zlib_Header zlib = *(Zlib_Header*)deflated.str;
    u8 method = (zlib.cmf & 0xF);
    u8 info = ((zlib.cmf & 0xF0) >> 4); // base-2 logarithm of the LZ77 window size, minus eight
    if (method == 8 && info <= 7 && !check_bit(zlib.flg, 5)) {
        Bit_Stream compression_stream = zero_struct;
        compression_stream.cursor = deflated.str + sizeof(Zlib_Header);
        u8 final = 0, type = 0;
        while (!final) {
            final = consume_bits(&compression_stream, 1);
            type = consume_bits(&compression_stream, 2);
            if (type == Zlib_No_Compression) {
                u16 len = consume_bits(&compression_stream, 16);
                u16 nlen = consume_bits(&compression_stream, 16); unused(nlen);
                u8 *dst = arena_pushn(arena, u8, len);
                memory_copy(dst, compression_stream.cursor, len);
                compression_stream.cursor += len;
                continue;
            } else if (type == Zlib_Dynamic_Compression) {
                // Read huffman code trees 
                u16 hlit  = consume_bits(&compression_stream, 5) + 257;
                u8 hdist  = consume_bits(&compression_stream, 5) + 1;
                u8 hclen  = consume_bits(&compression_stream, 4) + 4;
                
                local_persist u32 huffman_code_length_sequences[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
                u32 num_code_lengths = array_count(huffman_code_length_sequences);
                Huffman_Data *code_length_data = arena_pushn(scratch.arena, Huffman_Data, num_code_lengths);
                for (u8 i = 0; i < num_code_lengths; ++i) {
                    if (i < hclen)
                        code_length_data[i].length = consume_bits(&compression_stream, 3);
                    code_length_data[i].symbol = huffman_code_length_sequences[i];
                }
                // @todo: Make this take an array of Huffman_Datas instead
                Compact_Huffman_Table code_length_table = huffman_make(scratch.arena, 7, hclen, code_length_data);
            } else {
                fputs("I have yet to come across a png with static huffman codes. If you come across this message, you've got some work to do!\n", stderr);
                goto exit;
            }
            
            // decoding...
#if 0
            while (!end_of_block) {
                int value = ;// decode literal/length value from input stream
                if (value < 256) {
                    // Copy value (literal byte) to output stream
                } else if (value == 256) {
                    // end of block, break from loop
                } else if (value > 256) {
                    // Decode distance from input stream
                    // Move backwards distance bytes in output stream
                    // Copy length bytes from this position to output stream
                }
            }
#endif
        }
        
        // Blah blah blah then process Adler32
    } else {
        fputs("Invalid compression configuration!\n", stderr);
    }
    
    exit:
    release_scratch(scratch);
    return inflated;
}

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
        
        // Process chunks
        PNG_Critical_Data critical_data = zero_struct;
        critical_data.compressed_data = arena_pushn(scratch.arena, u8, 0);
        
        // @todo: Is all of this error checking unnecessary? What are the odds we get fed an invalid PNG file anyway?
        for (PNG_Chunk_Node *n = chunks.first; n; n = n->next) {
            PNG_Chunk chunk = n->chunk;
            if (!chunk.ancillary) {
                if (chunk.type.code == fourcc("IHDR")) {
                    if (n != chunks.first) {
                        fputs("PNG decode error! IHDR is not first!\n", stderr);
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
                    
                    if (!png_valid_color_config[critical_data.color_type][critical_data.bit_depth]) {
                        fputs("PNG decode error! Invalid color configuration!\n", stderr);
                        goto exit;
                    }
                } else if (chunk.type.code == fourcc("PLTE")) {
                    critical_data.palette_entry_count = chunk.length / 3;
                    critical_data.palette_entries = (PNG_Palette_Entry*)chunk.data;
                    
                    if (critical_data.bit_depth == 0 || critical_data.compressed_data_size != 0) {
                        fputs("PNG decode error! Incorrect ordering of palette chunk!\n", stderr);
                        goto exit;
                    }
                    
                    if (critical_data.color_type == PNG_Grayscale || critical_data.color_type == PNG_Grayscale_Alpha || critical_data.palette_present) {
                        fputs("PNG decode error! Unnecessary palette chunk!\n", stderr);
                        goto exit;
                    }
                    if (chunk.length % 3 != 0) {
                        fputs("PNG decode error! Invalid palette depth!\n", stderr);
                        goto exit;
                    }
                    if (critical_data.palette_entry_count > (1 << critical_data.bit_depth)) {
                        fputs("PNG decode error! palette entries exceed bit depth!\n", stderr);
                        goto exit;
                    }
                    critical_data.palette_present = true;
                } else if (chunk.type.code == fourcc("IDAT")) {
                    if (critical_data.compression != 0 || critical_data.filter != 0) {
                        fputs("PNG decode error! Unrecognized compression/filter mode\n", stderr);
                        goto exit;
                    }
                    u8 *c = arena_pushn(scratch.arena, u8, chunk.length);
                    memory_copy(c, chunk.data, chunk.length);
                    critical_data.compressed_data_size += chunk.length;
                } else if (chunk.type.code == fourcc("IEND")) {
                    if (n != chunks.last) {
                        fputs("PNG decode error! IEND is not last!\n", stderr);
                        goto exit;
                    }
                    critical_data.end_chunk_processed = true;
                } else {
                    fputs("PNG decode error! Unrecognized critical chunk!\n", stderr);
                    goto exit;
                };
                
            } else {
                
            }
        }
        
        String8 inflated_pixel_data = png_zlib_inflate(scratch.arena, str8(critical_data.compressed_data, critical_data.compressed_data_size));
        
        // Do we even need to care about filtering
        
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
    PNG_Bitmap_RGBA parsed_data = png_decode(scratch.arena, png_data);
    
    release_scratch(scratch);
    return 0;
}