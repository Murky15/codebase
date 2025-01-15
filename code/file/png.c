#include <stdio.h>

#define ENABLE_ASSERT 1
#define DEBUG 1
#include "base/include.h"
#include "os/include.h"
#include "base/include.c"
#include "os/include.c"

#define COLOR_TYPE_COUNT 6
#define BIT_DEPTH_COUNT 16

// 2 ^ Max code length  
#define HUFFMAN_PRIMARY_TABLE_SIZE 512     // 9 bits
#define HUFFMAN_SECONDARY_TABLE_SIZE 64    // 6 bits

#define HUFFMAN_CODE_LENGTH_MAX_CODE_SIZE  7
#define HUFFMAN_MAX_CODE_SIZE 15

#define LZ77_NUM_LEN_CODES  29
#define LZ77_NUM_DIST_CODES 30

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

// https://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art007
// Ok so I know that by only having a single level huffman table we become memory hogs and whatnot,
// but at the end of the day, while it is a lot of memory we reserve a ton on the scratch arena
// so it shouldn't be a problem. Besides, it'll all be cleared at the end of the function anyway.
// It's time to get piggy
typedef struct Huffman_Table Huffman_Table;
typedef struct Huffman_Table_Entry Huffman_Table_Entry;

struct Huffman_Table {
    Huffman_Table_Entry *array;
    u64 count;
};

struct Huffman_Table_Entry {
    u16 symbol;
    u16 length;
    // Huffman_Table *ptr;
};

typedef struct Huffman_Data {
    u16 symbol;
    u16 length;
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

global read_only u8 extra_length_bits[LZ77_NUM_LEN_CODES] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};

global read_only u8 extra_distance_bits[LZ77_NUM_DIST_CODES] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
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

core_function void
discard_bits (Bit_Stream *stream, u32 count) {
    stream->buffer >>= count;
    stream->count -= count;
}

core_function Huffman_Table
huffman_make_very_big (Arena *arena, u32 max_code_length, u32 num_codes, Huffman_Data *data) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    Huffman_Table result = zero_struct;
    
    u32 *num_symbols = arena_pushn(scratch.arena, u32, max_code_length+1);
    u32 *next_code = arena_pushn(scratch.arena, u32, max_code_length+1);
    u32 *codes = arena_pushn(scratch.arena, u32, num_codes);
    for (u32 i = 0; i < num_codes; ++i) { num_symbols[data[i].length]++; }
    num_symbols[0] = 0;
    u32 code = 0;
    for (u32 bits = 1; bits <= max_code_length; ++bits) {
        code = (code + num_symbols[bits-1]) << 1;
        next_code[bits] = code;
    }
    for (u32 n = 0; n < num_codes; ++n) {
        u32 length = data[n].length;
        if (length != 0) {
            codes[n] = next_code[length];
            next_code[length]++;
        }
    }
    
    u32 max_possible_code = (1 << max_code_length);
    result.count = max_possible_code;
    result.array = arena_pushn(arena, Huffman_Table_Entry, max_possible_code);
    for (u32 i = 0; i < num_codes; ++i) {
        if (data[i].length > 0) {
            Huffman_Table_Entry to_add = comp_lit(Huffman_Table_Entry, .symbol = data[i].symbol, .length = data[i].length);
            u32 len_diff = max_code_length - to_add.length;
            u32 code = codes[i];
            u32 flipped = 0;
            u32 bit_idx = to_add.length;
            while (code > 0) {
                flipped |= (code & 0x1) << --bit_idx;
                code >>= 1;
            }
            flipped <<= len_diff;
            printf("%d, %d, "BYTE_TO_BINARY_PATTERN", flipped: "BYTE_TO_BINARY_PATTERN"\n", data[i].symbol, data[i].length, byte_to_binary(codes[i]), byte_to_binary(flipped));
            result.array[flipped] = to_add;
        }
    }
    
    Huffman_Table_Entry last_entry = zero_struct;
    for (u32 i = 0; i < max_possible_code; ++i) {
        Huffman_Table_Entry *curr_entry = &result.array[i];
        if (curr_entry->length == 0)
            *curr_entry = last_entry;
        else
            last_entry = *curr_entry;
        //printf("Table["BYTE_TO_BINARY_PATTERN"] = S: %d, L: %d\n", byte_to_binary(i), curr_entry->symbol, curr_entry->length);
    }
    
    release_scratch(scratch);
    return result;
}

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
            
            Huffman_Table litlen_table = zero_struct;
            Huffman_Table dist_table = zero_struct;
            
            if (type == Zlib_No_Compression) {
                u16 len = consume_bits(&compression_stream, 16);
                discard_bits(&compression_stream, 16);
                u8 *dst = arena_pushn(arena, u8, len);
                memory_copy(dst, compression_stream.cursor, len);
                compression_stream.cursor += len;
                continue;
            } else if (type == Zlib_Dynamic_Compression) {
                // Read huffman code trees 
                u16 hlit  = consume_bits(&compression_stream, 5) + 257;
                u8 hdist  = consume_bits(&compression_stream, 5) + 1;
                u8 hclen  = consume_bits(&compression_stream, 4) + 4;
                
                //- @note Code length huffman table
                local_persist u32 huffman_code_length_sequences[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
                u32 num_code_lengths = array_count(huffman_code_length_sequences);
                Huffman_Data *code_length_data = arena_pushn(scratch.arena, Huffman_Data, num_code_lengths);
                for (u32 i = 0; i < hclen; ++i) {
                    code_length_data[i].length = consume_bits(&compression_stream, 3);
                    code_length_data[i].symbol = huffman_code_length_sequences[i];
                }
                Huffman_Table code_length_table = huffman_make_very_big(scratch.arena, HUFFMAN_CODE_LENGTH_MAX_CODE_SIZE, hclen, code_length_data);
                
                //- @note: codes
                u32 *code_lengths = arena_pushn(scratch.arena, u32, hlit + hdist);
                u32 current_sym = 0;
                for (u32 i = 0; i < hlit + hdist;) {
                    u8 code = peek_bits(&compression_stream, HUFFMAN_CODE_LENGTH_MAX_CODE_SIZE);
                    printf("Code: "BYTE_TO_BINARY_PATTERN"\n", byte_to_binary(code));
                    Huffman_Table_Entry entry = code_length_table.array[code];
                    consume_bits(&compression_stream, 7);
                    switch (entry.symbol) {
                        case 16: {
                            u8 rep_count = consume_bits(&compression_stream, 2) + 3;
                            for (u8 j = i; j < rep_count; ++j) {
                                code_lengths[j] = current_sym;
                            }
                            i += rep_count;
                        } break;
                        
                        case 17: {
                            u8 rep_count = consume_bits(&compression_stream, 3) + 3;
                            for (u8 j = i; j < rep_count; ++j) {
                                code_lengths[j] = 0;
                            }
                            i += rep_count;
                        } break;
                        
                        case 18: {
                            u8 rep_count = consume_bits(&compression_stream, 7) + 11;
                            for (u8 j = i; j < rep_count; ++j) {
                                code_lengths[j] = 0;
                            }
                            i += rep_count;
                        } break;
                        
                        default: {
                            assert(entry.symbol >= 0 && entry.symbol <= 15);
                            current_sym = code_lengths[i] = entry.symbol;
                            i++;
                        } break;
                    }
                }
                Huffman_Data *litlen_code_data = arena_pushn(scratch.arena, Huffman_Data, hlit);
                for (u32 i = 0; i < hlit; ++i) {
                    litlen_code_data[i].length = code_lengths[i];
                    litlen_code_data[i].symbol = i;
                }
                litlen_table = huffman_make_very_big(scratch.arena, HUFFMAN_MAX_CODE_SIZE, hlit, litlen_code_data);
                
                Huffman_Data *dist_code_data = arena_pushn(scratch.arena, Huffman_Data, hdist);
                for (u32 i = 0; i < hdist; ++i) {
                    dist_code_data[i].length = code_lengths[hlit + i];
                    dist_code_data[i].symbol = i;
                }
                dist_table = huffman_make_very_big(scratch.arena, HUFFMAN_MAX_CODE_SIZE, hdist, dist_code_data);
            } else {
                fputs("I have yet to come across a png with static huffman codes. If you come across this message, you've got some work to do!\n", stderr);
                goto exit;
            }
            
            while (true) {
                u32 code = peek_bits(&compression_stream, HUFFMAN_MAX_CODE_SIZE);
                Huffman_Table_Entry entry = litlen_table.array[code];
                consume_bits(&compression_stream, entry.length);
                u32 value = entry.symbol;
                if (value < 256) {
                    // Copy value (literal byte) to output stream
                    u8 *head = arena_pushn(arena, u8, 1);
                    *head = (u8)value;
                    inflated.len++;
                } else if (value > 256) {
                    u32 dist_code = peek_bits(&compression_stream, HUFFMAN_MAX_CODE_SIZE);
                    Huffman_Table_Entry dist_entry = dist_table.array[dist_code];
                    consume_bits(&compression_stream, dist_entry.length);
                    
                    u32 dist = dist_entry.symbol;
                    dist += consume_bits(&compression_stream, extra_distance_bits[dist]);
                    u64 length = (value - 254);
                    length += consume_bits(&compression_stream, extra_length_bits[length]);
                    
                    u64 range_start = inflated.len - dist;
                    String8 data = str8_sub(inflated, range_start, min(length, inflated.len));
                    // Copy length bytes from this position to output stream
                    u8 *dest = arena_pushn(arena, u8, length);
                    for (u64 i = 0; i < length; ++i) {
                        u64 wrapped_idx = i % dist;
                        dest[i] = data.str[wrapped_idx];
                    }
                    inflated.len += length;
                } else if (value == 256) {
                    break;
                }
            }
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
                printf("PNG: No ancillary chunks are supported yet!\n");
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
    //String8 png_data = os_read_file(scratch.arena, str8_lit("W:/code/file/png_tests/guy.png"), false);
    PNG_Bitmap_RGBA parsed_data = png_decode(scratch.arena, png_data);
    
    release_scratch(scratch);
    return 0;
}